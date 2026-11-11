#pragma once
#include "../utils/httplib.h"
#include "../utils/SessionManager.h"
#include "../db/Database.h"

class LocationController {
    Database& db;
    SessionManager& sessions;
public:
    LocationController(Database& db, SessionManager& sessions, httplib::Server& server);
};
