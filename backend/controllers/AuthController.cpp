#include "AuthController.h"
#include "../models/User.h"
#include "../utils/json.hpp"

using json = nlohmann::json;

AuthController::AuthController(Database& db, SessionManager& sessions, httplib::Server& server)
    : db(db), sessions(sessions)
{
    // POST /api/auth/login
    // Body: { "college_id": "CS001", "password": "secret" }
    server.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");

        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
            return;
        }

        std::string cid      = body.value("college_id", "");
        std::string password = body.value("password", "");

        if (cid.empty() || password.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"college_id and password required"})", "application/json");
            return;
        }

        User user = User::findByCollegeId(db, cid);
        if (!user.isValid() || !user.verifyPassword(password)) {
            res.status = 401;
            res.set_content(R"({"error":"Invalid credentials"})", "application/json");
            return;
        }

        std::string token = sessions.createSession(user.id, user.role);
        json resp = {
            {"token", token},
            {"role",  user.role},
            {"name",  user.name},
            {"id",    user.id}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // POST /api/auth/register
    // Body: { "college_id", "password", "name", "role" }
    server.Post("/api/auth/register", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");

        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
            return;
        }

        std::string cid      = body.value("college_id", "");
        std::string password = body.value("password",   "");
        std::string name     = body.value("name",       "");
        std::string role     = body.value("role",       "passenger");

        if (cid.empty() || password.empty() || name.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"college_id, password and name are required"})", "application/json");
            return;
        }
        if (role != "passenger" && role != "driver") {
            res.status = 400;
            res.set_content(R"({"error":"role must be passenger or driver"})", "application/json");
            return;
        }

        // Reject if college_id already taken
        User existing = User::findByCollegeId(db, cid);
        if (existing.isValid()) {
            res.status = 409;
            res.set_content(R"({"error":"College ID already registered"})", "application/json");
            return;
        }

        User u;
        u.collegeId    = cid;
        u.passwordHash = password; // create() hashes it
        u.name         = name;
        u.role         = role;

        if (!User::create(db, u)) {
            res.status = 500;
            res.set_content(R"({"error":"Registration failed"})", "application/json");
            return;
        }

        // Auto-login after registration
        User created = User::findByCollegeId(db, cid);
        std::string token = sessions.createSession(created.id, created.role);
        json resp = {
            {"token", token},
            {"role",  created.role},
            {"name",  created.name},
            {"id",    created.id}
        };
        res.set_content(resp.dump(), "application/json");
    });

    server.Options("/api/auth/register", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });

    // POST /api/auth/logout
    server.Post("/api/auth/logout", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string token = req.get_header_value("Authorization");
        sessions.invalidate(token);
        res.set_content(R"({"message":"Logged out"})", "application/json");
    });

    // Handle CORS preflight for login
    server.Options("/api/auth/login",  [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });
    server.Options("/api/auth/logout", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });
}
