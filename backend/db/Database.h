#pragma once
#include <string>
#include "../../sqlite3.h"

class Database {
private:
    sqlite3* db;

public:
    Database(const std::string& path);
    ~Database();

    bool execute(const std::string& sql);
    sqlite3_stmt* prepare(const std::string& sql);
    sqlite3* getRaw();
    void loadSchema(const std::string& schemaPath);
};
