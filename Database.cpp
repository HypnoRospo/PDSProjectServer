
//
// Created by enrico_scalabrino on 10/10/20.
//


//#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include "Database.h"

using namespace sql;
Database* Database::databaseptr_{nullptr};
std::mutex Database::mutexdb_;


 sql::Driver *driver;
 sql::Connection *con;
 sql::Statement *stmt;
 sql::ResultSet *res;
 sql::PreparedStatement *prep_stmt;

 Database* Database::create_instance()
{
    std::lock_guard<std::mutex> lock(mutexdb_);
    if (databaseptr_ == nullptr)
    {
        databaseptr_ = new Database();
        databaseptr_->connect();
    }
    else
    {
        std::cout << "Creazione del database instance gia' avvenuta, database creato in precedenza"<<std::endl;
    }
    return databaseptr_;
}

void Database::connect() {
    {
        try {
            /* Create a connection */
            driver = get_driver_instance();
            con = driver->connect("tcp://127.0.0.1:3306", "root", "");
            /* Connect to the MySQL test database */

            stmt = con->createStatement();
            stmt->execute("CREATE SCHEMA IF NOT EXISTS project_database");

            con->setSchema("project_database");

            stmt = con->createStatement();
            stmt->execute("DROP TABLE IF EXISTS users");
            stmt->execute("CREATE TABLE users(user CHAR(70) NOT NULL,password CHAR(70) NOT NULL,path CHAR(70) NOT NULL, PRIMARY KEY (user))");
            delete stmt;

            /* '?' is the supported placeholder syntax */
            prep_stmt = con->prepareStatement("INSERT INTO users(user, password,path) VALUES (?, ?, ?)");

            prep_stmt->setString(1, "user1");
            prep_stmt->setString(2, "password1");
            prep_stmt->setString(3, "path1");
            prep_stmt->execute();

            prep_stmt->setString(1, "utente2");
            prep_stmt->setString(2, "password2");
            prep_stmt->setString(3, "path2");
            prep_stmt->execute();

            //std::cout << "Utenti inseriti " << std::endl;
            delete prep_stmt;
            //delete con;

        } catch (sql::SQLException &e) {
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }

    }

}
bool Database::searchUser(std::string &user,std::string &pass){
    try {
        // sql::ResultSet* res = stmt->executeQuery("SELECT * FROM users"); //Not useful just for check that connection works
        prep_stmt = con->prepareStatement("SELECT user,password from users where ( user = ? and password = ? )");
        prep_stmt->setString(1, user);
        prep_stmt->setString(2, pass);
        res =  prep_stmt->executeQuery();
        // The prep_stmt above must return only 1 tuple because user is unique and then N<=1 and password check that N>0 => N = 1
        //Verifico che esiste una tupla ( che è unica ) e restituisco true.
        if(res->next())
            return true;
        else return false;
    }catch (sql::SQLException &e) {
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}