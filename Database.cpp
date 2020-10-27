
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
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include "Message.h"
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
            stmt->execute("CREATE TABLE users(user CHAR(70) NOT NULL,password CHAR(128) NOT NULL, PRIMARY KEY (user))");
            delete stmt;
            /* connect */

            //delete con;

        } catch (sql::SQLException &e) {
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }

    }

}

bool Database::registerUser(std::string &user,std::string &pass)
{

    boost::system::error_code ec;
    std::string path_server("../server_user/");
    std::string path_user(path_server+user);
    boost::filesystem::path directory(path_user);
    if (boost::filesystem::exists(directory) && boost::filesystem::is_directory(directory))
    {
        //
        //boost::filesystem::directory_iterator begin(directory);
        //boost::filesystem::directory_iterator end;
        /* //stampa il contenuto
        while(begin != end)
        {
            std::cout << *begin << " "; //controllo checksum or hash
            ++begin;
        }
        std::cout << "\n";
         */
        std::cout<<"Folder Utente presente nel sistema"<<std::endl;
        return false;
    }
    else
    {
        /* '?' is the supported placeholder syntax */
        prep_stmt = con->prepareStatement("INSERT INTO users(user, password) VALUES (?, ?)");

        char hashed_password[crypto_pwhash_STRBYTES];

        if (crypto_pwhash_str
                    (hashed_password, pass.c_str(), pass.length(),
                     crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE) != 0) {
            /* out of memory */
        }


        prep_stmt->setString(1, user);
        prep_stmt->setString(2, hashed_password);
        prep_stmt->execute();
        std::cout << "Utente inserito nel database " << std::endl;
        delete prep_stmt;

        boost::filesystem::current_path(path_server);
        boost::filesystem::create_directories(path_user,ec);
        return true;
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

bool Database::checkUser(MsgType msg, std::vector<unsigned char>& vect_user) {

    size_t pos=0;
    std::string user;
    std::string delimiter = " ";
    std::string decrypt;
    std::string encrypted(vect_user.begin(),vect_user.end());
    decrypt=Database::decrypt(vect_user);
    //  while ((pos = str.find(delimiter)) != std::string::npos) {
    pos = decrypt.find(delimiter);
    user = decrypt.substr(0, pos);
    decrypt.erase(0, pos + delimiter.length());
    std::string pass = decrypt.substr(0, decrypt.length());
    //  }
    std::cout << "username: " << user << " password: " << pass << std::endl;

     switch(msg)
     {
         case MsgType::REGISTER:
         {
             return Database::registerUser(user,pass);
         }
         case MsgType::LOGIN:
         {
             return Database::searchUser(user, pass);
         }
         default:
          return false;
     }
}

 std::string Database::decrypt(std::vector<unsigned char>& encrypted)
{
     size_t dim = encrypted.size()-crypto_secretbox_MACBYTES;
     unsigned char decrypted[dim];
    if (crypto_secretbox_open_easy(decrypted, reinterpret_cast<const unsigned char *>(encrypted.data()), encrypted.size(), nonce, key) != 0) {
        /* message forged! */
        std::cout<<"Errore decrypt. INSIDE DECRYPT FUNCTION, dim = " << dim<< std::endl;
        return "fallimento";
    }
    else
    {
        std::string result(reinterpret_cast<const char *>(decrypted),dim);
        return result;
    }
}

void Database::setNonce( unsigned char* data_nonce)
{
     for(int i =0; i<crypto_secretbox_NONCEBYTES; i++)
         nonce[i]=data_nonce[i];
}