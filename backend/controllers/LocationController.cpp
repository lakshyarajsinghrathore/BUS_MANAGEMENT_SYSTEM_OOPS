#include "LocationController.h"
#include "../utils/json.hpp"
#include "../utils/OccupancyManager.h"
#include <string>

using json = nlohmann::json;

static bool authLoc(SessionManager& sessions, const httplib::Request& req,
                    int& userId, std::string& role) {
    std::string token = req.get_header_value("Authorization");
    return sessions.validate(token, userId, role);
}

static void corsLoc(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Content-Type", "application/json");
}

LocationController::LocationController(Database& db, SessionManager& sessions, httplib::Server& server)
    : db(db), sessions(sessions)
{
    // POST /api/location/update  (driver — push current GPS position)
    // Body: { "lat": 28.6139, "lng": 77.2090 }
    server.Post("/api/location/update", [&](const httplib::Request& req, httplib::Response& res) {
        corsLoc(res);
        int userId; std::string role;
        if (!authLoc(sessions, req, userId, role) || role != "driver") {
            res.status = 403;
            res.set_content(R"({"error":"Drivers only"})", "application/json");
            return;
        }

        json body;
        try { body = json::parse(req.body); }
        catch (...) { res.status = 400; res.set_content(R"({"error":"Bad JSON"})", "application/json"); return; }

        double lat = body.value("lat", 0.0);
        double lng = body.value("lng", 0.0);

        // Upsert: update if exists, insert if first time
        std::string sql =
            "INSERT INTO live_location (driver_id, latitude, longitude, updated_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP) "
            "ON CONFLICT(driver_id) DO UPDATE SET "
            "latitude = excluded.latitude, longitude = excluded.longitude, updated_at = CURRENT_TIMESTAMP;";

        sqlite3_stmt* stmt = db.prepare(sql);
        sqlite3_bind_int(stmt,    1, userId);
        sqlite3_bind_double(stmt, 2, lat);
        sqlite3_bind_double(stmt, 3, lng);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        res.set_content(R"({"message":"Location updated"})", "application/json");
    });

    // GET /api/location/:driverId  (passenger — get a driver's current GPS position)
    server.Get(R"(/api/location/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        corsLoc(res);
        int userId; std::string role;
        if (!authLoc(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }

        int driverId = std::stoi(req.matches[1]);
        std::string sql = "SELECT latitude, longitude, updated_at FROM live_location WHERE driver_id = ?;";
        sqlite3_stmt* stmt = db.prepare(sql);
        sqlite3_bind_int(stmt, 1, driverId);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            double lat = sqlite3_column_double(stmt, 0);
            double lng = sqlite3_column_double(stmt, 1);
            const char* ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            json resp = {{"lat", lat}, {"lng", lng}, {"updated_at", ts ? ts : ""}};
            sqlite3_finalize(stmt);

            // Fetch active route and live occupancy for this driver
            std::string rsql =
                "SELECT r.id, b.capacity FROM routes r "
                "JOIN buses b ON r.bus_id = b.id "
                "WHERE r.driver_id = ? AND r.is_active = 1 "
                "ORDER BY r.id DESC LIMIT 1;";
            sqlite3_stmt* rstmt = db.prepare(rsql);
            if (rstmt) {
                sqlite3_bind_int(rstmt, 1, driverId);
                if (sqlite3_step(rstmt) == SQLITE_ROW) {
                    int rId = sqlite3_column_int(rstmt, 0);
                    int cap = sqlite3_column_int(rstmt, 1);
                    if (cap <= 0) cap = 40;
                    int occupied = 0, available = 0;
                    double occupiedPct = 0.0, availablePct = 0.0;
                    OccupancyManager::getInstance().getOccupancy(rId, cap, occupied, available, occupiedPct, availablePct);
                    resp["occupancy"] = {
                        {"route_id",          rId},
                        {"capacity",          cap},
                        {"occupied_seats",    occupied},
                        {"available_seats",   available},
                        {"occupancy_percent", occupiedPct},
                        {"available_percent", availablePct}
                    };
                }
                sqlite3_finalize(rstmt);
            }
            res.set_content(resp.dump(), "application/json");
        } else {
            sqlite3_finalize(stmt);
            res.status = 404;
            res.set_content(R"({"error":"Driver location not found"})", "application/json");
        }
    });

    // CORS preflight
    auto preflight = [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    };
    server.Options("/api/location/update",      preflight);
    server.Options(R"(/api/location/(\d+))", preflight);
}
