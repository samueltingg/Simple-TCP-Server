#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define PORT "3490"
#define BACKLOG 10

// Helper: set a file descriptor(socket) to non-blocking mode
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    struct addrinfo hints, *servinfo, *p;
    int listener, rv;
    int yes = 1;

    // Set up hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // Use my IP

    // getaddrinfo fills in the servinfo linked list with possible address structures to bind to, according to your hints
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to the first available
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // create Server socket
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;

        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(listener);
            continue;
        }
        // binds IP address & PORT to socket (which are contained in p->ai_addr)
        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            continue;
        }

        break;
    }
    // free linked list
    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Failed to bind to port %s\n", PORT);
        return -1;
    }

    if (set_nonblocking(listener) == -1) {
        perror("set_nonblocking");
        close(listener);
        return -1;
    }

    return listener;
}
