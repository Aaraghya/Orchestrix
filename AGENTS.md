# Orchestrix — Agent Instructions

## Project overview
Orchestrix is a C++17 command-line discrete-event simulator for adaptive data-center resource allocation.

## Development principles
- Read the project documentation before implementing features.
- Keep the architecture modular and understandable for a second-year CSE student.
- Use the C++ standard library wherever practical.
- Apply OOP principles where they serve a clear purpose.
- Do not implement the entire project in one pass.
- Never silently change agreed interfaces or simulation rules.
- Preserve existing files and user work.
- Add tests for meaningful behavior and edge cases.
- Build and run tests after each milestone.
- Never claim tests passed without actually running them.
- Document assumptions and known limitations.

## Implementation sequence
1. Domain models and resource accounting.
2. Allocation strategies and scheduler.
3. Discrete simulation clock and job lifecycle.
4. CLI and event logging.
5. Metrics, strategy comparison and report export.
6. Optional failure recovery and energy-aware scheduling.

## Constraints
- Use C++17 and CMake.
- Avoid unnecessary external dependencies.
- Do not add a GUI, database, networking layer or machine learning.
- Keep changes focused and reviewable.
- Do not implement optional features before the core simulation works.

## Completion criteria
A milestone is complete only when it builds, its tests pass, and its behavior matches the documented requirements.