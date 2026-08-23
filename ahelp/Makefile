CXX ?= g++
CXXFLAGS ?= -O2 -pipe
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean install test

all: ahelp

ahelp: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

test: export AHELP_CONFIG := $(CURDIR)/commands.tsv
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
	./ahelp --plain --no-pager > .ahelp-order-test.txt
	./ahelp -i --plain > .ahelp-order-interactive-test.txt
	awk '/DYNAMIC DISCOVERY/{dynamic=NR} /CUSTOM-BUILT UTILITIES/{custom=NR} /STANDARD \/ EXTERNAL COMMANDS/{standard=NR} END { exit dynamic < custom && custom < standard ? 0 : 1 }' .ahelp-order-test.txt
	awk '/STANDARD \/ EXTERNAL COMMANDS/{standard=NR} /CUSTOM-BUILT UTILITIES/{custom=NR} /DYNAMIC DISCOVERY/{dynamic=NR} END { exit standard < custom && custom < dynamic ? 0 : 1 }' .ahelp-order-interactive-test.txt
	! rg -q 'OpenAI Codex terminal agent|FILES, PACKAGES & PROCESSES' main.cpp
	rm -f .ahelp-workspace-test.txt .ahelp-standard-test.txt .ahelp-custom-test.txt .ahelp-full-test.txt .ahelp-grid-1-test.txt .ahelp-grid-2-test.txt .ahelp-grid-3-test.txt .ahelp-order-test.txt .ahelp-order-interactive-test.txt

install: ahelp
	install -Dm755 ahelp $(HOME)/.local/bin/ahelp
	install -Dm644 commands.tsv $(HOME)/.config/ahelp/commands.tsv

clean:
	rm -f ahelp
