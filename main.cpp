#include <iostream>
#include<thread>
#include <openssl/sha.h>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include "Server.h"

int main() {
    std::cout << "Server Program" << std::endl;
    std::cout << std::endl;

    std::cout <<"If you see the same value, then singleton was reused (yay!\n" <<
              "If you see different values, then 2 singletons were created (booo!!)\n" <<
              "Impossible with mutex locking.\n\n" <<
              "RESULT:\n";
    std::thread t1(ThreadBar);
    std::thread t2(ThreadFoo);
    t1.join();
    t2.join();

    return 0;
}
