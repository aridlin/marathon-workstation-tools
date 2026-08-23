CXX ?= g++
CXXFLAGS ?= -O2 -pipe
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean install test

all: ahelp

ahelp: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

test: ahelp
	./ahelp --plain --no-pager workspace > .ahelp-workspace-test.txt
	rg -q 'CUSTOM-BUILT UTILITIES' .ahelp-workspace-test.txt
	rg -q 'Choose display order' .ahelp-workspace-test.txt
	rg -q 'workspace-field' .ahelp-workspace-test.txt
	./ahelp --plain --no-pager flameshot > .ahelp-standard-test.txt
	rg -q 'STANDARD / EXTERNAL COMMANDS' .ahelp-standard-test.txt
	rg -q 'DESKTOP & CAPTURE' .ahelp-standard-test.txt
	rg -q 'flameshot' .ahelp-standard-test.txt
	./ahelp --plain --no-pager wemote > .ahelp-custom-test.txt
	rg -q 'CUSTOM-BUILT UTILITIES' .ahelp-custom-test.txt
	rg -q 'TERMINAL & CONTENT' .ahelp-custom-test.txt
	rg -q 'wemote' .ahelp-custom-test.txt
	./ahelp --plain --no-pager > .ahelp-full-test.txt
	! rg -q 'proliant-|rat-quest-|__pycache__| pn | pnpx | pnx ' .ahelp-full-test.txt
	COLUMNS=50 ./ahelp --plain --no-pager 'files, packages & processes' > .ahelp-grid-1-test.txt
	COLUMNS=90 ./ahelp --plain --no-pager 'files, packages & processes' > .ahelp-grid-2-test.txt
	COLUMNS=130 ./ahelp --plain --no-pager 'files, packages & processes' > .ahelp-grid-3-test.txt
	awk '{ line=$$0; count=gsub(/╭─/, "", line); if (count > max) max=count } END { exit max == 1 ? 0 : 1 }' .ahelp-grid-1-test.txt
	awk '{ line=$$0; count=gsub(/╭─/, "", line); if (count > max) max=count } END { exit max == 2 ? 0 : 1 }' .ahelp-grid-2-test.txt
	awk '{ line=$$0; count=gsub(/╭─/, "", line); if (count > max) max=count } END { exit max == 3 ? 0 : 1 }' .ahelp-grid-3-test.txt
	! rg -q 'COMMAND.*DESCRIPTION' .ahelp-grid-3-test.txt
	rm -f .ahelp-workspace-test.txt .ahelp-standard-test.txt .ahelp-custom-test.txt .ahelp-full-test.txt .ahelp-grid-1-test.txt .ahelp-grid-2-test.txt .ahelp-grid-3-test.txt

install: ahelp
	install -Dm755 ahelp $(HOME)/.local/bin/ahelp

clean:
	rm -f ahelp
