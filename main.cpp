#include "Server.h"
#include <memory>
int main() {
    std::unique_ptr<Server> server (Server::start(5000));
    server->work();
    return 0;
}
