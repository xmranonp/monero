# Monero daemon over the ANyONe Protocol
> A fork of [monero-project/monero](https://github.com/monero-project/monero) that adds native support for **ANyONe Protocol `.anyone` hidden services**, allowing monerod to route P2P traffic and accept inbound connections through the ANyONe onion-routing network.

---

## What is ANyONe Protocol?

[ANyONe Protocol](https://anyone.io) is a decentralized onion-routing network. Like Tor, it allows nodes to communicate privately through a layered encryption network. Hidden services on ANyONe use `.anyone` addresses — the same ED25519 cryptography as Tor v3 `.onion` addresses, with a different TLD and checksum.

ANyONe exposes a standard **SOCKS5 proxy interface**, which means it integrates with monerod the same way Tor does — no changes to the wallet or other tools are needed.

---

## What this fork adds

This fork adds `zone::anon` as a first-class anonymity network alongside Tor and I2P. The following files were modified or created:

| File | Change |
|------|--------|
| `src/net/anon_address.h` | **New** — `net::anon_address` class, mirrors `tor_address` |
| `src/net/anon_address.cpp` | **New** — parsing, validation, SOCKS5 connect, serialization |
| `contrib/epee/include/net/enums.h` | Added `zone::anon = 4` and `address_type::anon = 5` |
| `contrib/epee/include/net/net_utils_base.h` | Added `anon_address` forward declaration and serialization dispatch |
| `contrib/epee/src/net_utils_base.cpp` | Added `zone_to_string` and `zone_from_string` for `anon` |
| `src/net/CMakeLists.txt` | Added `anon_address.cpp` to the `net` library |
| `src/net/parse.cpp` | Added `.anyone` suffix detection in `get_network_address()` |
| `src/p2p/net_node.cpp` | Added `anon` zone parsing, inbound/outbound handling, SOCKS5 dispatch |
| `src/p2p/net_node.inl` | Updated `send_txs` loop guards and static asserts for `zone::anon` |
| `src/p2p/p2p_protocol_defs.h` | Added `#include "net/anon_address.h"` |

---

## Setup

### Step 1 — Install the ANyONe daemon

Download and install the `anon` daemon from [anyone-protocol/ator-protocol/releases](https://github.com/anyone-protocol/ator-protocol/releases).

### Step 2 — Configure anonrc

Create your anonrc file by running :

```
sudo tee /etc/anon/anonrc > /dev/null << 'EOF'
#Anon client-only
ORPort 0
DirPort 0
SocksPort 127.0.0.1:9050
SocksPolicy accept 127.0.0.1
SocksPolicy reject *

# Log
Log notice file /var/log/anon/notices.log
DataDirectory /var/lib/anon

# Hidden service for monerod P2P
HiddenServiceDir /var/lib/anon/monerod/
HiddenServicePort 18084 127.0.0.1:18084
HiddenServicePort 18089 127.0.0.1:18089
EOF
```

Restart `anon`:

```bash
sudo systemctl restart anon
```

Read your `.anyone` address:

```bash
sudo cat /var/lib/anon/monerod/hostname
```
---

### Step 3 — Install forked Monero daemon

Create monero user and group:

```bash
sudo useradd --system monero
```

Create monero config, data and log directories:

```bash
sudo mkdir -p /etc/monero    
sudo mkdir -p /var/lib/monero 
sudo mkdir -p /var/log/monero 
sudo chown monero:monero /etc/monero
sudo chown monero:monero /var/lib/monero
sudo chown monero:monero /var/log/monero
```

Download and install:

```bash
wget https://github.com/xmranonp/monero/releases/download/v0.18.5-anon-1/monerod-anon-0.18.5.0-ubuntu24-amd64.deb
sudo apt install ./monerod-anon-0.18.5.0-ubuntu24-amd64.deb
sudo systemctl daemon-reload
sudo systemctl enable --now monerod
```
---

### Step 4 — Configure monerod

Edit `/etc/monero/monerod.conf`:

```ini
# /etc/monero/monerod.conf
# Monero daemon fork
# Blockchain syncs over ANyONe network; transactions broadcast over ANyONe hidden services.
# https://docs.getmonero.org/interacting/monerod-reference/

# ── DATA ─────────────────────────────────────────────────────────────────────
data-dir=/home/monero/.bitmonero

# ── PRUNING ──────────────────────────────────────────────────────────────────
# Saves ~2/3 disk space with no loss of functionality - Comment both if you plan to run a full node.
prune-blockchain=1
sync-pruned-blocks=1

# ── SECURITY ─────────────────────────────────────────────────────────────────
check-updates=disabled
enable-dns-blocklist=1

# ── LOGGING ──────────────────────────────────────────────────────────────────
log-file=/var/log/monero/monero.log
log-level=0
max-log-file-size=2147483648

# ── BLOCKCHAIN SYNC OVER ANyONe ──────────────────────────────────────────────
# Routes all clearnet IPv4 sync traffic through the ANyONe SOCKS5 proxy.
proxy=127.0.0.1:9050

# ── TRANSACTION BROADCAST OVER ANyONe ────────────────────────────────────────
# Sends your own transactions and connects to .anyone peers via the ANyONe network.
tx-proxy=anon,127.0.0.1:9050,16,disable_noise

# Advertise this node's .anyone hidden service so peers can reach it inbound.
# Get your address with: sudo cat /var/lib/anon/monerod/hostname
anonymous-inbound=PASTE_YOUR_ADDRESS.anyone:18084,127.0.0.1:18084,16

# Keep a persistent connection to a known .anyone peer. 
add-priority-node=PASTE_PEER_ADDRESS.anyone:18084

# ── P2P BINDING ──────────────────────────────────────────────────────────────
# Bind to localhost only — no direct clearnet exposure
p2p-bind-ip=127.0.0.1
p2p-bind-port=18080
no-igd=1
hide-my-port=1

# ── RPC ──────────────────────────────────────────────────────────────────────
# Restricted RPC — reachable through the hidden service
rpc-restricted-bind-ip=127.0.0.1
rpc-restricted-bind-port=18089
# Unrestricted RPC — local only, for your own wallet
rpc-bind-ip=127.0.0.1
rpc-bind-port=18081

# ── MISC ─────────────────────────────────────────────────────────────────────
no-zmq=1
# Faster sync; Change to db-sync-mode=safe:sync when fully synced.
db-sync-mode=fast:async:250000000

```
---

### Step 5 — Start the daemon

```bash
sudo systemctl restart monerod
sudo systemctl status monerod
```

---

### Step 6 — Verifying it works

Check for leaks :

```bash
sudo ss -tnp | grep 9050
```

Test RPC reachability through ANyONe:

```bash
curl --socks5-hostname 127.0.0.1:9050 \
     http://abc123...xyz7654.anyone:18089/get_info
```
---

## Connecting a wallet over ANyONe

```bash
monero-wallet-cli \
  --proxy 127.0.0.1:9050 \
  --daemon-address abc123...xyz7654.anyone:18089
```
---

## Notes

- Blockchain sync does not run over `.anyone` hidden services (same as Tor/I2P — see Monero's [ANONYMITY_NETWORKS.md](https://github.com/monero-project/monero/blob/master/docs/ANONYMITY_NETWORKS.md)). -
- Sync happens over ANyONe network **exit nodes** via `proxy=`, while `.anyone` handles private transaction broadcast and inbound P2P.
- If no ANyONe peers are reachable, transactions queue until a peer becomes available — no clearnet fallback occurs
- Built on Ubuntu 24.04, amd64. Based on Monero v0.18.5.0 'Fluorine Fermi'.
---

