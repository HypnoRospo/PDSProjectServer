//
// Created by enrico_scalabrino on 10/10/20.
//

#ifndef PDSPROJECTSERVER_SOCKETWRAP_H
#define PDSPROJECTSERVER_SOCKETWRAP_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define SA struct sockaddr

#define INTERRUPTED_BY_SIGNAL (errno == EINTR)


int SSocket (int family, int type, int protocol);

void Bind (int sockfd, const SA *myaddr, socklen_t myaddrlen);

void Listen (int sockfd, int backlog);

int Accept (int socket, SA* cliaddr, socklen_t *addrlen);

ssize_t Send(int socket, const void *bufptr, size_t nbytes, int flags);
#endif //PDSPROJECTSERVER_SOCKETWRAP_H
