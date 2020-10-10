//
// Created by enrico_scalabrino on 10/10/20.
//
#include "Server.h"
#include <iostream>
#include <string>
#include <mutex>
#include <thread>

/**
 * Static methods should be defined outside the class.
 */

Server* Server::pinstance_{nullptr};
std::mutex Server::mutex_;

/**
 * The first time we call GetInstance we will lock the storage location
 *      and then we make sure again that the variable is null and then we
 *      set the value. RU:
 */
 Server *Server::getInstance(const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (pinstance_ == nullptr)
    {
        pinstance_ = new Server(value);
    }
    else
    {
        std::cout << "Accesso al database gia' avvenuto, utenti non inseriti nuovamente" << std::endl;
    }
    return pinstance_;
}

void ThreadFoo(){
    // Following code emulates slow initialization.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Server* singleton = Server::getInstance("FOO");
    std::cout << singleton->value() << "\n";
}

void ThreadBar(){
    // Following code emulates slow initialization.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Server* singleton = Server::getInstance("BAR");
    std::cout << singleton->value() << "\n";
}