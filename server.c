#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>      
#include <ws2tcpip.h>

#define PORT 8080
#define BUFFER_SIZE 104857600

#pragma comment(lib, "ws2_32.lib")
//AF_INET: use IPv4 (vs IPv6)
//SOCK_STREAM: use TCP (vs UDP)
//INADDR_ANY: the server accepts connections from any network interface
int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;

    //create server socket
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // config socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
}

