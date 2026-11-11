CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
EMBED     = src/twolib_data.inc
SRC       = src/main.cpp src/lexer.cpp src/parser.cpp src/image.cpp src/gen.cpp src/genexpr.cpp src/runtime.cpp
BIN       = 2oLang

all: $(BIN)

# Thư viện chuẩn (Lib/*.2o) được nhúng vào chính trình biên dịch
$(EMBED): $(wildcard Lib/*.2o) tools/embed.cpp
	$(CXX) -std=c++17 -O1 -o tools/embed.bin tools/embed.cpp
	./tools/embed.bin Lib $(EMBED)
	rm -f tools/embed.bin

$(BIN): $(SRC) $(EMBED) $(wildcard src/*.hpp) $(wildcard src/*.inc)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# Biên dịch tĩnh (dùng cho Alpine/musl và để chạy ở mọi nơi)
static: $(SRC) $(EMBED) $(wildcard src/*.hpp) $(wildcard src/*.inc)
	$(CXX) $(CXXFLAGS) -static -o $(BIN) $(SRC)

test: $(BIN)
	./tests/run.sh

clean:
	rm -rf $(BIN) tests/out $(EMBED) tools/embed.bin

.PHONY: all static test clean
