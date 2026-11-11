#pragma once
#include <string>
#include <vector>
#include "../db/Database.h"

struct Stop {
    int id;
    int routeId;
    int order;
    std::string name;
    double lat;
    double lng;
};

class Route {
public:
    int id;
    int driverId;
    int busId;
    std::string name;
    bool isActive;
    std::vector<Stop> stops;

    Route() : id(0), driverId(0), busId(0), isActive(true) {}

    static Route getActiveByDriver(Database& db, int driverId);
    static int create(Database& db, const Route& r);
    static bool deactivate(Database& db, int routeId);
    void addStop(Database& db, Stop& s);
    static std::vector<Route> getAllActive(Database& db);
    static std::vector<Stop> getStops(Database& db, int routeId);

    bool isValid() const { return id > 0; }
};
