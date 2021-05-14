CXX = gcc
CFLAGS = -g -rdynamic -Wall
LIBS = -lsctp
CENTRAL_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/central/*.c))
WORKER_FILES  = $(patsubst src/%.c,build/%.o,$(wildcard src/worker/*.c))
COMMON_FILES  = $(patsubst src/%.c,build/%.o,$(wildcard src/common/*.c))
SECPIPE_FILES = $(patsubst src/%.c,build/%.o,$(wildcard src/secpipe/*.c))
SECPIPE_DAEMON = src/secpipe/daemon/secpipe.c

.PHONY: all clean make_central make_worker

all: make_central make_worker secpipe

make_central: build central

make_worker: build worker

clean:
	rm -rf build/
	rm central
	rm worker
	rm secpipe

build/%.o: src/%.c
	$(CXX) -c $(CFLAGS) $< -o $@

build:
	mkdir -p build/central/
	mkdir -p build/worker/
	mkdir -p build/common/
	mkdir -p build/secpipe/

central: $(CENTRAL_FILES) $(COMMON_FILES) $(SECPIPE_FILES)
	$(CXX) $^ $(LIBS) -o central

worker: $(WORKER_FILES) $(COMMON_FILES) $(SECPIPE_FILES)
	$(CXX) $^ $(LIBS) -o worker

secpipe: $(SECPIPE_DAEMON)
	$(CXX) $^ $(LIBS) -o secpipe
