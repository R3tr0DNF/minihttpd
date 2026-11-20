#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "server.h"
#include "http.h"
#include "files.h"
#include "mime.h"

#define MAX_EVENTS 64

static void close_client(int epfd, struct client *client) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, client->fd, NULL);
    close(client->fd);

    if (client->file_fd >= 0){
        close(client->file_fd);
    }
    free(client);
}

static int http_response(int epfd, struct client *client, int status, const char *mime, size_t file_size, int file_fd, int keep_alive) {
    
    int header_length = build_http_response(status, mime, file_size, keep_alive, client->response, sizeof(client->response));

    if (header_length == -1) {
        if (file_fd >= 0) {
            close(file_fd);
        }
            return -1;
    }

    client->response_len = (size_t)header_length;
    client->response_sent = 0;
    client->file_fd = file_fd;
    client->file_size = file_fd >= 0 ? file_size : 0;
    client->file_offset = 0;
    client->file_buffer_len = 0;
    client->file_buffer_sent = 0;
    client->close_after_response = !keep_alive;

    struct epoll_event ev = {0};
    ev.events = EPOLLOUT;
    ev.data.ptr = client;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
        if (client->file_fd >= 0) {
            close(client->file_fd);
        }
        client->file_fd = -1;
        return -1;
    }

    return 0;
}

static int process_http_request(int epfd, struct client *client) {

    size_t bytes_consumed = 0;

    enum http_parse_result result = parse_http_request(client->buffer, client->buffer_len, &client->request, &bytes_consumed);

    if (result == HTTP_INCOMPLETE){
        return 0;
    }

    if (result == HTTP_BAD_REQUEST) {
        return http_response(epfd, client, HTTP_STATUS_BAD_REQUEST, "text/plain", 0, -1, 0) == -1 ? -1 : 1;
    }

    if (bytes_consumed == 0 || bytes_consumed > client->buffer_len){
        return -1;
    } 

    size_t remaining = client->buffer_len - bytes_consumed;
    memmove(client->buffer, client->buffer + bytes_consumed, remaining);
    client->buffer_len = remaining;

    if (strcmp(client->request.method, "GET") != 0) {
        return http_response(epfd, client, HTTP_STATUS_METHOD_NOT_ALLOWED, "text/plain", 0, -1, 0) == -1 ? -1 : 1;
    }

    int file_fd = -1;
    size_t file_size = 0;
    int status = open_file(client->request.target, &file_fd, &file_size);
    const char *mime = "text/plain";

    if (status == HTTP_STATUS_OK) {
        char mime_path[HTTP_TARGET_SIZE];
        size_t path_len = strcspn(client->request.target, "?#");

        if (path_len == 1 || client->request.target[path_len - 1] == '/') {
            mime = get_mime_type("index.html");
        } else {
            memcpy(mime_path, client->request.target, path_len);
            mime_path[path_len] = '\0';
            mime = get_mime_type(mime_path);
        }
    } else {
        file_size = 0;
    }

    int keep_alive = client->request.keep_alive && !client->read_closed;

    return http_response(epfd, client, status, mime, file_size, file_fd, keep_alive) == -1 ? -1 : 1;
}

int main(void) {
    struct client listener = {0};
    struct epoll_event ev = {0};
    struct epoll_event events[MAX_EVENTS];
    struct client *client;

    int sd, epfd, nsd, flags, n_events;
    ssize_t rv;

    sd = server_init();

    if (sd == -1) {
        perror("server_init");
        return 1;
    }

    flags = fcntl(sd, F_GETFL, 0);

    if (flags == -1 || fcntl(sd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        close(sd);
        return 1;
    }

    epfd = epoll_create1(EPOLL_CLOEXEC);

    if (epfd == -1) {
        perror("epoll_create1");
        close(sd);
        return 1;
    }

    listener.type = LISTENER;
    listener.fd = sd;

    ev.events = EPOLLIN;
    ev.data.ptr = &listener;

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
            if (errno == EINTR){
                continue;
            } 
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n_events; i++) {
            client = events[i].data.ptr;
            uint32_t event_flags = events[i].events;

            if (client->type == LISTENER) {
                while (1) {
                    nsd = accept(sd, NULL, NULL);

                    if (nsd == -1) {

                        if (errno == EINTR){
                            continue;
                        }

                        if (errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        } 

                        perror("accept");
                        break;
                    }

                    flags = fcntl(nsd, F_GETFL, 0);

                    if (flags == -1 || fcntl(nsd, F_SETFL, flags | O_NONBLOCK) == -1) {
                        perror("fcntl");
                        close(nsd);
                        continue;
                    }

                    struct client *new_client = calloc(1, sizeof(struct client));

                    if (new_client == NULL) {
                        perror("calloc");
                        close(nsd);
                        continue;
                    }

                    new_client->type = CLIENT;
                    new_client->fd = nsd;
                    new_client->file_fd = -1;

                    ev.events = EPOLLIN;
                    ev.data.ptr = new_client;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, nsd, &ev) == -1) {
                        perror("epoll_ctl");
                        free(new_client);
                        close(nsd);
                        continue;
                    }

                }

                continue;
            }

            if (event_flags & EPOLLERR) {
                close_client(epfd, client);
                continue;
            }

            if ((event_flags & EPOLLHUP) && !(event_flags & (EPOLLIN | EPOLLOUT))) {
                close_client(epfd, client);
                continue;
            }

            if ((event_flags & (EPOLLIN | EPOLLRDHUP)) && client->response_len == 0) {
                while (1) {
                    if (client->buffer_len >= SIZEBUF) {
                        if (http_response(epfd, client, HTTP_BAD_REQUEST, "text/plain", 0, -1, 0) == -1) {
                            close_client(epfd, client);
                            client = NULL;
                        }

                        break;
                    }

                    rv = recv( client->fd, client->buffer + client->buffer_len, SIZEBUF - client->buffer_len, 0 );

                    if (rv > 0) {
                        
                        client->buffer_len += (size_t)rv;

                        int result = process_http_request(epfd, client);

                        if (result == -1) {
                            close_client(epfd, client);
                            client = NULL;
                            break;
                        }

                        if (result == 1){
                            break;
                        } 
                        continue;
                    }

                    if (rv == 0) {
                        client->read_closed = 1;
                        break;
                    }

                    if (errno == EINTR){
                        continue;
                    } 
                    if (errno == EAGAIN || errno == EWOULDBLOCK){
                        break;
                    } 

                    perror("recv");
                    close_client(epfd, client);
                    client = NULL;
                    break;
                }

                if (client == NULL){
                     continue;
                 }

                if (client->read_closed && client->response_len == 0) {
                    if (client->buffer_len > 0) {
                        if (http_response(epfd, client, HTTP_BAD_REQUEST, "text/plain", 0, -1, 0) == -1) {
                            close_client(epfd, client);
                            continue;
                        }
                    } else {
                        close_client(epfd, client);
                        continue;
                    }
                }
            }

            if (client != NULL && (event_flags & EPOLLOUT)) {
                int failed = 0;
                int blocked = 0;

                while (client->response_sent < client->response_len) {
                    
                    rv = send(client->fd, client->response + client->response_sent, client->response_len - client->response_sent, MSG_NOSIGNAL);

                    if (rv > 0) {
                        client->response_sent += (size_t)rv;
                        continue;
                    }

                    if (rv == -1 && errno == EINTR){
                        continue;
                    }

                    if (rv == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
                        blocked = 1;
                        break;
                    } 

                    failed = 1;
                    break;
                }

                if (failed) {
                    close_client(epfd, client);
                    continue;
                }

                if (blocked){
                    continue;
                }
                
                while (client->file_fd >= 0 && ((size_t)client->file_offset < client->file_size || client->file_buffer_sent < client->file_buffer_len)) {
                    if (client->file_buffer_sent == client->file_buffer_len) {
                        client->file_buffer_len = 0;
                        client->file_buffer_sent = 0;

                        size_t remaining = client->file_size - client->file_offset;
                        size_t amount = remaining < sizeof(client->file_buffer) ? remaining : sizeof(client->file_buffer);

                        rv = read(client->file_fd, client->file_buffer, amount);

                        if (rv > 0) {
                            client->file_buffer_len = (size_t)rv;
                            client->file_offset += (size_t)rv;
                        } else if (rv == -1 && errno == EINTR) {
                            continue;
                        } else {
                            failed = 1;
                            break;
                        }
                    }

                    rv = send(client->fd, client->file_buffer + client->file_buffer_sent, client->file_buffer_len - client->file_buffer_sent, MSG_NOSIGNAL);

                    if (rv > 0) {
                        client->file_buffer_sent += (size_t)rv;
                        continue;
                    }

                    if (rv == -1 && errno == EINTR) {
                        continue;
                    }

                    if (rv == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        blocked = 1;
                        break;
                    }

                    failed = 1;
                    break;
                }

                if (failed) {
                    close_client(epfd, client);
                    continue;
                }

                if (blocked) {
                    continue;
                }

                if (client->file_fd >= 0) {
                    close(client->file_fd);
                    client->file_fd = -1;
                }

                if (client->close_after_response || client->read_closed) {
                    close_client(epfd, client);
                    continue;
                }

                client->response_len = 0;
                client->response_sent = 0;

                int pending = process_http_request(epfd, client);

                if (pending == -1) {
                    close_client(epfd, client);
                    continue;
                }

                if (pending == 1) {
                    continue;
                }

                ev.events = EPOLLIN;
                ev.data.ptr = client;

                if (epoll_ctl(epfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
                    perror("epoll_ctl MOD");
                    close_client(epfd, client);
                }
            }
        }
    }

    close(epfd);
    close(sd);

    return 0;
}