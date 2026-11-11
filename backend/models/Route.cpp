#include "Route.h"
#include "../../sqlite3.h"

Route Route::getActiveByDriver(Database& db, int driverId) {
    Route r;
    std::string sql =
        "SELECT id, driver_id, bus_id, name, is_active FROM routes "
        "WHERE driver_id = ? AND is_active = 1 ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return r;

    sqlite3_bind_int(stmt, 1, driverId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r.id       = sqlite3_column_int(stmt, 0);
        r.driverId = sqlite3_column_int(stmt, 1);
        r.busId    = sqlite3_column_int(stmt, 2);
        const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.name     = n ? n : "";
        r.isActive = sqlite3_column_int(stmt, 4);
    }
    sqlite3_finalize(stmt);
    return r;
}

int Route::create(Database& db, const Route& r) {
    std::string sql =
        "INSERT INTO routes (driver_id, bus_id, name, is_active) VALUES (?, ?, ?, 1);";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return -1;

    sqlite3_bind_int(stmt, 1, r.driverId);
    sqlite3_bind_int(stmt, 2, r.busId);
    sqlite3_bind_text(stmt, 3, r.name.c_str(), -1, SQLITE_STATIC);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db.getRaw()));
}

bool Route::deactivate(Database& db, int routeId) {
    std::string sql = "UPDATE routes SET is_active = 0 WHERE id = ?;";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_int(stmt, 1, routeId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

void Route::addStop(Database& db, Stop& s) {
    std::string sql =
        "INSERT INTO stops (route_id, stop_name, latitude, longitude, stop_order) "
        "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return;

    sqlite3_bind_int(stmt, 1, s.routeId);
    sqlite3_bind_text(stmt, 2, s.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, s.lat);
    sqlite3_bind_double(stmt, 4, s.lng);
    sqlite3_bind_int(stmt, 5, s.order);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<Route> Route::getAllActive(Database& db) {
    std::vector<Route> routes;
    std::string sql =
        "SELECT id, driver_id, bus_id, name FROM routes WHERE is_active = 1;";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return routes;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Route r;
        r.id       = sqlite3_column_int(stmt, 0);
        r.driverId = sqlite3_column_int(stmt, 1);
        r.busId    = sqlite3_column_int(stmt, 2);
        const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.name     = n ? n : "";
        r.isActive = true;
        routes.push_back(r);
    }
    sqlite3_finalize(stmt);
    return routes;
}

std::vector<Stop> Route::getStops(Database& db, int routeId) {
    std::vector<Stop> stops;
    std::string sql =
        "SELECT id, route_id, stop_name, latitude, longitude, stop_order "
        "FROM stops WHERE route_id = ? ORDER BY stop_order, id;";
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return stops;

    sqlite3_bind_int(stmt, 1, routeId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Stop s;
        s.id      = sqlite3_column_int(stmt, 0);
        s.routeId = sqlite3_column_int(stmt, 1);
        const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        s.name    = n ? n : "";
        s.lat     = sqlite3_column_double(stmt, 3);
        s.lng     = sqlite3_column_double(stmt, 4);
        s.order   = sqlite3_column_int(stmt, 5);
        stops.push_back(s);
    }
    sqlite3_finalize(stmt);
    return stops;
}
