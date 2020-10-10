//
// Created by enrico_scalabrino on 10/10/20.
//

#ifndef PDSPROJECTSERVER_SERVER_H
#define PDSPROJECTSERVER_SERVER_H

#include <iostream>
#include <string>
#include <mutex>

/**
 * The Singleton class defines the `GetInstance` method that serves as an
 * alternative to constructor and lets clients access the same instance of this
 * class over and over.
 */
class Server
{

    /**
     * The Singleton's constructor/destructor should always be private to
     * prevent direct construction/desctruction calls with the `new`/`delete`
     * operator.
     */
private:
    static Server * pinstance_;
    static std::mutex mutex_;

protected:
    Server(const std::string value): value_(value)
    {
    }
    ~Server() {}
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

    static Server *getInstance(const std::string& value);
    /**
     * Finally, any singleton should define some business logic, which can be
     * executed on its instance.
     */
    void SomeLogicNeeded()
    {
        // ...
    }

    std::string value() const{
        return value_;
    }
};

void ThreadFoo();
void ThreadBar();

#endif //PDSPROJECTSERVER_SERVER_H
