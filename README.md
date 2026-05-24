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

### Step 1 — Install the ANyONe client

Download and install the `anon` daemon from [anyone-protocol/ator-protocol/releases](https://github.com/anyone-protocol/ator-protocol/releases).

### Step 2 — Configure anonrc

Edit your `anonrc` file at `/etc/anon/anonrc`:

```
# Anon client-only
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

# Optional: expose RPC for wallets connecting over ANyONe
# HiddenServicePort 18089 127.0.0.1:18089
```

Restart the `anon` daemon:

```bash
sudo systemctl restart anon
```

Read your `.anyone` address:

```bash
sudo cat /var/lib/anon/monerod/hostname
# example output: abc123...xyz7654.anyone
```

---

### Step 3 — Install forked Monero

Create monero user and group:

```bash
sudo useradd --system monero
```

Create monero config, data and log directories:

```bash
sudo mkdir -p /etc/monero     # config
sudo mkdir -p /var/lib/monero # blockchain
sudo mkdir -p /var/log/monero # logs
sudo chown monero:monero /etc/monero
sudo chown monero:monero /var/lib/monero
sudo chown monero:monero /var/log/monero
```

Download and install:

```bash
wget https://github.com/xmranonp/monero/releases/download/v0.18.5-anon-1/monerod-anon-0.18.5.0-ubuntu24-amd64.deb
sudo dpkg -i monerod-anon-0.18.5.0-ubuntu24-amd64.deb
```

---

### Step 4 — Configure monerod

Edit `/etc/monero/monerod.conf` as shown below:

```ini
# /etc/monero/monerod.conf
#
# Configuration file for monerod. For all available options see the MoneroDocs:
# https://docs.getmonero.org/interacting/monerod-reference/

# Data directory (blockchain db and indices)
data-dir=/var/lib/monero/bitmonero

# Optional pruning - Saves 2/3 of disk space w/o degrading functionality
#prune-blockchain=1
# Allow downloading pruned blocks instead of pruning them yourself
#sync-pruned-blocks=1

# Centralized services
# Do not check DNS TXT records for a new version
check-updates=disabled
# Block known malicious nodes
enable-dns-blocklist=1

# Log file
log-file=/var/log/monero/monero.log
# Minimal logs, WILL NOT log peers or wallets connecting
log-level=0
# Set to 2GB to mitigate log trimming by monerod; configure logrotate instead
max-log-file-size=2147483648

# ANyONe: route outbound P2P and broadcast wallet transactions over ANyONe
# Format: tx-proxy=<zone>,<proxy-host>:<proxy-port>,<max-connections>
tx-proxy=anon,127.0.0.1:9050,16

# Advertise your .anyone address so peers can reach you inbound
# Format: anonymous-inbound=<your-address>:<port>,<local-bind>:<port>,<max-connections>
anonymous-inbound=PASTE_YOUR_ANYONE_HOSTNAME.anyone:18084,127.0.0.1:18084,16

# P2P bind locally
# Listen for P2P connections on localhost only (no clearnet exposure)
p2p-bind-ip=127.0.0.1
# Standard Monero P2P port
p2p-bind-port=18080

# RPC settings for daemon communication
# Bind restricted RPC interface to localhost only (no external access)
rpc-restricted-bind-ip=127.0.0.1
# Port for restricted RPC (safer, limited commands)
rpc-restricted-bind-port=18089

# Privacy flags
# Disable UPnP/IGD port mapping
no-igd=1
# Do not advertise your P2P port to peers in handshakes
hide-my-port=1

# Database sync mode
db-sync-mode=safe:sync

# Network limits
# Maximum number of outbound peer connections
out-peers=64
# Maximum number of inbound peer connections
in-peers=16
```

---

### Step 5 — Add peers manually

Since there are no ANyONe seed nodes yet, you must manually specify at least one known peer to bootstrap into the network.
Once connected, monerod discovers additional peers automatically via peerlist exchange.

Add peers directly:

```ini
# In /etc/monero/monerod.conf

add-peer=KNOWN_PEER_ADDRESS.anyone:18084           # Replace with a known .anyone peer address
#add-exclusive-node=<peer-address>.anyone:18084    # Use this instead if you want to connect ONLY to specific trusted nodes (not recommended for general use)
```

---

### Step 6 — Start the daemon

```bash
sudo systemctl start monerod
sudo systemctl status monerod
```

---

## Connecting a wallet over ANyONe

```bash
monero-wallet-cli \
  --proxy 127.0.0.1:9050 \
  --daemon-address abc123...xyz7654.anyone:18089
```

---

## Verifying it works

Check the monerod log for lines containing `zone=anon`:

```bash
sudo tail -f /var/log/monero/monero.log | grep anon
```

Test RPC reachability through ANyONe:

```bash
curl --socks5-hostname 127.0.0.1:9050 \
     http://abc123...xyz7654.anyone:18089/get_info
```

---

## Security notes

- Do **not** expose your clearnet IP alongside a `.anyone` node without `--hide-my-port`
- The `anon` daemon provides end-to-end encryption; SSL is redundant but harmless for RPC
- If no ANyONe peers are reachable, transactions queue until a peer becomes available — no clearnet fallback occurs

---

## License

Same as Monero — BSD 3-Clause. See [LICENSE](LICENSE).
