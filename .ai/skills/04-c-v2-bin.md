# Skill: C Refactoring — v2.0.0 (Binary & Scalability)
- **Goal:** Modernize architecture and data layer for scalability.
- **Rules:**
  1. Migrate persistence from .txt ASCII parsing to .dat binary serialization (write/read with fixed-size fields/no raw pointers in structs).
  2. Replace static arrays & O(n) file scans with dynamic structures (linked lists / hash index) loaded in memory.
  3. Separate business logic (src/core/) from GTK callbacks (src/ui/). Enforce strict input validation and leak-free malloc/ree.
