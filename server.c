#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT "3490"
#define BACKLOG 10
#define MAX_CLIENTS  FD_SETSIZE
#define BUF_SIZE 1024

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {
    int listener, new_fd, rv;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    char remoteIP[INET6_ADDRSTRLEN];
    char buf[BUF_SIZE];
    int nbytes;

    fd_set master_set, read_fds;
    int fdmax;

    // Set up address hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        exit(1);
    }

    // Create, bind and listen
    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;

        int yes = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
            perror("setsockopt");
            close(listener);
            continue;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Failed to bind\n");
        exit(2);
    }

    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }

    // Initialize the master fd_set
    FD_ZERO(&master_set);
    FD_ZERO(&read_fds);

    // Add listener to master set
    FD_SET(listener, &master_set);
    fdmax = listener;

    printf("Server: waiting for connections on port %s...\n", PORT);

    while(1) {
        read_fds = master_set;

        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    // New incoming connection
                    addrlen = sizeof remoteaddr;
                    new_fd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);
                    if (new_fd == -1) {
                        perror("accept");
                        continue;
                    }

                    FD_SET(new_fd, &master_set);
                    if (new_fd > fdmax) fdmax = new_fd;

                    inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
                              remoteIP, sizeof remoteIP);
                    printf("New connection from %s on socket %d\n", remoteIP, new_fd);

                } else {
                    // Handle client data
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        if (nbytes == 0) {
                            printf("Socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master_set);
                    } else {
                        // Echo message back to client
                        buf[nbytes] = '\0';
                        printf("Received: %s\n", buf);
                        send(i, "Hello from server", 17, 0);
                    }
                }
            }
        }
    }

    return 0;
}
