#include <iostream>
#include "Server.h"

int main() {

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
     Server::start(5000); //gli passeremo la porta tramite linea di comando
     //Server::start(5002); error message

    return 0;
}
