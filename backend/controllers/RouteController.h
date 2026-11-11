#pragma once
#include "../utils/httplib.h"
#include "../utils/SessionManager.h"
#include "../db/Database.h"

class RouteController {
    Database& db;
    SessionManager& sessions;
public:
    RouteController(Database& db, SessionManager& sessions, httplib::Server& server);
};
