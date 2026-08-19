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

echo "TUN interface '$DEV_NAME' is active and UP."