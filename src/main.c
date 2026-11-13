#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "server.h"

#define SIZEBUF 4096

int main(){
    int sd;
    struct sockaddr_in fsin;
    socklen_t alen = sizeof(fsin);
    int nsd;
    
    char buffer[SIZEBUF];
    ssize_t rv;

    sd = server_init();

    if (sd == -1){
        perror("server_init");
        return 1;
    }

    printf("Servidor iniciado correctamente");
    
    nsd = accept(sd, (struct sockaddr *)&fsin, &alen);

    if (nsd == -1){
        perror("accept");
        close(sd);
        return 1;
    }

    rv = recv(nsd, buffer, sizeof(buffer), 0);

    if (rv == -1){
        perror("recv");
        close(nsd);
        return 1;
    } else if (rv == 0){
        printf("Cliente cerro conexion");
    }

    
    


    return 0;

    
}