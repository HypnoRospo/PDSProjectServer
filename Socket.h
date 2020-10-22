//
// Created by enrico_scalabrino on 10/10/20.
//

#ifndef PDSPROJECTSERVER_SOCKET_H
#define PDSPROJECTSERVER_SOCKET_H

#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "SocketWrap.h"

#define CODA_ASCOLTO 15
//Paradigm RAII by Professor
class Socket {
private:
     int sockfd;
public:
    int getSockfd() const;

private:
    explicit Socket(int sockfd) : sockfd(sockfd)
     {
         std::cout << "Socket " <<sockfd<< " (active) created!" <<std::endl;
     }

     Socket &operator=(const Socket&) =delete;

     friend class ServerSocket;

public:
    Socket(const Socket &) = delete;
    Socket()
    {
         //sockfd = ::socket(..);
        sockfd = SSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sockfd < 0 ) throw std::runtime_error("Cannot create socket");
        std::cout << "Socket " <<sockfd <<" created" <<std::endl;
    }

    ~Socket()
    {
        if(sockfd!=0)
        {
            std::cout<<"Socket "<<sockfd<<" closed"<<std::endl;
            close(sockfd);
        }
    }

    Socket(Socket &&other)  noexcept :sockfd(other.sockfd)
    {
        other.sockfd=0;

    }

    Socket &operator=(Socket &&other)
    {
        if(sockfd!=0) close(sockfd);
        sockfd = other.sockfd;
        return *this;
    }

    //qui vanno le funzioni read e write con recv e send riscritte ( include .h)
    //ssize_t read(char*buffer,size_t len, int options){}
    //write,connect
};


 class ServerSocket : private Socket  //costruttore di ServerSocket ereditato privatamente, come i suoi metodi
{
public:
    ServerSocket(int port)
    {
        struct sockaddr_in sockaddrIn;

        /* impostazioni per ascoltare */
        sockaddrIn.sin_family = AF_INET;
        sockaddrIn.sin_port = htons(port);
        sockaddrIn.sin_addr.s_addr = htonl(INADDR_ANY);
        //bind
        Bind(sockfd, reinterpret_cast<struct sockaddr*>(&sockaddrIn), sizeof(sockaddrIn));
        std::cout<<"Socket created, listening on : "<<inet_ntoa(sockaddrIn.sin_addr)<<":"<< ntohs(sockaddrIn.sin_port)<<std::endl;
        //listen
        Listen(sockfd,CODA_ASCOLTO);

    }

    //Socket accept() {}
    // fd= ::accept..
    //return Socket(fd);

    Socket accept_request(struct sockaddr_in* addr,unsigned int* len)
    {
        int fd;
        try {
             fd = Accept(sockfd,reinterpret_cast<struct sockaddr*>(addr),len);
             if (fd <0 ) throw std::runtime_error("Cannot accept socket");
        }
        catch(std::runtime_error& re)
        {
            std::cout << "Impossibile stabilire la connessione con il client %s attraverso porta %u" <<
                      inet_ntoa(addr->sin_addr) << ntohs(addr->sin_port) << std::endl;
            std::cerr<<re.what()<<std::endl;
        }
        //gestire il throw to do -> basta una print tanto l'esecuzione continua nel server.cpp con continue


        return Socket(fd);
    }

    int getSock()
    {
        return getSockfd();
    }

};

int Socket::getSockfd() const {
    return sockfd;
}

#endif //PDSPROJECTSERVER_SOCKET_H
