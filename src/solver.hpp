#pragma once
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <cstdio>
#include <array>
#include <memory>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include "variable_grid.hpp"  // for ClauseList, Clause

enum class SolverStatus {
    SAT,
    UNSAT,
    ERROR
};

struct SolverResult {
    SolverStatus status;
    std::set<int> solution;  // set of true literals (positive = true, negative = false)
    std::string error_message;
};

// Convert clauses to DIMACS format string
inline std::string make_dimacs_string(const ClauseList& clauses, int num_variables) {
    std::ostringstream oss;
    oss << "p cnf " << num_variables << " " << clauses.size() << "\n";
    for (const auto& clause : clauses) {
        for (int lit : clause) {
            oss << lit << " ";
        }
        oss << "0\n";
    }
    return oss.str();
}

// Parse DIMACS output from solver
inline SolverResult parse_dimacs_output(const std::string& output) {
    SolverResult result;
    result.status = SolverStatus::ERROR;

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        if (line[0] == 's') {
            // Status line: "s SATISFIABLE" or "s UNSATISFIABLE"
            if (line.find("SATISFIABLE") != std::string::npos &&
                line.find("UNSATISFIABLE") == std::string::npos) {
                result.status = SolverStatus::SAT;
            } else if (line.find("UNSATISFIABLE") != std::string::npos) {
                result.status = SolverStatus::UNSAT;
            }
        } else if (line[0] == 'v') {
            // Variable line: "v 1 -2 3 -4 0"
            std::istringstream vss(line.substr(2));  // skip "v "
            int lit;
            while (vss >> lit) {
                if (lit != 0) {
                    result.solution.insert(lit);
                }
            }
        }
    }

    return result;
}

// Call the SAT solver with the given DIMACS string
// solver_name: name of solver executable in the solvers/ directory
// solver_path: optional full path to solver (overrides solver_name)
inline SolverResult call_solver(const std::string& dimacs_string,
                                const std::string& solver_name = "kissat",
                                const std::string& solver_path = "") {
    SolverResult result;
    result.status = SolverStatus::ERROR;

    // Determine solver path
    std::string full_path = solver_path;
    if (full_path.empty()) {
        // Look for solver in solvers/ directory relative to executable
        // For now, use a hardcoded relative path
        full_path = "solvers/" + solver_name;
    }

    // Write DIMACS to a temp file to avoid pipe buffer deadlock
    char temp_filename[] = "/tmp/sat_input_XXXXXX";
    int temp_fd = mkstemp(temp_filename);
    if (temp_fd < 0) {
        result.error_message = "Failed to create temp file";
        return result;
    }
    write(temp_fd, dimacs_string.c_str(), dimacs_string.size());
    close(temp_fd);

    // Create pipe for stdout
    int stdout_pipe[2];
    if (pipe(stdout_pipe) < 0) {
        unlink(temp_filename);
        result.error_message = "Failed to create pipe";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        unlink(temp_filename);
        result.error_message = "Failed to fork";
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);  // Close read end of stdout pipe

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);

        execlp(full_path.c_str(), solver_name.c_str(), "--quiet", temp_filename, nullptr);

        // If exec fails
        _exit(1);
    }

    // Parent process
    close(stdout_pipe[1]);  // Close write end of stdout pipe

    // Read solver's stdout
    std::string output;
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    close(stdout_pipe[0]);

    // Wait for child to finish
    int status;
    waitpid(pid, &status, 0);

    // Clean up temp file
    unlink(temp_filename);

    // Parse output
    result = parse_dimacs_output(output);

    if (result.status == SolverStatus::ERROR && result.error_message.empty()) {
        result.error_message = "Failed to parse solver output. Got: " + output.substr(0, 200);
    }

    return result;
}

// Convenience function: solve clauses directly
inline SolverResult solve(const ClauseList& clauses,
                          int num_variables,
                          const std::string& solver_name = "kissat") {
    std::string dimacs = make_dimacs_string(clauses, num_variables);
    return call_solver(dimacs, solver_name);
}
