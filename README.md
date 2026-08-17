# Link State Protocol ~ Bagadi Shashank (23b1040) and Marumamula Venkata Pranay (23b1073)

A simulation of the **Link State Routing Protocol** (as used in OSPF/IS-IS) using two kinds of processes:

- **Oracle node** (`oracle_node.cpp`) — a central process that knows the ground-truth network topology and hands out neighborhood information to each virtual node.
- **Virtual nodes** (`virtual_node.py`, `virtual_node.cpp`) — simulated routers that only know about the neighbors the oracle tells them about. They exchange Link State Packets (LSPs) with each other, flood them through the network, and use them to reconstruct the full topology so each node can compute shortest paths to every other node.

## How it works

1. **Topology setup.** The oracle reads a network topology from a config file (an upper-triangular adjacency matrix of link costs — `0`/blank means no link) and listens for incoming TCP connections from virtual nodes on port `5000`.
2. **Node registration.** Each virtual node connects to the oracle over TCP and announces its own UDP address/port. Once all expected nodes have registered, the oracle computes each node's direct neighbors and their link costs, and sends this back to each node as an initial link-state message.
3. **Neighbor discovery, not full topology.** A virtual node only ever learns its *own* neighbors from the oracle — it does not receive the whole network map. It must discover the rest of the network itself.
4. **Flooding link-state packets.** Each virtual node builds a Link State Packet (a fixed-size binary payload) describing its own ID, a sequence number, and the cost to each of its known neighbors, then sends it over UDP to all of its direct neighbors. On receiving a neighbor's LSP, a node:
   - drops it if it has already seen an equal-or-newer sequence number from that origin (staleness detection),
   - otherwise updates its local view of the network (its copy of the global adjacency matrix) and re-floods the packet to every neighbor except the one it arrived from (exclude-sender flooding).
   
   This is best-effort, UDP-based flooding — there is no acknowledgment or retransmission of individual lost packets. Consistency across the network relies on the periodic re-flood below rather than reliable delivery.
5. **Periodic re-flooding.** Nodes also re-flood their own LSP on a timer (every 20s), so the network eventually becomes consistent again even if earlier packets are lost.
6. **Live topology changes.** The oracle watches its config file for modifications. If links or nodes change, it detects the diff and pushes new neighbor information to the affected virtual nodes over TCP, which they use to update and re-flood their local state — so the simulated network can grow or have its link costs change at runtime.

## Files

| File | Description |
|---|---|
| `oracle_node.cpp` | The oracle process. Reads the topology config, accepts virtual node connections over TCP, distributes neighbor info, and pushes topology updates when the config file changes. (Windows/Winsock) |
| `virtual_node.py` | A virtual node implementation in Python. Connects to the oracle, floods/receives LSPs over UDP, maintains a local adjacency matrix of the discovered network. |
| `virtual_node.cpp` | A minimal C++ client used to register with the oracle over TCP and print incoming messages. (POSIX sockets) |

## Config file format

The oracle expects an adjacency matrix given as the **upper triangle only**, one row per line, whitespace-separated costs, `0` meaning no link. Lines starting with `#` are treated as comments.

```
# node0-1 node0-2 node0-3
5 3 0
# node1-2 node1-3
2 0
# node2-3
4
```

This describes a network with 4 nodes (0-3) and the given link costs.

## Running

Oracle node (Windows):
```
oracle_node.exe <config-file>
```

Virtual node (Python):
```
python virtual_node.py <ip-oracle> <port-oracle> <ip-vn> <port-vn>
```

Each virtual node connects to the oracle at `<ip-oracle>:<port-oracle>` and listens for LSP traffic on its own `<ip-vn>:<port-vn>` UDP endpoint.
