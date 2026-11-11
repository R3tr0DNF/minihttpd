#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUFSIZE 4096

int passiveTCP(const char *service, int qlen);
int errexit(const char *format, ...);
int HTTPserver(int fd);

/*HTTPSV*/

int
main (int argc, char *argv[]){
    int sd;
    char *service = "http";
    struct sockaddr_in cltaddr;
    unsigned int alen;
    int nsd;

    switch(argc){
        case 1:
            break;
        case 2:
            service = argv[1];
            break;
        default:
            errexit("usage: %s [port]\n", argv[0]);
    }

    sd = passiveTCP(service, 5);
    
    while(1){

        nsd = accept(sd, (struct sockaddr *)&cltaddr, &alen);
        HTTPserver(nsd);
        /* fd = open(frame, O_RDONLY); */


    }
}

int
HTTPserver(int fd){
    char buf[BUFSIZE];
    int cc;

    while (cc = read(fd, buf, sizeof buf)){
        if (cc < 0)
            errexit("server read: %s\n", strerror(errno));
        if (write(fd, buf, cc)< 0)
            errexit("server write: %s\n", strerror(errno));
    }
    return 0;
}