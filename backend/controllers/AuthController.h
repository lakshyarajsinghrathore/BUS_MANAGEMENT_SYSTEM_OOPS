#pragma once
#include "../utils/httplib.h"
#include "../utils/SessionManager.h"
#include "../db/Database.h"

class AuthController {
    Database& db;
    SessionManager& sessions;
public:
    AuthController(Database& db, SessionManager& sessions, httplib::Server& server);
};
