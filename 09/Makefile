
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -pedantic


OUTPUT_DIR = bin
EXECUTABLE = $(OUTPUT_DIR)/aoc-09

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:%.cpp=$(OUTPUT_DIR)/%.o)

all: build


run: build
	./$(EXECUTABLE)

build: $(EXECUTABLE)


$(OUTPUT_DIR):
	@mkdir -p $(OUTPUT_DIR)

$(OUTPUT_DIR)/%.o:  %.cpp | $(OUTPUT_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean up generated files
clean:
	rm -rf $(OUTPUT_DIR)
