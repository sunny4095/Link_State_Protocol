import socket
from select import select
from time import time

PERIODIC_FLOOD = 20

# IP_ADDR_ORACLE = input("Enter IP Address of Oracle : ")
# TCP_PORT_ORACLE = input("Enter TCP Port of Oracle : ")
# IP_ADDR = input("Enter my IP Address : ")
UDP_PORT = int(input("Enter My UDP Port :"))
IP_ADDR_ORACLE = "127.0.0.1"
IP_ADDR = "127.0.0.1"
TCP_PORT_ORACLE = 5000

tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_socket.connect((IP_ADDR_ORACLE, TCP_PORT_ORACLE))
tcp_socket.send(f"{IP_ADDR},{UDP_PORT}".encode())
link_state_info = tcp_socket.recv(1024).decode()

# for global information
adj_list = [[-1 for _ in range(26)] for _ in range(26)]

# process configuration message from oracle node, return self_id, neighbour_info and ip_port_to_id
def process_link_state_info(link_state_info):
    info = link_state_info.split(';')
    self_id = int(info[0].split(',')[0])
    neighbour_info = {}
    ip_port_to_id = {}

    for other_node_info in info[1:]:
        neigh_id, neigh_ip, neigh_udp_port, cost = other_node_info.split(',')
        neigh_id = int(neigh_id)
        neigh_udp_port = int(neigh_udp_port)
        cost = int(cost)
        neighbour_info[neigh_id] = (neigh_ip, neigh_udp_port)
        ip_port_to_id[(neigh_ip, neigh_udp_port)] = neigh_id

        # store cost into global information structure
        adj_list[self_id][neigh_id] = cost
        adj_list[neigh_id][self_id] = cost

    return self_id, neighbour_info, ip_port_to_id

self_id, neighbour_info, ip_port_to_id = process_link_state_info(link_state_info)

# seq num of last msg seen from that node, for other nodes, and curr seq num for this node
seq_num = {}
seq_num[self_id] = 0

udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_socket.bind((IP_ADDR, UDP_PORT))

# function to generate LSP
def generateLSP():
    packet = f"{self_id}|{seq_num[self_id]}|{adj_list[self_id]}"
    seq_num[self_id] += 1
    return packet

# function to flood LSPs to neighbours
def flood():
    print("flooding packets")
    packet = generateLSP()
    for neigh in neighbour_info:
        udp_socket.sendto(packet.encode(), neighbour_info[neigh])
    
def handle_update_ls():
    info, addr = udp_socket.recvfrom(1024)
    info = info.decode()
    id, seq, data = info.split('|')
    id, seq, data = int(id), int(seq), eval(data)

    print(f"received packet from {id} with seq num {seq}")
    
    # if packet old, return
    if id in seq_num and seq_num[id] >= seq:
        print("dropped the old packet")
        return
    
    # if packet new, update adj_list, and the latest seq_num we've seen from that guy
    adj_list[id] = data
    seq_num[id] = seq
    
    # obtain sender id
    sender_id = ip_port_to_id[addr]

    print("forwarding this packet to neighbours except sender")
    # forward packet to every neighbour except the one you received it from
    for neigh in neighbour_info:
        if neigh == sender_id:
            continue
        udp_socket.sendto(info.encode(), neighbour_info[neigh])
    
    return

def get_curr_size(adj_list):
    l = 0
    r = 25

    ans = r

    while l <= r:
        m = (l + r) // 2
        row_null = True
        for i in range(26):
            if adj_list[m][i] != -1:
                row_null = False
                break

        col_null = True
        for i in range(26):
            if adj_list[i][m] != -1:
                col_null = False
                break
        
        if row_null and col_null:
            ans = m
            r = m - 1
        else:
            l = m + 1

    return ans

flood()

last_time = time()

while True:
    # 1. listen on tcp port for topo changes
    # 2. listen on udp port for flooding messages
    # 3. periodically flood the network with self link state

    read_fds, _, _ = select([tcp_socket, udp_socket], [], [], PERIODIC_FLOOD)
    # printing current adjacency list
    curr_size = get_curr_size(adj_list)
    print("\n".join([" ".join(map(str, row[:curr_size])) for row in adj_list[:curr_size]]))

    curr_time = time()

    # timeout occurred, send flooding messages to neighbours
    if curr_time - last_time > PERIODIC_FLOOD:
        flood()
    else:
        # update link state of current node, adj_list and flood LSPs
        if tcp_socket in read_fds:
            adj_list[self_id] = [-1 for _ in range(26)]

            link_state_info = tcp_socket.recv(1024).decode()
            _, neighbour_info, ip_port_to_id = process_link_state_info(link_state_info)
            
            flood()

        # store info, forward if needed
        if udp_socket in read_fds:
            handle_update_ls()
        
    last_time = curr_time
