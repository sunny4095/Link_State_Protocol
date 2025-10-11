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

int main(){
    std::cout << "Enter IP of node : ";
    string IP_ADDR_NODE;
    cin >> IP_ADDR_NODE;
    std:cout << std::endl;
    std::cout << "Enter Port of Node : ";
    short PORT_SELF;
    cin >> PORT_SELF;

    // create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        cout << "Socket creation error" << endl;
        return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IP_ADDR_ORACLE);
    serv_addr.sin_port = htons(PORT);
    
    // connect to server
    if (connect(sockfd, (struct sockaddr *) & serv_addr, sizeof(serv_addr)) < 0){
        perror("Connection Falied");
        return 1;
    }
   string buffer = IP_ADDR_NODE + "," + to_string(PORT_SELF);
    if (send(sockfd, buffer.c_str(), buffer.length(), 0) < 0) {
        cout << "sending failed\n";
        return -1;
    }
    
    // recieve link state information
    char recv_buffer[BUFFER_SIZE];
    memset(recv_buffer, '\0', BUFFER_SIZE);
    int sizeread = recv(sockfd, recv_buffer, BUFFER_SIZE, 0);
    if (sizeread < 0) {
        perror("recieving message from server failed");
        return EXIT_FAILURE;
    }
    // recv_buffer[sizeread] = '\0';
    cout << recv_buffer << endl;

}