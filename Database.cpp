#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include "Database.h"
#include <sodium.h>
#include <sodium/crypto_pwhash.h>
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

 void Database::connect()
    {
        int count = 0;
        int maxTries = 3;

        while(true)
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
                //stmt->execute("DROP TABLE IF EXISTS users");
                stmt->execute("CREATE TABLE IF NOT EXISTS users(user CHAR(70) NOT NULL,password CHAR(128) NOT NULL, PRIMARY KEY (user))");
                delete stmt;
                /* connect */

                //delete con;
                break;

            } catch (sql::SQLException &e) {
                std::cout<<" Tentativo numero "<<count<<" di connessione al database..."<<std::endl;
                if (++count == maxTries)
                {
                    std::cout << " (MySQL error code: " << e.getErrorCode();
                    std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
                    throw e;

                }
                sleep(2);
            }
        }

    }

bool Database::registerUser(std::string &user,std::string &pass)
{
    int count = 0;
    int maxTries = 3;

    while(true)
    {
        try {
            //controlli sanificazione utente e password //todo
            prep_stmt = con->prepareStatement("SELECT * from users where user = ? ");
            prep_stmt->setString(1, user);
            res = prep_stmt->executeQuery();
            // The prep_stmt above must return only 1 tuple because user is unique and then N<=1 and password check that N>0 => N = 1
            //Verifico che esiste una tupla ( che è unica ) e restituisco true.
            if (res->next()) {
                //mandare messaggio di errore dato che il nickname e' gia' stato utilizzato -> con return false basta
                return false;
            } else {

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
                //delete prep_stmt;

                boost::system::error_code ec;
                boost::filesystem::create_directories(user, ec);
                return true;
            }
        }
        catch (sql::SQLException &e) {
            std::cout << " Tentativo numero " << count << " di registrazione utente al database..." << std::endl;
            Database::connect();
            if (++count == maxTries) {
                std::cout << " (MySQL error code: " << e.getErrorCode();
                std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
                throw e;

            }
            sleep(2);
        }
        }
}

bool Database::searchUser(std::string &user,std::string &pass){

    int count = 0;
    int maxTries = 3;

    while(true) {
        try {
            // sql::ResultSet* res = stmt->executeQuery("SELECT * FROM users"); //Not useful just for check that connection works
            prep_stmt = con->prepareStatement("SELECT * from users where user = ? ");
            prep_stmt->setString(1, user);
            res = prep_stmt->executeQuery();
            // The prep_stmt above must return only 1 tuple because user is unique and then N<=1 and password check that N>0 => N = 1
            //Verifico che esiste una tupla ( che è unica ) e restituisco true.
            if (res->next()) {
                std::string hashed_password = res->getString("password");

                if (crypto_pwhash_str_verify
                            (hashed_password.c_str(), pass.c_str(), pass.length()) != 0) {
                    /* wrong password */
                    return false;
                }
                return true;
            } else return false;
        }
        catch (sql::SQLException &e) {
            std::cout << " Tentativo numero " << count << " di login utente al database..." << std::endl;
            Database::connect();
            if (++count == maxTries) {
                std::cout << " (MySQL error code: " << e.getErrorCode();
                std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
                throw e;

            }
            sleep(2);
        }
    }
}

std::pair<std::string,bool> Database::checkUser(MsgType msg, std::vector<unsigned char>& vect_user) {

    size_t pos=0;
    std::string user;
    std::string delimiter = " ";
    std::string decrypt;
    //std::string encrypted(vect_user.begin(),vect_user.end());
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
             try {
                 return std::pair<std::string,bool>(user,Database::registerUser(user,pass));
             }
             catch(sql::SQLException &e)
             {
                 return std::pair<std::string,bool>(user,false);
             }
         }
         case MsgType::LOGIN:
         {
             try {
                 return std::pair<std::string,bool>(user,Database::searchUser(user,pass));
             }
             catch(sql::SQLException &e)
             {
                 return std::pair<std::string,bool>(user,false);
             }
         }
         default:
         return std::pair<std::string,bool>(user,false);
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