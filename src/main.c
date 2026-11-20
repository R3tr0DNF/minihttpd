
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
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

    char response[SIZEBUF];
    size_t response_len;
    size_t response_sent;

    int close_after_response;

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

static int queue_http_response(int epfd, struct client *client, int status_code, const char *content_type, size_t content_length, int keep_alive) {
    int response_size = build_http_response(status_code, content_type, content_length, keep_alive, client->response, sizeof(client->response));

    if (response_size == -1) {
        return -1;
    }

    client->response_len = (size_t)response_size;
    client->response_sent = 0;
    client->close_after_response = !keep_alive;

    struct epoll_event client_ev;

    client_ev.events = EPOLLOUT;
    client_ev.data.fd = client->fd;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, client->fd, &client_ev) == -1) {
        perror("epoll_ctl EPOLLOUT");
        return -1;
    }

    return 0;
}

static int send_client_response(struct client *client) {
    while (client->response_sent < client->response_len) {
        ssize_t sent = send(client->fd, client->response + client->response_sent, client->response_len - client->response_sent, MSG_NOSIGNAL);

        if (sent > 0) {
            client->response_sent += (size_t)sent;
            continue;
        }

        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
        }

        return -1;
    }

    return 1;
}

static int process_buffered_request(int epfd, struct client *client) {
    size_t bytes_consumed = 0;

    if (client->buffer_len == 0) {
        return 0;
    }

    enum http_parse_result result = parse_http_request(client->buffer, client->buffer_len, &client->request, &bytes_consumed);

    if (result == HTTP_INCOMPLETE) {
        return 0;
    }

    if (result == HTTP_BAD_REQUEST) {
        return queue_http_response(epfd, client, 400, "text/plain", 0, 0) == -1 ? -1 : 1;
    }

    if (bytes_consumed == 0 || bytes_consumed > client->buffer_len) {
        return -1;
    }

    if (strcmp(client->request.method, "GET") != 0) {
        return queue_http_response(epfd, client, 405, "text/plain", 0, 0) == -1 ? -1 : 1;
    }

    size_t remaining = client->buffer_len - bytes_consumed;

    memmove(client->buffer, client->buffer + bytes_consumed, remaining);
    client->buffer_len = remaining;

    if (queue_http_response(epfd, client, 200, "text/plain", 0, 1) == -1) {
        return -1;
    }

    return 1;
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

                if (events[i].events & EPOLLOUT) {
                    int result = send_client_response(client);

                    if (result == -1) {
                        fprintf(stderr, "Error sending response to fd %d\n", fd);
                        close_client(epfd, fd);
                        continue;
                    }

                    if (result == 0) {
                        continue;
                    }

                    if (client->close_after_response) {
                        close_client(epfd, fd);
                        continue;
                    }

                    client->response_len = 0;
                    client->response_sent = 0;

                    int pending = process_buffered_request(epfd, client);

                    if (pending == -1) {
                        fprintf(stderr, "Error processing buffered request from fd %d\n", fd);
                        close_client(epfd, fd);
                        continue;
                    }

                    if (pending == 1) {
                        continue;
                    }

                    client_ev.events = EPOLLIN;
                    client_ev.data.fd = fd;

                    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &client_ev) == -1) {
                        perror("epoll_ctl EPOLLIN");
                        close_client(epfd, fd);
                        continue;
                    }

                    continue;
                }

                if (client->buffer_len >= SIZEBUF) {
                    fprintf(stderr, "Request buffer is full for fd %d\n", client->fd);

                    close_client(epfd, fd);
                    continue;
                }

                rv = recv(client->fd, client->buffer + client->buffer_len, SIZEBUF - client->buffer_len, 0);

                if (rv > 0) {
                    client->buffer_len += (size_t)rv;

                    int result = process_buffered_request(epfd, client);

                    if (result == -1) {
                        fprintf(stderr, "Error processing request from fd %d\n", fd);
                        close_client(epfd, fd);
                        continue;
                    }
                }

                if (rv == 0) {
                    close_client(epfd, fd);
                    continue;
                }

                if (rv == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }

                    if (errno == EINTR) {
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
