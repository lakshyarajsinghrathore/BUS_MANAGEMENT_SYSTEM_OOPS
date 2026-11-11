#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif
#include "utils/httplib.h"
#include "db/Database.h"
#include "utils/SessionManager.h"
#include "controllers/AuthController.h"
#include "controllers/RouteController.h"
#include "controllers/LocationController.h"

int main() {
    std::cout << "=== College Bus Management System ===" << std::endl;

    Database db("bus_management.db");
    std::cout << "[OK] Database opened." << std::endl;

    db.loadSchema("backend/db/schema.sql");
    std::cout << "[OK] Schema loaded." << std::endl;

    SessionManager sessions(db);
    httplib::Server server;

    server.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        res.status = 500;
        try {
            if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            res.set_content(std::string("{\"error\":\"Internal error: ") + e.what() + "\"}", "application/json");
        } catch (...) {
            res.set_content("{\"error\":\"Internal server error\"}", "application/json");
        }
    });

    // Health check endpoint for Render / uptime monitors
    server.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // Wire up all API controllers
    AuthController     auth(db, sessions, server);
    RouteController    routes(db, sessions, server);
    LocationController location(db, sessions, server);

    int port = 8080;
    if (const char* envPort = std::getenv("PORT")) {
        try {
            int parsed = std::stoi(envPort);
            if (parsed > 0 && parsed <= 65535) port = parsed;
        } catch (...) {}
    }

    std::cout << "[OK] All controllers registered." << std::endl;
    std::cout << "[>>] Server running on http://0.0.0.0:" << port << std::endl;

    while (true) {
        try {
            server.listen("0.0.0.0", port);
        } catch (const std::exception& ex) {
            std::cerr << "[ERROR] Server error: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "[ERROR] Unknown server exception." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}
