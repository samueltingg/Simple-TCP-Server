#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define PORT "3490"
#define BACKLOG 10

int main(void) {
    int listener, rv;
    struct addrinfo hints, *servinfo, *p;

    // Set up hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // Use my IP

    // Resolve address info
    // getaddrinfo fills in the servinfo linked list with possible address structures to bind to, according to your hints
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through results and bind to the first available
    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            perror("socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(listener);
            continue;
        }

        // binds IP address & PORT to socket (which are contained in p->ai_addr)
        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(listener);
            continue;
        }

        break;
    }
    // free linked list
    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Failed to bind\n");
        return 2;
    }

    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        close(listener);
        return 3;
    }

    printf("Listening on port %s\n", PORT);

    // Pause to keep program running so you can test the open port
    getchar();  // Press enter to exit

    close(listener);
    return 0;
}
