#include "utils.h"

#include "db.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

namespace {


    sql::Driver * driver = nullptr; // get_driver_instance();


}


Db::Db(std::string const & host, std::string const & user, std::string const & password) {
    try {
        c_ = driver->connect(host, user, password);
    } catch (sql::SQLException const & e) {
        throw STR("SQL ERROR: code " << e.getErrorCode() << ", state: " << e.getSQLState());
    }
}

void Db::query(std::string const & query) {
    sql::Statement * s = nullptr;
    sql::ResultSet * r = nullptr;
    try {
        std::unique_ptr<sql::Statement> s(c_->createStatement());
        std::unique_ptr<sql::ResultSet> r(s->executeQuery(query));
    } catch (sql::SQLException const & e) {
        throw STR("SQL ERROR: code " << e.getErrorCode() << ", state: " << e.getSQLState());
    }
}




void Db::addProject(ClonedProject * project) {
    query(STR("INSERT INTO projects VALUES ()"));

}

void Db::addFile(TokenizedFile* file) {

}


