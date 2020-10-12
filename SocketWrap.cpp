//
// Created by enrico_scalabrino on 10/10/20.
//


#include "SocketWrap.h"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h> // unix sockets


int SSocket (int family, int type, int protocol) // this name is necessary due to conflict
{
    int n;
    if ( (n = ::socket(family,type,protocol)) < 0)
       throw std::runtime_error ("Error - socket() failed");
    return n;
}

void Bind (int sockfd, const SA *myaddr,  socklen_t myaddrlen)
{
    if ( ::bind(sockfd, myaddr, myaddrlen) != 0)
        throw std::runtime_error ("Error - bind() failed");
}

void Listen (int sockfd, int backlog)
{
    char *ptr;
    if ( (ptr = getenv("LISTENQ")) != nullptr)
        backlog = atoi(ptr);
    if ( ::listen(sockfd,backlog) < 0 )
        throw std::runtime_error ("Error - listen() failed");
}


int Accept (int socket, SA* cliaddr, socklen_t *addrlen){
    int conn;
    do{
        if((conn = ::accept(socket, cliaddr, addrlen)) < 0){
            if (INTERRUPTED_BY_SIGNAL ||
                errno == EPROTO || errno == ECONNABORTED || errno == EMFILE ||
                errno == ENFILE || errno == ENOBUFS || errno == ENOMEM){
                continue;
            }
            else
                conn = -1;
        }
        else break;
    }while(true);
    return conn;
}

ssize_t Send(int socket, const void *bufptr, size_t nbytes, int flags){
    ssize_t sent;
    if((sent = ::send(socket, bufptr, nbytes, flags)) != (ssize_t)nbytes)
        throw std::runtime_error("Error, send() failed.");
    return sent;
}