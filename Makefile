CXX = gcc
CFLAGS = -g -rdynamic -Wall
COMMON_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/common/*.c))
CLIENT_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/client/*.c))
SERVER_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/server/*.c))

.PHONY: client server bdirs clean
all: client server

build/%.o: src/%.c
	$(CXX) -c $(CFLAGS) $< -o $@

bdirs:
	mkdir -p bin
	mkdir -p build/common/
	mkdir -p build/client/
	mkdir -p build/server/

client: $(COMMON_FILES) $(CLIENT_FILES) | bdirs
	$(CXX) -o bin/client $^

server: $(COMMON_FILES) $(SERVER_FILES) | bdirs
	$(CXX) -o bin/server $^

clean:
	rm -rf build/
