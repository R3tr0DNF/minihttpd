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

    if (sd == -1){
        perror("server_init");
        return 1;
    }
    
    /*Change the configuration of the socket*/
    flags_sd = fcntl(sd, F_GETFL,0);
    if (flags_sd == -1){
        perror("fcntl F_GETFL");
        close(sd);
        return 1;
    }

    if (fcntl(sd, F_SETFL, flags_sd | O_NONBLOCK) == -1){
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

                    nsd = accept(sd, NULL, NULL);

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

                    flags_nsd = fcntl(nsd, F_GETFL, 0);

                    if (flags_nsd == -1){
                        perror("fcntl F_GETFL");
                        close(nsd);
                        continue;
                    }

                    if(fcntl(nsd, F_SETFL, flags_nsd | O_NONBLOCK) == -1){
                        perror("fcntl F_SETFL");
                        close(nsd);
                        continue;
                    }

                    client_ev.events = EPOLLIN;
                    client_ev.data.fd = nsd;

                    if(epoll_ctl(epfd, EPOLL_CTL_ADD, nsd, &client_ev) == -1){
                        perror("epoll_ctl");
                        close(nsd);
                        continue;
                    }

                }
                
            } else {

                rv = recv(fd, buffer, SIZEBUF, 0);
                
                if (rv > 0){
                    buffer[rv] = '\0';
                    printf("Received data from fd %d: %s\n", fd, buffer);
                }

                if (rv == 0){
                    printf("Client disconnected, fd: %d\n", fd);

                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == -1){
                        perror("epoll_ctl DEL");                       
                    }
            
                    close(fd);
                    continue; 
                }
                
                if (rv == -1){
                   if(errno == EAGAIN || errno == EWOULDBLOCK){
                    continue;
                   }

                   if(errno == EINTR){
                    printf("Call interrupted...");
                    continue;
                   }

                    perror("epoll_ctl DEL");
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == -1){
                    perror("epoll_ctl DEL");                       
                    
                    close(fd);
                    continue; 
                   }


                }


            }
        }
    }

    return 0;

    
}