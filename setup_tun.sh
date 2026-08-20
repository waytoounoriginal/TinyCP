#!/usr/bin/env bash
# Starter script to create and bring up the virtual TUN interface (tun0) for TinyCP stack.
set -e

DEV_NAME="${1:-tun0}"
IP_ADDR="${2:-10.0.0.1/24}"

echo "Configuring TUN interface '$DEV_NAME' with IP '$IP_ADDR'..."

ip link show "$DEV_NAME" &>/dev/null || sudo ip tuntap add dev "$DEV_NAME" mode tun
sudo ip addr add "$IP_ADDR" dev "$DEV_NAME" 2>/dev/null || true
sudo ip link set dev "$DEV_NAME" up

for f in ip_forward; do
    sudo sysctl -qw net.ipv4.$f=1
done

for f in rp_filter accept_local; do
    sudo sysctl -qw net.ipv4.conf.all.$f=$([[ $f == rp_filter ]] && echo 0 || echo 1)
    sudo sysctl -qw net.ipv4.conf."$DEV_NAME".$f=$([[ $f == rp_filter ]] && echo 0 || echo 1)
done

for f in send_redirects accept_redirects; do
    sudo sysctl -qw net.ipv4.conf.all.$f=0
    sudo sysctl -qw net.ipv4.conf."$DEV_NAME".$f=0
done

# Detect default internet/external interface
EXT_IF=$(ip route show default 2>/dev/null | awk '{print $5}' | head -n1)
if [ -n "$EXT_IF" ]; then
    echo "Configuring NAT and forwarding via external interface '$EXT_IF'..."
    
    # Masquerade outbound traffic from 10.0.0.0/24
    sudo iptables -t nat -C POSTROUTING -s 10.0.0.0/24 -o "$EXT_IF" -j MASQUERADE 2>/dev/null || \
        sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o "$EXT_IF" -j MASQUERADE

    # Allow forwarding between TUN and external interface
    sudo iptables -C FORWARD -i "$DEV_NAME" -o "$EXT_IF" -j ACCEPT 2>/dev/null || \
        sudo iptables -A FORWARD -i "$DEV_NAME" -o "$EXT_IF" -j ACCEPT

    sudo iptables -C FORWARD -i "$EXT_IF" -o "$DEV_NAME" -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null || \
        sudo iptables -A FORWARD -i "$EXT_IF" -o "$DEV_NAME" -m state --state RELATED,ESTABLISHED -j ACCEPT
fi

PORT_FORWARD="${3:-}"
if [ -n "$PORT_FORWARD" ] && [ -n "$EXT_IF" ]; then
    echo "Configuring inbound port forward for port $PORT_FORWARD -> 10.0.0.2:$PORT_FORWARD..."
    sudo iptables -t nat -C PREROUTING -i "$EXT_IF" -p tcp --dport "$PORT_FORWARD" -j DNAT --to-destination 10.0.0.2:"$PORT_FORWARD" 2>/dev/null || \
        sudo iptables -t nat -A PREROUTING -i "$EXT_IF" -p tcp --dport "$PORT_FORWARD" -j DNAT --to-destination 10.0.0.2:"$PORT_FORWARD"
    sudo iptables -C FORWARD -i "$EXT_IF" -o "$DEV_NAME" -p tcp --dport "$PORT_FORWARD" -j ACCEPT 2>/dev/null || \
        sudo iptables -A FORWARD -i "$EXT_IF" -o "$DEV_NAME" -p tcp --dport "$PORT_FORWARD" -j ACCEPT
fi

echo "TUN interface '$DEV_NAME' is active, UP, and routed via '$EXT_IF'."