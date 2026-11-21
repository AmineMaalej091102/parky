# Project: Parky (Portfolio Revival)
- **Domain:** Desktop Parking & Services Management System (Roles: Admin, Parking Agent, Citizen).
- **Features:** CRUD users/parkings/agents, agent-parking assignment, real-time spot status, parking services, reservations, reviews, complaints (éclamations), + bonus (billing/tariffs, occupancy stats, multi-criteria search/filter).
- **Tech Stack:** C, GTK+ 2, Glade 2.12.2 (legacy 2007 UI builder).
- **Environment Constraints (STRICT):**
  - Host: Windows + WSL2 + Docker Desktop (Ubuntu 22.04 dual boot must remain untouched).
  - NEVER suggest VMware, VirtualBox, Vagrant, or a new WSL distro.
  - Run/build inside Docker (ubuntu:18.04 + Glade 2.12.2 compiled from source + libgtk2.0-dev + gcc) via WSLg GUI.
- **Git & Release Roadmap:**
  - Fresh clean GitHub repo (main, dev, eat/*). Conventional Commits (eat:, efactor:, docs:, chore:).
  - **v1.0.0:** Authentic legacy C/GTK2 code, .txt ASCII persistence, static arrays / O(n) search, clean folder structure, Dockerized, recruiter-ready README.
  - **v2.0.0:** Refactored modular C, binary persistence (.dat via write/read), dynamic data structures (linked lists/hash), memory safety (malloc/ree).
