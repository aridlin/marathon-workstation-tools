#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

constexpr int kWorkspaceSlots = 10;
constexpr int kMaximumManagedDisplays = 5;

struct Monitor {
    std::string name;
    std::string description;
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    double scale = 1.0;
    bool focused = false;
};

struct Range {
    int first = 1;
    int last = 10;
};

struct State {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* status = nullptr;
    GtkWidget* apply = nullptr;
    std::vector<Monitor> monitors;
    std::string main_monitor;
    bool rebuilding = false;
};

State state;

std::string config_path() {
    return std::string(g_get_user_config_dir()) + "/hypr/workspace-displays.json";
}

std::string apply_script_path() {
    return std::string(g_get_home_dir()) + "/.config/hypr/workspace_monitor_apply.sh";
}

std::string remember_script_path() {
    return std::string(g_get_home_dir()) + "/.config/hypr/workspace_monitor_remember.sh";
}

std::vector<Range> partition_ranges(int count, int main_index) {
    if (count < 1 || count > kMaximumManagedDisplays) return {};
    if (main_index < 0 || main_index >= count) main_index = 0;

    std::vector<int> sizes(static_cast<std::size_t>(count), kWorkspaceSlots / count);
    int remainder = kWorkspaceSlots % count;
    if (remainder > 0) {
        ++sizes[static_cast<std::size_t>(main_index)];
        --remainder;
    }
    for (int i = 0; i < count && remainder > 0; ++i) {
        if (i == main_index) continue;
        ++sizes[static_cast<std::size_t>(i)];
        --remainder;
    }

    std::vector<Range> ranges;
    int first = 1;
    for (const int size : sizes) {
        ranges.push_back({first, first + size - 1});
        first += size;
    }
    return ranges;
}

bool run_command(const std::vector<std::string>& arguments, std::string& output,
                 std::string& error_text) {
    std::vector<gchar*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& value : arguments) argv.push_back(const_cast<gchar*>(value.c_str()));
    argv.push_back(nullptr);

    gchar* stdout_data = nullptr;
    gchar* stderr_data = nullptr;
    gint status = 0;
    GError* error = nullptr;
    const gboolean ok = g_spawn_sync(nullptr, argv.data(), nullptr, G_SPAWN_SEARCH_PATH,
                                     nullptr, nullptr, &stdout_data, &stderr_data,
                                     &status, &error);
    if (stdout_data) output = stdout_data;
    if (stderr_data) error_text = stderr_data;
    if (error) {
        if (error_text.empty()) error_text = error->message;
        g_error_free(error);
    }
    g_free(stdout_data);
    g_free(stderr_data);
    return ok && status == 0;
}

std::vector<Monitor> detect_monitors(std::string& error_text) {
    std::string output;
    if (!run_command({"hyprctl", "-j", "monitors", "all"}, output, error_text)) return {};

    JsonParser* parser = json_parser_new();
    GError* parse_error = nullptr;
    if (!json_parser_load_from_data(parser, output.c_str(), -1, &parse_error)) {
        error_text = parse_error ? parse_error->message : "Invalid monitor JSON";
        if (parse_error) g_error_free(parse_error);
        g_object_unref(parser);
        return {};
    }

    std::vector<Monitor> monitors;
    JsonNode* root = json_parser_get_root(parser);
    if (root && JSON_NODE_HOLDS_ARRAY(root)) {
        JsonArray* array = json_node_get_array(root);
        const guint count = json_array_get_length(array);
        for (guint i = 0; i < count; ++i) {
            JsonObject* object = json_array_get_object_element(array, i);
            if (!object) continue;
            const bool disabled = json_object_has_member(object, "disabled") &&
                                  json_object_get_boolean_member(object, "disabled");
            const int width = json_object_has_member(object, "width")
                                  ? static_cast<int>(json_object_get_int_member(object, "width"))
                                  : 0;
            if (disabled || width <= 0 || !json_object_has_member(object, "name")) continue;

            Monitor monitor;
            monitor.name = json_object_get_string_member(object, "name");
            if (json_object_has_member(object, "description")) {
                monitor.description = json_object_get_string_member(object, "description");
            }
            monitor.width = width;
            monitor.height = json_object_has_member(object, "height")
                                 ? static_cast<int>(json_object_get_int_member(object, "height"))
                                 : 0;
            monitor.x = json_object_has_member(object, "x")
                            ? static_cast<int>(json_object_get_int_member(object, "x"))
                            : 0;
            monitor.y = json_object_has_member(object, "y")
                            ? static_cast<int>(json_object_get_int_member(object, "y"))
                            : 0;
            monitor.scale = json_object_has_member(object, "scale")
                                ? json_object_get_double_member(object, "scale")
                                : 1.0;
            monitor.focused = json_object_has_member(object, "focused") &&
                              json_object_get_boolean_member(object, "focused");
            monitors.push_back(std::move(monitor));
        }
    }
    g_object_unref(parser);

    std::sort(monitors.begin(), monitors.end(), [](const Monitor& left, const Monitor& right) {
        if (left.x != right.x) return left.x < right.x;
        if (left.y != right.y) return left.y < right.y;
        return left.name < right.name;
    });
    return monitors;
}

void read_preferences(std::vector<std::string>& order, std::string& main_monitor) {
    gchar* contents = nullptr;
    gsize length = 0;
    if (!g_file_get_contents(config_path().c_str(), &contents, &length, nullptr)) return;

    JsonParser* parser = json_parser_new();
    if (json_parser_load_from_data(parser, contents, static_cast<gssize>(length), nullptr)) {
        JsonNode* root = json_parser_get_root(parser);
        if (root && JSON_NODE_HOLDS_OBJECT(root)) {
            JsonObject* object = json_node_get_object(root);
            if (json_object_has_member(object, "main")) {
                const char* value = json_object_get_string_member(object, "main");
                if (value) main_monitor = value;
            }
            if (json_object_has_member(object, "order")) {
                JsonArray* array = json_object_get_array_member(object, "order");
                if (array) {
                    for (guint i = 0; i < json_array_get_length(array); ++i) {
                        const char* value = json_array_get_string_element(array, i);
                        if (value && *value) order.emplace_back(value);
                    }
                }
            }
        }
    }
    g_object_unref(parser);
    g_free(contents);
}

void apply_preference_order(std::vector<Monitor>& monitors, const std::vector<std::string>& order) {
    std::map<std::string, Monitor> by_name;
    for (auto& monitor : monitors) by_name.emplace(monitor.name, std::move(monitor));

    std::vector<Monitor> arranged;
    std::set<std::string> used;
    for (const auto& name : order) {
        const auto found = by_name.find(name);
        if (found != by_name.end() && used.insert(name).second) arranged.push_back(found->second);
    }
    for (const auto& [name, monitor] : by_name) {
        if (used.insert(name).second) arranged.push_back(monitor);
    }
    monitors = std::move(arranged);
}

int main_monitor_index() {
    for (std::size_t i = 0; i < state.monitors.size(); ++i) {
        if (state.monitors[i].name == state.main_monitor) return static_cast<int>(i);
    }
    return 0;
}

void set_status(const std::string& message, bool error = false) {
    if (!state.status) return;
    const std::string escaped = g_markup_escape_text(message.c_str(), -1);
    const std::string markup = error
                                   ? "<span foreground='#ff718c'>" + escaped + "</span>"
                                   : "<span foreground='#70c28b'>" + escaped + "</span>";
    gtk_label_set_markup(GTK_LABEL(state.status), markup.c_str());
}

void rebuild_list();

void choose_main(GtkToggleButton* button, gpointer data) {
    if (state.rebuilding || !gtk_toggle_button_get_active(button)) return;
    const int index = GPOINTER_TO_INT(data);
    if (index >= 0 && index < static_cast<int>(state.monitors.size())) {
        state.main_monitor = state.monitors[static_cast<std::size_t>(index)].name;
        rebuild_list();
    }
}

void move_monitor(GtkButton*, gpointer data) {
    const int encoded = GPOINTER_TO_INT(data);
    const int index = std::abs(encoded) - 1;
    const int delta = encoded < 0 ? -1 : 1;
    const int destination = index + delta;
    if (index < 0 || destination < 0 ||
        index >= static_cast<int>(state.monitors.size()) ||
        destination >= static_cast<int>(state.monitors.size())) return;
    std::swap(state.monitors[static_cast<std::size_t>(index)],
              state.monitors[static_cast<std::size_t>(destination)]);
    rebuild_list();
}

void rebuild_list() {
    if (!state.list) return;
    state.rebuilding = true;
    GList* children = gtk_container_get_children(GTK_CONTAINER(state.list));
    for (GList* item = children; item; item = item->next) {
        gtk_widget_destroy(GTK_WIDGET(item->data));
    }
    g_list_free(children);

    const int managed = std::min<int>(kMaximumManagedDisplays, state.monitors.size());
    int primary = main_monitor_index();
    if (primary >= managed) primary = -1;
    const auto ranges = primary >= 0 ? partition_ranges(managed, primary) : std::vector<Range>{};

    GSList* radio_group = nullptr;
    for (std::size_t i = 0; i < state.monitors.size(); ++i) {
        const auto& monitor = state.monitors[i];
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(box), 10);
        gtk_container_add(GTK_CONTAINER(row), box);

        GtkWidget* radio = gtk_radio_button_new(radio_group);
        radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
        gtk_widget_set_tooltip_text(radio, "Use this display as the main/focus display");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), monitor.name == state.main_monitor);
        g_signal_connect(radio, "toggled", G_CALLBACK(choose_main),
                         GINT_TO_POINTER(static_cast<int>(i)));
        gtk_box_pack_start(GTK_BOX(box), radio, FALSE, FALSE, 0);

        GtkWidget* labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget* name = gtk_label_new(nullptr);
        const std::string escaped_name = g_markup_escape_text(monitor.name.c_str(), -1);
        std::string name_markup = "<b>" + escaped_name + "</b>";
        if (monitor.focused) name_markup += "  <span foreground='#70c28b'>connected · focused</span>";
        else name_markup += "  <span foreground='#76ac84'>connected</span>";
        gtk_label_set_markup(GTK_LABEL(name), name_markup.c_str());
        gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
        gtk_box_pack_start(GTK_BOX(labels), name, FALSE, FALSE, 0);

        std::string details = monitor.description.empty() ? "Unknown display" : monitor.description;
        details += "  ·  " + std::to_string(monitor.width) + "×" + std::to_string(monitor.height);
        GtkWidget* detail = gtk_label_new(details.c_str());
        gtk_label_set_ellipsize(GTK_LABEL(detail), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(detail), 0.0f);
        gtk_style_context_add_class(gtk_widget_get_style_context(detail), "dim-label");
        gtk_box_pack_start(GTK_BOX(labels), detail, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), labels, TRUE, TRUE, 0);

        std::string assignment = "unmanaged";
        if (static_cast<int>(i) < managed && !ranges.empty()) {
            const Range range = ranges[i];
            assignment = "sub " + std::to_string(range.first) + "–" + std::to_string(range.last);
        }
        GtkWidget* range_label = gtk_label_new(assignment.c_str());
        gtk_widget_set_size_request(range_label, 88, -1);
        gtk_box_pack_start(GTK_BOX(box), range_label, FALSE, FALSE, 0);

        GtkWidget* up = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
        GtkWidget* down = gtk_button_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_sensitive(up, i > 0);
        gtk_widget_set_sensitive(down, i + 1 < state.monitors.size());
        g_signal_connect(up, "clicked", G_CALLBACK(move_monitor),
                         GINT_TO_POINTER(-static_cast<int>(i + 1)));
        g_signal_connect(down, "clicked", G_CALLBACK(move_monitor),
                         GINT_TO_POINTER(static_cast<int>(i + 1)));
        gtk_box_pack_start(GTK_BOX(box), up, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), down, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(state.list), row);
    }

    state.rebuilding = false;
    gtk_widget_show_all(state.list);
    const bool valid = managed > 0 && main_monitor_index() < managed;
    gtk_widget_set_sensitive(state.apply, valid);
    if (state.monitors.size() > kMaximumManagedDisplays) {
        set_status("Only the first five displays are grid-managed; reorder to choose them.", true);
    } else if (!valid && !state.monitors.empty()) {
        set_status("Move the selected main display into the first five positions.", true);
    }
}

void refresh_monitors(GtkButton*, gpointer) {
    std::string error_text;
    auto monitors = detect_monitors(error_text);
    if (monitors.empty()) {
        set_status(error_text.empty() ? "No active Hyprland displays found." : error_text, true);
        return;
    }

    std::vector<std::string> order;
    std::string configured_main;
    read_preferences(order, configured_main);
    apply_preference_order(monitors, order);
    state.monitors = std::move(monitors);

    const auto known = std::find_if(state.monitors.begin(), state.monitors.end(),
                                    [&](const Monitor& monitor) {
                                        return monitor.name == configured_main;
                                    });
    if (known != state.monitors.end()) {
        state.main_monitor = configured_main;
    } else {
        const auto focused = std::find_if(state.monitors.begin(), state.monitors.end(),
                                          [](const Monitor& monitor) { return monitor.focused; });
        state.main_monitor = focused != state.monitors.end() ? focused->name : state.monitors.front().name;
    }
    rebuild_list();
    set_status("Detected " + std::to_string(state.monitors.size()) + " active display(s).");
}

bool save_preferences(std::string& error_text) {
    JsonBuilder* builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "version");
    json_builder_add_int_value(builder, 1);
    json_builder_set_member_name(builder, "main");
    json_builder_add_string_value(builder, state.main_monitor.c_str());
    json_builder_set_member_name(builder, "order");
    json_builder_begin_array(builder);
    for (const auto& monitor : state.monitors) {
        json_builder_add_string_value(builder, monitor.name.c_str());
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    json_generator_set_pretty(generator, TRUE);
    gchar* json = json_generator_to_data(generator, nullptr);

    const std::string directory = std::string(g_get_user_config_dir()) + "/hypr";
    if (g_mkdir_with_parents(directory.c_str(), 0700) != 0) {
        error_text = std::strerror(errno);
    } else {
        const std::string temporary = config_path() + ".tmp";
        GError* error = nullptr;
        if (!g_file_set_contents(temporary.c_str(), json, -1, &error) ||
            g_rename(temporary.c_str(), config_path().c_str()) != 0) {
            error_text = error ? error->message : std::strerror(errno);
            if (error) g_error_free(error);
            g_unlink(temporary.c_str());
        }
    }

    g_free(json);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);
    return error_text.empty();
}

void apply_configuration(GtkButton*, gpointer) {
    std::string error_text;
    if (!save_preferences(error_text)) {
        set_status("Could not save: " + error_text, true);
        return;
    }
    std::string output;
    if (!run_command({apply_script_path()}, output, error_text)) {
        set_status("Saved, but apply failed: " + error_text, true);
        return;
    }
    set_status("Applied. Workspace ownership now follows this display order.");
}

void activate(GtkApplication* app, gpointer) {
    if (state.window) {
        gtk_window_present(GTK_WINDOW(state.window));
        return;
    }
    state.app = app;
    state.window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state.window), "Workspace Display Layout");
    gtk_window_set_default_size(GTK_WINDOW(state.window), 760, 520);
    gtk_window_set_icon_name(GTK_WINDOW(state.window), "workspace-display-manager");

    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(
        css,
        "window { background: #00130b; color: #cfe3d2; }"
        "list, list row { background: #061b12; color: #cfe3d2; }"
        "list row { border-bottom: 1px solid #123b27; }"
        "button { background: #092417; color: #cfe3d2; border-color: #3f8f61; }"
        "button:hover { background: #17402b; }"
        ".title { color: #70c28b; font-size: 18px; font-weight: bold; }"
        ".dim-label { color: #76ac84; }",
        -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gtk_widget_get_screen(state.window), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(root), 18);
    gtk_container_add(GTK_CONTAINER(state.window), root);

    GtkWidget* title = gtk_label_new("WORKSPACE DISPLAY LAYOUT");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_box_pack_start(GTK_BOX(root), title, FALSE, FALSE, 0);

    GtkWidget* explanation = gtk_label_new(
        "Each 10-slot sub-row is split across displays. The split repeats for every main row; "
        "for example, two displays own 21–25 and 26–30. Select the main display and reorder below.");
    gtk_label_set_line_wrap(GTK_LABEL(explanation), TRUE);
    gtk_label_set_xalign(GTK_LABEL(explanation), 0.0f);
    gtk_box_pack_start(GTK_BOX(root), explanation, FALSE, FALSE, 0);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    state.list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state.list), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroller), state.list);
    gtk_box_pack_start(GTK_BOX(root), scroller, TRUE, TRUE, 0);

    state.status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(state.status), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(state.status), TRUE);
    gtk_box_pack_start(GTK_BOX(root), state.status, FALSE, FALSE, 0);

    GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* refresh = gtk_button_new_with_label("Detect displays");
    GtkWidget* close = gtk_button_new_with_label("Close");
    state.apply = gtk_button_new_with_label("Apply layout");
    g_signal_connect(refresh, "clicked", G_CALLBACK(refresh_monitors), nullptr);
    g_signal_connect(close, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        gtk_window_close(GTK_WINDOW(state.window));
    }), nullptr);
    g_signal_connect(state.apply, "clicked", G_CALLBACK(apply_configuration), nullptr);
    gtk_box_pack_start(GTK_BOX(actions), refresh, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(actions), state.apply, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(actions), close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), actions, FALSE, FALSE, 0);

    g_signal_connect(state.window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) {
        state.window = nullptr;
        state.list = nullptr;
        state.status = nullptr;
        state.apply = nullptr;
    }), nullptr);

    refresh_monitors(nullptr, nullptr);
    gtk_widget_show_all(state.window);
}

bool event_is_monitor_change(const std::string& line) {
    return line.rfind("monitoradded>>", 0) == 0 ||
           line.rfind("monitoraddedv2>>", 0) == 0 ||
           line.rfind("monitorremoved>>", 0) == 0 ||
           line.rfind("monitorremovedv2>>", 0) == 0;
}

bool event_is_workspace_change(const std::string& line) {
    return line.rfind("workspace>>", 0) == 0 ||
           line.rfind("workspacev2>>", 0) == 0 ||
           line.rfind("focusedmon>>", 0) == 0 ||
           line.rfind("focusedmonv2>>", 0) == 0;
}

void run_apply_script() {
    std::string output;
    std::string error;
    run_command({apply_script_path()}, output, error);
}

int run_daemon() {
    run_apply_script();
    std::string pending;
    gint64 last_apply = g_get_monotonic_time();
    while (true) {
        const char* runtime = g_getenv("XDG_RUNTIME_DIR");
        const char* signature = g_getenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (!runtime || !signature) {
            g_usleep(G_USEC_PER_SEC);
            continue;
        }
        const std::string path = std::string(runtime) + "/hypr/" + signature + "/.socket2.sock";
        if (path.size() >= sizeof(sockaddr_un::sun_path)) {
            std::cerr << "Hyprland event socket path is too long\n";
            return 2;
        }

        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            g_usleep(G_USEC_PER_SEC);
            continue;
        }
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
        if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close(fd);
            g_usleep(G_USEC_PER_SEC);
            continue;
        }

        char buffer[4096];
        ssize_t length = 0;
        while ((length = read(fd, buffer, sizeof(buffer))) > 0) {
            pending.append(buffer, static_cast<std::size_t>(length));
            std::size_t newline = 0;
            while ((newline = pending.find('\n')) != std::string::npos) {
                const std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                if (event_is_monitor_change(line)) {
                    const gint64 now = g_get_monotonic_time();
                    if (now - last_apply < G_USEC_PER_SEC) continue;
                    g_usleep(300 * 1000);
                    run_apply_script();
                    last_apply = g_get_monotonic_time();
                } else if (event_is_workspace_change(line)) {
                    std::string output;
                    std::string error;
                    run_command({remember_script_path()}, output, error);
                }
            }
        }
        close(fd);
        g_usleep(500 * 1000);
    }
}

int print_plan(int count, int main_index, bool tsv) {
    const auto ranges = partition_ranges(count, main_index);
    if (ranges.empty()) {
        std::cerr << "display count must be between 1 and 5\n";
        return 2;
    }
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        if (tsv) {
            std::cout << i << '\t' << ranges[i].first << '\t' << ranges[i].last << '\n';
        } else {
            std::cout << "monitor " << (i + 1) << ": " << ranges[i].first << '-'
                      << ranges[i].last;
            if (static_cast<int>(i) == main_index) std::cout << " main";
            std::cout << '\n';
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--daemon") == 0) return run_daemon();
    if (argc >= 3 && (std::strcmp(argv[1], "--print-plan") == 0 ||
                      std::strcmp(argv[1], "--print-plan-tsv") == 0)) {
        const int count = std::atoi(argv[2]);
        const int main_index = argc >= 4 ? std::atoi(argv[3]) : 0;
        return print_plan(count, main_index,
                          std::strcmp(argv[1], "--print-plan-tsv") == 0);
    }
    if (argc >= 2 && std::strcmp(argv[1], "--apply") == 0) {
        run_apply_script();
        return 0;
    }

    g_set_prgname("workspace-display-manager");
    GtkApplication* app = gtk_application_new("pl.aridlin.WorkspaceDisplayManager",
                                               G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
