CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I src

# Find all test source files
TEST_SRCS = $(wildcard test/test_*.cpp)
# Generate test binary names (strip .cpp)
TEST_BINS = $(TEST_SRCS:.cpp=)

.PHONY: all tests clean run-tests

all: tests

tests: $(TEST_BINS)

# Pattern rule for building test binaries
test/%: test/%.cpp src/*.hpp src/*.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Run all tests
run-tests: tests
	@echo "Running all tests..."
	@for test in $(TEST_BINS); do \
		echo "\n=== Running $$test ==="; \
		./$$test || exit 1; \
	done
	@echo "\n=== All tests passed ==="

clean:
	rm -f $(TEST_BINS)
