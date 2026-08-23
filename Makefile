SUBDIRS := ahelp workspace-field workspace-display-manager

.PHONY: all test install clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

test: all
	$(MAKE) -C ahelp test
	$(MAKE) -C workspace-display-manager test

install: all
	$(MAKE) -C ahelp install
	$(MAKE) -C workspace-field install
	$(MAKE) -C workspace-display-manager install
	install -d $(HOME)/.config/hypr
	install -m755 workspace-display-manager/scripts/*.sh $(HOME)/.config/hypr/
	install -Dm644 workspace-display-manager/workspace-display-manager.svg \
		$(HOME)/.local/share/icons/hicolor/scalable/apps/workspace-display-manager.svg
	install -d $(HOME)/.local/share/applications
	sed 's|@HOME@|$(HOME)|g' workspace-display-manager/workspace-display-manager.desktop.in \
		> $(HOME)/.local/share/applications/workspace-display-manager.desktop
	@printf '\nInstalled user files. Add examples/hyprland.conf to your Hyprland config, then reload.\n'

clean:
	@for directory in $(SUBDIRS); do $(MAKE) -C $$directory clean; done
