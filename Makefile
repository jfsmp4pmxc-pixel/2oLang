CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC       = src/main.cpp src/lexer.cpp src/parser.cpp src/image.cpp src/gen.cpp src/genexpr.cpp src/runtime.cpp
BIN       = 2oLang

all: $(BIN)

$(BIN): $(SRC) $(wildcard src/*.hpp) $(wildcard src/*.inc)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# Biên dịch tĩnh (dùng cho Alpine/musl và để chạy ở mọi nơi)
static: $(SRC) $(wildcard src/*.hpp) $(wildcard src/*.inc)
	$(CXX) $(CXXFLAGS) -static -o $(BIN) $(SRC)

test: $(BIN)
	./tests/run.sh

clean:
	rm -rf $(BIN) tests/out

.PHONY: all static test clean
