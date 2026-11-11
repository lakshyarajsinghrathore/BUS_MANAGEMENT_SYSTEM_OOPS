#pragma once
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>

class OccupancyManager {
private:
    std::mutex mtx;
    // routeId -> { passengerUserId -> last_heartbeat_time }
    std::unordered_map<int, std::unordered_map<int, std::chrono::steady_clock::time_point>> activeRiders;
    const int EXPIRY_SECONDS = 30;

    void cleanupExpiredLocked(int routeId, const std::chrono::steady_clock::time_point& now) {
        auto it = activeRiders.find(routeId);
        if (it == activeRiders.end()) return;
        auto& riders = it->second;
        for (auto rit = riders.begin(); rit != riders.end(); ) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - rit->second).count();
            if (elapsed > EXPIRY_SECONDS) {
                rit = riders.erase(rit);
            } else {
                ++rit;
            }
        }
    }

public:
    static OccupancyManager& getInstance() {
        static OccupancyManager instance;
        return instance;
    }

    void recordHeartbeat(int routeId, int userId) {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        cleanupExpiredLocked(routeId, now);
        activeRiders[routeId][userId] = now;
    }

    void removePassenger(int routeId, int userId) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = activeRiders.find(routeId);
        if (it != activeRiders.end()) {
            it->second.erase(userId);
        }
    }

    int getOccupiedCount(int routeId) {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        cleanupExpiredLocked(routeId, now);
        auto it = activeRiders.find(routeId);
        if (it == activeRiders.end()) return 0;
        return static_cast<int>(it->second.size());
    }

    void getOccupancy(int routeId, int capacity, int& occupied, int& available, double& occupiedPct, double& availablePct) {
        std::lock_guard<std::mutex> lock(mtx);
        if (capacity <= 0) capacity = 40;
        auto now = std::chrono::steady_clock::now();
        cleanupExpiredLocked(routeId, now);
        auto it = activeRiders.find(routeId);
        int count = (it != activeRiders.end()) ? static_cast<int>(it->second.size()) : 0;
        occupied = std::min(count, capacity);
        available = std::max(0, capacity - occupied);
        occupiedPct = (occupied * 100.0) / capacity;
        availablePct = (available * 100.0) / capacity;
    }
};
