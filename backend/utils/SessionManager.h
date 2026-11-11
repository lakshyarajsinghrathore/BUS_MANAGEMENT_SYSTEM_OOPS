#pragma once
#include <string>
#include "../db/Database.h"

class SessionManager {
private:
    Database& db;
    std::string generateToken(int userId);

public:
    SessionManager(Database& db) : db(db) {}

    std::string createSession(int userId, const std::string& role);
    bool validate(const std::string& token, int& userId, std::string& role);
    void invalidate(const std::string& token);
};
