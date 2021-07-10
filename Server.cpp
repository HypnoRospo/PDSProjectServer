#include "Server.h"
#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <filesystem>
#include <sodium/crypto_pwhash.h>
#include <boost/asio/ts/buffer.hpp>
#include "Socket.h"
#include "SocketWrap.h"
#include "Database.h"
#include "Message.h"
#include <boost/filesystem.hpp>
#include <boost/crc.hpp>
#include <cppconn/exception.h>



#define PRIVATE_BUFFER_SIZE  8192
#define TIMEOUT_SECONDI 1000
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
#define OK_CHECKSUM "-SERVER: CHECKSUM CORRETTO\r\n"
#define ERRORE_CHECKSUM "-SERVER: CHECKSUM NON CORRETTO\r\n"
#define OK_FILE "-SERVER: FILE MANDATO CON SUCCESSO\r\n"
#define OK_FILE_R "-SERVER: FILE RICEVUTO CON SUCCESSO\r\n"
#define OK_FILE_DELETED "-SERVER: FILE o PATH ELIMINATO CON SUCCESSO\r\n"
#define TIMEOUT_ERR "-SERVER: TIMEOUT SESSION EXPIRED\r\n"

ssize_t send_msg_client(int sock,std::string& msg_client);
ssize_t send_file(int socket,  off_t fsize, FILE* file,std::string& file_path);
ssize_t leggi_comando(int socket, std::vector<unsigned char>& buffer,size_t size);
std::string calculate_checksum(std::ifstream &ifs);
Message::message_header<MsgType> leggi_header(int socket);
void thread_work();
void create_threads();

/**
 * Static methods should be defined outside the class.
 */

Server* Server::pinstance_{nullptr}; /* singleton */
std::mutex Server::mutex_; /* singleton */
std::vector<std::thread> threads;
ServerSocket *ss {nullptr};  /* Puntatore alla struttura dati creata dal professore */
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
        int count = 0;
        int maxTries = 3;

        while(true)
        {
            try {
                ss = new ServerSocket(port);
                break;
            }
            catch(std::runtime_error &re)
            {
                if (++count == maxTries)
                {
                    std::cout<<re.what()<<std::endl;
                    exit(EXIT_FAILURE);
                }
                sleep(5);
            }
        }
        pinstance_->setServerPath("../server_users/");
        boost::filesystem::create_directories(Server::getServerPath());
        boost::filesystem::current_path(Server::getServerPath());
    }
    else
    {
        std::cout << "Creazione del server gia' avvenuta, server creato in precedenza con porta: " << pinstance_->getPort()<< std::endl;
    }
    return pinstance_;
}

const std::string &Server::getServerPath() {
    return pinstance_->server_path;
}

void Server::setServerPath(const std::string &serverPath) {
    server_path = serverPath;
}

void create_threads()
{
    unsigned int Num_Threads =  std::thread::hardware_concurrency()-3;//da modificare

    for(int i = 0; i < Num_Threads; i++)
    {  threads.emplace_back(thread_work);}

    /* in realta' il thread principale potrebbe gia' finire, valutare come gestire al meglio questa situazione */

    for(int j=0; j<Num_Threads ;j++)
    { threads[j].detach();}

    thread_work();
}


void Server::work() {
    if(pinstance_)
    {
        std::cout << "Server Program" << std::endl;
        std::cout << std::endl;
        try{
            Database::create_instance();
        }
        catch(sql::SQLException &e)
        {
            std::cout<<"Impossibile connettersi al database, riprovare piu' tardi."<<std::endl;
            std::cout<<"Chiusura immediata"<<std::endl;
            exit(EXIT_FAILURE);
        }

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

    size_t original_bytes_to_recv = size;
    // Continue looping while there are still bytes to receive
    auto buffer_tmp=(unsigned char*) malloc(sizeof(unsigned char)*size);

    while (size > 0)
    {
        ssize_t ret = recv(socket, buffer_tmp, size, 0);
        if (ret <= 0)
        {
            // Error or connection closed
            return ret;
        }
        // We have received ret bytes
        size -= ret;  // Decrease size to receive for next iteration
        buffer_tmp+= ret;     // Increase pointer to point to the next part of the buffer
    }
    buffer_tmp-=original_bytes_to_recv;
    buffer.insert(buffer.begin(),buffer_tmp,buffer_tmp+original_bytes_to_recv);
    free(buffer_tmp);
    return original_bytes_to_recv;  // Now all data have been received
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
        {    //controllo dimensione & sicurezza?
            msg.size=comm;
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
                case 9:
                    msg.id=MsgType::NEW_FILE;
                    break;
                case 10:
                    msg.id=MsgType::DELETE;
                    break;
                case 11:
                    msg.id=MsgType::END;
                    break;
                case 12:
                    msg.id = MsgType::ELEMENT_CLIENT;
                    break;

                default:
                    msg.id=MsgType::ERROR;
                    break;
            }
        }
    }
    return msg;
}

ssize_t send_file(int socket,  off_t fsize, FILE* file,std::string& file_path){

    ssize_t send;
    std::vector<char> filedata(fsize);

    std::string del="\r\n";
    file_path=file_path+del;

    std::string total_to_send;
    total_to_send.append(OK);
    total_to_send.append(file_path);
    total_to_send.append(std::to_string(fsize));
    total_to_send.append(del);
    /* OK command
    if((send = Send(socket, OK, strlen(OK), MSG_NOSIGNAL)) !=
       strlen(OK)){
        return send;
    }


    if((send = Send(socket, file_path.c_str(), file_path.size(), MSG_NOSIGNAL)) !=
       file_path.size()){
        return send;
    }

    file_dim = htonl(fsize);
    if((send = Send(socket, &file_dim, sizeof(file_dim), MSG_NOSIGNAL)) !=
       sizeof(file_dim)){
        return send;
    }

    if((send = Send(socket, del.c_str() ,del.size(), MSG_NOSIGNAL)) !=
       del.size()){
        return send;
    }
     */
    fread(&filedata[0],filedata.size()*sizeof(char), 1, file);
    std::string data (filedata.begin(),filedata.end());
    total_to_send.append(data);



    send = Send(socket, total_to_send.data(), total_to_send.size(), MSG_NOSIGNAL);
    if(send != fsize){
        return send;
    }


    /*
    file_time = htonl(tstamp);
    if((send = Send(socket, &file_time, sizeof(file_time), MSG_NOSIGNAL)) !=
       sizeof(file_time)){
        return send;
    }
     */
    return fsize;
}

/* rivedere e testare bene questa parte ! */
/* gestione critica delle risorse -> controllare bene e semmai riprogettare */

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
    std::string user_now;
    std::vector<Socket> wrapper;
    /* ciclo per accettare le connessioni*/
    while (true) {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        //std::cout << "In attesa di connessioni..." << std::endl;
        try{
            wrapper.push_back(ss->accept_request(&addr,&len));
            s_connesso= wrapper.back().getSockfd();
            if (s_connesso < 0)
                continue;

            std::cout << "Connessione stabilita con -> " <<
                      inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl;

        }
        catch(std::runtime_error &re)
        {
            std::cout<<re.what()<<std::endl;
            continue;
        }
        /* mtx.lock cosi dovrebbe fungere */
        /*
        mtx.lock();
        s_connesso= sockets.back().getSockfd();
        my_socket_index=sockets.size()-1;
        mtx.unlock();
         */

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
                            std::pair<std::string,bool> usr_logged;
                            if((usr_logged=Database::checkUser(MsgType::REGISTER,buffer)).second)
                            {
                                std::cout<<"Utente registrato e loggato correttamente"<<std::endl;
                                client_msg=OK_REGISTER;
                                send_msg_client(s_connesso,client_msg);
                                logged=true;
                                user_now=usr_logged.first;
                            }
                            else
                            {
                                std::cout<<"Utente gia' registrato o errore nella registrazione,mandato TRY AGAIN"<<std::endl;
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
                                std::pair<std::string,bool> usr_logged;
                                if((usr_logged=Database::checkUser(MsgType::LOGIN,buffer)).second)
                                {
                                    std::cout<<"Utente loggato correttamente"<<std::endl;
                                    client_msg=OK_LOGIN;
                                    send_msg_client(s_connesso,client_msg);
                                    logged=true;
                                    user_now=usr_logged.first;
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

                        std::string path_init= buffer_str.substr(0,user_now.size());

                        if(path_init!=user_now)
                        {
                            std::cout<<"Path iniziale non corrisponde all'user attuale, impossibile procedere."<<std::endl;
                            /* send error message to client and close the connection */
                            client_msg=ERRORE_RICHIESTA_FILE;
                            send_msg_client(s_connesso,client_msg);
                            continue;
                        }

                        if (read_result == -1) {
                            std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                            continue;
                        }
                        std::cout << "Comando ricevuto: " << buffer.data() << std::endl;
                        incoming_message << buffer;
                        /* controllo inizio GET e fine  */

                        try{
                            filesize=boost::filesystem::file_size(buffer_str);
                            fileptr = fopen(buffer_str.c_str(), "rb");
                        }
                        catch(boost::filesystem::filesystem_error &fe)
                        {
                            std::cout << "Impossibile aprire il file: " << incoming_message.body.data() << std::endl;
                            /* send error message to client and close the connection */
                            client_msg=ERRORE_RICHIESTA_FILE;
                            send_msg_client(s_connesso,client_msg);
                            continue;
                        }
                        std::cout << "Ricerca file andata a buon fine." << std::endl;
                        send_result = send_file(s_connesso, filesize,
                                                fileptr,buffer_str);
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

                    case(MsgType::NEW_FILE):
                    {
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
                            //creare un file e metterlo nel nosstro direttorio
                            size_t pos=0;
                            std::string delimiter = "\r\n";
                            std::string path_user;
                            std::string body(incoming_message.body.begin(),incoming_message.body.end());
                            pos = body.find(delimiter);
                            path_user = body.substr(0, pos);
                            body.erase(0, pos + delimiter.length());
                            boost::filesystem::path target =Server::getServerPath()+path_user;
                            //controllo cartelle + smanetting
                            pos = target.string().find_last_of('/');
                            if((pos+1)!=target.size())
                                boost::filesystem::create_directories(target.string().substr(0,pos));
                            else boost::filesystem::create_directories(target);
                            //
                            client_msg=OK_FILE_R;
                            if((pos+1)!=target.size())
                            {
                                std::ofstream  os(target,std::ios::out | std::ios::binary | std::ios::trunc);
                                if (os.is_open())
                                {
                                    os<<body;
                                    os.close();
                                }
                                else {
                                    std::cout << "Unable to open file";
                                    client_msg=ERRORE;
                                }
                            }
                            send_msg_client(s_connesso,client_msg);
                        }
                        break;
                    }

                    case(MsgType::ELEMENT_CLIENT): {
                        read_result = leggi_comando(s_connesso, buffer, incoming_message.header.size);
                        std::cout << "BUFFER SIZE:  " << buffer.size() << std::endl;
                        if (read_result == -1) {
                            std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                            break;
                        } else {
                            if (std::filesystem::exists("../server_users/" + user_now + "/")) {
                                incoming_message << buffer; //possiamo passare direttamente body cipher, funziona anche i messaggi
                                std::cout << "File presenti nella folder del client " <<user_now << ": " << incoming_message.body.data() << std::endl;
                                bool flag = false;
                                std::vector<std::string> elements;
                                std::string body(incoming_message.body.begin(), incoming_message.body.end());
                                while (!flag) {
                                    size_t pos = 0;
                                    std::string delimiter = "\r\n";
                                    std::string path_user;
                                    //std::string body(incoming_message.body.begin(), incoming_message.body.end());
                                    pos = body.find(delimiter);
                                    path_user = body.substr(0, pos);
                                    body.erase(0, pos + delimiter.length());
                                    std::string target = Server::getServerPath() + path_user;
                                    elements.push_back(target);
                                    if (body.empty())
                                        flag = true;
                                }


                                std::vector<std::string> tempfs2;
                                for (auto &file :  std::filesystem::recursive_directory_iterator("../server_users/" + user_now + "/")) {
                                    // if (!file.is_directory()) {
                                    tempfs2.push_back(file.path());
                                    std::cout << "File o cartella dell'utente " << user_now << " presente nel server: " << file << std::endl;
                                    // }
                                }

                                for (auto ele_server : tempfs2) {
                                    bool flag_check = false;
                                    for (auto ele_client : elements) {
                                        if (ele_client == ele_server)
                                            flag_check = true;
                                    }
                                    if (flag_check == false) {
                                        if(std::filesystem::exists(ele_server))
                                            std::filesystem::remove_all(ele_server);
                                    }
                                }

/*
                            std::vector<std::string> dirtodelete;
                            for (auto &filedir :  std::filesystem::recursive_directory_iterator(
                                    "../server_users/" + user_now + "/")) {
                                if (filedir.is_directory()) {
                                    if (std::filesystem::is_empty(filedir))
                                        dirtodelete.push_back(filedir.path());
                                }
                            }
                            for (auto it : dirtodelete)
                                std::filesystem::remove(it);
*/
                            }else
                                std::filesystem::create_directories("../server_users/" + user_now + "/");
                        }


                        break;
                    }


                    case(MsgType::CRC): {

                        read_result = leggi_comando(s_connesso, buffer, incoming_message.header.size);
                        std::cout << "BUFFER SIZE:  " << buffer.size() << std::endl;
                        if (read_result == -1) {
                            std::cout << "Impossibile leggere il comando dal client, riprovare." << std::endl;
                            break;
                        } else {
                            incoming_message
                                    << buffer; //possiamo passare direttamente body cipher, funziona anche i messaggi
                            std::cout << "Comando ricevuto: " << incoming_message.body.data() << std::endl;
                            //creare un file e metterlo nel nosstro direttorio
                            size_t pos = 0;
                            std::string delimiter = "\r\n";
                            std::string path_user;
                            std::string body(incoming_message.body.begin(), incoming_message.body.end());
                            pos = body.find(delimiter);
                            path_user = body.substr(0, pos);
                            body.erase(0, pos + delimiter.length());
                            boost::filesystem::path target = Server::getServerPath() + path_user;
                            /* Il body ora contiene solo il checksum */
                            std::string checksum = body.substr(0,body.find(delimiter));
                            /* Se l'if restituisce TRUE non c'è bisogno di richiedere il file, se FALSE allora c'è bisogno del file */

                            if(boost::filesystem::exists(target))
                            {
                                std::ifstream  ifs(target,std::ios::binary);
                                if (checksum == calculate_checksum(ifs)){

                                    //todo: Server dice al client che è gia tutto ok, else dice al client di inviare il file
                                    std::cout<<"Checksum corrispondente, file gia' presente nel server"<<std::endl;
                                    client_msg=OK_CHECKSUM;
                                }
                                else
                                {
                                    std::cout<<"Checksum non corrisposto, file diverso da quello presente nel sistema, sincronizzazione necessaria"<<std::endl;
                                    client_msg=ERRORE_CHECKSUM;
                                    client_msg.append(path_user);
                                    client_msg.append(delimiter);
                                }
                            }
                            else
                            {
                                std::cout<<"File non presente nel sistema, sincronizzazione necessaria"<<std::endl;
                                client_msg=ERRORE_CHECKSUM;
                                client_msg.append(path_user);
                                client_msg.append(delimiter);
                            }

                            send_msg_client(s_connesso,client_msg);
                        }
                        break;
                    }

                    case (MsgType::DELETE):
                    {
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
                            //creare un file e metterlo nel nosstro direttorio
                            size_t pos=0;
                            std::string delimiter = "\r\n";
                            std::string path_user;
                            std::string body(incoming_message.body.begin(),incoming_message.body.end());
                            pos = body.find(delimiter);
                            path_user = body.substr(0, pos);
                            //body.erase(0, pos + delimiter.length());
                            boost::filesystem::path target =Server::getServerPath()+path_user;
                            /*
                            if(boost::filesystem::is_regular_file(target))
                            {
                                if(!boost::filesystem::remove(target)) {
                                    client_msg = ERR_DELETED;
                                }
                            }
                            else
                             */
                            boost::filesystem::remove_all(target);
                            client_msg = OK_FILE_DELETED;
                            send_msg_client(s_connesso,client_msg);
                        }
                        break;
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

                    case (MsgType::END):
                    {
                        logged=false;
                        std::cout<<"Ricevuto messaggio di chiusura connessione."<<std::endl;
                        break;
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
            buffer.clear();
            buffer.shrink_to_fit();
        }

        //stampare informazioni

        std::cout<<"Chiusura socket id: "<<wrapper.back().getSockfd()<<std::endl;
        wrapper.clear();
        std::cout << "In attesa di connessioni..." << std::endl;
    }
}


std::string calculate_checksum(std::ifstream &ifs) {

    std::streamsize const  buffer_size = PRIVATE_BUFFER_SIZE;

    try
    {
        boost::crc_32_type  result;
        clock_t tStart = clock();
        if ( ifs )
        {
            // get length of file:
            ifs.seekg (0, std::ifstream::end);
            int length = ifs.tellg();
            ifs.seekg (0, std::ifstream::beg);
            //

            if(length > buffer_size)
            {
                std::vector<char>   buffer(buffer_size);
                while(ifs.read(&buffer[0], buffer_size))
                    result.process_bytes(&buffer[0], ifs.gcount());
            }
            else {
                std::vector<char> buffer(length);
                ifs.read(&buffer[0],length);
                result.process_bytes(&buffer[0],ifs.gcount());
            }
        }
        else
        {
            std::cerr << "Impossibile aprire il file '"<< std::endl;
        }
        std::cout<<"Tempo impiegato per il calcolo del CRC: "<<(double)(clock() - tStart)/CLOCKS_PER_SEC;

        std::cout << std::hex << std::uppercase << result.checksum() << std::endl;

        std::stringstream stream;
        stream << std::hex << std::uppercase << result.checksum();
        return stream.str();

    }
    catch ( std::exception &e )
    {
        std::cerr << "Found an exception with '" << e.what() << "'." << std::endl;
        return e.what();
        /* VA GESTITA LA RETURN ADATTA */
    }

}
