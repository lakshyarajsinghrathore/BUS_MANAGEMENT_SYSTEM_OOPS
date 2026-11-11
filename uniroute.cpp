#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// Base Class: Demonstrates Encapsulation & Inheritance
class User {
protected:
    int id;
    string name;
    string collegeId;

public:
    User() {
        id = 0;
        name = "";
        collegeId = "";
    }

    User(int uId, string uName, string cId) {
        id = uId;
        name = uName;
        collegeId = cId;
    }

    virtual ~User() {}

    // Polymorphic display method
    virtual void display() const {
        cout << "ID: " << id << " | Name: " << name << " | College ID: " << collegeId << endl;
    }

    string getName() const { return name; }
};

// Derived Class 1: Driver
class Driver : public User {
private:
    string busNumber;

public:
    Driver() : User() {
        busNumber = "Unassigned";
    }

    Driver(int uId, string uName, string cId, string bNum)
        : User(uId, uName, cId), busNumber(bNum) {}

    void display() const override {
        cout << "[Driver] ID: " << id << " | Name: " << name 
             << " | Emp ID: " << collegeId << " | Assigned Bus: " << busNumber << endl;
    }
};

// Derived Class 2: Passenger
class Passenger : public User {
private:
    string pickupStop;

public:
    Passenger() : User() {
        pickupStop = "";
    }

    Passenger(int uId, string uName, string cId, string stop)
        : User(uId, uName, cId), pickupStop(stop) {}

    void display() const override {
        cout << "[Passenger] ID: " << id << " | Name: " << name 
             << " | Roll No: " << collegeId << " | Stop: " << pickupStop << endl;
    }
};

// Class: Bus Management
class Bus {
private:
    int busId;
    string busNumber;
    int capacity;
    int currentPassengers;

public:
    Bus() {
        busId = 0;
        busNumber = "";
        capacity = 40;
        currentPassengers = 0;
    }

    Bus(int id, string number, int cap) {
        busId = id;
        busNumber = number;
        capacity = cap;
        currentPassengers = 0;
    }

    void boardPassenger() {
        if (currentPassengers < capacity) {
            currentPassengers++;
            cout << "\nPassenger boarded successfully." << endl;
        } else {
            cout << "\nAlert: Bus is at full capacity!" << endl;
        }
    }

    void displayStatus() const {
        cout << "\nBus ID: " << busId << " | Number: " << busNumber 
             << " | Capacity: " << capacity 
             << " | On-Board: " << currentPassengers 
             << " | Available Seats: " << (capacity - currentPassengers) << endl;
    }

    int getAvailableSeats() const {
        return capacity - currentPassengers;
    }
};

// Class: Route Management
class Route {
private:
    int routeId;
    string routeName;
    string startPoint;
    string endPoint;
    int totalStops;

public:
    Route() {
        routeId = 0;
        routeName = "";
        startPoint = "";
        endPoint = "";
        totalStops = 0;
    }

    Route(int rId, string rName, string start, string end, int stops) {
        routeId = rId;
        routeName = rName;
        startPoint = start;
        endPoint = end;
        totalStops = stops;
    }

    void displayRoute() const {
        cout << "\nRoute ID: " << routeId << " | Title: " << routeName 
             << "\nFrom: " << startPoint << " ---> To: " << endPoint 
             << " (" << totalStops << " Waypoint Stops)" << endl;
    }

    // File Handling: Appends route trip to text file
    void saveToFile() const {
        ofstream file("trip_log.txt", ios::app);
        if (file.is_open()) {
            file << "Route: " << routeName << " | From: " << startPoint 
                 << " | To: " << endPoint << " | Status: Active\n";
            file.close();
            cout << "Trip details appended to trip_log.txt successfully.\n";
        } else {
            cout << "Error opening log file.\n";
        }
    }
};

// Main Driver Application
int main() {
    Driver driver1(101, "Ramesh Singh", "EMP-DRV-01", "RJ-27-PA-1001");
    Passenger pass1(201, "Aarav Sharma", "25EGICS124", "City Center");
    Passenger pass2(202, "Priya Verma", "25EGICS144", "Subhash Nagar");

    Bus campusBus(1, "RJ-27-PA-1001", 40);
    Route campusRoute(1, "Route 1: City Center to GITS Campus", "City Center", "GITS Campus", 4);

    int choice;
    do {
        cout << "\n======================================================\n";
        cout << "      UNIROUTE - COLLEGE BUS MANAGEMENT SYSTEM       \n";
        cout << "======================================================\n";
        cout << "1. Display User Profiles (Driver & Passengers)\n";
        cout << "2. Display Route & Waypoints\n";
        cout << "3. Display Live Bus & Seat Status\n";
        cout << "4. Board a Passenger (Simulate Heartbeat)\n";
        cout << "5. Save Active Trip Log to File\n";
        cout << "0. Exit System\n";
        cout << "======================================================\n";
        cout << "Enter your choice: ";
        
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "\nInvalid input. Please enter a valid number.\n";
            continue;
        }

        switch (choice) {
            case 1:
                cout << "\n--- REGISTERED USERS ---\n";
                driver1.display();
                pass1.display();
                pass2.display();
                break;

            case 2:
                cout << "\n--- ROUTE DETAILS ---\n";
                campusRoute.displayRoute();
                break;

            case 3:
                cout << "\n--- VEHICLE CAPACITY STATUS ---\n";
                campusBus.displayStatus();
                break;

            case 4:
                campusBus.boardPassenger();
                campusBus.displayStatus();
                break;

            case 5:
                campusRoute.saveToFile();
                break;

            case 0:
                cout << "\nThank you for using UniRoute System. Goodbye!\n";
                break;

            default:
                cout << "\nInvalid choice. Please select options 0 to 5.\n";
                break;
        }

    } while (choice != 0);

    cout << "\nPress Enter to exit console...";
    cin.ignore(1000, '\n');
    cin.get();

    return 0;
}
