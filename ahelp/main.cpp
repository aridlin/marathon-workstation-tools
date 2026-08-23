#include <algorithm>
#include <cerrno>
#include <cctype>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wchar.h>

namespace fs = std::filesystem;

namespace {

constexpr char kSeparator = 0x1f;

struct Entry {
    std::string name;
    std::string description;
    std::string icon;
};

struct CommandSubcategory {
    std::string title;
    std::string icon;
    std::vector<Entry> entries;
};

struct Terminal {
    int width = 96;
    int height = 30;
    bool color = false;
};

const std::unordered_map<std::string, std::string> kFunctionDescriptions = {
    {"cd_up", "Go up N directory levels; used by the cd. alias."},
    {"pls", "Correct the previous command with thefuck."},
    {"random_potato_fact", "Print one random fact from the local potato-facts file."},
    {"swayhelp", "Show the legacy Sway keybinding cheat sheet."},
    {"ytdp", "Download a video into ~/Downloads with yt-dlp."},
    {"ytmp", "Download audio as MP3 into ~/Downloads with yt-dlp."},
};

const std::vector<CommandSubcategory> kStandardCommands = {
    {"AI, DEVELOPMENT & DATA", "󰧑", {
    {"codex", "OpenAI Codex terminal agent.", "󰚩"},
    {"pnpm", "Fast, disk-efficient JavaScript package manager.", ""},
    {"uv", "Fast Python project and package manager.", ""},
    {"uvx", "Run Python tools in disposable uv environments.", "󰏗"},
    {"harlequin", "Terminal SQL IDE and database client.", "󰆼"},
    {"datasette", "Explore and publish SQLite databases through a web interface.", ""},
    }},
    {"DESKTOP & CAPTURE", "󰍹", {
    {"firefox", "Firefox web browser.", "󰈹"},
    {"walker", "Fast Wayland application launcher.", "󰍉"},
    {"ghostty", "GPU-accelerated terminal emulator.", "󰊠"},
    {"flameshot", "Capture, annotate, and copy screenshots.", "󰹑"},
    }},
    {"SYSTEM & CONNECTIVITY", "󰒋", {
    {"btop", "Interactive system and process monitor.", ""},
    {"nmtui", "NetworkManager terminal interface.", "󰤨"},
    {"syncthing", "Peer-to-peer file synchronization service.", "󰓦"},
    }},
    {"GAMES & COMPATIBILITY", "󰊗", {
    {"flashpoint", "Launch the local Flashpoint web-game archive.", "󰈸"},
    {"gameconqueror", "Launch the GameConqueror memory scanner.", "󰮂"},
    {"hyprshade", "Manage Hyprland screen shaders.", "󰖨"},
    }},
};

const std::vector<CommandSubcategory> kCustomCommands = {
    {"TERMINAL & CONTENT", "󰆍", {
    {"ahelp", "Show this polished personal command reference.", "󰋖"},
    {"overcalc", "Evaluate rich math with Unicode, LaTeX, steps, JSON, and derivatives.", "󰃬"},
    {"mdunicode", "Convert Markdown, Discord formatting, LaTeX, and math into Unicode.", "󰗊"},
    {"fckmpeg", "Guided CLI/TUI media converter with terminal previews.", "󰈙"},
    {"wemote", "Search and insert emoji from the terminal.", "󰞅"},
    {"cp1-notif", "Render notifications and print them on a Paperang CP1 thermal printer.", "󰐪"},
    }},
    {"WORKSPACES & WAYLAND", "", {
    {"workspace-field", "Open the fast graphical 10x10 Hyprland workspace field.", "󰆾"},
    {"workspace-display-manager", "Choose display order and main output for the 2D workspace grid.", "󰹑"},
    {"wayfreeze", "Freeze a Wayland screen region for inspection.", "󰜺"},
    {"igpu-watch", "Monitor Intel integrated-GPU activity.", "󰢮"},
    {"hypr-marathon-session", "Start the full Marathon Hyprland login session.", "󰍹"},
    {"hypr-marathon-nested", "Start the Marathon Hyprland profile nested inside this session.", "󰖲"},
    {"waydroid-phone-multitouch.py", "Forward Android multitouch events into a Waydroid touchscreen device.", ""},
    }},
    {"FILES, PACKAGES & PROCESSES", "󰉋", {
    {"wfind", "Interactive recursive file and directory finder.", "󰍉"},
    {"wparu", "Interactive package search, install, and removal frontend for paru.", ""},
    {"wproc", "Interactive process browser and signal sender.", "󰄉"},
    {"wmount", "Interactive removable-volume mount and unmount tool.", "󰋊"},
    {"wzip", "Interactive ZIP archive creator.", "󰿺"},
    {"wtar", "Interactive tar archive creator.", "󰗄"},
    {"wvenv", "Create, enter, and manage a project Python virtual environment.", "󰆧"},
    {"wchmod", "Interactive file-permission picker.", "󰌾"},
    }},
    {"NETWORK", "󰒍", {
    {"wssh", "Choose and connect to hosts from ~/.ssh/config.", "󰣀"},
    {"wserve", "Serve the current directory with a practical browser file interface.", "󰒍"},
    }},
};

const std::set<std::string> kIgnoredDynamicCommands = {
    "__pycache__",
    "pn",
    "pnpx",
    "pnx",
    "proliant-overflow-finalize",
    "proliant-tv-copy",
    "rat-quest-unblock-notify",
};

const std::vector<std::string> kIgnoredDynamicPrefixes = {
    "proliant-",
    "rat-quest-",
};

bool ignored_dynamic_command(const std::string& name) {
    if (kIgnoredDynamicCommands.contains(name)) return true;
    return std::any_of(kIgnoredDynamicPrefixes.begin(), kIgnoredDynamicPrefixes.end(),
                       [&](const std::string& prefix) { return name.starts_with(prefix); });
}

std::string home_directory() {
    if (const char* home = std::getenv("HOME")) return home;
    return ".";
}

int text_width(const std::string& text) {
    std::mbstate_t state{};
    const char* cursor = text.data();
    std::size_t remaining = text.size();
    int width = 0;
    while (remaining > 0) {
        wchar_t character = 0;
        const std::size_t consumed = std::mbrtowc(&character, cursor, remaining, &state);
        if (consumed == static_cast<std::size_t>(-1) || consumed == static_cast<std::size_t>(-2)) {
            ++width;
            ++cursor;
            --remaining;
            state = {};
            continue;
        }
        if (consumed == 0) break;
        const auto codepoint = static_cast<std::uint32_t>(character);
        const bool nerd_glyph = (codepoint >= 0xE000 && codepoint <= 0xF8FF) ||
                                (codepoint >= 0xF0000 && codepoint <= 0xFFFFD) ||
                                (codepoint >= 0x100000 && codepoint <= 0x10FFFD);
        const int cell_width = wcwidth(character);
        // Nerd Font private-use glyphs render as one terminal cell even when
        // locale-specific wcwidth tables report -1 or 2.
        width += nerd_glyph ? 1 : (cell_width < 0 ? 1 : cell_width);
        cursor += consumed;
        remaining -= consumed;
    }
    return width;
}

std::string truncate_to(const std::string& text, int maximum) {
    if (text_width(text) <= maximum) return text;
    if (maximum <= 1) return maximum == 1 ? "…" : "";
    std::mbstate_t state{};
    const char* cursor = text.data();
    std::size_t remaining = text.size();
    std::string output;
    int width = 0;
    while (remaining > 0) {
        wchar_t character = 0;
        const std::size_t consumed = std::mbrtowc(&character, cursor, remaining, &state);
        const std::size_t safe_consumed =
            consumed == static_cast<std::size_t>(-1) || consumed == static_cast<std::size_t>(-2)
                ? 1
                : consumed;
        if (safe_consumed == 0) break;
        int cells = 1;
        if (consumed != static_cast<std::size_t>(-1) && consumed != static_cast<std::size_t>(-2)) {
            const auto codepoint = static_cast<std::uint32_t>(character);
            const bool nerd_glyph = (codepoint >= 0xE000 && codepoint <= 0xF8FF) ||
                                    (codepoint >= 0xF0000 && codepoint <= 0xFFFFD) ||
                                    (codepoint >= 0x100000 && codepoint <= 0x10FFFD);
            const int measured = wcwidth(character);
            cells = nerd_glyph ? 1 : (measured < 0 ? 1 : measured);
        } else {
            state = {};
        }
        if (width + cells > maximum - 1) break;
        output.append(cursor, safe_consumed);
        width += cells;
        cursor += safe_consumed;
        remaining -= safe_consumed;
    }
    return output + "…";
}

std::string pad_right(const std::string& text, int width) {
    const std::string fitted = truncate_to(text, width);
    return fitted + std::string(static_cast<std::size_t>(std::max(0, width - text_width(fitted))), ' ');
}

std::vector<std::string> wrap_text(const std::string& text, int width) {
    if (width < 4) return {truncate_to(text, width)};
    std::istringstream input(text);
    std::vector<std::string> lines;
    std::string word;
    std::string current;
    while (input >> word) {
        if (text_width(word) > width) {
            if (!current.empty()) {
                lines.push_back(current);
                current.clear();
            }
            while (text_width(word) > width) {
                const std::string part = truncate_to(word, width);
                const std::string without_ellipsis = part.size() >= 3
                                                         ? part.substr(0, part.size() - 3)
                                                         : part;
                lines.push_back(without_ellipsis);
                word.erase(0, without_ellipsis.size());
                if (without_ellipsis.empty()) break;
            }
        }
        if (current.empty()) current = word;
        else if (text_width(current) + 1 + text_width(word) <= width) current += " " + word;
        else {
            lines.push_back(current);
            current = word;
        }
    }
    if (!current.empty()) lines.push_back(current);
    if (lines.empty()) lines.emplace_back();
    return lines;
}

std::string repeat(const std::string& glyph, int count) {
    std::string result;
    for (int i = 0; i < count; ++i) result += glyph;
    return result;
}

Terminal terminal_info(bool plain) {
    Terminal terminal;
    winsize size{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
        if (size.ws_col > 0) terminal.width = size.ws_col;
        if (size.ws_row > 0) terminal.height = size.ws_row;
    } else if (const char* columns = std::getenv("COLUMNS")) {
        terminal.width = std::max(40, std::atoi(columns));
    }
    terminal.width = std::clamp(terminal.width, 42, 320);
    const char* term = std::getenv("TERM");
    terminal.color = !plain && isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR") &&
                     (!term || std::strcmp(term, "dumb") != 0);
    return terminal;
}

struct Paint {
    bool enabled = false;

    std::string code(const char* value) const { return enabled ? std::string("\033[") + value + 'm' : ""; }
    std::string reset() const { return code("0"); }
    std::string border(const std::string& text) const { return code("38;2;63;143;97") + text + reset(); }
    std::string title(const std::string& text) const { return code("1;38;2;112;194;139") + text + reset(); }
    std::string subtle(const std::string& text) const { return code("38;2;118;172;132") + text + reset(); }
    std::string warning(const std::string& text) const { return code("1;38;2;255;113;140") + text + reset(); }

    std::string cell(const std::string& text, int width, bool name, bool alternate) const {
        if (!enabled) return " " + pad_right(text, width) + " ";
        const char* background = alternate ? "48;2;9;36;23" : "48;2;6;27;18";
        const char* foreground = name ? "1;38;2;112;194;139" : "38;2;207;227;210";
        return code(background) + code(foreground) + " " + pad_right(text, width) + " " + reset();
    }
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool matches(const Entry& entry, const std::string& query) {
    if (query.empty()) return true;
    return lower(entry.name + " " + entry.description).find(query) != std::string::npos;
}

std::string run_zsh_scan(int& exit_status) {
    const std::string script =
        "source \"$HOME/.zshrc\" >/dev/null 2>&1\n"
        "for name in ${(ok)aliases}; do\n"
        "  print -r -- \"A\"$'\\x1f'\"$name\"$'\\x1f'\"${aliases[$name]}\"\n"
        "done\n"
        "for name in ${(ok)functions}; do\n"
        "  [[ ${functions_source[$name]-} == \"$HOME/.zshrc\" ]] || continue\n"
        "  print -r -- \"F\"$'\\x1f'\"$name\"\n"
        "done\n";

    int descriptors[2];
    if (pipe(descriptors) != 0) {
        exit_status = errno;
        return {};
    }
    const pid_t child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        exit_status = errno;
        return {};
    }
    if (child == 0) {
        setenv("POTATO_BANNER_SHOWN", "1", 1);
        dup2(descriptors[1], STDOUT_FILENO);
        const int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) dup2(null_fd, STDERR_FILENO);
        close(descriptors[0]);
        close(descriptors[1]);
        execlp("zsh", "zsh", "-dfc", script.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    close(descriptors[1]);
    std::string output;
    char buffer[8192];
    ssize_t count = 0;
    while ((count = read(descriptors[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<std::size_t>(count));
    }
    close(descriptors[0]);
    int status = 0;
    waitpid(child, &status, 0);
    exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    if (std::getenv("AHELP_DEBUG")) {
        std::cerr << "ahelp: zsh scan bytes=" << output.size()
                  << " status=" << exit_status << '\n';
    }
    return output;
}

void collect_shell(std::vector<Entry>& aliases, std::vector<Entry>& functions, int& status) {
    std::istringstream lines(run_zsh_scan(status));
    std::string line;
    std::set<std::string> seen_aliases;
    std::set<std::string> seen_functions;
    while (std::getline(lines, line)) {
        const std::size_t first = line.find(kSeparator);
        if (first == std::string::npos) continue;
        const std::string kind = line.substr(0, first);
        const std::size_t second = line.find(kSeparator, first + 1);
        const std::string name = line.substr(first + 1, second == std::string::npos
                                                           ? std::string::npos
                                                           : second - first - 1);
        if (kind == "A" && second != std::string::npos && seen_aliases.insert(name).second) {
            aliases.push_back({name, "→ " + line.substr(second + 1), "󰘳"});
        } else if (kind == "F" && seen_functions.insert(name).second) {
            const auto description = kFunctionDescriptions.find(name);
            functions.push_back({name,
                                 description != kFunctionDescriptions.end()
                                     ? description->second
                                     : "User function defined by ~/.zshrc.",
                                 "󰊕"});
        }
    }
}

std::string find_in_path(const std::string& name) {
    const char* path_value = std::getenv("PATH");
    if (!path_value) return {};
    std::istringstream paths(path_value);
    std::string directory;
    while (std::getline(paths, directory, ':')) {
        const fs::path candidate = fs::path(directory) / name;
        if (access(candidate.c_str(), X_OK) == 0) return candidate.string();
    }
    return {};
}

std::string dynamic_icon(const std::string& name, const fs::path& path) {
    if (path.extension() == ".py") return "";
    std::error_code error;
    if (fs::is_symlink(path, error)) return "󰌷";
    if (!name.empty() && name.front() == 'w') return "󰆍";
    return "";
}

std::vector<Entry> collect_dynamic_commands() {
    std::map<std::string, fs::path> paths;
    std::set<std::string> curated_names;
    for (const auto* categories : {&kStandardCommands, &kCustomCommands}) {
        for (const auto& category : *categories) {
            for (const auto& entry : category.entries) curated_names.insert(entry.name);
        }
    }
    for (const fs::path& directory : {fs::path(home_directory()) / ".local/bin",
                                      fs::path(home_directory()) / "bin"}) {
        std::error_code error;
        if (!fs::is_directory(directory, error)) continue;
        for (const auto& item : fs::directory_iterator(directory, error)) {
            const std::string name = item.path().filename().string();
            if (name.empty() || name.front() == '.') continue;
            if (ignored_dynamic_command(name)) continue;
            if (curated_names.contains(name)) continue;
            const auto status = item.symlink_status(error);
            if (error || (!fs::is_regular_file(status) && !fs::is_symlink(status))) continue;
            if (access(item.path().c_str(), X_OK) == 0) paths.emplace(name, item.path());
        }
    }

    std::vector<Entry> entries;
    for (const auto& [name, path] : paths) {
        std::string text;
        std::error_code error;
        if (fs::is_symlink(path, error)) {
            text = "Discovered link to " + fs::weakly_canonical(path, error).string() + '.';
        } else {
            text = "Discovered executable in " + path.parent_path().string() + '.';
        }
        entries.push_back({name, std::move(text), dynamic_icon(name, path)});
    }
    return entries;
}

std::string titled_border(const std::string& left, const std::string& right,
                          const std::string& title, int width) {
    const std::string label = "─ " + truncate_to(title, std::max(1, width - 5)) + " ";
    return left + label + repeat("─", std::max(0, width - 2 - text_width(label))) + right;
}

void render_banner(std::ostringstream& output, const Paint& paint, int width,
                   std::size_t standard, std::size_t custom,
                   std::size_t aliases, std::size_t functions,
                   std::size_t discovered,
                   const std::string& query) {
    output << paint.border(titled_border("╭", "╮", "  AHELP · LIVE WORKSTATION INDEX", width)) << '\n';
    const std::string summary = "󰏗  " + std::to_string(standard) + " standard    󰋖  " +
                                std::to_string(custom) + " custom    󰘳  " +
                                std::to_string(aliases) + " aliases    󰊕  " +
                                std::to_string(functions) + " functions    󰌷  " +
                                std::to_string(discovered) + " new";
    output << paint.border("│") << paint.cell(summary, width - 4, true, false)
           << paint.border("│") << '\n';
    std::string source = query.empty()
                             ? "organized reference first · live discoveries below"
                             : "filter: “" + query + "” · organized and live sections";
    output << paint.border("│") << paint.cell(source, width - 4, false, true)
           << paint.border("│") << '\n';
    output << paint.border("╰" + repeat("─", width - 2) + "╯") << '\n';
}

void render_category_header(std::ostringstream& output, const Paint& paint, int width,
                            const std::string& icon, const std::string& title,
                            std::size_t count) {
    const std::string label = "━━ " + icon + "  " + title + " · " + std::to_string(count) + " ";
    output << '\n' << paint.title(label + repeat("━", std::max(0, width - text_width(label)))) << '\n';
}

struct GridColumns {
    int name = 14;
    int description = 20;
};

std::vector<GridColumns> grid_columns(int width, std::size_t entry_count) {
    // A pair is COMMAND + DESCRIPTION. The count grows with the live terminal,
    // while every spare cell is redistributed instead of fixing card widths.
    const int across = std::max(1, std::min(static_cast<int>(entry_count), width / 40));
    const int distributable = width - 1;
    const int base_span = distributable / across;
    const int remainder = distributable % across;
    std::vector<GridColumns> columns;
    columns.reserve(static_cast<std::size_t>(across));
    for (int index = 0; index < across; ++index) {
        const int span = base_span + (index < remainder ? 1 : 0);
        const int content = span - 6;
        int name = std::clamp(content / 2, 12, 26);
        int description = content - name;
        if (description < 16) {
            description = 16;
            name = content - description;
        }
        columns.push_back({name, description});
    }
    return columns;
}

std::string grid_rule(const std::vector<GridColumns>& columns,
                      const std::string& left, const std::string& joint,
                      const std::string& right) {
    std::string line = left;
    for (std::size_t index = 0; index < columns.size(); ++index) {
        line += repeat("─", columns[index].name + 2);
        line += joint;
        line += repeat("─", columns[index].description + 2);
        line += index + 1 == columns.size() ? right : joint;
    }
    return line;
}

std::string take_identifier_chunk(std::string& remaining, int width) {
    if (text_width(remaining) <= width) {
        std::string chunk = remaining;
        remaining.clear();
        return chunk;
    }
    std::string chunk = truncate_to(remaining, width);
    if (chunk.size() >= 3 && chunk.ends_with("…")) chunk.resize(chunk.size() - 3);
    const std::size_t natural = chunk.find_last_of("-_./");
    if (natural != std::string::npos && natural + 1 >= chunk.size() / 2) {
        chunk.resize(natural + 1);
    }
    if (chunk.empty()) {
        chunk = remaining.substr(0, 1);
    }
    remaining.erase(0, chunk.size());
    return chunk;
}

std::vector<std::string> wrap_command(const Entry& entry, int width) {
    std::vector<std::string> lines;
    std::string remaining = entry.name;
    const int icon_cells = text_width(entry.icon) + 2;
    lines.push_back(entry.icon + "  " +
                    take_identifier_chunk(remaining, std::max(4, width - icon_cells)));
    while (!remaining.empty()) lines.push_back(take_identifier_chunk(remaining, width));
    return lines;
}

void render_section(std::ostringstream& output, const Paint& paint, int width,
                    const std::string& title, const std::vector<Entry>& entries) {
    if (entries.empty()) return;
    const auto columns = grid_columns(width, entries.size());
    const std::size_t across = columns.size();
    output << '\n' << paint.border(titled_border("╭", "╮", title + "  " +
                                                    std::to_string(entries.size()), width))
           << '\n';
    output << paint.border(grid_rule(columns, "├", "┬", "┤")) << '\n';
    output << paint.border("│");
    for (const auto& column : columns) {
        output << paint.cell("COMMAND", column.name, true, false)
               << paint.border("│")
               << paint.cell("DESCRIPTION", column.description, true, false)
               << paint.border("│");
    }
    output << '\n' << paint.border(grid_rule(columns, "├", "┼", "┤")) << '\n';

    const std::size_t row_count = (entries.size() + across - 1) / across;
    for (std::size_t row = 0; row < row_count; ++row) {
        std::vector<std::vector<std::string>> names(across);
        std::vector<std::vector<std::string>> descriptions(across);
        std::size_t row_height = 1;
        for (std::size_t card = 0; card < across; ++card) {
            const std::size_t entry_index = row * across + card;
            if (entry_index >= entries.size()) {
                names[card] = {""};
                descriptions[card] = {""};
                continue;
            }
            names[card] = wrap_command(entries[entry_index], columns[card].name);
            descriptions[card] = wrap_text(entries[entry_index].description,
                                           columns[card].description);
            row_height = std::max(row_height, std::max(names[card].size(),
                                                       descriptions[card].size()));
        }

        const bool alternate = row % 2 != 0;
        for (std::size_t line = 0; line < row_height; ++line) {
            output << paint.border("│");
            for (std::size_t card = 0; card < across; ++card) {
                const std::string name = line < names[card].size() ? names[card][line] : "";
                const std::string description = line < descriptions[card].size()
                                                    ? descriptions[card][line]
                                                    : "";
                output << paint.cell(name, columns[card].name, true, alternate)
                       << paint.border("│")
                       << paint.cell(description, columns[card].description, false, alternate)
                       << paint.border("│");
            }
            output << '\n';
        }
        if (row + 1 < row_count) {
            output << paint.border(grid_rule(columns, "├", "┼", "┤")) << '\n';
        }
    }
    output << paint.border(grid_rule(columns, "╰", "┴", "╯")) << '\n';
}

void write_output(const std::string& output, bool use_pager) {
    if (use_pager && isatty(STDOUT_FILENO) && find_in_path("less").size()) {
        std::signal(SIGPIPE, SIG_IGN);
        if (FILE* pager = popen(
                "LESSUTFCHARDEF='E000-F8FF:p,F0000-FFFFD:p,100000-10FFFD:p' less -RFX",
                "w")) {
            std::fwrite(output.data(), 1, output.size(), pager);
            pclose(pager);
            return;
        }
    }
    std::cout << output;
}

}  // namespace

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "");
    bool plain = false;
    bool pager = false;
    std::vector<std::string> query_parts;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--plain") plain = true;
        else if (argument == "-i" || argument == "--interactive") pager = true;
        else if (argument == "--no-pager") pager = false;
        else if (argument == "--help" || argument == "-h") {
            std::cout << "usage: ahelp [-i|--interactive] [--plain] [search terms]\n";
            return 0;
        } else query_parts.push_back(argument);
    }
    std::string query;
    for (const auto& part : query_parts) {
        if (!query.empty()) query += ' ';
        query += lower(part);
    }

    int shell_status = 0;
    std::vector<CommandSubcategory> standard = kStandardCommands;
    std::vector<CommandSubcategory> custom = kCustomCommands;
    std::vector<Entry> aliases;
    std::vector<Entry> functions;
    collect_shell(aliases, functions, shell_status);
    std::vector<Entry> discovered = collect_dynamic_commands();

    const auto filter = [&](std::vector<Entry>& entries) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [&](const Entry& entry) { return !matches(entry, query); }),
                      entries.end());
    };
    const auto filter_categories = [&](std::vector<CommandSubcategory>& categories) {
        for (auto& category : categories) {
            if (!query.empty() && lower(category.title).find(query) != std::string::npos) continue;
            filter(category.entries);
        }
        categories.erase(std::remove_if(categories.begin(), categories.end(),
                                        [](const CommandSubcategory& category) {
                                            return category.entries.empty();
                                        }),
                         categories.end());
    };
    const auto category_count = [](const std::vector<CommandSubcategory>& categories) {
        std::size_t count = 0;
        for (const auto& category : categories) count += category.entries.size();
        return count;
    };
    filter_categories(standard);
    filter_categories(custom);
    filter(aliases);
    filter(functions);
    filter(discovered);

    const std::size_t standard_count = category_count(standard);
    const std::size_t custom_count = category_count(custom);
    const std::size_t dynamic_count = aliases.size() + functions.size() + discovered.size();

    const Terminal terminal = terminal_info(plain);
    const Paint paint{terminal.color};
    std::ostringstream output;
    render_banner(output, paint, terminal.width, standard_count, custom_count,
                  aliases.size(), functions.size(), discovered.size(), query);
    if (standard_count > 0) {
        render_category_header(output, paint, terminal.width, "󰏗", "STANDARD / EXTERNAL COMMANDS",
                               standard_count);
        for (const auto& category : standard) {
            render_section(output, paint, terminal.width,
                           category.icon + "  " + category.title, category.entries);
        }
    }
    if (custom_count > 0) {
        render_category_header(output, paint, terminal.width, "󰋖", "CUSTOM-BUILT UTILITIES",
                               custom_count);
        for (const auto& category : custom) {
            render_section(output, paint, terminal.width,
                           category.icon + "  " + category.title, category.entries);
        }
    }
    if (dynamic_count > 0) {
        render_category_header(output, paint, terminal.width, "󰌷", "DYNAMIC DISCOVERY",
                               dynamic_count);
        render_section(output, paint, terminal.width, "󰘳  ZSH ALIASES", aliases);
        render_section(output, paint, terminal.width, "󰊕  ZSH FUNCTIONS", functions);
        render_section(output, paint, terminal.width, "󰌷  UNLISTED EXECUTABLES", discovered);
    }
    if (shell_status != 0) {
        output << '\n' << paint.warning("⚠ Zsh scan exited with status " + std::to_string(shell_status)) << '\n';
    }
    if (standard_count == 0 && custom_count == 0 && dynamic_count == 0) {
        output << '\n' << paint.warning("󰅙  No command matched “" + query + "”.") << '\n';
    }
    output << '\n';
    for (const auto& line : wrap_text(
             "󰋖  ahelp SEARCH filters live · -i enables pager/navigation · --plain disables color",
             terminal.width - 4)) {
        output << paint.subtle("  " + line) << '\n';
    }
    const std::string rendered = output.str();
    const int line_count = static_cast<int>(std::count(rendered.begin(), rendered.end(), '\n'));
    write_output(rendered, pager && line_count > terminal.height - 2);
    return 0;
}
