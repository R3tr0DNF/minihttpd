#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "server.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define SIZEBUF 4096

int main(){
    int sd;
    struct sockaddr_in fsin;
    socklen_t alen = sizeof(fsin);
    int nsd;
    int flags;
    
    char buffer[SIZEBUF];
    ssize_t rv;


    sd = server_init();
    flags = fcntl(sd, F_GETFL,0);


    if (sd == -1){
        perror("server_init");
        return 1;
    }

    printf("Servidor iniciado correctamente");
    
    while (1)
    {
    
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