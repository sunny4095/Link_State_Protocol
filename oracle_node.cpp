#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 5000
#define BUFFER_SIZE 1024
#define IP_ADDR_ORACLE "127.0.0.1"

using namespace std;

vector<vector<int>> loadAdjacencyMatrix(const string &filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Error opening file: " + filename);
    }

    vector<vector<int>> upper;
    string line;

    while (getline(file, line)) {
        // trim leading whitespace (spaces/tabs)
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        if (line.empty() || line[0] == '#')
            continue; // skip comments and blanks

        stringstream ss(line);
        vector<int> row;
        string token;

        while (ss >> token) {
            row.push_back(stoi(token));
        }

        upper.push_back(row);
    }

    file.close();

    int n = upper.size() + 1; // since only upper triangle is given
    vector<vector<int>> adj(n, vector<int>(n, 0));

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int val = upper[i][j - i - 1];
            adj[i][j] = val;
            adj[j][i] = val;
        }
    }

    return adj;
}

vector<int> getComponents(string ipaddr){
    vector<int> ans;
    string current = "";
    for (char c : ipaddr){
        if (c == '.'){
            ans.push_back(stoi(current));
            current = "";
            continue;
        }

        current += c;
    }

    ans.push_back(stoi(current)); 
    return ans;   
}

void generate_link_state_message(unsigned char * buffer, int n, vector<vector<int>> &mat, unordered_map<int, pair<string,int>> &client_info, vector<int> &client_sockets) {
    // set to 0 initially
    memset((void *) buffer, 0, BUFFER_SIZE);
    
    int base_index = 0;
    buffer[base_index] = n;
    int fd = client_sockets[n];
    auto [ipaddr, port] = client_info[fd];

    vector<int> vecip = getComponents(ipaddr);
    int first = vecip[0];
    int second = vecip[1];
    int third = vecip[2];
    int fourth = vecip[3];

    buffer[base_index + 1] = first;
    buffer[base_index + 2] = second;
    buffer[base_index + 3] = third;
    buffer[base_index + 4] = fourth;

    buffer[base_index + 5] = port / 256;
    buffer[base_index + 6] = port % 256;

    buffer[base_index + 7] = 0;
    buffer[base_index + 8] = 0;

    base_index += 9;

    int num_nodes = mat.size();

    for (int adj_node=0; adj_node<num_nodes; adj_node++) {
        if (mat[n][adj_node] <= 0) continue;
        
        int cost = mat[n][adj_node];
        int adj_fd = client_sockets[adj_node];
        auto [adjip, adjport] = client_info[adj_fd];
        
        buffer[base_index] = adj_node;

        vector<int> vecip = getComponents(adjip);
        int first = vecip[0];
        int second = vecip[1];
        int third = vecip[2];
        int fourth = vecip[3];

        buffer[base_index + 1] = first;
        buffer[base_index + 2] = second;
        buffer[base_index + 3] = third;
        buffer[base_index + 4] = fourth;

        buffer[base_index + 5] = adjport / 256;
        buffer[base_index + 6] = adjport % 256;

        buffer[base_index + 7] = cost / 256;
        buffer[base_index + 8] = cost % 256;

        base_index += 9;
    }

    for (int index = base_index; index < base_index + 9; index++)
        buffer[index] = 255;
}

pair<string, int> getVNAddrPort(unsigned char * buffer){
    string first = to_string(buffer[0]);
    string second = to_string(buffer[1]);
    string third = to_string(buffer[2]);
    string fourth = to_string(buffer[3]);
    string ipaddr = first + "." + second + "." + third + "." + fourth;

    int udpport = buffer[4];
    udpport *= 256;
    udpport += (int) buffer[5];

    return {ipaddr, udpport};
}

// accept more nodes
int acceptMoreNodes(int oracle_fd, int old_size, vector<vector<int>> & new_adj, 
    unordered_map<int, pair<string,int>> & client_info,
    vector<int> & client_sockets){
    cout << "hiiii" << endl;
    int new_size = new_adj.size();
    int new_nodes = new_size - old_size;
    int connections_read = 0;
    int connections_established = 0;

    // accept new_nodes number of new connections
    fd_set read_fd;
    int max_fd;
    while (connections_read < new_nodes) {
        FD_ZERO(&read_fd);
        FD_SET(oracle_fd, &read_fd);
        max_fd = oracle_fd;
        for (int i=old_size; i<new_size; i++) {
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
            return -1;
        }
        
        // If oracle fd is set, we have a new connection
        if (FD_ISSET(oracle_fd, &read_fd)) {
            struct sockaddr_in client_addr;
            int addr_size = sizeof(client_addr);
            int new_socket_fd = accept(oracle_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addr_size);
            if (new_socket_fd < 0) {
                perror("Accepting new connection failed");
                return -1;
            }
            cout << "Accepted Connection From IP Addr :" <<  inet_ntoa(client_addr.sin_addr) << "in Port" << client_addr.sin_port << endl;
            client_sockets[old_size + connections_established++] = new_socket_fd;
        }
        
        // if a socket fd is set we are ready to read
        for (int i=old_size; i<new_size; i++) {
            int client_fd = client_sockets[i];
            if (client_fd != -1 && FD_ISSET(client_fd, &read_fd)) {
                unsigned char buffer[BUFFER_SIZE];
                memset(buffer, '\0', BUFFER_SIZE);

                recv(client_fd, (char *)buffer, BUFFER_SIZE, 0);

                // store information about virtual node
                client_info[client_fd] = getVNAddrPort(buffer);
                connections_read++;
                cout << "Client" << i << " has info :";
                cout << client_info[client_fd].first << " " << client_info[client_fd].second << endl;
            }
        }
    }

    // send link state to these nodes and their neighbours
    // find nodes that you need to send to
    vector<bool> active(new_size, false);
    for (int i = old_size; i < new_size; i++){
        active[i] = true;
        for (int j = 0; j < new_size; j++){
            if (new_adj[i][j] > 0) active[j] = true;
        }
    }

    // send the link state information
    for (int i=0; i<new_size; i++) {
        if (!active[i]) continue;
        int fd = client_sockets[i];
        unsigned char buffer[BUFFER_SIZE];
        generate_link_state_message(buffer, i, new_adj, client_info, client_sockets);
        send(fd, (char *) buffer, BUFFER_SIZE, 0);        
    }

    return 0;
}


int main(int argc, char * argv[]) {
    // check if filename was provided
    if (argc < 2){
        cout << "Usage : " << argv[0] << " <config-file>" << endl;
        return 1;
    }

    string filename = argv[1];

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    filesystem::path file = filename;
    auto last_write = filesystem::last_write_time(file);
    vector<vector<int>> adj = loadAdjacencyMatrix("config.txt");
    int num_nodes = adj.size();
    cout << "Adjacency Matrix:\n";
    for (auto &row : adj) {
        for (auto val : row)
            cout << setw(4) << val;
        cout << '\n';
    }

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
    // oracle_server.sin_addr.s_addr = inet_addr(IP_ADDR_ORACLE);
    inet_pton(AF_INET, IP_ADDR_ORACLE, &oracle_server.sin_addr);
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
    
    acceptMoreNodes(oracle_fd, 0, adj, client_info, client_sockets);

    // if file changes, convey that information
    while (true) {
        cout << "checking for modifications in config file\n";
        auto check_write = filesystem::last_write_time(file);
        if (check_write != last_write) {
            cout << "Config File Has been Modified\n";
            vector<vector<int>> new_adj = loadAdjacencyMatrix("config.txt");
            int new_size = new_adj.size();
            if (new_size > num_nodes){
                if (acceptMoreNodes(oracle_fd, num_nodes, new_adj, client_info, client_sockets) < 0){
                    cout << "Could not accept more nodes" << endl;
                    return -1;
                }
            }
            else {
                for (int i=0; i<num_nodes; i++) {
                    bool changed = false;
                    for (int j=0; j<num_nodes; j++) {
                        if (new_adj[i][j] != adj[i][j]) {
                            changed = true;
                            break;
                        }
                    }
                    if (changed) {
                        // send link state of i
                        int fd = client_sockets[i];
                        unsigned char buffer[BUFFER_SIZE];
                        generate_link_state_message(buffer, i, new_adj, client_info, client_sockets);
                        if (send(fd, (char *) buffer, BUFFER_SIZE, 0) < 0) {
                            perror("Sending failed");
                            return EXIT_FAILURE;
                        }
                    }
                }
            }
            adj = new_adj;
            last_write = check_write;
        }
        Sleep(10000);
    }

    closesocket(oracle_fd);
    WSACleanup();
}