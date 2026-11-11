#include "RouteController.h"
#include "../models/Route.h"
#include "../utils/json.hpp"
#include "../utils/OccupancyManager.h"
#include <string>

using json = nlohmann::json;

// Helper: extract and validate session token from request header
static bool authenticate(SessionManager& sessions, const httplib::Request& req,
                         int& userId, std::string& role) {
    std::string token = req.get_header_value("Authorization");
    return sessions.validate(token, userId, role);
}

static void cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Content-Type", "application/json");
}

RouteController::RouteController(Database& db, SessionManager& sessions, httplib::Server& server)
    : db(db), sessions(sessions)
{
    // POST /api/route/create  (driver only)
    // Body: { "bus_number": "BUS-07", "route_name": "Campus → City",
    //         "stops": [{"name":"Gate A","lat":0.0,"lng":0.0,"order":1}, ...] }
    server.Post("/api/route/create", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role) || role != "driver") {
            res.status = 403;
            res.set_content(R"({"error":"Drivers only"})", "application/json");
            return;
        }

        json body;
        try { body = json::parse(req.body); }
        catch (...) { res.status = 400; res.set_content(R"({"error":"Bad JSON"})", "application/json"); return; }

        std::string busNumber  = body.value("bus_number",  "");
        std::string routeName  = body.value("route_name",  "");
        auto        stopsArray = body.value("stops", json::array());

        if (busNumber.empty() || stopsArray.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"bus_number and stops are required"})", "application/json");
            return;
        }

        // Upsert bus record
        std::string busSql = "INSERT OR IGNORE INTO buses (bus_number, driver_id) VALUES (?, ?);";
        sqlite3_stmt* bstmt = db.prepare(busSql);
        sqlite3_bind_text(bstmt, 1, busNumber.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(bstmt, 2, userId);
        sqlite3_step(bstmt); sqlite3_finalize(bstmt);

        // Get bus id
        int busId = 0;
        std::string getBus = "SELECT id FROM buses WHERE bus_number = ?;";
        sqlite3_stmt* gs = db.prepare(getBus);
        sqlite3_bind_text(gs, 1, busNumber.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(gs) == SQLITE_ROW) busId = sqlite3_column_int(gs, 0);
        sqlite3_finalize(gs);

        // Deactivate any existing active route for this driver
        db.execute("UPDATE routes SET is_active = 0 WHERE driver_id = " + std::to_string(userId) + ";");

        // Create new route
        Route r;
        r.driverId = userId;
        r.busId    = busId;
        r.name     = routeName;
        int routeId = Route::create(db, r);

        // Insert stops
        int orderIdx = 1;
        for (auto& s : stopsArray) {
            if (!s.is_object()) continue;
            Stop stop;
            stop.routeId = routeId;
            stop.name    = s.value("name",  "Stop");
            stop.lat     = s.value("lat",   0.0);
            stop.lng     = s.value("lng",   0.0);
            int ord = orderIdx;
            if (s.contains("stop_order") && s["stop_order"].is_number()) {
                ord = s["stop_order"].get<int>();
            } else if (s.contains("order") && s["order"].is_number()) {
                ord = s["order"].get<int>();
            }
            stop.order   = ord > 0 ? ord : orderIdx;
            orderIdx++;
            r.addStop(db, stop);
        }

        // Only save or update template if driver explicitly checked "Save as template"
        if (body.value("save_as_template", false)) {
            int tid = 0;
            std::string findTmpl = "SELECT id FROM route_templates WHERE name = ?;";
            sqlite3_stmt* ft = db.prepare(findTmpl);
            if (ft) {
                sqlite3_bind_text(ft, 1, routeName.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(ft) == SQLITE_ROW) tid = sqlite3_column_int(ft, 0);
                sqlite3_finalize(ft);
            }
            if (tid == 0) {
                std::string insTmpl = "INSERT INTO route_templates (name, bus_number) VALUES (?, ?);";
                sqlite3_stmt* it = db.prepare(insTmpl);
                if (it) {
                    sqlite3_bind_text(it, 1, routeName.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(it, 2, busNumber.c_str(), -1, SQLITE_STATIC);
                    sqlite3_step(it);
                    sqlite3_finalize(it);
                    tid = static_cast<int>(sqlite3_last_insert_rowid(db.getRaw()));
                }
            }
            if (tid > 0) {
                db.execute("DELETE FROM template_stops WHERE template_id = " + std::to_string(tid) + ";");
                std::string ssql = "INSERT INTO template_stops (template_id, stop_name, latitude, longitude, stop_order) VALUES (?, ?, ?, ?, ?);";
                int tOrderIdx = 1;
                for (auto& s : stopsArray) {
                    if (!s.is_object()) continue;
                    sqlite3_stmt* sstmt = db.prepare(ssql);
                    if (!sstmt) continue;
                    sqlite3_bind_int(sstmt, 1, tid);
                    std::string sname = s.value("name", "Stop");
                    sqlite3_bind_text(sstmt, 2, sname.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_double(sstmt, 3, s.value("lat", 0.0));
                    sqlite3_bind_double(sstmt, 4, s.value("lng", 0.0));
                    int ord = tOrderIdx;
                    if (s.contains("stop_order") && s["stop_order"].is_number()) {
                        ord = s["stop_order"].get<int>();
                    } else if (s.contains("order") && s["order"].is_number()) {
                        ord = s["order"].get<int>();
                    }
                    sqlite3_bind_int(sstmt, 5, ord > 0 ? ord : tOrderIdx);
                    tOrderIdx++;
                    sqlite3_step(sstmt);
                    sqlite3_finalize(sstmt);
                }
            }
        }

        json resp = {{"route_id", routeId}, {"message", "Route created"}};
        res.set_content(resp.dump(), "application/json");
    });

    // GET /api/route/active  (driver — see their own active route)
    server.Get("/api/route/active", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }

        Route r = Route::getActiveByDriver(db, userId);
        if (!r.isValid()) {
            res.set_content(R"({"active":false})", "application/json"); return;
        }

        auto stops = Route::getStops(db, r.id);
        json stopsJson = json::array();
        for (auto& s : stops)
            stopsJson.push_back({{"id",s.id},{"name",s.name},{"lat",s.lat},{"lng",s.lng},{"order",s.order}});

        std::string busNumber = "";
        std::string bsql = "SELECT bus_number FROM buses WHERE id = ?;";
        sqlite3_stmt* bstmt = db.prepare(bsql);
        if (bstmt) {
            sqlite3_bind_int(bstmt, 1, r.busId);
            if (sqlite3_step(bstmt) == SQLITE_ROW) {
                const char* bn = reinterpret_cast<const char*>(sqlite3_column_text(bstmt, 0));
                if (bn) busNumber = bn;
            }
            sqlite3_finalize(bstmt);
        }

        json resp = {
            {"active",     true},
            {"route_id",   r.id},
            {"route_name", r.name},
            {"bus_id",     r.busId},
            {"bus_number", busNumber.empty() ? ("BUS-" + std::to_string(r.busId)) : busNumber},
            {"stops",      stopsJson}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // PUT /api/route/deactivate  (driver — end the ride)
    server.Put("/api/route/deactivate", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role) || role != "driver") {
            res.status = 403; res.set_content(R"({"error":"Drivers only"})", "application/json"); return;
        }

        db.execute("UPDATE routes SET is_active = 0 WHERE driver_id = " + std::to_string(userId) + ";");
        res.set_content(R"({"message":"Ride ended"})", "application/json");
    });

    // GET /api/routes/all  (passenger — list all active routes)
    server.Get("/api/routes/all", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }

        auto routes = Route::getAllActive(db);
        json arr = json::array();
        for (auto& r : routes) {
            auto stops = Route::getStops(db, r.id);
            std::string busNumber = "";
            int busCapacity = 40;
            std::string bsql = "SELECT bus_number, capacity FROM buses WHERE id = ?;";
            sqlite3_stmt* bstmt = db.prepare(bsql);
            if (bstmt) {
                sqlite3_bind_int(bstmt, 1, r.busId);
                if (sqlite3_step(bstmt) == SQLITE_ROW) {
                    const char* bn = reinterpret_cast<const char*>(sqlite3_column_text(bstmt, 0));
                    if (bn) busNumber = bn;
                    int cap = sqlite3_column_int(bstmt, 1);
                    if (cap > 0) busCapacity = cap;
                }
                sqlite3_finalize(bstmt);
            }

            int occupied = 0, available = 0;
            double occupiedPct = 0.0, availablePct = 0.0;
            OccupancyManager::getInstance().getOccupancy(r.id, busCapacity, occupied, available, occupiedPct, availablePct);

            arr.push_back({
                {"route_id",          r.id},
                {"route_name",        r.name},
                {"driver_id",         r.driverId},
                {"bus_id",            r.busId},
                {"bus_number",        busNumber.empty() ? ("BUS-" + std::to_string(r.busId)) : busNumber},
                {"stop_count",        stops.size()},
                {"capacity",          busCapacity},
                {"occupied_seats",    occupied},
                {"available_seats",   available},
                {"occupancy_percent", occupiedPct},
                {"available_percent", availablePct}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // GET /api/route/:id/stops  (passenger — see stops of a specific route)
    server.Get(R"(/api/route/(\d+)/stops)", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }

        int routeId = std::stoi(req.matches[1]);
        auto stops  = Route::getStops(db, routeId);
        json arr    = json::array();
        for (auto& s : stops)
            arr.push_back({{"id",s.id},{"name",s.name},{"lat",s.lat},{"lng",s.lng},{"order",s.order}});
        res.set_content(arr.dump(), "application/json");
    });

    // POST /api/route/:id/heartbeat (passenger co-location heartbeat)
    server.Post(R"(/api/route/(\d+)/heartbeat)", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }
        int routeId = std::stoi(req.matches[1]);
        OccupancyManager::getInstance().recordHeartbeat(routeId, userId);
        res.set_content(R"({"status":"ok","message":"Heartbeat recorded"})", "application/json");
    });

    // POST /api/route/:id/leave (passenger disembarks / leaves bus)
    server.Post(R"(/api/route/(\d+)/leave)", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }
        int routeId = std::stoi(req.matches[1]);
        OccupancyManager::getInstance().removePassenger(routeId, userId);
        res.set_content(R"({"status":"ok","message":"Passenger disembarked"})", "application/json");
    });

    // GET /api/route/:id/occupancy
    server.Get(R"(/api/route/(\d+)/occupancy)", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }
        int routeId = std::stoi(req.matches[1]);
        int capacity = 40;
        std::string bsql = "SELECT b.capacity FROM routes r JOIN buses b ON r.bus_id = b.id WHERE r.id = ?;";
        sqlite3_stmt* bstmt = db.prepare(bsql);
        if (bstmt) {
            sqlite3_bind_int(bstmt, 1, routeId);
            if (sqlite3_step(bstmt) == SQLITE_ROW) {
                int cap = sqlite3_column_int(bstmt, 0);
                if (cap > 0) capacity = cap;
            }
            sqlite3_finalize(bstmt);
        }
        int occupied = 0, available = 0;
        double occupiedPct = 0.0, availablePct = 0.0;
        OccupancyManager::getInstance().getOccupancy(routeId, capacity, occupied, available, occupiedPct, availablePct);

        json resp = {
            {"route_id",          routeId},
            {"capacity",          capacity},
            {"occupied_seats",    occupied},
            {"available_seats",   available},
            {"occupancy_percent", occupiedPct},
            {"available_percent", availablePct}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // GET /api/templates (list saved route templates)
    server.Get("/api/templates", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }

        std::string sql =
            "SELECT t.id, t.name, t.bus_number, COUNT(s.id) "
            "FROM route_templates t "
            "LEFT JOIN template_stops s ON t.id = s.template_id "
            "GROUP BY t.id ORDER BY t.id;";
        sqlite3_stmt* stmt = db.prepare(sql);
        json arr = json::array();
        if (stmt) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int tid = sqlite3_column_int(stmt, 0);
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* bus  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                int count = sqlite3_column_int(stmt, 3);
                arr.push_back({
                    {"id", tid},
                    {"name", name ? name : ""},
                    {"bus_number", bus ? bus : ""},
                    {"stop_count", count}
                });
            }
            sqlite3_finalize(stmt);
        }
        res.set_content(arr.dump(), "application/json");
    });

    // GET /api/templates/:id (get stops for template)
    server.Get(R"(/api/templates/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role)) {
            res.status = 401; res.set_content(R"({"error":"Unauthorized"})", "application/json"); return;
        }

        int templateId = std::stoi(req.matches[1]);
        std::string sql =
            "SELECT id, stop_name, latitude, longitude, stop_order "
            "FROM template_stops WHERE template_id = ? ORDER BY stop_order;";
        sqlite3_stmt* stmt = db.prepare(sql);
        if (!stmt) { res.status = 500; return; }

        sqlite3_bind_int(stmt, 1, templateId);
        json arr = json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int sid = sqlite3_column_int(stmt, 0);
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            double lat = sqlite3_column_double(stmt, 2);
            double lng = sqlite3_column_double(stmt, 3);
            int order = sqlite3_column_int(stmt, 4);
            arr.push_back({{"id", sid}, {"name", name ? name : ""}, {"lat", lat}, {"lng", lng}, {"order", order}});
        }
        sqlite3_finalize(stmt);
        res.set_content(arr.dump(), "application/json");
    });

    // POST /api/templates (create new template)
    server.Post("/api/templates", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role) || role != "driver") {
            res.status = 403; res.set_content(R"({"error":"Drivers only"})", "application/json"); return;
        }

        json body;
        try { body = json::parse(req.body); } catch (...) {
            res.status = 400; res.set_content(R"({"error":"Bad JSON"})", "application/json"); return;
        }

        std::string name = body.value("name", "");
        std::string busNumber = body.value("bus_number", "BUS-01");
        auto stopsArray = body.value("stops", json::array());

        if (name.empty() || stopsArray.empty()) {
            res.status = 400; res.set_content(R"({"error":"name and stops required"})", "application/json"); return;
        }

        std::string tsql = "INSERT INTO route_templates (name, bus_number) VALUES (?, ?);";
        sqlite3_stmt* tstmt = db.prepare(tsql);
        if (!tstmt) { res.status = 500; return; }
        sqlite3_bind_text(tstmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(tstmt, 2, busNumber.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(tstmt);
        sqlite3_finalize(tstmt);

        int templateId = static_cast<int>(sqlite3_last_insert_rowid(db.getRaw()));

        std::string ssql = "INSERT INTO template_stops (template_id, stop_name, latitude, longitude, stop_order) VALUES (?, ?, ?, ?, ?);";
        for (auto& s : stopsArray) {
            sqlite3_stmt* sstmt = db.prepare(ssql);
            if (!sstmt) continue;
            sqlite3_bind_int(sstmt, 1, templateId);
            std::string sname = s.value("name", "Stop");
            sqlite3_bind_text(sstmt, 2, sname.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(sstmt, 3, s.value("lat", 0.0));
            sqlite3_bind_double(sstmt, 4, s.value("lng", 0.0));
            sqlite3_bind_int(sstmt, 5, s.value("order", 0));
            sqlite3_step(sstmt);
            sqlite3_finalize(sstmt);
        }

        json resp = {{"template_id", templateId}, {"message", "Template saved"}};
        res.set_content(resp.dump(), "application/json");
    });

    // DELETE /api/templates/:id (delete saved template)
    server.Delete(R"(/api/templates/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        int userId; std::string role;
        if (!authenticate(sessions, req, userId, role) || role != "driver") {
            res.status = 403; res.set_content(R"({"error":"Drivers only"})", "application/json"); return;
        }

        int templateId = std::stoi(req.matches[1]);
        db.execute("DELETE FROM template_stops WHERE template_id = " + std::to_string(templateId) + ";");
        db.execute("DELETE FROM route_templates WHERE id = " + std::to_string(templateId) + ";");

        res.set_content(R"({"message":"Template deleted"})", "application/json");
    });

    // CORS preflight for all route endpoints
    auto preflight = [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    };
    server.Options("/api/route/create",             preflight);
    server.Options("/api/route/active",             preflight);
    server.Options("/api/route/deactivate",         preflight);
    server.Options("/api/routes/all",               preflight);
    server.Options(R"(/api/route/(\d+)/stops)",     preflight);
    server.Options(R"(/api/route/(\d+)/heartbeat)", preflight);
    server.Options(R"(/api/route/(\d+)/leave)",     preflight);
    server.Options(R"(/api/route/(\d+)/occupancy)", preflight);
    server.Options("/api/templates",                preflight);
    server.Options(R"(/api/templates/(\d+))",       preflight);
}
