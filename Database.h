//
// Created by enrico_scalabrino on 10/10/20.
//

#ifndef PDSPROJECTSERVER_DATABASE_H
#define PDSPROJECTSERVER_DATABASE_H


#include <mutex>

class Database {

private:
    static Database* databaseptr_;
    static std::mutex mutexdb_;
    void connect();

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



    Database* create_instance();
    static bool searchUser(std::string &user,std::string &pass);



protected:
    ~Database() = default;

};

#endif //PDSPROJECTSERVER_DATABASE_H
