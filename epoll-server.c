#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define PORT "3490"
#define BACKLOG 10
#define MAX_EVENTS 64
#define BUF_SIZE 1024

// Helper: get IPv4 or IPv6 address string
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Helper: set a file descriptor to non-blocking mode
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    int listener, new_fd, rv;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t addrlen;
    char client_ip[INET6_ADDRSTRLEN];
    char buf[BUF_SIZE];
    int nbytes;

    int epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];

    // Prepare address info hints for IPv4/IPv6 + passive binding
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    

    // Resolve local address and port
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        exit(1);
    }

    // Create socket, bind, and listen
    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;

        int yes = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(listener);
            continue;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Failed to bind to port %s\n", PORT);
        exit(2);
    }

    // Start listening
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }

    // Set listener to non-blocking mode (good practice for epoll)
    if (set_nonblocking(listener) == -1) {
        perror("set_nonblocking");
        close(listener);
        exit(4);
    }

    // Create epoll instance
    if ((epoll_fd = epoll_create1(0)) == -1) {
        perror("epoll_create1");
        close(listener);
        exit(5);
    }

    // Register the listener socket to epoll
    ev.events = EPOLLIN;  // Wait for incoming connection events
    ev.data.fd = listener;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        close(listener);
        exit(6);
    }

    printf("Server: waiting for connections on port %s...\n", PORT);

    // Main event loop
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        // Process all ready events
        for (int i = 0; i < nfds; i++) {
            // Event on the listener: accept new connection
            if (events[i].data.fd == listener) {
                addrlen = sizeof client_addr;
                new_fd = accept(listener, (struct sockaddr*)&client_addr, &addrlen);
                if (new_fd == -1) {
                    perror("accept");
                    continue;
                }

                // Set new connection to non-blocking
                if (set_nonblocking(new_fd) == -1) {
                    perror("set_nonblocking (new_fd)");
                    close(new_fd);
                    continue;
                }

                // Add new connection to epoll monitoring
                ev.events = EPOLLIN;
                ev.data.fd = new_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
                    perror("epoll_ctl: new_fd");
                    close(new_fd);
                    continue;
                }

                // Report new connection
                inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr),
                          client_ip, sizeof client_ip);
                printf("New connection from %s on socket %d\n", client_ip, new_fd);

            } else {
                // Event on a client socket: read data
                nbytes = recv(events[i].data.fd, buf, sizeof buf - 1, 0);
                if (nbytes <= 0) {
                    // Connection closed or error
                    if (nbytes == 0)
                        printf("Socket %d hung up\n", events[i].data.fd);
                    else
                        perror("recv");

                    // Remove from epoll monitoring and close
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    close(events[i].data.fd);
                } else {
                    // Null-terminate, print received message
                    buf[nbytes] = '\0';
                    printf("Received from %d: %s\n", events[i].data.fd, buf);

                    // Send response
                    send(events[i].data.fd, "Hello from server", 17, 0);
                }
            }
        }
    }

    // Cleanup
    close(listener);
    close(epoll_fd);
    return 0;
}
