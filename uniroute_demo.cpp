#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <limits>

using namespace std;

// ========================================================
// Module 1: Custom Exception & User Base Class (Inheritance & Polymorphism)
// ========================================================
class BusException : public runtime_error {
public:
    BusException(const string& message) : runtime_error(message) {}
};

class User {
protected:
    int id;
    string name;
    string collegeId;

public:
    User(int uId, string uName, string cId) 
        : id(uId), name(uName), collegeId(cId) {}

    virtual ~User() {}

    virtual void display() const = 0;
    virtual string getRole() const = 0;

    int getId() const { return id; }
    string getName() const { return name; }
    string getCollegeId() const { return collegeId; }
};

class Driver : public User {
private:
    string licenseNumber;
    int assignedBusId;

public:
    Driver(int uId, string uName, string cId, string lic, int bId = 0)
        : User(uId, uName, cId), licenseNumber(lic), assignedBusId(bId) {}

    void setAssignedBus(int bId) { assignedBusId = bId; }
    int getAssignedBus() const { return assignedBusId; }
    string getLicense() const { return licenseNumber; }

    string getRole() const override { return "Driver"; }

    void display() const override {
        cout << left << setw(6) << id 
             << setw(18) << name 
             << setw(16) << collegeId 
             << setw(12) << "Driver" 
             << "Bus ID: " << (assignedBusId ? to_string(assignedBusId) : "None") 
             << endl;
    }
};

class Passenger : public User {
private:
    string pickupStop;

public:
    Passenger(int uId, string uName, string cId, string stop)
        : User(uId, uName, cId), pickupStop(stop) {}

    string getPickupStop() const { return pickupStop; }

    string getRole() const override { return "Passenger"; }

    void display() const override {
        cout << left << setw(6) << id 
             << setw(18) << name 
             << setw(16) << collegeId 
             << setw(12) << "Passenger" 
             << "Pickup: " << pickupStop 
             << endl;
    }
};

// ========================================================
// Module 2: Fleet & Vehicle Management (Encapsulation & Friend Function)
// ========================================================
class Bus {
private:
    int busId;
    string busNumber;
    int capacity;
    int driverId;

public:
    static int totalBuses;

    Bus(int id, string number, int cap = 40)
        : busId(id), busNumber(number), capacity(cap), driverId(0) {
        totalBuses++;
    }

    int getBusId() const { return busId; }
    string getBusNumber() const { return busNumber; }
    int getCapacity() const { return capacity; }
    int getDriverId() const { return driverId; }

    void assignDriver(int dId) {
        if (dId <= 0) throw BusException("Invalid Driver ID provided.");
        driverId = dId;
    }

    void display() const {
        cout << left << setw(8) << busId 
             << setw(18) << busNumber 
             << setw(12) << capacity 
             << "Driver ID: " << (driverId ? to_string(driverId) : "Unassigned") 
             << endl;
    }

    friend void printBusSummary(const Bus& b, const Driver& d);
};

int Bus::totalBuses = 0;

void printBusSummary(const Bus& b, const Driver& d) {
    cout << "\n[Assignment Verified] Bus " << b.busNumber 
         << " (Cap: " << b.capacity << ") assigned to Driver " << d.getName() 
         << " (" << d.getCollegeId() << ")" << endl;
}

// ========================================================
// Module 3: Route & Stop Sequencing (Composition & Operator Overloading)
// ========================================================
class Stop {
private:
    int stopId;
    string stopName;
    double latitude;
    double longitude;
    int stopOrder;

public:
    Stop(int id, string name, double lat, double lng, int order)
        : stopId(id), stopName(name), latitude(lat), longitude(lng), stopOrder(order) {}

    int getStopId() const { return stopId; }
    string getStopName() const { return stopName; }
    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    int getStopOrder() const { return stopOrder; }

    bool operator<(const Stop& other) const {
        return stopOrder < other.stopOrder;
    }

    void display() const {
        cout << "  Stop " << stopOrder << ": " << setw(24) << left << stopName 
             << " Coordinates: (" << fixed << setprecision(4) << latitude 
             << ", " << longitude << ")" << endl;
    }
};

class Route {
private:
    int routeId;
    string routeName;
    int busId;
    int driverId;
    vector<Stop> stops;
    bool isActive;

public:
    Route(int rId, string rName, int bId, int dId)
        : routeId(rId), routeName(rName), busId(bId), driverId(dId), isActive(true) {}

    int getRouteId() const { return routeId; }
    string getRouteName() const { return routeName; }
    int getBusId() const { return busId; }
    int getDriverId() const { return driverId; }
    bool getStatus() const { return isActive; }

    void addStop(const Stop& s) {
        stops.push_back(s);
        sort(stops.begin(), stops.end());
    }

    void displayItinerary() const {
        cout << "\nRoute " << routeId << ": " << routeName 
             << " | Bus ID: " << busId << " | Driver ID: " << driverId 
             << " | Status: " << (isActive ? "ACTIVE" : "COMPLETED") << endl;
        cout << "Scheduled Stops (" << stops.size() << " total):" << endl;
        for (const auto& s : stops) {
            s.display();
        }
    }
};

// ========================================================
// Module 4: Real-Time GPS Telemetry
// ========================================================
class LiveLocation {
private:
    int driverId;
    double latitude;
    double longitude;
    double speed;
    string lastUpdated;

public:
    LiveLocation(int dId, double lat = 0.0, double lng = 0.0, double spd = 0.0)
        : driverId(dId), latitude(lat), longitude(lng), speed(spd), lastUpdated("Just now") {}

    void updatePosition(double lat, double lng, double spd) {
        if (lat < -90.0 || lat > 90.0 || lng < -180.0 || lng > 180.0) {
            throw BusException("Invalid GPS coordinates: Out of geographical bounds.");
        }
        if (spd < 0.0) {
            throw BusException("Invalid speed: Speed cannot be negative.");
        }
        latitude = lat;
        longitude = lng;
        speed = spd;
        lastUpdated = "Updated just now (5s interval)";
    }

    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    double getSpeed() const { return speed; }

    void display() const {
        cout << "Driver " << driverId << " Telemetry -> Lat: " 
             << fixed << setprecision(5) << latitude 
             << " | Lng: " << longitude 
             << " | Speed: " << setprecision(1) << speed << " km/h"
             << " [" << lastUpdated << "]" << endl;
    }
};

// ========================================================
// Module 5: Thread-Safe Live Occupancy Engine (Singleton, Concurrency)
// ========================================================
class OccupancyManager {
private:
    mutex mtx;
    unordered_map<int, unordered_map<int, chrono::steady_clock::time_point>> activeRiders;
    const int EXPIRY_SECONDS = 30;

    OccupancyManager() {}

    void cleanupExpiredLocked(int routeId, const chrono::steady_clock::time_point& now) {
        auto it = activeRiders.find(routeId);
        if (it == activeRiders.end()) return;
        auto& riders = it->second;
        for (auto rit = riders.begin(); rit != riders.end(); ) {
            auto elapsed = chrono::duration_cast<chrono::seconds>(now - rit->second).count();
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
        lock_guard<mutex> lock(mtx);
        auto now = chrono::steady_clock::now();
        cleanupExpiredLocked(routeId, now);
        activeRiders[routeId][userId] = now;
    }

    void removePassenger(int routeId, int userId) {
        lock_guard<mutex> lock(mtx);
        auto it = activeRiders.find(routeId);
        if (it != activeRiders.end()) {
            it->second.erase(userId);
        }
    }

    void getOccupancy(int routeId, int capacity, int& occupied, int& available, double& occupiedPercent) {
        lock_guard<mutex> lock(mtx);
        auto now = chrono::steady_clock::now();
        cleanupExpiredLocked(routeId, now);

        occupied = 0;
        auto it = activeRiders.find(routeId);
        if (it != activeRiders.end()) {
            occupied = static_cast<int>(it->second.size());
        }

        if (capacity <= 0) capacity = 40;
        available = max(0, capacity - occupied);
        occupiedPercent = (static_cast<double>(occupied) / capacity) * 100.0;
    }
};

// ========================================================
// Module 6: Interactive Console Application Driver (Clean Pre-Seeded Outputs)
// ========================================================
int main() {
    vector<unique_ptr<User>> users;
    vector<Bus> fleet;
    vector<Route> activeRoutes;
    vector<LiveLocation> telemetryRecords;

    // Seed Initial Baseline Data
    users.push_back(make_unique<Driver>(101, "Ramesh Singh", "EMP-DRV-01", "DL-RJ-2015-098", 1));
    users.push_back(make_unique<Driver>(102, "Mohan Lal", "EMP-DRV-02", "DL-RJ-2018-441", 2));
    users.push_back(make_unique<Passenger>(201, "Aarav Sharma", "25EGICS124", "City Center"));
    users.push_back(make_unique<Passenger>(202, "Priya Verma", "25EGICS144", "Subhash Nagar"));

    fleet.push_back(Bus(1, "RJ-27-PA-1001", 40));
    fleet[0].assignDriver(101);
    fleet.push_back(Bus(2, "RJ-27-PA-1002", 40));
    fleet[1].assignDriver(102);

    Route r1(1, "Route 1: City Center to GITS Campus", 1, 101);
    r1.addStop(Stop(1, "City Center Point", 24.5854, 73.7125, 1));
    r1.addStop(Stop(2, "Subhash Nagar Circle", 24.5920, 73.7250, 2));
    r1.addStop(Stop(3, "Pratap Nagar Bypass", 24.6050, 73.7480, 3));
    r1.addStop(Stop(4, "GITS Dabok Main Gate", 24.6180, 73.8560, 4));
    activeRoutes.push_back(r1);

    telemetryRecords.push_back(LiveLocation(101, 24.5854, 73.7125, 35.5));

    int choice;
    do {
        cout << "\n======================================================\n";
        cout << "      UNIROUTE - COLLEGE BUS MANAGEMENT SYSTEM       \n";
        cout << "======================================================\n";
        cout << "1. View User Accounts (Polymorphism Demo)\n";
        cout << "2. View Fleet Inventory & Driver Assignment\n";
        cout << "3. View Route Itinerary & Ordered Waypoints\n";
        cout << "4. Live GPS Telemetry Update & Ingestion\n";
        cout << "5. Live Passenger Heartbeat & Occupancy Calculation\n";
        cout << "6. Exception Handling Demonstration\n";
        cout << "0. Exit System\n";
        cout << "======================================================\n";
        cout << "Enter your choice: ";
        
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\n[ERROR] Invalid input. Please enter a number.\n";
            continue;
        }

        try {
            switch (choice) {
                case 1: {
                    cout << "\n--- REGISTERED USERS & ROLES ---\n";
                    cout << left << setw(6) << "ID" << setw(18) << "Name" 
                         << setw(16) << "College/Emp ID" << setw(12) << "Role" 
                         << "Details\n";
                    cout << "------------------------------------------------------\n";
                    for (const auto& u : users) {
                        u->display();
                    }
                    break;
                }
                case 2: {
                    cout << "\n--- FLEET INVENTORY (Total Registered: " << Bus::totalBuses << ") ---\n";
                    cout << left << setw(8) << "Bus ID" << setw(18) << "Registration" 
                         << setw(12) << "Capacity" << "Status\n";
                    cout << "------------------------------------------------------\n";
                    for (const auto& b : fleet) {
                        b.display();
                    }
                    Driver* d = dynamic_cast<Driver*>(users[0].get());
                    if (d) printBusSummary(fleet[0], *d);
                    break;
                }
                case 3: {
                    cout << "\n--- ACTIVE TRANSIT ROUTES & STOP SEQUENCING ---\n";
                    for (const auto& r : activeRoutes) {
                        r.displayItinerary();
                    }
                    break;
                }
                case 4: {
                    cout << "\n--- REAL-TIME GPS TELEMETRY INGESTION ---\n";
                    cout << "Current State:\n";
                    telemetryRecords[0].display();

                    cout << "\nSimulating 5-second driver GPS packet ingestion...\n";
                    telemetryRecords[0].updatePosition(24.5940, 73.7290, 42.0);
                    cout << "[SUCCESS] Ingestion Acknowledged:\n";
                    telemetryRecords[0].display();
                    break;
                }
                case 5: {
                    cout << "\n--- THREAD-SAFE OCCUPANCY & SEAT ESTIMATION ---\n";
                    OccupancyManager& occ = OccupancyManager::getInstance();

                    cout << "Simulating on-board passenger heartbeats on Route 1...\n";
                    occ.recordHeartbeat(1, 201);
                    occ.recordHeartbeat(1, 202);

                    int occupied = 0, available = 0;
                    double occPct = 0.0;
                    occ.getOccupancy(1, 40, occupied, available, occPct);

                    cout << "Active Route: Route 1 | Total Bus Capacity: 40\n";
                    cout << "Active On-Board Riders : " << occupied << endl;
                    cout << "Available Free Seats   : " << available << endl;
                    cout << "Current Congestion Rate: " << fixed << setprecision(1) << occPct << "%\n";
                    break;
                }
                case 6: {
                    cout << "\n--- EXCEPTION HANDLING DEMONSTRATION ---\n";
                    cout << "Attempting to assign an invalid Driver ID (-5) to Bus 1...\n";
                    fleet[0].assignDriver(-5);
                    break;
                }
                case 0: {
                    cout << "\nExiting UniRoute College Bus Management System. Goodbye!\n";
                    break;
                }
                default: {
                    cout << "\n[ERROR] Invalid menu choice. Please select from 0 to 6.\n";
                    break;
                }
            }
        }
        catch (const BusException& ex) {
            cout << "\n[CAUGHT EXCEPTION] BusException: " << ex.what() << endl;
        }
        catch (const exception& ex) {
            cout << "\n[CAUGHT STANDARD EXCEPTION] Error: " << ex.what() << endl;
        }

    } while (choice != 0);

    cout << "\nPress Enter to exit console...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();

    return 0;
}
