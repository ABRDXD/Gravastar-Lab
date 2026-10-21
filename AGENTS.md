# GravastarLab constraints

All application code, scientific calculations, independent references, tests,
benchmarks, validation utilities, and development tools must be C++20.
CMake is permitted only for build configuration, dependency integration and
CTest registration. Do not introduce Python, shell scripts, JavaScript,
Rust, or another implementation/automation language. JSON and Markdown are
data and documentation. Direct invocations of compiled build tools are allowed.

Do not delete a legacy Python file until its replacement builds and passes
equivalent CTest coverage. Record the migration evidence. If a capability
cannot be implemented within the language constraint, explain the blocker.

Keep the independent project self-contained. Preserve unknown concurrent
changes; coordinate before modifying another contributor's work. Work on an
isolated branch when others are modifying the repository. Scientific reports
must distinguish implementation consistency, energy conditions, and stability.
Never describe project-selected radii as recovered paper parameters. Do not
start P2 optics until blocking P1 findings are resolved and its chart coverage
is specified.
