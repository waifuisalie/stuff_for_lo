# Makefile for the TDE 2 SIMPLEX solver.
# Build:  make          (produces ./simplex)
# Clean:  make clean

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:.cpp=.o)
BIN      := simplex

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
