# Linux System Monitoring Tool Makefile
# Optimized for Linux systems

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -D_GNU_SOURCE
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -O0 -D_GNU_SOURCE -DDEBUG
LDFLAGS = -lm

# Directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Source and target
SRCFILE = main.c
TARGET = sysmon
DEBUG_TARGET = sysmon_debug

# Default target
all: $(TARGET)

# Build optimized version
$(TARGET): $(SRCFILE)
	@echo "Building optimized system monitoring tool..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCFILE) $(LDFLAGS)
	@echo "Build complete. Run './$(TARGET) -h' for usage information."

# Build debug version
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SRCFILE)
	@echo "Building debug version..."
	$(CC) $(DEBUG_CFLAGS) -o $(DEBUG_TARGET) $(SRCFILE) $(LDFLAGS)
	@echo "Debug build complete."

# Install the binary
install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/
	@echo "Installation complete. You can now run 'sysmon' from anywhere."

# Uninstall the binary
uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstallation complete."

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(TARGET) $(DEBUG_TARGET)
	@echo "Clean complete."

# Test the build (basic functionality check)
test: $(TARGET)
	@echo "Running basic functionality test..."
	@./$(TARGET) -o || (echo "Test failed!" && exit 1)
	@echo "Basic test passed."

# Show help for Makefile targets
help:
	@echo "Linux System Monitoring Tool - Makefile Help"
	@echo "=============================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all         - Build optimized version (default)"
	@echo "  debug       - Build debug version with symbols"
	@echo "  install     - Install to $(BINDIR)"
	@echo "  uninstall   - Remove from $(BINDIR)"
	@echo "  clean       - Remove build artifacts"
	@echo "  test        - Run basic functionality test"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                    # Build optimized version"
	@echo "  make debug              # Build debug version"
	@echo "  sudo make install       # Install system-wide"
	@echo "  make test               # Test the built binary"
	@echo "  make clean              # Clean up build files"
	@echo ""
	@echo "Tool usage:"
	@echo "  ./$(TARGET) -h          # Show tool help"
	@echo "  ./$(TARGET)             # Monitor all resources"
	@echo "  ./$(TARGET) -c -r 2     # Monitor CPU with 2s refresh"

# Force rebuild
rebuild: clean all

# Static analysis (if available)
lint:
	@command -v cppcheck >/dev/null 2>&1 && \
		echo "Running static analysis..." && \
		cppcheck --enable=all --std=c99 $(SRCFILE) || \
		echo "cppcheck not available, skipping static analysis"

# Performance optimized build
performance: CFLAGS += -O3 -march=native -flto
performance: $(TARGET)
	@echo "Performance-optimized build complete."

# Check system compatibility
check-system:
	@echo "Checking system compatibility..."
	@test -r /proc/stat || (echo "ERROR: /proc/stat not readable" && exit 1)
	@test -r /proc/meminfo || (echo "ERROR: /proc/meminfo not readable" && exit 1)
	@echo "System compatibility check passed."

# Show build information
info:
	@echo "Build Information:"
	@echo "=================="
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Source: $(SRCFILE)"
	@echo "Target: $(TARGET)"
	@echo "Install prefix: $(PREFIX)"

.PHONY: all debug install uninstall clean test help rebuild lint performance check-system info 