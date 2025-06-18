# Makefile for File Watcher
# Production-ready C++ file monitoring tool

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2 -DNDEBUG
DEBUG_FLAGS = -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG
LDFLAGS = -pthread

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INSTALL_DIR = /usr/local/bin

# Files
TARGET = filewatcher
SOURCES = filewatcher.cpp
HEADERS = filewatcher.h
OBJECTS = $(BUILD_DIR)/filewatcher.o

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: CXXFLAGS += -DNDEBUG -O2
release: $(BIN_DIR)/$(TARGET)

# Debug build
.PHONY: debug
debug: CXXFLAGS = $(DEBUG_FLAGS)
debug: $(BIN_DIR)/$(TARGET)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile object file
$(BUILD_DIR)/%.o: %.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(BIN_DIR)/$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	@echo "Built $(TARGET) successfully!"

# Install to system
.PHONY: install
install: release
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Installation requires root privileges. Please run with sudo."; \
		exit 1; \
	fi
	cp $(BIN_DIR)/$(TARGET) $(INSTALL_DIR)/
	chmod 755 $(INSTALL_DIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(INSTALL_DIR)/"

# Uninstall from system
.PHONY: uninstall
uninstall:
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Uninstallation requires root privileges. Please run with sudo."; \
		exit 1; \
	fi
	rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "Uninstalled $(TARGET) from $(INSTALL_DIR)/"

# Clean build files
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Cleaned build files"

# Run tests
.PHONY: test
test: debug
	@echo "Running basic functionality tests..."
	@mkdir -p test_dir
	@$(BIN_DIR)/$(TARGET) --help
	@echo "Test directory created: test_dir"
	@echo "You can now test the file watcher with: $(BIN_DIR)/$(TARGET) test_dir"

# Package for distribution
.PHONY: package
package: clean release
	@echo "Creating distribution package..."
	@mkdir -p dist/filewatcher-1.0
	@cp $(BIN_DIR)/$(TARGET) dist/filewatcher-1.0/
	@cp $(SOURCES) dist/filewatcher-1.0/
	@cp $(HEADERS) dist/filewatcher-1.0/
	@cp Makefile dist/filewatcher-1.0/
	@echo "#!/bin/bash" > dist/filewatcher-1.0/install.sh
	@echo "sudo cp filewatcher /usr/local/bin/" >> dist/filewatcher-1.0/install.sh
	@echo "sudo chmod 755 /usr/local/bin/filewatcher" >> dist/filewatcher-1.0/install.sh
	@echo "echo 'File Watcher installed successfully!'" >> dist/filewatcher-1.0/install.sh
	@chmod +x dist/filewatcher-1.0/install.sh
	@cd dist && tar -czf filewatcher-1.0.tar.gz filewatcher-1.0/
	@echo "Package created: dist/filewatcher-1.0.tar.gz"

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all       - Build release version (default)"
	@echo "  release   - Build optimized release version"
	@echo "  debug     - Build debug version with symbols"
	@echo "  install   - Install to system (requires sudo)"
	@echo "  uninstall - Remove from system (requires sudo)"
	@echo "  clean     - Remove build files"
	@echo "  test      - Run basic tests"
	@echo "  package   - Create distribution package"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make release"
	@echo "  make debug"
	@echo "  sudo make install"
	@echo "  make clean"

# Dependencies
$(OBJECTS): $(SOURCES) $(HEADERS)
