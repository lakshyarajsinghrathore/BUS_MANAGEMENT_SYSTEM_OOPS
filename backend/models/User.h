#pragma once
#include <string>
#include "../db/Database.h"

class User {
public:
    int id;
    std::string collegeId;
    std::string passwordHash;
    std::string role;
    std::string name;

    User() : id(0) {}

    static User findByCollegeId(Database& db, const std::string& cid);
    static bool create(Database& db, const User& u);
    bool verifyPassword(const std::string& plain) const;

    bool isValid() const { return id > 0; }
};
