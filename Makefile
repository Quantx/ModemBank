CXX = gcc
CFLAGS = -g -rdynamic -Wall
COMMON_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/common/*.c))
CLIENT_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/client/*.c))
SERVER_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/server/*.c))

.PHONY: client server build_client build_server bdirs clean
all: client server

build/%.o: src/%.c
	$(CXX) -c $(CFLAGS) $< -o $@

bdirs:
	mkdir -p bin
	mkdir -p build/common/
	mkdir -p build/client/
	mkdir -p build/server/

compile_client: $(COMMON_FILES) $(CLIENT_FILES)
	$(CXX) -o bin/client $^

compile_server: $(COMMON_FILES) $(SERVER_FILES)
	$(CXX) -o bin/server $^

client: bdirs compile_client

server: bdirs compile_server

clean:
	rm -rf build/
	rm -rf bin/
