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
    [[nodiscard]] int getSockfd() const;

private:
    explicit Socket(int sockfd) : sockfd(sockfd)
     {
         std::cout << "Socket " <<sockfd<< " (active) created!" <<std::endl;
     }

public: Socket &operator=(const Socket&) =delete;

     friend class ServerSocket;

public:

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

    Socket(const Socket & other)
    {
        this->sockfd=other.sockfd;
    }

    Socket(Socket &&other)   noexcept :sockfd(other.sockfd)
    {
        other.sockfd=0;
    }

    Socket &operator=(Socket &other)
    noexcept     {
        sockfd = other.sockfd;
        return *this;
    }

    Socket &operator=(Socket &&other)
 noexcept     {
        if(sockfd!=0) close(sockfd);
        sockfd = other.sockfd;
        other.sockfd=0;
        return *this;
    }

    //qui vanno le funzioni read e write con recv e send riscritte ( include .h)
    //ssize_t read(char*buffer,size_t len, int options){}
    //write,connect
};


 class ServerSocket : private Socket  //costruttore di ServerSocket ereditato privatamente, come i suoi metodi
{
public:
    explicit ServerSocket(int port)
    {
        struct sockaddr_in sockaddrIn{};

        /* impostazioni per ascoltare */
        sockaddrIn.sin_family = AF_INET;
        sockaddrIn.sin_port = htons(port);
        sockaddrIn.sin_addr.s_addr = htonl(INADDR_ANY);
        //bind
        try{
            Bind(sockfd, reinterpret_cast<struct sockaddr*>(&sockaddrIn), sizeof(sockaddrIn));
        }
        catch(std::runtime_error &re)
        {
            throw re;
        }

        std::cout<<"Socket created, listening on : "<<inet_ntoa(sockaddrIn.sin_addr)<<":"<< ntohs(sockaddrIn.sin_port)<<std::endl;
        //listen
        try{
            Listen(sockfd,CODA_ASCOLTO);
        }
        catch(std::runtime_error &re)
        {
            throw re;
        }
    }

    //Socket accept() {}
    // fd= ::accept..
    //return Socket(fd);
  //todo gestirlo

    Socket accept_request(struct sockaddr_in* addr,unsigned int* len)
    {
        int fd;
        int count = 0;
        int maxTries = 3;

        while(true)
        {
                fd = Accept(sockfd,reinterpret_cast<struct sockaddr*>(addr),len);
                if (fd <0 )
                {
                    std::cout << "Impossibile stabilire la connessione con il client %s attraverso porta %u" <<
                              inet_ntoa(addr->sin_addr) << ntohs(addr->sin_port) << std::endl;
                    //ritentare
                    if (++count == maxTries)
                    {
                        throw std::runtime_error("Impossibile accettare una connessione");
                    }
                    sleep(2);
                }
                else break;

        }
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
