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

public:   //we make decision in future, public or private with a parameter
    Server()
    {
        AccessDatabase();
    }

private:
    static Server * pinstance_;
    static std::mutex mutex_;

protected:
    explicit Server(std::string  value): value_(std::move(value))
    {
        AccessDatabase();
    }
    ~Server() = default;
    std::string value_;

public:
    /**
     * Singletons should not be cloneable.
     */
    Server(Server &other) = delete;
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
   friend void ThreadFoo();
   friend void ThreadBar();

private:
    static Server *getInstance(const std::string& value);
    /**
     * Finally, any singleton should define some business logic, which can be
     * executed on its instance.
     */
     static void AccessDatabase()
    {
        // ...
       Database::connect();
    }
    [[nodiscard]] std::string value() const{
        return value_;
    }
};

void ThreadFoo();
void ThreadBar();

#endif //PDSPROJECTSERVER_SERVER_H
