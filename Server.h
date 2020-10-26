//
// Created by enrico_scalabrino on 10/10/20.
//

#ifndef PDSPROJECTSERVER_SERVER_H
#define PDSPROJECTSERVER_SERVER_H

#include <iostream>
#include <string>
#include <mutex>
#include <utility>
#include "Database.h"

/**
 * The Singleton class defines the `GetInstance` method that serves as an
 * alternative to constructor and lets clients access the same instance of this
 * class over and over.
 */
class Server
{

    /**
     * The Singleton's constructor/destructor should always be private to
     * prevent direct construction/destruction calls with the `new`/`delete`
     * operator.
     */
private:
    static Server * pinstance_;
    static std::mutex mutex_;
    std::string server_path;

protected:
    explicit Server(int  port): port_(port) {}
    int port_;

public:
    /**
     * Singletons should not be cloneable.
     */
    Server(Server &other) = delete;
    ~Server() = default;

    /**
     * Singletons should not be assignable.
     */
    void operator=(const Server &) = delete;
    /**
     * This is the static method that controls the access to the singleton
     * instance. On the first run, it creates a singleton object and places it
     * into the static field. On subsequent runs, it returns the client existing
     * object stored in the static field.
     */
    static Server *start( int port);
    void work();
    /**
     * Finally, any singleton should define some business logic, which can be
     * executed on its instance.
     */

     //access database for example
     //...


    int getPort() const{
        return port_;
    }
};

#endif //PDSPROJECTSERVER_SERVER_H
