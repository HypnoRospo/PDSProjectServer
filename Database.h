//
// Created by enrico_scalabrino on 10/10/20.
//

#ifndef PDSPROJECTSERVER_DATABASE_H
#define PDSPROJECTSERVER_DATABASE_H


class Database {

    /**
 * Singletons should not be cloneable.
 */
    Database(Database &other) = delete;
    /**
     * Singletons should not be assignable.
     */
    void operator=(const Database &) = delete;

protected:
    ~Database() = default;
     Database() = default;

public:
   static void connect();

};


#endif //PDSPROJECTSERVER_DATABASE_H
