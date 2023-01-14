//
// Created by alonn on 6/18/2022.
//
#include "netinet/in.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/socket.h"
#include "unistd.h"
#include "arpa/inet.h"
#include "string.h"
#include "netdb.h"
#include <iostream>
#include "cstring"

#define CLIENT_RUN 4
#define SERVER_RUN 3
#define BUFF_SIZE 256
#define CLIENTS_NUM 5
#define ZERO 0

int get_connection(int s) {
    int t;
    if ((t = accept(s, nullptr, nullptr)) < ZERO) {
        return -1;
    }
    return t;
}

int call_socket(char *hostname, unsigned short portnum) {
    struct sockaddr_in sa;
    struct hostent *hp;
    int s;
    if ((hp = gethostbyname(hostname)) == NULL) {
        return -1;
    }
    memset(&sa, 0, sizeof(sa));
    memcpy((char *) &sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons((u_short) portnum);
    if ((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        close(s);
        return -1;
    }
    return s;
}

int read_data(int s, char *buf, int n) {
    int bcount;
    int br;
    bcount = 0;
    br = 0;
    while (bcount < n) {
        br = read(s, buf, n - bcount);
        if (br > 0) {
            bcount += br;
            buf += br;
        }
        if (br < 1) {
            return -1;
        }
    }
    return bcount;
}

int establish(unsigned short portnum) { // copied from slides - no idea whats going on
    char myname[BUFF_SIZE + 1];
    int s;
    struct sockaddr_in sa;
    struct hostent *hp;
    gethostname(myname, BUFF_SIZE);
    hp = gethostbyname(myname);
    if (hp == NULL) {
        return -1;
    }
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_port = htons(portnum);
    if ((s = socket(AF_INET, SOCK_STREAM, ZERO)) < ZERO) {
        return -1;
    }
    if (bind(s, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < ZERO) {
        close(s);
        return -1;
    }
    if (listen(s, CLIENTS_NUM) < ZERO) {
        close(s);
        return -1;
    }
    return s;
}

void client_side(int port, char *cmd) {
//    int sock = 0;
//    int valread;
    int client_fd;
//    struct sockaddr_in serv_addr;
    char buffer[BUFF_SIZE];
    char hostname[BUFF_SIZE + 1];
    gethostname(hostname, BUFF_SIZE);
    client_fd = call_socket(hostname, port);
    if (client_fd < ZERO) {
        std::cerr << "System Error: call_socket failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    memset(buffer,0,BUFF_SIZE);
    std::strncpy(buffer,cmd,BUFF_SIZE-1);
    if (write(client_fd, buffer, BUFF_SIZE) < ZERO) {
        std::cerr << "System Error : failed to write commnad" << std::endl;
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    close(client_fd);
}

void server_side(int port) {
    int server_fd;
//    int new_socket, valread;
    char buffer[BUFF_SIZE];
    server_fd = establish(port);
    if (server_fd < ZERO) {
        std::cerr << "System Error - Could not establish server" << std::endl;
        exit(EXIT_FAILURE);
    }
    // now we have to listen
    while (true) {
        int client_s = get_connection(server_fd);
        memset(buffer,0,BUFF_SIZE);
        read_data(client_s, buffer, BUFF_SIZE-1);
        if (system(buffer) < ZERO) {
            std::cerr << "system error - server unable to do cmd" << std::endl;
            exit(EXIT_FAILURE);
        }
        close(client_s);
    }
}

int main(int argc, char *argv[]) {
    if (argc == CLIENT_RUN) { // client uses ./sockets client <port> <cmd>
        client_side(atoi(argv[2]), argv[3]);
    } else { //server uses ./sockets server <port>
        server_side(atoi(argv[2]));
    }
    return EXIT_SUCCESS;
}
