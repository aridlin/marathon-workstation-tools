CXX ?= g++
CXXFLAGS ?= -O2 -pipe
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic
PKGS = gtk+-3.0 json-glib-1.0

.PHONY: all clean install test

all: workspace-display-manager

workspace-display-manager: main.cpp
	$(CXX) $(CXXFLAGS) $(shell pkg-config --cflags $(PKGS)) $< -o $@ $(shell pkg-config --libs $(PKGS))

test: workspace-display-manager
	./test-partitions.sh ./workspace-display-manager

install: workspace-display-manager
	install -Dm755 workspace-display-manager $(HOME)/.local/bin/workspace-display-manager

clean:
	rm -f workspace-display-manager
