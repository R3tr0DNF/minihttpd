#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "server.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define SIZEBUF 4096
#define MAX_EVENTS 64

int main(){
    int sd;
    struct sockaddr_in fsin;
    socklen_t alen = sizeof(fsin);
    int nsd;
    int flags;
    int epfd;
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    int n_events;
    
    char buffer[SIZEBUF];
    ssize_t rv;


    sd = server_init();

    if (sd == -1){
        perror("server_init");
        return 1;
    }
    
    /*Change the configuration of the socket*/
    flags = fcntl(sd, F_GETFL,0);
    if (flags == -1){
        perror("fcntl F_GETFL");
        close(sd);
        return 1;
    }

    if (fcntl(sd, F_SETFL, flags | O_NONBLOCK) == -1){
        perror("fcntl F_SETFL");
        close(sd);
        return 1;
    }

    /*Creating the epoll*/
    epfd = epoll_create1(EPOLL_CLOEXEC);

    if (epfd == -1){
        perror("epoll_create1");
        close(sd);
        return 1;
    }
    
    ev.events = EPOLLIN;
    ev.data.fd = sd;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &ev) == -1){
        perror("epoll_ctl");
        close(epfd);
        close(sd);
        return 1;
    }

    printf("Servidor iniciado correctamente");

    while (1)
    {
        
        n_events = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (n_events == -1){
            if (errno == EINTR){
                continue;
            }
            
            perror("epoll_wait");
            break;
        }

        for(int i = 0; i < n_events; i++){
            int fd = events[i].data.fd;

            if (fd == sd){
                while (1){
                    
                    nsd = accept(sd, (struct sockaddr *)&fsin, &alen);

                    if (nsd == -1){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }

                        if (errno == EINTR){
                            continue;
                        }

                        perror("accept");
                        break;
                    }

                }
                
            } else {

            }
        }
    
        nsd = accept(sd, (struct sockaddr *)&fsin, &alen);

        if (nsd == -1){
            perror("accept");
            close(sd);
            return 1;
        }

        rv = recv(nsd, buffer, sizeof(buffer) - 1, 0);

        if (rv == -1){
            perror("recv");
            close(nsd);
            close(sd);
            return 0;
        } else if (rv == 0){
            printf("Cliente cerro conexion");
        } else {
            buffer[rv] = '\0';
            printf("Cliente dice: %s\n", buffer);
        }
    }
    


    return 0;

    
}