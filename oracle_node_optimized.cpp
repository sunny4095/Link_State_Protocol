#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define IP_ADDR_ORACLE "172.25.153.68"

using namespace std;

int main() {
    int num_nodes = 5;
    ifstream infile("config.txt");
    vector<vector<pair<int,int>>> adj;//(num_nodes);
    vector<vector<int>> rows;
    string line;
    int cur_line = 0;
    while (getline(infile, line)) {
        stringstream ss(line);
        vector<int> row;
        int k;
        while (ss >> k){
            row.push_back(k);
        }

        if (row.empty()) {
            cur_line++;
            continue;
        }
        int _len = row.size();
        for (int i=0; i<_len; i++) {
            int u = cur_line;
            int v = cur_line+i+1;
            int cost = row[i];

            if (cost != -1) {
                adj[u].push_back({v,cost});
                adj[v].push_back({u,cost});
            }
        }
        cur_line++;
    }

    // debug code. Print adjacency list
    // cout << "Adjacency List:\n";
    // for (int i = 0; i < (int)adj.size(); ++i) {
    //     cout << i << ": ";
    //     for (auto &p : adj[i])
    //         cout << "(" << p.first << ", " << p.second << ") ";
    //     cout << "\n";
    // }

    int oracle_fd;
    struct sockaddr_in oracle_server;
    
    // CREATE ORACLE SOCKET
    oracle_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (oracle_fd == 0) {
        perror("oracle socket failed");
        return EXIT_FAILURE;
    }

    // BIND THE SOCKET
    oracle_server.sin_family = AF_INET;
    oracle_server.sin_port = htons(PORT);
    oracle_server.sin_addr.s_addr = inet_addr(IP_ADDR_ORACLE);
    if (bind(oracle_fd, (struct sockaddr *) &oracle_server, sizeof(oracle_server)) < 0) {
        perror("binding the oracle socket failed");
        return EXIT_FAILURE;
    }

    // LISTEN ON THE PORT
    if (listen(oracle_fd, num_nodes) < 0) {
        perror("Listening on oracle socket failed");
        return EXIT_FAILURE;
    }
    cout << "Oracle Node Listening on Port : " << PORT << endl;

    // ACCEPT and READ
    int connections_established=0, connections_read = 0;
    // socketfd -> <ipaddr,udpport>
    unordered_map<int, pair<string,int>> client_info;  // socketfd -> <ipaddr,udpport>
    vector<int> client_sockets(num_nodes, -1);
    fd_set read_fd;
    int max_fd;
    while (connections_read < num_nodes) {
        FD_ZERO(&read_fd);
        FD_SET(oracle_fd, &read_fd);
        max_fd = oracle_fd;
        for (int i=0; i<num_nodes; i++) {
            if (client_sockets[i] != -1) {
                FD_SET(client_sockets[i], &read_fd);
            }
            if (client_sockets[i] > max_fd) {
                max_fd = client_sockets[i];
            }
        }
        int n = select(max_fd+1, &read_fd, NULL, NULL, NULL);
        if (n < 0) {
            perror("Selecting Failed");
            return EXIT_FAILURE;
        }
        
        // If oracle fd is set, we have a new connection
        if (FD_ISSET(oracle_fd, &read_fd)) {
            struct sockaddr_in client_addr;
            int addr_size = sizeof(client_addr);
            int new_socket_fd = accept(oracle_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addr_size);
            if (new_socket_fd < 0) {
                perror("Accepting new connection failed");
                return EXIT_FAILURE;
            }
            cout << "Accepted Connection From IP Addr :" <<  inet_ntoa(client_addr.sin_addr) << "in Port" << client_addr.sin_port << endl;
            client_sockets[connections_established++] = new_socket_fd;
        }
        
        // if a socket fd is set we are ready to read
        for (int i=0; i<num_nodes; i++) {
            int client_fd = client_sockets[i];
            if (client_fd != -1 && FD_ISSET(client_fd, &read_fd)) {
                char buffer[BUFFER_SIZE];
                memset(buffer, '\0', BUFFER_SIZE);

                recv(client_fd, buffer, BUFFER_SIZE, 0);

                // cout << buffer << endl;

                // store information about virtual node
                {
                    char ipaddr[BUFFER_SIZE];
                    int index = 0;
                    while (buffer[index] != ','){
                        ipaddr[index] = buffer[index];
                        index++;
                    }
                    int length = index;
                    ipaddr[length] = '\0';
                    index++;
                    int start = index;
                    char udpport[BUFFER_SIZE];
                    while (buffer[index] != '\0'){
                        udpport[index - start] = buffer[index];
                        index++;
                    }
                    udpport[index - start] = '\0';

                    int udp_port = atoi(udpport);
                    string ipaddrstr = ipaddr;
                    client_info[client_fd] = {ipaddrstr, udp_port};
                    connections_read++;
                    cout << "Client" << i << " has info :";
                    cout << ipaddrstr << " " << udp_port << endl;
                }

            }
        }
    }

    // send the link state information
    for (int i=0; i<num_nodes; i++) {
        int fd = client_sockets[i];
        auto [ipaddr, port] = client_info[fd];
        // send (node alphabet, ip addr, udp port, cost) of neighbors
        
        // send info of curr node
        string msg = "";
        msg += to_string(i) + "," + ipaddr + "," + to_string(port) + "," + to_string(0);

        for (auto [adj_node, cost] : adj[i]){
            int adj_fd = client_sockets[adj_node];
            auto [adjip, adjport] = client_info[adj_fd];
            msg += ";" + to_string(adj_node) + "," + adjip + "," 
            + to_string(adjport) + "," + to_string(cost);
        }
        msg[msg.length()] = '\0';
        send(fd, msg.c_str(), msg.length(), 0);        
    }

    // if file changes, convey that information
    
}