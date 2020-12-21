#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 1
#define BUFF_SIZE 8192

void reuse_port(int sfd)
{
    int yes = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
}

// get sockaddr IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *buff;
    buff = (char*) malloc(BUFF_SIZE * sizeof(char));

    // #1 getaddrinfo()
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int s = getaddrinfo(argv[1], argv[2], &hints, &servinfo); // #1
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        free(buff);
        exit(EXIT_FAILURE);
    }

    // #2 socket()
    // #3 bind()
    int sfd;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol); // #2
        if (sfd == -1) {
            perror("server: socket");
            continue;
        }
        reuse_port(sfd);
        if (bind(sfd, p->ai_addr, p->ai_addrlen) == -1) { // #3
            close(sfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    
    freeaddrinfo(servinfo);

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        free(buff);
        exit(EXIT_FAILURE);
    }

    // #4 listen()
    if (listen(sfd, BACKLOG) == -1) { // #4
        perror("listen");
        free(buff);
        exit(EXIT_FAILURE);
    }
    printf("server: waiting for connections...\n");

    // #5 accept()
    // #6 recv()
    socklen_t sin_size;
    int new_fd;
    struct sockaddr_storage their_addr;
    char ip6[INET6_ADDRSTRLEN];
    int message_length;

    while(1) {
        sin_size = sizeof(their_addr);
        new_fd = accept(sfd, (struct sockaddr *) &their_addr, &sin_size); // #5
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *) &their_addr),
                  ip6, sizeof(ip6));
        printf("server: got connection from %s\n", ip6);

        message_length = recv(new_fd, &buff, BUFF_SIZE, 0); // #6
        if (message_length == -1)
            perror("recv");
        if (!fork()) {
            close(sfd);
            close(new_fd);
            exit(EXIT_SUCCESS);
        }
        close(new_fd);
        printf("%s", &buff);
    }
    free(buff);
    return 0;
}

