#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

int main(){

    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;

    struct sockaddr_in fsin;

    int rc;
    int sd = -1; /*server sock*/
    int last_error = 0;
    int nsd = -1; /*client sock*/
    unsigned int alen;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    rc = getaddrinfo(NULL, "5000", &hints, &result);

    if (rc != 0){
        printf("can't resolve");
        return -1;
    }

    /*create a socket*/

    for(rp = result; rp != NULL; rp = rp->ai_next){

        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sd == -1){
            last_error = errno;
            continue;
        }

        if(bind(sd, rp ->ai_addr, rp->ai_addrlen) == -1){
            last_error = errno;
            close(sd);
            sd = -1;
            continue;
        } else {
            if (listen(sd, 5)== -1){
                last_error = errno;
                close(sd);
                sd = -1;
                continue;
            } else {
               break;
            }
        }        
    }

    freeaddrinfo(result);

    /*socket on accept*/
    nsd = accept(sd, (struct sockaddr*)&fsin, &alen);

}
