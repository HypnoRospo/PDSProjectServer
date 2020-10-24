
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
#include <sodium.h>
#include <sodium/crypto_pwhash.h>

using namespace sql;
Database* Database::databaseptr_{nullptr};
std::mutex Database::mutexdb_;
unsigned char key[crypto_secretbox_KEYBYTES] ={"pds_project_key"};
unsigned char nonce[crypto_secretbox_NONCEBYTES]={};

 sql::Driver *driver;
 sql::Connection *con;
 sql::Statement *stmt;
 sql::ResultSet *res;
 sql::PreparedStatement *prep_stmt;
 std::map<std::string,std::string> map;
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
            stmt->execute("CREATE TABLE users(user CHAR(70) NOT NULL,password CHAR(128) NOT NULL,path CHAR(70) NOT NULL, PRIMARY KEY (user))");
            delete stmt;

            /* '?' is the supported placeholder syntax */
            prep_stmt = con->prepareStatement("INSERT INTO users(user, password,path) VALUES (?, ?, ?)");

            char hashed_password[crypto_pwhash_STRBYTES];
            std::string pass = "password1";

            if (crypto_pwhash_str
                        (hashed_password, pass.c_str(), pass.length(),
                         crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE) != 0) {
                /* out of memory */
            }

            prep_stmt->setString(1, "user1");
            prep_stmt->setString(2, hashed_password);
            prep_stmt->setString(3, "path1");
            prep_stmt->execute();

            std::cout << "Utenti inseriti " << std::endl;
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
            prep_stmt = con->prepareStatement("SELECT * from users where user = ? ");
            prep_stmt->setString( 1,user);
            res =  prep_stmt->executeQuery();
            // The prep_stmt above must return only 1 tuple because user is unique and then N<=1 and password check that N>0 => N = 1
            //Verifico che esiste una tupla ( che è unica ) e restituisco true.
            if(res->next()) {
                std::string hashed_password = res->getString("password");

                if (crypto_pwhash_str_verify
                            (hashed_password.c_str(), pass.c_str(), pass.length()) != 0) {
                    /* wrong password */
                    return false;
                }
                //allora esiste, prendo il path ( se non e' presente )
                std::string path= res->getString("path");
                auto it= map.find(user);
                if(it == map.end()) // non esiste
                    map[user]=path; //oppure insert ({..})
                return true;
            }
            else return false;
        }
        catch (sql::SQLException &e) {
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}

bool Database::checkUser(std::vector<char> &vect_user) {
    size_t pos = 0;
    std::string user;
    std::string delimiter = " ";
    std::string decrypt;
    decrypt=Database::decrypt(vect_user);
    //  while ((pos = str.find(delimiter)) != std::string::npos) {
    pos = decrypt.find(delimiter);
    user = decrypt.substr(0, pos);
    decrypt.erase(0, pos + delimiter.length());
    std::string pass = decrypt.substr(0, decrypt.length());
    //  }
    std::cout << "username: " << user << " password: " << pass << std::endl;
    return Database::searchUser(user, pass);
}

 std::string Database::decrypt(std::vector<char>& vect)
{
    unsigned char decrypted[vect.size()];
    if (crypto_secretbox_open_easy(decrypted, (const unsigned char *) vect.data(), vect.size(), nonce, key) != 0) {
        /* message forged! */
        std::cout<<"Errore decrypt." <<std::endl;
    }
    return std::string(reinterpret_cast<const char *>(decrypted));
}

void Database::setNonce(const char* data_nonce)
{
     for(int i =0; i<crypto_secretbox_NONCEBYTES ; i++)
         nonce[i]=data_nonce[i];
}