//
// Created by enrico_scalabrino on 10/10/20.
//
#include "Server.h"
#include <iostream>
#include <string>
#include <mutex>
#include <thread>
//#include <openssl/sha.h>
#include <boost/asio/ts/buffer.hpp>
//#include <boost/asio/ts/internet.hpp>
#include "Socket.h"
#include "SocketWrap.h"
#include "Database.h"
#include "Message.h"
#include <boost/filesystem.hpp>

#define BUFFER_DIM 255
#define DIM_FILENAME 255
#define TIMEOUT_SECONDI 15
#define TIMEOUT_MICROSECONDI 0
#define GET "GET "
#define TERMINAZIONE "\r\n\0"
#define OK "+OK\r\n"
#define FINE "FINE\r\n\0"
#define ERRORE "-SERVER: ERRORE, file non trovato o errore generico\r\n"
#define ERRORE_LOGIN "-SERVER: Impossibile procedere, necessaria autenticazione\r\n"
#define OK_LOGIN "-SERVER: CLIENT LOGGED\r\n"
#define OK_NONCE "-SERVER: NONCE SETTED\r\n"
#define OK_REGISTER "-SERVER: REGISTRAZIONE AVVENUTA CON SUCCESSO\r\n"
#define ERRORE_REGISTER "-SERVER: Registrazione non avvenuta, utente presente nel sistema\r\n"

ssize_t send_err(int sock);
ssize_t send_err_login(int sock);
ssize_t send_err_register(int sock);
ssize_t send_register_ok(int sock);
ssize_t send_login_ok(int sock);
ssize_t send_nonce_ok(int sock);
ssize_t ssend(int socket, const void *bufptr, size_t nbytes, int flags);
ssize_t send_file(int socket, const off_t fsize, time_t tstamp, FILE* file);
ssize_t leggi_comando(int socket, char *buffer, size_t buffdim);
MsgType leggi_header(int socket);
void thread_work();
void create_threads();

/**
 * Static methods should be defined outside the class.
 */

Server* Server::pinstance_{nullptr};
std::mutex Server::mutex_;
std::vector<std::thread> threads;
ServerSocket *ss {nullptr};
std::vector<Socket> sockets;
std::mutex mtx;           // mutex for critical section
/**
 * The first time we call GetInstance we will lock the storage location
 *      and then we make sure again that the variable is null and then we
 *      set the value. RU:
 */
Server *Server::start(const int port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (pinstance_ == nullptr)
    {
        pinstance_ = new Server(port);
        ss = new ServerSocket(port);
        boost::filesystem::create_directories("../server_user/");
    }
    else
    {
        std::cout << "Creazione del server gia' avvenuta, server creato in precedenza con porta: " << pinstance_->getPort()<< std::endl;
    }
    return pinstance_;
}

void create_threads()
{
    unsigned int Num_Threads =  std::thread::hardware_concurrency()-1;

    for(int i = 0; i < Num_Threads; i++)
    {  threads.emplace_back(thread_work);}

    for(int j=0; j<Num_Threads ;j++)
    {  threads[j].join();}

}


void Server::work() {
    std::cout << "Server Program" << std::endl;
    std::cout << std::endl;
    Database::create_instance();
    std::cout << "In attesa di connessioni..." << std::endl;
    create_threads();
}

ssize_t ssend(int socket, const void *bufptr, size_t nbytes, int flags){
  return Send(socket,bufptr,nbytes,flags);
}

ssize_t send_err(int sock){
    ssize_t s;
    s = ssend(sock, ERRORE, sizeof(ERRORE), MSG_NOSIGNAL);
    std::cout<<"Errore connessione"<<std::endl;
    return s;
}

ssize_t send_err_login(int sock){
    ssize_t s;
    s = ssend(sock, ERRORE_LOGIN, sizeof(ERRORE_LOGIN), MSG_NOSIGNAL);
    return s;
}

ssize_t send_err_register(int sock){
    ssize_t s;
    s = ssend(sock, ERRORE_REGISTER, sizeof(ERRORE_REGISTER), MSG_NOSIGNAL);
    return s;
}


ssize_t send_login_ok(int sock)
{
    ssize_t  s;
    s = ssend(sock, OK_LOGIN, sizeof(OK_LOGIN), MSG_NOSIGNAL);
    return s;
}

ssize_t send_nonce_ok(int sock)
{
    ssize_t  s;
    s = ssend(sock, OK_NONCE, sizeof(OK_NONCE), MSG_NOSIGNAL);
    return s;
}
ssize_t send_register_ok(int sock)
{
    ssize_t  s;
    s = ssend(sock, OK_REGISTER, sizeof(OK_REGISTER), MSG_NOSIGNAL);
    return s;
}

ssize_t leggi_comando(int socket, char *buffer, size_t buffdim){
    ssize_t read, read_count = 0;
    char comm= 0;

    while(comm != '\n' && read_count < buffdim - 1){

        /* legge un carattere e controlla i possibili errori */
        do{
            if(read = recv(socket, &comm, sizeof(comm), 0) < 0){
                if(INTERRUPTED_BY_SIGNAL)
                    continue;
                else
                    return -1;
            }
            else break;
        }while(true);

        /* salva il byte letto nel buffer */
        if(read == 0) {
            buffer[read_count] = comm;
            read_count++;
        }
    }
    /* appendere per farlo diventare una stringa */
    buffer[read_count] = '\0';
    return read_count-1;
}

MsgType leggi_header(int socket)
{
    char comm;
    ssize_t read;
    /* legge un carattere e controlla i possibili errori */
    do{
        if(read = recv(socket, &comm, sizeof(uint32_t), 0) < 0){
            if(INTERRUPTED_BY_SIGNAL)
                continue;
            else
                return MsgType::ERROR;
        }
        else break;
    }while(true);

    /* salva il byte letto nel buffer */
        switch(comm)
        {
            case 0:
                return MsgType::NONCE;
            case 1:
                return MsgType::GETPATH;
            case 2:
                return MsgType::LOGIN;
            case 3:
                return MsgType::LOGOUT;
            case 4:
                return MsgType::REGISTER;


            default:
                break;
        }
    return MsgType::ERROR;
}

ssize_t send_file(int socket, const off_t fsize, time_t tstamp, FILE* file){
    ssize_t send;
    char c,*filedata;
    uint32_t file_dim, file_time;

    filedata = (char*)malloc(fsize);

    /* OK command */
    if((send = ssend(socket, OK, strlen(OK), MSG_NOSIGNAL)) !=
       strlen(OK)){
        free(filedata);
        return send;
    }

    /*  dimensione file  */
    file_dim = htonl(fsize);
    if((send = ssend(socket, &file_dim, sizeof(file_dim), MSG_NOSIGNAL)) !=
       sizeof(file_dim)){
        free(filedata);
        return send;
    }

    /* prima leggo tutto e poi mando*/
    for(int i = 0; i < fsize; i++){
        int f = fread(&c, sizeof(c), 1, file);
        if(f < 0){
            free(filedata);
            return -1;
        }
        filedata[i] = c;
    }
    send = ssend(socket, filedata, fsize, MSG_NOSIGNAL);
    if(send != fsize){
        free(filedata);
        return send;
    }

    /* timestamp */
    file_time = htonl(tstamp);
    if((send = ssend(socket, &file_time, sizeof(file_time), MSG_NOSIGNAL)) !=
       sizeof(file_time)){
        free(filedata);
        return send;
    }

    free(filedata);
    return fsize;
}


void thread_work()
{
    ssize_t read_result;
    ssize_t send_result;
    char buffer[BUFFER_DIM];
    char filename[DIM_FILENAME];
    uint32_t filesize;
    uint32_t filelastmod;
    FILE *fileptr;
    int s_connesso;
    unsigned long my_socket_index;
    bool logged=false;

    /* ciclo per accettare le connessioni*/
    while (true) {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        sockets.push_back( ss->accept_request(&addr,&len));
        mtx.lock();
        s_connesso= sockets.back().getSockfd();
        my_socket_index=sockets.size()-1;
        mtx.unlock();
        if (s_connesso < 0)
            continue;

        std::cout << "Connessione stabilita con -> " <<
                  inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl;

    /* ciclo che riceve i comandi client */
    while (true) {
        Message::message<MsgType> incoming_message;
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(s_connesso, &read_set);

        struct timeval t;
        t.tv_sec = TIMEOUT_SECONDI;
        t.tv_usec = TIMEOUT_MICROSECONDI;
        /* timing impostato a 15 */

        std::cout << "In attesa di ricevere comandi..." << std::endl;

        /* ricezione comando client */
        int s;
        if ((s = select(s_connesso + 1, &read_set, nullptr, nullptr, &t)) > 0) {

            incoming_message.set_id(leggi_header(s_connesso));

            switch(incoming_message.header.id)
            {
                case (MsgType::NONCE):
                    std::cout<<"Header di nonce ricevuto"<<std::endl;
                    read_result = leggi_comando(s_connesso, buffer, BUFFER_DIM);
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        break;
                    }
                    else
                    {
                        std::string body_nonce; // o vector<char> indifferente..tanto viene riallocato in incoming message
                        std::copy(&buffer[0],&buffer[read_result],std::back_inserter(body_nonce));
                        incoming_message << body_nonce; //possiamo passare direttamente body cipher, funziona anche i messaggi

                        std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;
                        Database::setNonce(incoming_message.body.data());
                        send_nonce_ok(s_connesso);
                    }
                    break;
                 case (MsgType::LOGIN):
                     std::cout<<"Header di login ricevuto"<<std::endl;
                    read_result = leggi_comando(s_connesso, buffer, BUFFER_DIM);
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        break;
                    }
                    else
                    {
                         std::vector<char> body_cipher;
                         std::copy(&buffer[0],&buffer[read_result],std::back_inserter(body_cipher));
                         incoming_message << body_cipher; //possiamo passare direttamente body cipher, funziona anche i messaggi

                         std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;

                        if(Database::checkUser(MsgType::LOGIN,body_cipher))
                        {
                            std::cout<<"Utente loggato correttamente"<<std::endl;
                            logged=true;
                            send_login_ok(s_connesso);
                        }
                        else
                        {
                            std::cout<<"Utente non riconosciuto"<<std::endl;
                            send_err_login(s_connesso);
                        }
                        break;
                    }

                case(MsgType::GETPATH):

                    if(!logged)
                    {
                        std::cout<<"Utente non loggato, mandato messaggio al client" <<std::endl;
                        send_err_login(s_connesso);
                        break;
                    }

                    read_result = leggi_comando(s_connesso, buffer, BUFFER_DIM);
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        break;
                    }

                    std::cout << "Comando ricevuto: " << buffer << std::endl;
                    incoming_message << std::string(buffer);
                    /* controllo inizio GET e fine  */
                    if (strncmp(GET, buffer, strlen(GET)) == 0 &&
                        strcmp(TERMINAZIONE, buffer + strlen(buffer) - strlen(TERMINAZIONE)) == 0) {

                        int i, j = 0;
                        memset(filename, 0, sizeof(char) * DIM_FILENAME);
                        for (i = strlen(GET); i < strlen(buffer) &&
                                              (buffer[i] != '\r' && buffer[i + 1] != '\n'); i++) {

                            filename[j] = buffer[i];
                            j++;
                        }
                        struct stat filestat;
                        /* open file and get its stat */
                        if ((fileptr = fopen(filename, "rb")) == NULL ||
                            stat(filename, &filestat) != 0) {
                            std::cout << "Impossibile aprire il file: " << filename << std::endl;
                            /* send error message to client and close the connection */
                            send_err(s_connesso);
                            break;
                        }
                        filesize = filestat.st_size;
                        filelastmod = filestat.st_mtime;

                        std::cout << "Ricerca file andata a buon fine." << std::endl;
                        send_result = send_file(s_connesso, filesize, filelastmod,
                                                fileptr);
                        if (send_result < 0) {

                            send_err(s_connesso);
                            break;
                        }
                        std::cout << "File mandato con successo." << std::endl;
                        fclose(fileptr);
                        continue;

                    } else if (strcmp(FINE, buffer) == 0) {
                        break;
                    } else {
                        send_err(s_connesso);
                        break;
                    }

                case (MsgType::LOGOUT):
                    logged=false;
                    std::cout<<"Ricevuto messaggio di Logout, chiusura connessione."<<std::endl;
                    break;


                case(MsgType::REGISTER):
                    std::cout<<"Header register ricevuto"<<std::endl;
                    read_result = leggi_comando(s_connesso, buffer, BUFFER_DIM);
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        break;
                    }
                    else
                    {
                        std::vector<char> body_cipher_r; // o vector<char> indifferente..tanto viene riallocato in incoming message
                        std::copy(&buffer[0],&buffer[read_result],std::back_inserter(body_cipher_r));
                        incoming_message << body_cipher_r; //possiamo passare direttamente body cipher, funziona anche i messaggi

                        std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;

                        if(Database::checkUser(MsgType::REGISTER,body_cipher_r))
                        {
                            std::cout<<"Utente registrato e loggato correttamente"<<std::endl;
                            send_register_ok(s_connesso);
                        }
                        else
                        {
                            std::cout<<"Utente gia' registrato"<<std::endl;
                            send_err_register(s_connesso);
                        }
                        logged=true;
                        break;
                    }
                default:
                    break;
            }
            if(!logged && incoming_message.header.id!=MsgType::NONCE)
                break;
        }
            /* select() ritorna 0, timeout */
        else if (s == 0) {
            std::cout << "Nessuna risposta dopo " << TIMEOUT_SECONDI << " secondi, chiusura." << std::endl;
            send_err(s_connesso);
            break;

        } /* fallimento select() */
        else {
            std::cout << "Errore nella select()" << std::endl;
            send_err(s_connesso);
            break;
        }
    }

        mtx.lock();
        sockets.erase(sockets.begin()+my_socket_index);
        mtx.unlock();

        std::cout << "In attesa di connessioni..." << std::endl;
    }
}