CXX ?= g++
CXXFLAGS ?= -O2 -pipe
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic
PKGS = gtk+-3.0 json-glib-1.0

.PHONY: all clean install

all: workspace-field

workspace-field: main.cpp
	$(CXX) $(CXXFLAGS) $(shell pkg-config --cflags $(PKGS)) $< -o $@ $(shell pkg-config --libs $(PKGS))

install: workspace-field
	install -Dm755 workspace-field $(HOME)/.local/bin/workspace-field

clean:
	rm -f workspace-field
