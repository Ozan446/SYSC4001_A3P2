# SYSC 4001 A - Operating Systems | Fall 2025
# Carleton University

# Assignment 3 - Part II: Concurrent TA Processes
# Makefile for C++ IPC Implementation

# ============================================================

# Use C++ Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

# Directory Definitions
SRC_DIR = src
BIN_DIR = bin

# Student ID Naming Convention
STUDENT_IDS = 101322055_101310590

# Source and Binary Definitions
PART2A_SRC = $(SRC_DIR)/part2a_$(STUDENT_IDS).cpp
PART2B_SRC = $(SRC_DIR)/part2b_$(STUDENT_IDS).cpp

PART2A_BIN = $(BIN_DIR)/part2a
PART2B_BIN = $(BIN_DIR)/part2b

# Default argument for concurrent processes (can be overridden: make run_a X=4)
X ?= 3

# ============================================================

.PHONY: all clean run_a run_b

# Default target: builds both executables
all: $(BIN_DIR) $(PART2A_BIN) $(PART2B_BIN)

# Creates the output directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compilation Rule for Part 2.a (Unsynchronized)
$(PART2A_BIN): $(PART2A_SRC)
	$(CXX) $(CXXFLAGS) $< -o $@

# Compilation Rule for Part 2.b (Synchronized)
$(PART2B_BIN): $(PART2B_SRC)
	$(CXX) $(CXXFLAGS) $< -o $@

# Run Part 2.a (Unsynchronized)
# Usage: make run_a X=4
run_a: all
	@echo "--- Running Part 2.a with $(X) concurrent TA processes ---"
	./$(PART2A_BIN) $(X)

# Run Part 2.b (Synchronized)
# Usage: make run_b X=4
run_b: all
	@echo "--- Running Part 2.b with $(X) concurrent TA processes ---"
	./$(PART2B_BIN) $(X)

# Clean target: removes the binary directory
clean:
	@echo "Cleaning compiled binaries..."
	rm -rf $(BIN_DIR)
