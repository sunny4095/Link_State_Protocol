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
ipaslist = [int(i) for i in IP_ADDR.split(".")]
ipaslist.extend([UDP_PORT // 256, UDP_PORT % 256])
tcp_socket.send(bytes(ipaslist))
link_state_info = tcp_socket.recv(1024)

# for global information
adj_list = [[0 for _ in range(26)] for _ in range(26)]

# process configuration message from oracle node, return self_id, neighbour_info and ip_port_to_id
def process_link_state_info(link_state_info):
    self_id = int(link_state_info[0])

    neighbour_info = {}
    ip_port_to_id = {}

    base_index = 9

    while base_index + 9 < len(link_state_info) and int(link_state_info[base_index]) != 255:
        neigh_id = int(link_state_info[base_index])
        first, second, third, fourth = link_state_info[base_index + 1:base_index + 5]
        first, second, third, fourth = int(first), int(second), int(third), int(fourth)
        neigh_ip = str(first) + "." + str(second) + "." + str(third) + "." + str(fourth)
        neigh_port = int(link_state_info[base_index + 5]) * 256 
        neigh_port += int(link_state_info[base_index + 6])
        cost = int(link_state_info[base_index + 7]) * 256
        cost += int(link_state_info[base_index + 8])

        neighbour_info[neigh_id] = (neigh_ip, neigh_port)
        ip_port_to_id[(neigh_ip, neigh_port)] = neigh_id

        # store cost into global information structure
        adj_list[self_id][neigh_id] = cost
        adj_list[neigh_id][self_id] = cost

        base_index += 9

    return self_id, neighbour_info, ip_port_to_id

self_id, neighbour_info, ip_port_to_id = process_link_state_info(link_state_info)

# seq num of last msg seen from that node, for other nodes, and curr seq num for this node
seq_num = {}
seq_num[self_id] = 0

udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_socket.bind((IP_ADDR, UDP_PORT))

# function to generate LSP, returns a bytes array
def generateLSP():
    packet = [0 for _ in range(55)]

    packet[0] = self_id
    packet[1] = seq_num[self_id] // 256
    packet[2] = seq_num[self_id] % 256

    base_index = 3
    for neigh in range(26):
        packet[base_index] = adj_list[self_id][neigh] // 256
        packet[base_index + 1] = adj_list[self_id][neigh] % 256
        base_index += 2

    seq_num[self_id] += 1
    print(packet)
    return bytes(packet)

# function to flood LSPs to neighbours
def flood():
    print("flooding packets")
    packet = generateLSP()
    for neigh in neighbour_info:
        udp_socket.sendto(packet, neighbour_info[neigh])
    
def handle_update_ls():
    info, addr = udp_socket.recvfrom(1024)

    id = int(info[0])
    seq = int(info[1]) * 256 + int(info[2])

    print(f"received packet from {id} with seq num {seq}")
    
    # if packet old, return
    if id in seq_num and seq_num[id] >= seq:
        print("dropped the old packet")
        return
    
    base_index = 3
    for neigh in range(26):
        # copy values from info to adj_list
        adj_list[id][neigh] = int(info[base_index]) * 256 + int(info[base_index + 1])
        base_index += 2

    # update highest seq num seen
    seq_num[id] = seq

    # obtain sender id
    sender_id = ip_port_to_id[addr]

    print("forwarding this packet to neighbours except sender")
    # forward packet to every neighbour except the one you received it from
    for neigh in neighbour_info:
        if neigh == sender_id:
            continue
        udp_socket.sendto(info, neighbour_info[neigh])
    
    return

def get_curr_size(adj_list):
    l = 0
    r = 25

    ans = r

    while l <= r:
        m = (l + r) // 2
        row_null = True
        for i in range(26):
            if adj_list[m][i] != 0:
                row_null = False
                break

        col_null = True
        for i in range(26):
            if adj_list[i][m] != 0:
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
            adj_list[self_id] = [0 for _ in range(26)]

            link_state_info = tcp_socket.recv(1024)
            _, neighbour_info, ip_port_to_id = process_link_state_info(link_state_info)
            
            flood()

        # store info, forward if needed
        if udp_socket in read_fds:
            handle_update_ls()
        
    last_time = curr_time
