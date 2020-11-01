//
// Created by enrico_scalabrino on 10/10/20.
//
#include "Server.h"
#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <sodium/crypto_pwhash.h>
#include <boost/asio/ts/buffer.hpp>
#include "Socket.h"
#include "SocketWrap.h"
#include "Database.h"
#include "Message.h"
#include <boost/filesystem.hpp>


#define TIMEOUT_SECONDI 360
#define TIMEOUT_MICROSECONDI 0
#define OK "+OK\r\n"
#define ERRORE "-SERVER: ERRORE GENERICO\r\n"
#define ERRORE_LOGIN "-SERVER: LOGIN FALLITO\r\n"
#define ERRORE_LOGOUT "-SERVER: LOGOUT FALLITO\r\n"
#define ERRORE_RICHIESTA_FILE "-SERVER: FILE CERCATO NON TROVATO\r\n"
#define OK_LOGIN "-SERVER: CLIENT LOGGED\r\n"
#define OK_LOGOUT "-SERVER: CLIENT LOGOUT\r\n"
#define OK_NONCE "-SERVER: NONCE SETTED\r\n"
#define OK_REGISTER "-SERVER: REGISTRAZIONE AVVENUTA\r\n"
#define ERRORE_REGISTER "-SERVER: REGISTRAZIONE FALLITA\r\n"
#define OK_FILE "-SERVER: FILE MANDATO CON SUCCESSO\r\n"
#define TIMEOUT_ERR "-SERVER: TIMEOUT SESSION EXPIRED\r\n"
#define CRC_HEADER "-SERVER: CRC\r\n"

ssize_t send_msg_client(int sock,std::string& msg_client);
ssize_t send_file(int socket,  off_t fsize, time_t tstamp, FILE* file);
ssize_t leggi_comando(int socket, std::vector<unsigned char>& buffer,size_t size);
void progress_bar();
Message::message_header<MsgType> leggi_header(int socket);
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
        pinstance_->setServerPath("../server_user/");
        boost::filesystem::create_directories(pinstance_->getServerPath());
    }
    else
    {
        std::cout << "Creazione del server gia' avvenuta, server creato in precedenza con porta: " << pinstance_->getPort()<< std::endl;
    }
    return pinstance_;
}

const std::string &Server::getServerPath() const {
    return server_path;
}

void Server::setServerPath(const std::string &serverPath) {
    server_path = serverPath;
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
    if(pinstance_)
    {
        std::cout << "Server Program" << std::endl;
        std::cout << std::endl;
        Database::create_instance();
        std::cout << "In attesa di connessioni..." << std::endl;
        create_threads();
    }
    else std::cout<<"Errore, non si e' inizializzato un server in una determinata porta.."
                    "(start() method)"<<std::endl;

}


ssize_t send_msg_client(int sock,std::string& msg_client)
{
    ssize_t s;
    s = Send(sock, msg_client.c_str(), msg_client.length(), MSG_NOSIGNAL);
    return s;
}

ssize_t leggi_comando(int socket, std::vector<unsigned char>& buffer,size_t size){

    ssize_t read, read_count = 0,counter=0;
    unsigned char comm= 0;
    /*
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket sock(io_context);
    sock.assign(boost::asio::ip::tcp::v4(), socket);
    size_t read_count = sock.read_some(boost::asio::buffer(buffer, size));

     */

    while(counter < size){

        /* legge un carattere e controlla i possibili errori */
        do{
            if(read = recv(socket, &comm, sizeof(comm), 0) < 0){
                if(INTERRUPTED_BY_SIGNAL)
                    continue;
                else
                {
                    read=-1;
                    return read;
                }

            }
            else break;
        }while(true);

        buffer.push_back(comm);
        read_count++;
        counter++;
    }
    /* appendere per farlo diventare una stringa */

    return read_count;
}

Message::message_header<MsgType> leggi_header(int socket)
{
    int comm,counter=0;
    ssize_t read;
    Message::message_header<MsgType> msg;
    /* legge un carattere e controlla i possibili errori */
    while(counter<2)
    {
        do{
            if(read = recv(socket, &comm, sizeof(comm), 0) < 0){
                if(INTERRUPTED_BY_SIGNAL)
                    continue;
                else
                {
                    read=0;
                    msg.size=read;
                    msg.id=MsgType::ERROR;
                    return msg;
                }

            }
            else break;
        }while(true);

         comm=ntohl(comm);
         counter++;

        if(counter==1)
        {

            if(comm < crypto_pwhash_STRBYTES)  //controllo dimensione & sicurezza //todo
                msg.size=comm;
            else{
                msg.size=0; //null
                msg.id=MsgType::ERROR;
                return msg;
            }
        }
        else
        {
            /* salva il byte letto nel buffer */
            switch(comm)
            {
                case 0:
                    msg.id=MsgType::NONCE;
                    break;
                case 1:
                    msg.id=MsgType::GETPATH;
                    break;
                case 2:
                    msg.id=MsgType::LOGIN;
                    break;
                case 3:
                    msg.id=MsgType::LOGOUT;
                    break;
                case 4:
                    msg.id=MsgType::REGISTER;
                    break;
                case 5:
                    msg.id=MsgType::CRC;
                    break;
                case 6:
                    msg.id=MsgType::ERROR;
                    break;
                case 7:
                    msg.id=MsgType::TRY_AGAIN_REGISTER;
                    break;
                case 8:
                    msg.id=MsgType::TRY_AGAIN_LOGIN;
                    break;

                default:
                    msg.id=MsgType::ERROR;
                    break;
            }
        }
    }
    return msg;
}

ssize_t send_file(int socket, const off_t fsize, time_t tstamp, FILE* file){

    uint32_t file_dim, file_time;
    ssize_t send;
    std::vector<char> filedata(fsize);
    char c;

    /* OK command */
    if((send = Send(socket, OK, strlen(OK), MSG_NOSIGNAL)) !=
       strlen(OK)){
        return send;
    }

    /*  dimensione file  */
    file_dim = htonl(fsize);
    if((send = Send(socket, &file_dim, sizeof(file_dim), MSG_NOSIGNAL)) !=
       sizeof(file_dim)){
        return send;
    }

    /* prima leggo tutto e poi mando*/
    for(int i = 0; i < fsize; i++){
        int f = fread(&c, sizeof(c), 1, file);
        if(f < 0){
            return -1;
        }
        filedata[i]=c;
    }
    send = Send(socket, filedata.data(), fsize, MSG_NOSIGNAL);
    if(send != fsize){
        return send;
    }

    /* timestamp */
    file_time = htonl(tstamp);
    if((send = Send(socket, &file_time, sizeof(file_time), MSG_NOSIGNAL)) !=
       sizeof(file_time)){
        return send;
    }
    return fsize;
}

void progress_bar()
{
    float progress = 0.0;
    while (progress < 1.0) {
        size_t barWidth = 70;

        std::cout << "[";
        size_t pos = barWidth * progress;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << " %\r";
        std::cout.flush();

        progress += 0.16; // for demonstration only
    }
    std::cout << std::endl;


}

void thread_work()
{
    ssize_t read_result;
    ssize_t send_result;
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
        //std::cout << "In attesa di connessioni..." << std::endl;
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
        std::vector<unsigned char> buffer;
        std::string client_msg;
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

            incoming_message.header=leggi_header(s_connesso);

            switch(incoming_message.header.id)
            {
                case (MsgType::NONCE):
                {
                    std::cout<<"Header di nonce ricevuto"<<std::endl;
                    read_result = leggi_comando(s_connesso, buffer,incoming_message.header.size);
                    std::cout<<"BUFFER SIZE:  "<<buffer.size()<<std::endl;
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        break;
                    }
                    else
                    {
                        incoming_message << buffer; //possiamo passare direttamente body cipher, funziona anche i messaggi
                        std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;
                        Database::setNonce(buffer.data());
                        client_msg=OK_NONCE;
                        send_msg_client(s_connesso,client_msg);
                    }
                    break;
                }

                case(MsgType::REGISTER):
                {
                    std::cout<<"Header register ricevuto"<<std::endl;
                    read_result = leggi_comando(s_connesso, buffer,incoming_message.header.size);
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        break;
                    }
                    else
                    {
                        incoming_message << buffer; //possiamo passare direttamente body cipher, funziona anche i messaggi

                        std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;
                        std::cout<<"BUFFER SIZE:  "<<buffer.size()<<std::endl;
                        if(Database::checkUser(MsgType::REGISTER,buffer))
                        {
                            std::cout<<"Utente registrato e loggato correttamente"<<std::endl;
                            client_msg=OK_REGISTER;
                            send_msg_client(s_connesso,client_msg);
                            logged=true;
                        }
                        else
                        {
                            std::cout<<"Utente gia' registrato,mandato TRY AGAIN"<<std::endl;
                            client_msg=ERRORE_REGISTER;
                            send_msg_client(s_connesso,client_msg);
                            MsgType id = MsgType::TRY_AGAIN_REGISTER;
                            uint32_t id_newtork=htonl(incoming_message.get_id_uint32(id));
                            Send(s_connesso,&id_newtork,sizeof(id_newtork),MSG_NOSIGNAL);
                            continue;
                        }

                        break;
                    }
                }

                 case (MsgType::LOGIN):
                 {
                     std::cout<<"Header di login ricevuto"<<std::endl;
                     read_result = leggi_comando(s_connesso, buffer,incoming_message.header.size);
                     if (read_result == -1) {
                         std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                         break;
                     }
                     else
                     {
                         incoming_message << buffer;

                         std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;
                         std::cout<<"BUFFER SIZE:  "<<buffer.size()<<std::endl;

                         if(logged)
                         {
                             std::cout<<"Utente gia' loggato"<<std::endl;
                             client_msg=OK_LOGIN;
                             send_msg_client(s_connesso,client_msg);
                         }
                         else
                         {
                             if(Database::checkUser(MsgType::LOGIN,buffer))
                             {
                                 std::cout<<"Utente loggato correttamente"<<std::endl;
                                 client_msg=OK_LOGIN;
                                 send_msg_client(s_connesso,client_msg);
                                 logged=true;
                             }
                             else {
                                 std::cout << "Utente non riconosciuto" << std::endl;
                                 client_msg=ERRORE_LOGIN;
                                 send_msg_client(s_connesso,client_msg);
                                 MsgType id = MsgType::TRY_AGAIN_LOGIN;
                                 uint32_t id_newtork=htonl(incoming_message.get_id_uint32(id));
                                 Send(s_connesso,&id_newtork,sizeof(id_newtork),MSG_NOSIGNAL);
                                 continue;
                             }
                         }

                         break;
                     }
                 }


                case(MsgType::GETPATH):
                {
                    read_result = leggi_comando(s_connesso, buffer,incoming_message.header.size);
                    if(!logged)
                    {
                        std::cout<<"Utente non loggato, mandato messaggio al client" <<std::endl;
                        client_msg=ERRORE_LOGIN;
                        send_msg_client(s_connesso,client_msg);
                        MsgType id = MsgType::TRY_AGAIN_LOGIN;
                        uint32_t id_newtork=htonl(incoming_message.get_id_uint32(id));
                        Send(s_connesso,&id_newtork,sizeof(id_newtork),MSG_NOSIGNAL);
                        continue;
                    }
                    std::string buffer_str(buffer.begin(),buffer.end());
                    if (read_result == -1) {
                        std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                        continue;
                    }
                    std::cout << "Comando ricevuto: " << buffer.data() << std::endl;
                    incoming_message << buffer;
                    /* controllo inizio GET e fine  */

                        struct stat filestat;
                        /* open file and get its stat */
                        if ((fileptr = fopen(incoming_message.body.data(), "rb")) == nullptr ||
                            stat(incoming_message.body.data(), &filestat) != 0) {
                            std::cout << "Impossibile aprire il file: " << incoming_message.body.data() << std::endl;
                            /* send error message to client and close the connection */
                            client_msg=ERRORE_RICHIESTA_FILE;
                            send_msg_client(s_connesso,client_msg);
                            continue;
                        }
                        filesize = filestat.st_size;
                        filelastmod = filestat.st_mtime;

                        std::cout << "Ricerca file andata a buon fine." << std::endl;
                        send_result = send_file(s_connesso, filesize, filelastmod,
                                                fileptr);
                        if (send_result < 0) {
                            client_msg=ERRORE_RICHIESTA_FILE;
                            send_msg_client(s_connesso,client_msg);
                            continue;
                        }
                        std::cout << "File mandato con successo." << std::endl;
                         client_msg=OK_FILE;
                        send_msg_client(s_connesso,client_msg);
                        fclose(fileptr);
                        continue;
                }

                case (MsgType::LOGOUT):
                {
                    if(logged)
                    {
                        logged=false;
                        std::cout<<"Ricevuto messaggio di Logout, chiusura connessione."<<std::endl;
                        client_msg=OK_LOGOUT;
                        send_msg_client(s_connesso,client_msg);
                        continue;
                    }
                    else
                    {
                        client_msg=ERRORE_LOGOUT;
                        send_msg_client(s_connesso,client_msg);
                        continue;
                    }
                }
                case(MsgType::ERROR):
                {
                    logged=false;
                    break;
                }
                default:
                {
                    logged=false;
                    break;
                }
            }
            if(!logged && incoming_message.header.id!=MsgType::NONCE)
                break;
        }
            /* select() ritorna 0, timeout */
        else if (s == 0) {
            std::cout << "Nessuna risposta dopo " << TIMEOUT_SECONDI << " secondi, chiusura." << std::endl;
            client_msg=TIMEOUT_ERR;
            send_msg_client(s_connesso,client_msg);
            logged=false;
            continue;

        } /* fallimento select() */
        else {
            std::cout << "Errore nella select()" << std::endl;
            client_msg=ERRORE;
            send_msg_client(s_connesso,client_msg);
            break;
        }
    }

        mtx.lock();
        sockets.erase(sockets.begin()+my_socket_index);
        mtx.unlock();


        std::cout << "In attesa di connessioni..." << std::endl;
    }
}