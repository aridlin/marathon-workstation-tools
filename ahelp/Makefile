CXX ?= g++
CXXFLAGS ?= -O2 -pipe
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean install test

all: ahelp

ahelp: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

test: ahelp
	./ahelp --plain --no-pager workspace > /tmp/ahelp-workspace-test.txt
	rg -q 'workspace-display-manager' /tmp/ahelp-workspace-test.txt
	rg -q 'workspace-field' /tmp/ahelp-workspace-test.txt
	./ahelp --plain --no-pager cp1 > /tmp/ahelp-cp1-test.txt
	rg -q 'cp1-notif' /tmp/ahelp-cp1-test.txt
	./ahelp --plain --no-pager overcalc > /tmp/ahelp-overcalc-test.txt
	rg -q 'overcalc' /tmp/ahelp-overcalc-test.txt
	rm -f /tmp/ahelp-workspace-test.txt /tmp/ahelp-cp1-test.txt /tmp/ahelp-overcalc-test.txt

install: ahelp
	install -Dm755 ahelp $(HOME)/.local/bin/ahelp

clean:
	rm -f ahelp
