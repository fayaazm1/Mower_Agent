// Ardumower Sunray Configurable Simulation (with JSON settings)
#include "config.h"
#include "robot.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <unistd.h>
#include <fstream>
#include <ctime>

// Prevent Arduino macro conflicts before including nlohmann/json
#ifdef A0
#undef A0
#undef A1
#undef A2
#undef A3
#undef A4
#undef A5
#undef A6
#undef A7
#endif
#ifdef B0
#undef B0
#undef B1
#undef B2
#undef B3
#undef B4
#undef B5
#undef B6
#undef B7
#endif

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---- Global simulation variables ----
double simBattery = 100.0;
double drain_rate = 0.05;
double charge_rate = 0.1;
double cutting_height = 3.0;
double grass_default_height = 5.0;
double maxX = 10.0, maxY = 10.0;
std::string scheduled_start = "00:00";
bool charging = false;
bool started = false;

// ---- Load configuration from settings.json ----
void loadSettings() {
    std::ifstream file("settings.json");
    if (!file.is_open()) {
        std::cerr << "⚠️ Could not open settings.json, using defaults.\n";
        return;
    }
    json config;
    file >> config;

    if (config.contains("battery_level")) simBattery = config["battery_level"];
    if (config.contains("battery_drain_rate")) drain_rate = config["battery_drain_rate"];
    if (config.contains("charging_rate")) charge_rate = config["charging_rate"];
    if (config.contains("cutting_height_mm")) cutting_height = config["cutting_height_mm"];
    if (config.contains("grass_default_height_mm")) grass_default_height = config["grass_default_height_mm"];
    if (config.contains("max_x")) maxX = config["max_x"];
    if (config.contains("max_y")) maxY = config["max_y"];
    if (config.contains("scheduled_start")) scheduled_start = config["scheduled_start"].get<std::string>();

}

// ---- Check if it’s time to start mowing ----
bool isScheduledTime() {
    time_t now = time(0);
    struct tm *ltm = localtime(&now);
    char current[6];
    snprintf(current, sizeof(current), "%02d:%02d", ltm->tm_hour, ltm->tm_min);
    return (std::string(current) == scheduled_start);
}

// ---- ASCII Map Display ----
void drawAsciiMap(double x, double y) {
    const int size = 20;
    std::system("clear");
    std::cout << "Sunray ASCII Simulator\n";
    std::cout << "Battery: " << std::fixed << std::setprecision(1) << simBattery
              << "% | State: " << (charging ? "CHARGING" : "MOWING")
              << " | Cutting Height: " << cutting_height << "mm\n\n";
    for (int j = size - 1; j >= 0; j--) {
        for (int i = 0; i < size; i++) {
            int px = static_cast<int>(std::round(x));
            int py = static_cast<int>(std::round(y));
            std::cout << ((i == px && j == py) ? 'M' : '.');
        }
        std::cout << '\n';
    }
}

// ---- Setup ----
void setup() {
    loadSettings();
    start();
    CONSOLE.println("Loaded config-based settings!");
    setOperation(OP_MOW);
}

// ---- Main Loop ----
// === Main Loop: Row-by-row mowing with obstacle avoidance ===
#include <vector>
#include <cmath>
#include <fstream>
#include <unistd.h>

struct Obstacle {
    double x, y;
};

// Define obstacles in meters (e.g., [x,y] in 10x10 field)
std::vector<Obstacle> obstacles = {
    {4.0, 4.0}, {6.0, 2.0}, {2.0, 7.0}
};

bool isObstacle(double x, double y) {
    for (auto& obs : obstacles) {
        double dist = std::sqrt((x - obs.x)*(x - obs.x) + (y - obs.y)*(y - obs.y));
        if (dist < 0.4) return true;  // within 40cm of obstacle
    }
    return false;
}

void loop() {
    static double x = 0.0, y = 0.0;
    static double maxX = 10.0, maxY = 10.0;
    static double step = 0.2;       // move 20 cm each loop
    static double rowHeight = 0.4;  // move up 40 cm per row
    static bool movingRight = true;
    static std::string state = "MOWING";
    static double battery = 100.0;
    static bool charging = false;

    if (!charging) {
        // Move horizontally left-to-right or right-to-left
        if (movingRight) x += step;
        else x -= step;

        // Prevent moving out of bounds
        if (x > maxX) { x = maxX; movingRight = false; y += rowHeight; }
        else if (x < 0) { x = 0; movingRight = true; y += rowHeight; }

        // Skip rows with obstacles
        for (auto& obs : obstacles) {
            double dist = std::sqrt((x - obs.x)*(x - obs.x) + (y - obs.y)*(y - obs.y));
            if (dist < 0.4) {
                y += rowHeight;  // skip row
                movingRight = !movingRight;
            }
        }

        // Stop mowing when top reached
        if (y >= maxY) {
            state = "RETURNING_TO_DOCK";
            x -= step * (x > 0 ? 1 : 0);
            y -= step * (y > 0 ? 1 : 0);
            if (x <= 0 && y <= 0) {
                state = "CHARGING";
                charging = true;
            }
        }

        battery -= 0.01;  // slower drain
        if (battery <= 10.0) {
            charging = true;
            state = "CHARGING";
        }

    } else {
        // charging phase
        battery += 0.1;
        if (battery >= 100.0) {
            battery = 100.0;
            charging = false;
            state = "MOWING";
            x = y = 0.0; // restart mowing cycle from dock
            movingRight = true;
        }
    }

    drawAsciiMap(x, y);

    std::ofstream posFile("position.txt");
    if (posFile.is_open()) {
        posFile << x << "," << y << "," << battery << "," << state;
        posFile.close();
    }

    usleep(100000);
}
