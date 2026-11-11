#include "User.h"
#include "../../sqlite3.h"
#include <functional>
#include <sstream>

// Simple hash: in real project use bcrypt or SHA-256 library
static std::string hashPassword(const std::string& plain) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(plain));
}

User User::findByCollegeId(Database& db, const std::string& cid) {
    User u;
    std::string sql = "SELECT id, college_id, password_hash, role, name FROM users WHERE college_id = ?;";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return u;

    sqlite3_bind_text(stmt, 1, cid.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u.id           = sqlite3_column_int(stmt, 0);
        u.collegeId    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u.role         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u.name         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    }
    sqlite3_finalize(stmt);
    return u;
}

bool User::create(Database& db, const User& u) {
    std::string hashed = hashPassword(u.passwordHash);
    std::string sql =
        "INSERT INTO users (college_id, password_hash, role, name) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, u.collegeId.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hashed.c_str(),          -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, u.role.c_str(),          -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, u.name.c_str(),          -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool User::verifyPassword(const std::string& plain) const {
    return passwordHash == std::to_string(std::hash<std::string>{}(plain));
}
