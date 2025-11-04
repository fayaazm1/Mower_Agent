# Mower Agent – Phase 1
This repository contains the Phase 1 implementation of the AI Mower Agent project, developed as part of the UMBC AI Agent Computing course.

## 📘 Overview
Mower Agent is an autonomous robotic mower simulation that uses GPS and sensor-based localization to navigate within a predefined map.  
It is built on top of open-source robotic frameworks but refactored, cleaned, and customized for research and educational demonstration.

## ⚙️ Features
- Simulated real-world or virtual environment deployment
- Battery and GPS tracking
- Obstacle detection and avoidance
- ROS-compatible communication structure
- Logging and console output for position and status

## 🧠 Purpose
This project demonstrates agent-based control and navigation logic, focusing on:
- Perception (GPS, sensor state)
- Decision-making (navigation, target following)
- Action (motor control and environment updates)

## 🚀 How to Run (Linux)
1. Clone this repository  
   ```bash
   git clone https://github.com/fayaazm1/Mower_Agent.git
   cd Mower_Agent/linux
   ```
2. Configure build options in `config.h`.
3. Build the executable:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   sudo ./mower
   ```

## 📈 Phase 1 Deliverables
- Agent Development and Deployment
- Feature Demonstration
- Code Analysis and Future Plan

## �� Author
**Fayaaz M**  
Graduate Student, University of Maryland, Baltimore County (UMBC)


