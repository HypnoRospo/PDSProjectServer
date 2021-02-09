#ifndef PDSPROJECTSERVER_DATABASE_H
#define PDSPROJECTSERVER_DATABASE_H


#include <mutex>
#include <vector>
#include "Message.h"
class Database {

private:
    static Database* databaseptr_;
    static std::mutex mutexdb_;
     static void connect();

public:
    /**
 * Singletons should not be cloneable.
 */
    Database(Database &other) = delete;
    /**
     * Singletons should not be assignable.
     */
    void operator=(const Database &) = delete;
     Database() = default;



    static Database* create_instance();
    static bool searchUser(std::string &user,std::string &pass);
    static std::pair<std::string,bool> checkUser(MsgType msg, std::vector<unsigned char>& vect_user);
    static bool registerUser(std::string &user,std::string &pass);
    static void setNonce( unsigned char* data_nonce);
    static std::string decrypt( std::vector<unsigned char>& encrypted);


protected:
    ~Database() = default;

};

#endif //PDSPROJECTSERVER_DATABASE_H
