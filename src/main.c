
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "server.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"

#define SIZEBUF 4096
#define MAX_EVENTS 64

struct client {
    int fd;
    char buffer[SIZEBUF];
    size_t buffer_len;
    struct http_request request;
    struct client *next;
};

struct client *clients = NULL;

void close_client(int epfd, int fd) {
    struct client **current = &clients;

    while (*current != NULL && (*current)->fd != fd) {
        current = &(*current)->next;
    }

    if (*current == NULL) {
        return;
    }

    struct client *client = *current;

    *current = client->next;


    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        perror("epoll_ctl DEL");
    }

    close(fd);
    free(client);
}

int main() {
    int sd;
    int nsd;
    int flags_sd;
    int flags_nsd;
    int epfd;

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    struct epoll_event client_ev;

    int n_events;
    char buffer[SIZEBUF];
    ssize_t rv;

    sd = server_init();

    if (sd == -1) {
        perror("server_init");
        return 1;
    }

    flags_sd = fcntl(sd, F_GETFL, 0);

    if (flags_sd == -1) {
        perror("fcntl F_GETFL");
        close(sd);
        return 1;
    }

    if (fcntl(sd, F_SETFL, flags_sd | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        close(sd);
        return 1;
    }

    epfd = epoll_create1(EPOLL_CLOEXEC);

    if (epfd == -1) {
        perror("epoll_create1");
        close(sd);
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = sd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &ev) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(sd);
        return 1;
    }

    printf("Servidor iniciado correctamente\n");

    while (1) {
        n_events = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (n_events == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n_events; i++) {
            int fd = events[i].data.fd;

            if (fd == sd) {
                while (1) {
                    nsd = accept(sd, NULL, NULL);

                    if (nsd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }

                        if (errno == EINTR) {
                            continue;
                        }

                        perror("accept");
                        break;
                    }

                    flags_nsd = fcntl(nsd, F_GETFL, 0);

                    if (flags_nsd == -1) {
                        perror("fcntl F_GETFL");
                        close(nsd);
                        continue;
                    }

                    if (fcntl(nsd, F_SETFL, flags_nsd | O_NONBLOCK) == -1) {
                        perror("fcntl F_SETFL");
                        close(nsd);
                        continue;
                    }

                    struct client *client = calloc(1, sizeof(struct client));

                    if (client == NULL) {
                        perror("calloc");
                        close(nsd);
                        continue;
                    }

                    client->fd = nsd;

                    client_ev.events = EPOLLIN;
                    client_ev.data.fd = nsd;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, nsd, &client_ev) == -1) {
                        perror("epoll_ctl");
                        free(client);
                        close(nsd);
                        continue;
                    }

                    client->next = clients;
                    clients = client;

                    printf("New client connected, fd: %d\n", nsd);
                }

            } else {

                struct client *client = clients;

                while (client != NULL && client->fd != fd) {
                    client = client->next;
                }

                if (client == NULL) {
                    fprintf(stderr, "Client not found: fd %d\n", fd);
                    continue;
                }

                if (client->buffer_len >= SIZEBUF) {
                    fprintf(stderr, "Request buffer is full for fd %d\n", client->fd);

                    close_client(epfd, fd);
                    continue;
                }

                rv = recv(
                    client->fd,
                    client->buffer + client->buffer_len,
                    SIZEBUF - client->buffer_len,
                    0
                );

                if (rv > 0) {
                    client->buffer_len += (size_t)rv;

                    printf("Received data from fd %d:\n", fd);
                    fwrite(client->buffer, 1, client->buffer_len, stdout);
                    printf("\n");

                    size_t bytes_consumed = 0;

                    enum http_parse_result result = parse_http_request(
                        client->buffer,
                        client->buffer_len,
                        &client->request,
                        &bytes_consumed
                    );

                    if (result == HTTP_INCOMPLETE) {
                        printf("Incomplete HTTP request from fd %d\n", client->fd);
                    }

                    else if (result == HTTP_COMPLETE) {
                        printf("Complete HTTP request from fd %d\n", client->fd);
                        printf("Method: %s\n", client->request.method);
                        printf("Target: %s\n", client->request.target);
                        printf("Version: %s\n", client->request.version);
                    }

                    else if (result == HTTP_BAD_REQUEST) {
                        printf("Bad HTTP request from fd %d\n", client->fd);
                    }
                }

                if (rv == 0) {
                    printf("Client disconnected, fd: %d\n", fd);

                    close_client(epfd, fd);
                    continue;
                }

                if (rv == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }

                    if (errno == EINTR) {
                        printf("Call interrupted...\n");
                        continue;
                    }

                    perror("recv");
                    close_client(epfd, fd);
                    continue;
                }
            }
        }
    }

    close(epfd);
    close(sd);

    return 0;
}
