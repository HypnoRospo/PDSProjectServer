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

void Database::connect() {
    {
        try {
            sql::Driver *driver;
            sql::Connection *con;
            sql::Statement *stmt;
            //sql::ResultSet *res;
            sql::PreparedStatement *prep_stmt;

            /* Create a connection */
            driver = get_driver_instance();
            con = driver->connect("tcp://127.0.0.1:3306", "root", "");
            /* Connect to the MySQL test database */

            stmt = con->createStatement();
            stmt->execute("CREATE SCHEMA IF NOT EXISTS project_database");

            con->setSchema("project_database");

            stmt = con->createStatement();
            stmt->execute("DROP TABLE IF EXISTS users");
            stmt->execute("CREATE TABLE users(user CHAR(70),password CHAR(70))");
            delete stmt;

            /* '?' is the supported placeholder syntax */
            prep_stmt = con->prepareStatement("INSERT INTO users(user, password) VALUES (?, ?)");

            prep_stmt->setString(1, "utente1");
            prep_stmt->setString(2, "password1");
            prep_stmt->execute();

            prep_stmt->setString(1, "utente2");
            prep_stmt->setString(2, "password2");
            prep_stmt->execute();

            std::cout << "Utenti inseriti " << std::endl;
            delete prep_stmt;
            delete con;

        } catch (sql::SQLException &e) {
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }

    }
}

