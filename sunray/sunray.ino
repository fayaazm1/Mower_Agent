#include "config.h"
#include "robot.h"

// Access real battery object from battery.cpp
extern Battery battery;

// ==== C++ STL for simulation output ====
#include <iostream>
#include <iomanip>
#include <cmath>
#include <unistd.h>
#include <fstream>

// ---------------------------------------------------------
// Lawn grid / coverage map (20m x 20m, 1m per cell)
// ---------------------------------------------------------
const int    GRID_SIZE   = 20;
const double GRID_SCALE  = 1.0;   // 1 meter per cell
bool visited[GRID_SIZE][GRID_SIZE] = {false};

// Flag: did we finish covering the entire lawn?
bool g_coverageDone = false;

// ---------------------------------------------------------
// Obstacles (e.g., trees) on the lawn
// ---------------------------------------------------------
bool obstacle[GRID_SIZE][GRID_SIZE] = {false};

struct Obstacle {
  int gx;
  int gy;
};

// You can add more obstacles here
const Obstacle OBSTACLES[] = {
  {10, 1},   // tree in 2nd row from bottom
};

const int NUM_OBSTACLES = sizeof(OBSTACLES) / sizeof(OBSTACLES[0]);

// For convenience, treat the first obstacle as "the tree"
const int TREE_X = OBSTACLES[0].gx;
const int TREE_Y = OBSTACLES[0].gy;

// Total mowable cells (all grid cells minus obstacle cells)
int TOTAL_MOWABLE_CELLS = GRID_SIZE * GRID_SIZE;

void initObstacles() {
  // Clear any previous obstacle flags (defensive)
  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      obstacle[r][c] = false;
    }
  }

  int obstacleCount = 0;
  for (int i = 0; i < NUM_OBSTACLES; i++) {
    int gx = OBSTACLES[i].gx;
    int gy = OBSTACLES[i].gy;
    if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
      if (!obstacle[gy][gx]) {
        obstacle[gy][gx] = true;
        obstacleCount++;
      }
    }
  }

  TOTAL_MOWABLE_CELLS = GRID_SIZE * GRID_SIZE - obstacleCount;
}

bool isObstacleCell(int gx, int gy) {
  if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) return false;
  return obstacle[gy][gx];
}

// ---------------------------------------------------------
// NEW FEATURE: Simple daily scheduling
// ---------------------------------------------------------
//
// We simulate a "minute of day" [0..1439] and define a mowing window.
// In this version, SCHEDULING_ENABLED is set to false so that
// scheduling is controlled externally by the Python dashboard,
// and the mower is always allowed to mow.
//

// Toggle this to enable the internal (sim-only) scheduling feature
const bool SCHEDULING_ENABLED = false;   // DISABLED: dashboard handles schedule

// One mowing window: 08:00–20:00 (unused when scheduling is disabled)
struct ScheduleSlot {
  int startMinute;   // inclusive
  int endMinute;     // exclusive
};

const ScheduleSlot scheduleSlots[] = {
  {8 * 60, 20 * 60},   // 08:00–20:00
};

const int NUM_SLOTS_SCHED = sizeof(scheduleSlots) / sizeof(scheduleSlots[0]);

bool isWithinSchedule(int minuteOfDay) {
  if (!SCHEDULING_ENABLED) return true;   // scheduling OFF → always allowed

  for (int i = 0; i < NUM_SLOTS_SCHED; i++) {
    if (minuteOfDay >= scheduleSlots[i].startMinute &&
        minuteOfDay <  scheduleSlots[i].endMinute) {
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------
// Helper: check if coverage == 100% (ignoring obstacle cells)
// ---------------------------------------------------------
bool isCoverageDone() {
  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      if (obstacle[r][c]) continue;   // skip tree cells
      if (!visited[r][c]) {
        return false;
      }
    }
  }
  return true;
}

// Coverage percent helper
double getCoveragePercent() {
  int coveredCount = 0;
  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      if (obstacle[r][c]) continue;
      if (visited[r][c]) coveredCount++;
    }
  }
  if (TOTAL_MOWABLE_CELLS <= 0) return 0.0;
  return (double)coveredCount / (double)TOTAL_MOWABLE_CELLS * 100.0;
}


// ---------------------------------------------------------
// Helper: write x, y, coverage, battery to shared file
// Format: x,y,coverage,battery
// Example: 12,4,23.1,93
// ---------------------------------------------------------
void writeTelemetry(double x, double y) {
  double cov = getCoveragePercent();   // coverage in %
  double bat = battery.distanceSOC;    // battery in %

  std::ofstream posFile("/home/fayaaz/MowerShared/vm_output/position.txt");
  if (posFile.is_open()) {
    posFile << x << "," << y << "," << cov << "," << bat;
    posFile.close();
  }
}



// ---------------------------------------------------------
// ASCII map drawing
// ---------------------------------------------------------
void drawAsciiMap(double x, double y) {
  const char mower = 'M';

  // compute grid cell for robot
  int px = static_cast<int>(std::round(x * GRID_SCALE));
  int py = static_cast<int>(std::round(y * GRID_SCALE));

  // clamp to grid and mark visited
  if (px >= 0 && px < GRID_SIZE && py >= 0 && py < GRID_SIZE) {
    visited[py][px] = true;
  }

  std::system("clear");
  std::cout << "Sunray ASCII Simulator\n";
  std::cout << "Use keys: o=obstacle, r=rain, l=lowbat, Ctrl+C=exit\n\n";

  for (int j = GRID_SIZE - 1; j >= 0; j--) {
    for (int i = 0; i < GRID_SIZE; i++) {
      if (i == px && j == py) {
        std::cout << mower;          // robot here
      } else if (isObstacleCell(i, j)) {
        std::cout << 'T';            // tree / obstacle
      } else if (visited[j][i]) {
        std::cout << '*';            // already covered
      } else {
        std::cout << '.';            // not visited
      }
    }
    std::cout << '\n';
  }

  // coverage % (only mowable cells)
  int coveredCount = 0;
  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      if (obstacle[r][c]) continue;
      if (visited[r][c]) coveredCount++;
    }
  }
  double percent = 0.0;
  if (TOTAL_MOWABLE_CELLS > 0) {
    percent = (double)coveredCount / (double)TOTAL_MOWABLE_CELLS * 100.0;
  }

  std::cout << "\nCoverage: " << std::fixed << std::setprecision(1)
            << percent << "%\n";

  // position
  std::cout << "Robot position (x,y): " << std::fixed << std::setprecision(2)
            << x << ", " << y << std::endl;

  // battery
  std::cout << "Battery: " << std::fixed << std::setprecision(0)
            << battery.distanceSOC << "%\n";
}

// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------
void setup() {
  start(); // initialize real robot core (stateEstimator, battery, ops, etc.)

  battery.distanceSOC = 100;  // start full battery

  initObstacles();            // place obstacles on the map

  CONSOLE.println("Forcing mower to start in MowOp mode...");
  setOperation(OP_MOW);
}

// ---- Scheduling clock state (global) ----
static int simMinute = 7 * 60 + 30;  // start at 07:30 (before schedule)
static int tickCount = 0;            // loop ticks -> minutes
static int simDay    = 1;            // Day counter
const  int MAX_SIM_DAYS = 3;         // simulate up to 3 days

// Global flag so printing code can see whether we are allowed to mow
bool g_insideSchedule = true;        // updated each loop

// ---------------------------------------------------------
// Enhancement 2: mid-run recharge + resume
// ---------------------------------------------------------
bool   g_midRunUsed      = false;  // only trigger once
bool   g_midRunGoingHome = false;  // field -> dock
bool   g_midRunCharging  = false;  // charging at dock
bool   g_midRunGoingBack = false;  // dock -> resume point
double g_midRunResumeX   = 0.0;    // where we stopped around 50%
double g_midRunResumeY   = 0.0;

// ---------------------------------------------------------
// Helper: advance simulated time (10 loops = 1 minute)
// Handles day rollover and max-day stop
// ---------------------------------------------------------
void advanceSimTime() {
  tickCount++;
  if (tickCount >= 10) {           // every 10 loops ≈ 1 minute
    tickCount = 0;
    simMinute++;

    if (simMinute >= 24 * 60) {    // new day
      simMinute = 0;
      simDay++;

      std::cout << "\n--- END OF DAY " << (simDay - 1)
                << " ---  starting Day " << simDay << " ---\n";

      if (simDay > MAX_SIM_DAYS) {
        std::cout << "\nMax simulated days reached. Stopping.\n";
        // freeze so the log stays visible
        while (true) {
          usleep(1000000);
        }
      }
    }
  }
}

// ---------------------------------------------------------
// Main simulation loop
// ---------------------------------------------------------
void loop() {
  // Robot position in meters
  static double x = 0.0, y = 0.0;

  // Discrete grid cell used for coverage + U-turn logic
  static int gx = 0;
  static int gy = 0;

  // FIELD SIZE: sweep full 20x20 grid (0..19)
  static const int maxGX = GRID_SIZE - 1; // 19
  static const int maxGY = GRID_SIZE - 1; // 19;

  // serpentine sweep state
  static bool goingRight = true;   // true = move +x, false = move -x

  // detour around tree (old proven logic)
  static bool detourActive      = false;
  static int  detourStepIndex   = 0;
  static int  detourLen         = 0;
  static int  detourDX[8];
  static int  detourDY[8];

  // compute if mowing is allowed now (based on sim time)
  bool allowedNow = isWithinSchedule(simMinute);
  g_insideSchedule = allowedNow;

  // remember old position for distance calc
  double prevX = x;
  double prevY = y;

  int hh = simMinute / 60;
  int mm = simMinute % 60;

  // ---------------------------------------------------
  // 1) If NOT in schedule window → go/stay HOME
  // ---------------------------------------------------
  if (!allowedNow) {
    // If we're away from home, gently drive back to (0,0)
    if (!(x < 0.5 && y < 0.5)) {
      x -= (x * 0.1);
      y -= (y * 0.1);

      double dx = x - prevX;
      double dy = y - prevY;
      double distance = std::sqrt(dx*dx + dy*dy);
      battery.updateByDistance(distance);
    }
    // else: already docked at home, no movement, no extra drain

    // Draw map + state
    drawAsciiMap(x, y);

    // Optional: write position to file for external visualizers
    writeTelemetry(x, y);

    // show simulated time + day
    std::cout << "Day " << simDay << " | Sim time: "
              << std::setfill('0') << std::setw(2) << hh
              << ":" << std::setw(2) << mm << std::setfill(' ') << "\n";

    std::cout << "Schedule window: 08:00–20:00\n";
    std::cout << "Status: Waiting (outside schedule)\n";
    if (battery.distanceSOC < 100.0) {
      std::cout << "Robot is charging at dock...\n\n";
    } else {
      std::cout << "Robot is parked at dock (fully charged)...\n\n";
    }

    // Slow down simulation + advance simulated clock
    usleep(150000);
    advanceSimTime();
    return; // do NOT run mowing logic when outside schedule
  }

  // ---------------------------------------------------
  // 2) If coverage is DONE → drive to dock and stay there
  // ---------------------------------------------------
  if (g_coverageDone) {
    // homing behaviour after full coverage
    if (!(x < 0.5 && y < 0.5)) {
      prevX = x;
      prevY = y;

      x -= (x * 0.1);
      y -= (y * 0.1);

      double dx = x - prevX;
      double dy = y - prevY;
      double distance = std::sqrt(dx*dx + dy*dy);
      battery.updateByDistance(distance);
    } else {
      // Already at dock
      std::cout << "COVERAGE COMPLETE & DOCKED AT HOME.\n";
    }

    // Draw map + state
    drawAsciiMap(x, y);
    // Write position file
    writeTelemetry(x, y);

    // Show time + day
    std::cout << "Day " << simDay << " | Sim time: "
              << std::setfill('0') << std::setw(2) << hh
              << ":" << std::setw(2) << mm << std::setfill(' ') << "\n";

    std::cout << "Status: Coverage finished → going/staying at dock\n\n";

    usleep(150000);
    advanceSimTime();
    return;
  }

  // ---------------------------------------------------
  // 3) Enhancement 2: mid-run recharge state machine
  //    (only active after we trigger it at ~50% coverage)
  // ---------------------------------------------------
  if (g_midRunUsed && !g_coverageDone &&
      (g_midRunGoingHome || g_midRunCharging || g_midRunGoingBack)) {

    if (g_midRunGoingHome) {
      // Move 10% of the remaining vector toward dock each loop
      double dx = -x;
      double dy = -y;
      double stepX = dx * 0.10;
      double stepY = dy * 0.10;

      double dist = std::sqrt(stepX*stepX + stepY*stepY);
      x += stepX;
      y += stepY;
      battery.updateByDistance(dist);

      if (std::fabs(x) < 0.3 && std::fabs(y) < 0.3) {
        x = 0.0;
        y = 0.0;
        g_midRunGoingHome = false;
        g_midRunCharging  = true;
        std::cout << "[Mid-run] Arrived at dock — starting recharge.\n";
      }

      writeTelemetry(x, y);


      std::cout << "Day " << simDay << " | Sim time: "
                << std::setfill('0') << std::setw(2) << hh
                << ":" << std::setw(2) << mm << std::setfill(' ') << "\n";
      std::cout << "Status: Mid-run → going to dock for recharge\n\n";

      usleep(200000);
      advanceSimTime();
      return;
    }

    if (g_midRunCharging) {
      if (battery.distanceSOC < 100.0) {
        battery.distanceSOC += 5.0;
        if (battery.distanceSOC > 100.0) battery.distanceSOC = 100.0;
        std::cout << "[Mid-run] Charging at dock... SOC = "
                  << battery.distanceSOC << "%\n";
      } else {
        g_midRunCharging  = false;
        g_midRunGoingBack = true;
        std::cout << "[Mid-run] Fully charged — heading back to resume point.\n";
      }

      drawAsciiMap(x, y);
      writeTelemetry(x, y);


      std::cout << "Day " << simDay << " | Sim time: "
                << std::setfill('0') << std::setw(2) << hh
                << ":" << std::setw(2) << mm << std::setfill(' ') << "\n";
      std::cout << "Status: Mid-run → charging at dock\n\n";

      usleep(200000);
      advanceSimTime();
      return;
    }

    if (g_midRunGoingBack) {
      // Move 10% of remaining vector toward resume point
      double dx = g_midRunResumeX - x;
      double dy = g_midRunResumeY - y;
      double stepX = dx * 0.10;
      double stepY = dy * 0.10;

      double dist = std::sqrt(stepX*stepX + stepY*stepY);
      x += stepX;
      y += stepY;
      battery.updateByDistance(dist);

      if (std::fabs(x - g_midRunResumeX) < 0.3 &&
          std::fabs(y - g_midRunResumeY) < 0.3) {
        x = g_midRunResumeX;
        y = g_midRunResumeY;

        // sync grid cell with resume point
        gx = static_cast<int>(std::round(x));
        gy = static_cast<int>(std::round(y));

        g_midRunGoingBack = false;   // back to work
        std::cout << "[Mid-run] Back at resume point — continuing mowing.\n";
      }

      drawAsciiMap(x, y);
      writeTelemetry(x, y);


      std::cout << "Day " << simDay << " | Sim time: "
                << std::setfill('0') << std::setw(2) << hh
                << ":" << std::setw(2) << mm << std::setfill(' ') << "\n";
      std::cout << "Status: Mid-run → returning to resume point\n\n";

      usleep(200000);
      advanceSimTime();
      return;
    }
  }

  // ---------------------------------------------------
  // 4) Normal mowing behaviour + U-shape detour around tree
  //     (using discrete gx/gy like in your old working code)
  // ---------------------------------------------------

  if (gy > maxGY) gy = maxGY;

  if (detourActive) {
    // ---- follow the precomputed U-shaped path ----
    if (detourStepIndex < detourLen) {
      gx += detourDX[detourStepIndex];
      gy += detourDY[detourStepIndex];

      if (gx < 0)    gx = 0;
      if (gx > maxGX) gx = maxGX;
      if (gy < 0)    gy = 0;
      if (gy > maxGY) gy = maxGY;

      detourStepIndex++;
      if (detourStepIndex >= detourLen) {
        detourActive = false; // finished detour
      }
    }
  } else {
    // ---- normal serpentine + DETECT when tree is ahead ----
    bool approachingTreeFromLeft  = (gy == TREE_Y && goingRight && (gx + 1 == TREE_X));
    bool approachingTreeFromRight = (gy == TREE_Y && !goingRight && (gx - 1 == TREE_X));

    if (approachingTreeFromLeft || approachingTreeFromRight) {
      // Fixed 4-step U-shape, mirrored by direction
      detourLen       = 4;
      detourStepIndex = 0;

      if (approachingTreeFromLeft) {
        // coming from left, goingRight
        detourDX[0] =  0; detourDY[0] = +1; // up
        detourDX[1] = +1; detourDY[1] =  0; // above tree
        detourDX[2] = +1; detourDY[2] =  0; // above neighbour
        detourDX[3] =  0; detourDY[3] = -1; // down
      } else {
        // coming from right, goingLeft
        detourDX[0] =  0; detourDY[0] = +1; // up
        detourDX[1] = -1; detourDY[1] =  0; // above tree
        detourDX[2] = -1; detourDY[2] =  0; // above neighbour
        detourDX[3] =  0; detourDY[3] = -1; // down
      }

      detourActive = true;

      // apply first step immediately this loop
      gx += detourDX[0];
      gy += detourDY[0];
      if (gx < 0)    gx = 0;
      if (gx > maxGX) gx = maxGX;
      if (gy < 0)    gy = 0;
      if (gy > maxGY) gy = maxGY;

      detourStepIndex = 1; // next loop will do step 1
    } else {
      // ---- pure serpentine, 1 cell per loop ----
      if (goingRight) {
        gx++;
        if (gx > maxGX) {
          gx = maxGX;
          gy++;
          goingRight = false;
        }
      } else {
        gx--;
        if (gx < 0) {
          gx = 0;
          gy++;
          goingRight = true;
        }
      }
      if (gy > maxGY) gy = maxGY;
    }
  }

  // convert grid cell → world coordinates (1 cell = 1 m)
  x = static_cast<double>(gx);
  y = static_cast<double>(gy);

  // Battery drain based on movement
  {
    double dx = x - prevX;
    double dy = y - prevY;
    double distance = std::sqrt(dx*dx + dy*dy);
    battery.updateByDistance(distance);
  }

  // ---------------------------------------------------
  // 5) Generic low-battery auto-dock (for final run)
  //     (independent of mid-run sequence)
  // ---------------------------------------------------
  if (battery.distanceSOC <= 30.0 && !g_midRunGoingHome &&
      !g_midRunCharging && !g_midRunGoingBack) {

    std::cout << "\nLOW BATTERY → Returning to HOME (0,0)...\n";

    // Move robot toward home
    x -= (x * 0.1);
    y -= (y * 0.1);

    double dx = x - prevX;
    double dy = y - prevY;
    double distance = std::sqrt(dx*dx + dy*dy);
    battery.updateByDistance(distance);

    if (x < 0.5 && y < 0.5) {
      x = 0;
      y = 0;
      std::cout << "Docked at HOME.\n";

      drawAsciiMap(x, y);

      // Optional external position write
      writeTelemetry(x, y);


      usleep(200000);
      advanceSimTime();
      return;
    }
  }

  // Draw ASCII grid + coverage + battery
  drawAsciiMap(x, y);

  // ---------------------------------------------------
  // 6) Trigger mid-run behaviour once at ~50% coverage
  // ---------------------------------------------------
  if (!g_midRunUsed) {
    double cov = getCoveragePercent();
    if (cov >= 50.0 && cov < 100.0) {
      g_midRunUsed      = true;
      g_midRunGoingHome = true;
      g_midRunCharging  = false;
      g_midRunGoingBack = false;

      g_midRunResumeX = x;
      g_midRunResumeY = y;

      battery.distanceSOC = 0.0;  // visually drop to 0%

      std::cout << "[Mid-run] Hit ~50% coverage → forcing dock + recharge.\n";
    }
  }

  // After drawing, check if we just completed full coverage
  if (!g_coverageDone && isCoverageDone()) {
    std::cout << "COVERAGE COMPLETE — scheduling return to dock.\n";
    g_coverageDone = true;
  }

  // Output position for external visualizers (optional)
  {
    writeTelemetry(x, y);
  }

  // show simulated time + day
  std::cout << "Day " << simDay << " | Sim time: "
            << std::setfill('0') << std::setw(2) << hh
            << ":" << std::setw(2) << mm << std::setfill(' ') << "\n";

  // Slow down simulation (same feel as before, but a bit slower)
  usleep(300000);

  // advance simulated clock
  advanceSimTime();
}
