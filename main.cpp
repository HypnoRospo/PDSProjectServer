#include <iostream>
#include<thread>
//#include <openssl/sha.h>
#include <boost/asio/ts/buffer.hpp>
//#include <boost/asio/ts/internet.hpp>
#include "Server.h"
#include "Socket.h"

#define BUFFER_DIM 255
#define DIM_FILENAME 255
#define TIMEOUT_SECONDI 15
#define TIMEOUT_MICROSECONDI 0
#define GET "GET "
#define TERMINAZIONE "\r\n\0"
#define OK "+OK\r\n"
#define FINE "FINE\r\n\0"
#define ERRORE "-ERR\r\n"


ssize_t send_err(int sock);
ssize_t ssend(int socket, const void *bufptr, size_t nbytes, int flags);
ssize_t send_file(int socket, const off_t fsize, time_t tstamp, FILE* file);
ssize_t leggi_comando(int socket, char *buffer, size_t buffdim);


int main() {

    //Server *server = new Server();

    ServerSocket ss (5000); //gli passeremo la porta tramite linea di comando


    int s_connesso;
    ssize_t 			read_result;
    ssize_t				send_result;

    char				buffer[BUFFER_DIM];
    char				filename[DIM_FILENAME];
    uint32_t			filesize;
    uint32_t			filelastmod;
    FILE*			    fileptr;

    std::cout << "Server Program" << std::endl;
    std::cout << std::endl;


    /*
    std::cout <<"If you see the same value, then singleton was reused (yay!\n" <<
              "If you see different values, then 2 singletons were created (booo!!)\n" <<
              "Impossible with mutex locking.\n\n" <<
              "RESULT:\n";
    std::thread t1(ThreadBar);
    std::thread t2(ThreadFoo);
    t1.join();
    t2.join();

     */

    /* ciclo per accettare le connessioni*/
    while(true){
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        std::cout<<"In attesa di connessioni..."<<std::endl;
        Socket s = ss.accept_request(&addr,&len);
        s_connesso=s.getSockfd();
        if(s_connesso < 0){
            std::cout<<"Impossibile stabilire la connessione con il client %s attraverso porta %u"<<
                     inet_ntoa(addr.sin_addr)<<ntohs(addr.sin_port)<<std::endl;
            continue;
        }
        std::cout<<"Connessione stabilita con -> "<<
        inet_ntoa(addr.sin_addr)<<":"<<ntohs(addr.sin_port)<<std::endl;

        /* ciclo che riceve i comandi client */
        while(true){
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(s_connesso, &read_set);

            struct timeval	t;
            t.tv_sec = TIMEOUT_SECONDI;
            t.tv_usec = TIMEOUT_MICROSECONDI;
            /* timing impostato a 15 */


            std::cout<<"In attesa di ricevere comandi..."<<std::endl;

            /* ricezione comando client */
            int s;
            if((s = select(s_connesso+1, &read_set, nullptr, nullptr, &t)) > 0){

                read_result = leggi_comando(s_connesso, buffer, BUFFER_DIM);
                if(read_result == -1){
                    std::cout<<"Impossibile leggere il comando dal client, riprovare."<<std::endl;
                    break;
                }

                std::cout<<"Comando ricevuto: "<<buffer<<std::endl;

                /* controllo inizio GET e fine  */
                if(strncmp(GET, buffer, strlen(GET)) == 0 &&
                   strcmp(TERMINAZIONE, buffer+strlen(buffer)-strlen(TERMINAZIONE)) == 0){

                    int i, j= 0;
                    memset(filename, 0, sizeof(char)*DIM_FILENAME);
                    for(i = strlen(GET); i < strlen(buffer) &&
                                         (buffer[i] != '\r' && buffer[i+1] != '\n'); i++){

                        filename[j] = buffer[i];
                        j++;
                    }
                    struct stat			filestat;
                    /* open file and get its stat */
                    if((fileptr = fopen(filename, "rb")) == NULL ||
                       stat(filename, &filestat) != 0){
                  std::cout<<"Impossibile aprire il file: " <<filename<<std::endl;
                        /* send error message to client and close the connection */
                        send_err(s_connesso);
                        break;
                    }
                    filesize = filestat.st_size;
                    filelastmod = filestat.st_mtime;

                    std::cout<<"Ricerca file andata a buon fine."<<std::endl;
                    send_result = send_file(s_connesso, filesize, filelastmod,
                                             fileptr);
                    if (send_result < 0){

                        send_err(s_connesso);
                        break;
                    }
                    std::cout<<"File mandato con successo."<<std::endl;
                    fclose(fileptr);
                    continue;

                }
                else if(strcmp(FINE, buffer) == 0){
                    break;
                }
                else{
                    send_err(s_connesso);
                    break;
                }
            }
                /* select() ritorna 0, timeout */
            else if (s == 0){
                std::cout<<"Nessuna risposta dopo" <<TIMEOUT_SECONDI<<"secondi, chiusura."<<std::endl;
                send_err(s_connesso);
                break;

            } /* fallimento select() */
            else{
                std::cout<<"Errore nella select()"<<std::endl;
                send_err(s_connesso);
                break;
            }
        }
    }
    return 0;
}

ssize_t ssend(int socket, const void *bufptr, size_t nbytes, int flags){
    ssize_t sent;
    if((sent = send(socket, bufptr, nbytes, flags)) != (ssize_t)nbytes)
        std::cout<<"Errore, send() fallita."<<std::endl;

        return sent;
}

ssize_t send_err(int sock){
    ssize_t s;
    s = ssend(sock, ERRORE, sizeof(ERRORE), MSG_NOSIGNAL);
    std::cout<<"Errore nella send"<<std::endl;
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
        if(read == 0){
            buffer[read_count] = comm;
            read_count++;
        }/* se read=0 niente da leggere ancora*/
        else {
            break;
        }
    }
    /* appendere per farlo diventare una stringa */
    buffer[read_count] = '\0';
    return read;
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
