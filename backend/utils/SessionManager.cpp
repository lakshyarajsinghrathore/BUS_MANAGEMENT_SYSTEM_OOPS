#include "SessionManager.h"
#include <functional>
#include <sstream>
#include <chrono>

std::string SessionManager::generateToken(int userId) {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::hash<std::string> hasher;
    std::string raw = std::to_string(userId) + "_" + std::to_string(now);
    return std::to_string(hasher(raw));
}

std::string SessionManager::createSession(int userId, const std::string& role) {
    std::string token = generateToken(userId);

    std::string sql = "INSERT INTO sessions (token, user_id, role, expires_at) VALUES (?, ?, ?, datetime('now', '+24 hours'));";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (stmt) {
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, userId);
        sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return token;
}

bool SessionManager::validate(const std::string& token, int& userId, std::string& role) {
    bool valid = false;
    std::string sql = "SELECT user_id, role FROM sessions WHERE token = ? AND expires_at > datetime('now');";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (stmt) {
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            userId = sqlite3_column_int(stmt, 0);
            const char* r = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            role = r ? r : "";
            valid = true;
        }
        sqlite3_finalize(stmt);
    }
    return valid;
}

void SessionManager::invalidate(const std::string& token) {
    std::string sql = "DELETE FROM sessions WHERE token = ?;";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (stmt) {
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
