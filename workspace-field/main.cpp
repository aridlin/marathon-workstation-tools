#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int kGridSize = 10;
constexpr int kCell = 50;
constexpr int kGap = 5;
constexpr int kGridX = 62;
constexpr int kGridY = 76;
constexpr int kWindowWidth = kGridX + kGridSize * kCell + (kGridSize - 1) * kGap + 22;
constexpr int kWindowHeight = kGridY + kGridSize * kCell + (kGridSize - 1) * kGap + 38;

struct AppGroup {
    std::string app;
    int count = 0;
};

struct State {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* canvas = nullptr;
    GtkIconTheme* icon_theme = nullptr;
    int active_workspace = -1;
    int hover_main = -1;
    int hover_sub = -1;
    double visual_main = 0.0;
    double visual_sub = 0.0;
    double target_main = 0.0;
    double target_sub = 0.0;
    bool user_moved_selection = false;
    bool resident = false;
    bool held = false;
    guint refresh_source = 0;
    gint64 animation_last_us = 0;
    guint animation_source = 0;
    bool pressing = false;
    int pressed_main = -1;
    int pressed_sub = -1;
    gint64 press_started_us = 0;
    cairo_surface_t* base_surface = nullptr;
    int base_scale = 0;
    std::map<int, std::vector<AppGroup>> workspaces;
    std::map<int, std::vector<std::string>> titles;
    std::unordered_map<std::string, GdkPixbuf*> icons;
};

State state;

void invalidate_base() {
    if (state.base_surface) {
        cairo_surface_destroy(state.base_surface);
        state.base_surface = nullptr;
    }
    state.base_scale = 0;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string canonical(const std::string& value) {
    std::string result;
    for (const unsigned char c : lower(value)) {
        if (std::isalnum(c)) result.push_back(static_cast<char>(c));
    }
    return result;
}

std::string friendly_name(std::string app) {
    const std::string value = lower(app);
    if (value.find("firefox") != std::string::npos) return "Firefox";
    if (value.find("vesktop") != std::string::npos) return "Vesktop";
    if (value.find("discord") != std::string::npos) return "Discord";
    if (value.find("ghostty") != std::string::npos) return "Ghostty";
    if (value.find("steam") != std::string::npos) return "Steam";
    if (value.find("signal") != std::string::npos) return "Signal";
    if (value.find("obsidian") != std::string::npos) return "Obsidian";
    if (value.find("spotify") != std::string::npos) return "Spotify";
    if (value.find("thunar") != std::string::npos) return "Thunar";
    if (value.find("kitty") != std::string::npos) return "Kitty";
    if (value.find("vlc") != std::string::npos) return "VLC";
    if (value.find("obs") != std::string::npos) return "OBS";
    if (app.empty()) return "Window";
    app[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(app[0])));
    return app;
}

std::string ellipsize_utf8(const std::string& text, glong max_characters) {
    if (text.empty() || g_utf8_strlen(text.c_str(), -1) <= max_characters) return text;
    const char* end = g_utf8_offset_to_pointer(text.c_str(), std::max<glong>(1, max_characters - 1));
    return std::string(text.c_str(), end) + "…";
}

bool workspace_coordinates(int id, int& main, int& sub) {
    if (id < 11 || id > 110) return false;
    main = (id - 1) / 10;
    sub = (id - 1) % 10 + 1;
    return main >= 1 && main <= 10 && sub >= 1 && sub <= 10;
}

bool run_json(const char* command, JsonParser* parser) {
    gchar* output = nullptr;
    gchar* error_output = nullptr;
    gint status = 0;
    gchar* argv[] = {
        const_cast<gchar*>("hyprctl"),
        const_cast<gchar*>("-j"),
        const_cast<gchar*>(command),
        nullptr,
    };

    GError* error = nullptr;
    const gboolean ok = g_spawn_sync(nullptr, argv, nullptr, G_SPAWN_SEARCH_PATH,
                                      nullptr, nullptr, &output, &error_output,
                                      &status, &error);
    bool parsed = false;
    if (ok && status == 0 && output) {
        parsed = json_parser_load_from_data(parser, output, -1, &error);
    }
    if (error) g_error_free(error);
    g_free(output);
    g_free(error_output);
    return parsed;
}

std::string member_string(JsonObject* object, const char* member) {
    if (!json_object_has_member(object, member)) return {};
    const char* value = json_object_get_string_member(object, member);
    return value ? value : "";
}

bool refresh_state() {
    std::map<int, std::map<std::string, int>> raw;
    std::map<int, std::vector<std::string>> raw_titles;

    JsonParser* active_parser = json_parser_new();
    if (run_json("activeworkspace", active_parser)) {
        JsonNode* root = json_parser_get_root(active_parser);
        if (root && JSON_NODE_HOLDS_OBJECT(root)) {
            JsonObject* object = json_node_get_object(root);
            if (json_object_has_member(object, "id")) {
                state.active_workspace = static_cast<int>(json_object_get_int_member(object, "id"));
            }
        }
    }
    g_object_unref(active_parser);

    JsonParser* clients_parser = json_parser_new();
    if (!run_json("clients", clients_parser)) {
        g_object_unref(clients_parser);
        return G_SOURCE_CONTINUE;
    }

    JsonNode* root = json_parser_get_root(clients_parser);
    if (root && JSON_NODE_HOLDS_ARRAY(root)) {
        JsonArray* clients = json_node_get_array(root);
        const guint length = json_array_get_length(clients);
        for (guint i = 0; i < length; ++i) {
            JsonObject* client = json_array_get_object_element(clients, i);
            if (!client || !json_object_has_member(client, "workspace")) continue;
            JsonObject* workspace = json_object_get_object_member(client, "workspace");
            if (!workspace || !json_object_has_member(workspace, "id")) continue;
            const int id = static_cast<int>(json_object_get_int_member(workspace, "id"));
            int main = 0;
            int sub = 0;
            if (!workspace_coordinates(id, main, sub)) continue;

            std::string app = member_string(client, "class");
            if (app.empty()) app = member_string(client, "initialClass");
            const std::string title = member_string(client, "title");
            if (app.empty()) app = title;
            if (canonical(app) == "workspacefield" ||
                title == "Workspace Field") continue;
            if (app.empty()) app = "window";
            raw[id][lower(app)]++;
            raw_titles[id].push_back(title.empty() ? friendly_name(app) : title);
        }
    }
    state.titles = std::move(raw_titles);
    g_object_unref(clients_parser);

    state.workspaces.clear();
    for (auto& [id, groups] : raw) {
        auto& destination = state.workspaces[id];
        for (const auto& [app, count] : groups) destination.push_back({app, count});
        std::sort(destination.begin(), destination.end(), [](const AppGroup& a, const AppGroup& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.app < b.app;
        });
    }

    invalidate_base();
    if (state.canvas) gtk_widget_queue_draw(state.canvas);
    return G_SOURCE_CONTINUE;
}

std::vector<std::string> icon_candidates(const std::string& app) {
    const std::string value = lower(app);
    std::vector<std::string> candidates{app, value};
    auto add = [&](const char* icon) { candidates.emplace_back(icon); };

    if (value.find("firefox") != std::string::npos) add("firefox");
    if (value.find("vesktop") != std::string::npos) add("vesktop");
    if (value.find("discord") != std::string::npos) add("discord");
    if (value.find("ghostty") != std::string::npos) add("com.mitchellh.ghostty");
    if (value.find("steam") != std::string::npos) add("steam");
    if (value.find("signal") != std::string::npos) add("signal-desktop");
    if (value.find("obsidian") != std::string::npos) add("obsidian");
    if (value.find("spotify") != std::string::npos) add("spotify");
    if (value.find("thunar") != std::string::npos) add("org.xfce.thunar");
    if (value.find("kitty") != std::string::npos) add("kitty");
    if (value.find("vlc") != std::string::npos) add("vlc");
    if (value.find("obs") != std::string::npos) add("com.obsproject.Studio");
    if (value.find("code") != std::string::npos) add("visual-studio-code");
    if (value.find("waydroid") != std::string::npos) add("waydroid");
    return candidates;
}

GdkPixbuf* load_theme_icon(const std::vector<std::string>& candidates, int size) {
    const int scale = state.window ? std::max(1, gtk_widget_get_scale_factor(state.window)) : 1;
    for (const auto& name : candidates) {
        if (!gtk_icon_theme_has_icon(state.icon_theme, name.c_str())) continue;
        GError* error = nullptr;
        GdkPixbuf* pixbuf = gtk_icon_theme_load_icon_for_scale(
            state.icon_theme, name.c_str(), size, scale,
            static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_FORCE_REGULAR),
            &error);
        if (error) g_error_free(error);
        if (pixbuf) return pixbuf;
    }
    return nullptr;
}

GdkPixbuf* app_icon(const std::string& app) {
    const std::string key = lower(app);
    const auto existing = state.icons.find(key);
    if (existing != state.icons.end()) return existing->second;

    GdkPixbuf* pixbuf = load_theme_icon(icon_candidates(app), 36);
    state.icons[key] = pixbuf;
    return pixbuf;
}

void rounded_rectangle(cairo_t* cr, double x, double y, double width, double height, double radius) {
    const double degrees = G_PI / 180.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc(cr, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc(cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path(cr);
}

void set_font(cairo_t* cr, double size, cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL) {
    cairo_select_font_face(cr, "JetBrainsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL, weight);
    cairo_set_font_size(cr, size);
}

void centered_text(cairo_t* cr, const std::string& text, double center_x, double baseline_y) {
    cairo_text_extents_t extents{};
    cairo_text_extents(cr, text.c_str(), &extents);
    cairo_move_to(cr, center_x - (extents.width / 2.0 + extents.x_bearing), baseline_y);
    cairo_show_text(cr, text.c_str());
}

void draw_fallback_icon(cairo_t* cr, const std::string& app, double x, double y,
                        double size, double alpha) {
    const std::string key = canonical(app);
    guint hash = g_str_hash(key.c_str());
    const double red = 0.22 + ((hash >> 16) & 0xff) / 1024.0;
    const double green = 0.46 + ((hash >> 8) & 0xff) / 1024.0;
    const double blue = 0.28 + (hash & 0xff) / 1400.0;
    cairo_set_source_rgba(cr, red, green, blue, alpha);
    rounded_rectangle(cr, x, y, size, size, 8);
    cairo_fill(cr);

    std::string initial = "?";
    const std::string name = friendly_name(app);
    if (!name.empty()) initial = name.substr(0, 1);
    cairo_set_source_rgba(cr, 0.0, 0.075, 0.043, alpha);
    set_font(cr, size * 0.50, CAIRO_FONT_WEIGHT_BOLD);
    centered_text(cr, initial, x + size / 2.0, y + size * 0.69);
}

void draw_icon(cairo_t* cr, const std::string& app, double x, double y,
               double size, double alpha) {
    GdkPixbuf* pixbuf = app_icon(app);
    if (!pixbuf) {
        draw_fallback_icon(cr, app, x, y, size, alpha);
        return;
    }
    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / width, size / height);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint_with_alpha(cr, alpha);
    cairo_restore(cr);
}

void draw_workspace_apps(cairo_t* cr, const std::vector<AppGroup>& groups,
                         double cell_x, double cell_y) {
    if (groups.empty()) return;
    constexpr double icon_size = 36.0;
    const double icon_x = cell_x + (kCell - icon_size) / 2.0;
    const double icon_y = cell_y + (kCell - icon_size) / 2.0;

    if (groups.size() == 1) {
        draw_icon(cr, groups.front().app, icon_x, icon_y, icon_size, 0.78);
    } else {
        const double slice = icon_size / groups.size();
        for (std::size_t i = 0; i < groups.size(); ++i) {
            cairo_save(cr);
            cairo_rectangle(cr, icon_x + slice * i, icon_y, slice + 0.5, icon_size);
            cairo_clip(cr);
            draw_icon(cr, groups[i].app, icon_x, icon_y, icon_size, 0.82);
            cairo_restore(cr);
        }
    }

    int total = 0;
    for (const auto& group : groups) total += group.count;
    if (total > static_cast<int>(groups.size())) {
        const std::string count = std::to_string(total);
        cairo_set_source_rgba(cr, 0.30, 0.68, 0.47, 0.98);
        cairo_arc(cr, cell_x + kCell - 8, cell_y + 8, 8.5, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.0, 0.075, 0.043, 1.0);
        set_font(cr, count.size() > 1 ? 7.5 : 9.0, CAIRO_FONT_WEIGHT_BOLD);
        centered_text(cr, count, cell_x + kCell - 8, cell_y + 11.2);
    }
}

void draw_base(cairo_t* cr) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    rounded_rectangle(cr, 1.5, 1.5, kWindowWidth - 3, kWindowHeight - 3, 12);
    cairo_set_source_rgba(cr, 0.0, 0.075, 0.043, 0.965);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgba(cr, 0.31, 0.68, 0.47, 0.82);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.81, 0.89, 0.82, 1.0);
    set_font(cr, 16, CAIRO_FONT_WEIGHT_BOLD);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, "WORKSPACE FIELD");

    cairo_set_source_rgba(cr, 0.46, 0.67, 0.52, 0.92);
    set_font(cr, 8.5, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_move_to(cr, 20, 48);
    cairo_show_text(cr, "MAIN →   SUB ↓");
    cairo_move_to(cr, kWindowWidth - 202, 30);
    cairo_show_text(cr, "click to teleport  ·  esc to close");

    set_font(cr, 9, CAIRO_FONT_WEIGHT_BOLD);
    for (int main = 1; main <= kGridSize; ++main) {
        const double x = kGridX + (main - 1) * (kCell + kGap);
        cairo_set_source_rgba(cr, 0.71, 0.82, 0.74, 0.85);
        centered_text(cr, std::to_string(main), x + kCell / 2.0, kGridY - 10);
    }

    for (int sub = 1; sub <= kGridSize; ++sub) {
        const double y = kGridY + (sub - 1) * (kCell + kGap);
        cairo_set_source_rgba(cr, 0.46, 0.67, 0.52, 0.88);
        set_font(cr, 9, CAIRO_FONT_WEIGHT_BOLD);
        centered_text(cr, std::to_string(sub), kGridX - 28, y + kCell / 2.0 + 3);

        for (int main = 1; main <= kGridSize; ++main) {
            const int id = main * 10 + sub;
            const double x = kGridX + (main - 1) * (kCell + kGap);
            const bool active = id == state.active_workspace;
            const auto found = state.workspaces.find(id);
            const bool occupied = found != state.workspaces.end() && !found->second.empty();

            rounded_rectangle(cr, x, y, kCell, kCell, 8);
            if (active) {
                cairo_set_source_rgba(cr, 0.25, 0.56, 0.38, 0.94);
            } else if (occupied) {
                cairo_set_source_rgba(cr, 0.0, 0.18, 0.095, 0.92);
            } else {
                cairo_set_source_rgba(cr, 0.0, 0.115, 0.063, 0.76);
            }
            cairo_fill_preserve(cr);
            cairo_set_line_width(cr, active ? 2.0 : 1.0);
            cairo_set_source_rgba(cr, active ? 0.46 : 0.31, active ? 0.67 : 0.68,
                                  active ? 0.52 : 0.47, active ? 1.0 : 0.28);
            cairo_stroke(cr);

            if (occupied) draw_workspace_apps(cr, found->second, x, y);

            const std::string number = std::to_string(id);
            cairo_set_source_rgba(cr, active ? 0.0 : 0.71, active ? 0.075 : 0.82,
                                  active ? 0.043 : 0.74, occupied ? 0.30 : 0.48);
            set_font(cr, 8.0, CAIRO_FONT_WEIGHT_BOLD);
            cairo_move_to(cr, x + 4, y + kCell - 5);
            cairo_show_text(cr, number.c_str());

            if (active) {
                cairo_set_source_rgba(cr, 0.0, 0.075, 0.043, 1.0);
                cairo_arc(cr, x + kCell - 6.5, y + kCell - 6.5, 2.6, 0, 2 * G_PI);
                cairo_fill(cr);
            }
        }
    }
}

void draw_overlays(cairo_t* cr) {
    if (state.hover_main >= 1 && state.hover_sub >= 1) {
        const int id = state.hover_main * 10 + state.hover_sub;
        const auto found = state.titles.find(id);
        if (found != state.titles.end() && !found->second.empty()) {
            const std::size_t count = std::min<std::size_t>(3, found->second.size());
            std::vector<std::string> lines;
            double widest = 0.0;
            set_font(cr, 7.8, CAIRO_FONT_WEIGHT_BOLD);
            for (std::size_t i = 0; i < count; ++i) {
                lines.push_back(ellipsize_utf8(found->second[i], 48));
                cairo_text_extents_t extents{};
                cairo_text_extents(cr, lines.back().c_str(), &extents);
                widest = std::max(widest, extents.width);
            }
            const double center_x = kWindowWidth / 2.0 + 24.0;
            const double pill_width = std::min<double>(widest + 30.0, 470.0);
            const double pill_height = 6.0 + count * 9.0;
            const double pill_x = center_x - pill_width / 2.0;
            const double pill_y = 34.0;
            rounded_rectangle(cr, pill_x, pill_y, pill_width, pill_height, 7.0);
            cairo_set_source_rgba(cr, 0.0, 0.18, 0.095, 0.88);
            cairo_fill_preserve(cr);
            cairo_set_line_width(cr, 1.0);
            cairo_set_source_rgba(cr, 0.31, 0.68, 0.47, 0.42);
            cairo_stroke(cr);
            for (std::size_t i = 0; i < lines.size(); ++i) {
                const double baseline = pill_y + 10.0 + i * 9.0;
                cairo_set_source_rgba(cr, 0.46, 0.82, 0.57, 0.95);
                cairo_arc(cr, pill_x + 9.0, baseline - 2.6, 1.7, 0, 2 * G_PI);
                cairo_fill(cr);
                cairo_set_source_rgba(cr, 0.81, 0.89, 0.82, 0.94);
                cairo_move_to(cr, pill_x + 16.0, baseline);
                cairo_show_text(cr, lines[i].c_str());
            }
        }
    }

    // A separate highlight glides between target cells. It intentionally sits
    // above the icon layer so motion remains legible even on occupied cells.
    if (state.hover_main >= 1 && state.hover_sub >= 1) {
        const auto wrap_coordinate = [](double value) {
            value = std::fmod(value, static_cast<double>(kGridSize));
            return value < 0.0 ? value + kGridSize : value;
        };
        const double wrapped_main = wrap_coordinate(state.visual_main);
        const double wrapped_sub = wrap_coordinate(state.visual_sub);
        cairo_save(cr);
        const double grid_span = kGridSize * kCell + (kGridSize - 1) * kGap;
        cairo_rectangle(cr, kGridX - 2, kGridY - 2, grid_span + 4, grid_span + 4);
        cairo_clip(cr);
        for (const int copy_y : {-1, 0, 1}) {
            for (const int copy_x : {-1, 0, 1}) {
                const double logical_main = wrapped_main + copy_x * kGridSize;
                const double logical_sub = wrapped_sub + copy_y * kGridSize;
                const double x = kGridX + logical_main * (kCell + kGap);
                const double y = kGridY + logical_sub * (kCell + kGap);
                rounded_rectangle(cr, x - 1.5, y - 1.5, kCell + 3, kCell + 3, 10);
                cairo_set_source_rgba(cr, 0.31, 0.68, 0.47, 0.075);
                cairo_fill_preserve(cr);
                cairo_set_line_width(cr, 2.2);
                cairo_set_source_rgba(cr, 0.46, 0.82, 0.57, 0.92);
                cairo_stroke(cr);
            }
        }
        cairo_restore(cr);
    }

    if (state.pressing && state.pressed_main >= 1 && state.pressed_sub >= 1) {
        const double elapsed = (g_get_monotonic_time() - state.press_started_us) / 1000.0;
        const double progress = std::clamp(elapsed / 165.0, 0.0, 1.0);
        const double pulse = std::sin(progress * G_PI);
        const double inset = 3.0 + pulse * 3.5;
        const double x = kGridX + (state.pressed_main - 1) * (kCell + kGap);
        const double y = kGridY + (state.pressed_sub - 1) * (kCell + kGap);
        rounded_rectangle(cr, x + inset, y + inset, kCell - inset * 2,
                          kCell - inset * 2, 7);
        cairo_set_source_rgba(cr, 0.0, 0.075, 0.043, 0.34 * pulse);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 1.5);
        cairo_set_source_rgba(cr, 0.62, 0.91, 0.70, 0.62 * pulse);
        cairo_stroke(cr);
    }
}

void ensure_base_surface(GtkWidget* widget) {
    const int scale = std::max(1, gtk_widget_get_scale_factor(widget));
    if (!state.base_surface || state.base_scale != scale) {
        invalidate_base();
        state.base_scale = scale;
        state.base_surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, kWindowWidth * scale, kWindowHeight * scale);
        cairo_surface_set_device_scale(state.base_surface, scale, scale);
        cairo_t* base_cr = cairo_create(state.base_surface);
        draw_base(base_cr);
        cairo_destroy(base_cr);
        cairo_surface_flush(state.base_surface);
    }
}

gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer) {
    ensure_base_surface(widget);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, state.base_surface, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    draw_overlays(cr);
    return FALSE;
}

bool pointer_workspace(double x, double y, int& main, int& sub) {
    const double relative_x = x - kGridX;
    const double relative_y = y - kGridY;
    if (relative_x < 0 || relative_y < 0) return false;
    const int column = static_cast<int>(relative_x) / (kCell + kGap);
    const int row = static_cast<int>(relative_y) / (kCell + kGap);
    if (column < 0 || column >= kGridSize || row < 0 || row >= kGridSize) return false;
    if (std::fmod(relative_x, kCell + kGap) >= kCell ||
        std::fmod(relative_y, kCell + kGap) >= kCell) return false;
    main = column + 1;
    sub = row + 1;
    return true;
}

void update_tooltip(int main, int sub) {
    if (!state.canvas || main < 1 || sub < 1) {
        gtk_widget_set_tooltip_text(state.canvas, nullptr);
        return;
    }
    const int id = main * 10 + sub;
    std::string tooltip = "Main " + std::to_string(main) + "  ·  Sub " +
                          std::to_string(sub) + "  ·  Workspace " + std::to_string(id);
    const auto found = state.workspaces.find(id);
    if (found != state.workspaces.end()) {
        for (const auto& group : found->second) {
            tooltip += "\n" + friendly_name(group.app);
            if (group.count > 1) tooltip += " ×" + std::to_string(group.count);
        }
    } else {
        tooltip += "\nEmpty";
    }
    gtk_widget_set_tooltip_text(state.canvas, tooltip.c_str());
}

void teleport_to(int main, int sub);

void dismiss_popup() {
    if (state.resident && state.window) {
        gtk_widget_hide(state.window);
    } else if (state.app) {
        g_application_quit(G_APPLICATION(state.app));
    }
}

gboolean animation_tick(GtkWidget*, GdkFrameClock* frame_clock, gpointer) {
    const gint64 now = gdk_frame_clock_get_frame_time(frame_clock);
    const double elapsed_ms = state.animation_last_us > 0
                                  ? (now - state.animation_last_us) / 1000.0
                                  : 16.0;
    state.animation_last_us = now;
    bool keep_running = false;

    if (state.hover_main >= 1 && state.hover_sub >= 1) {
        const double factor = 1.0 - std::exp(-elapsed_ms / 52.0);
        const double max_units = std::max(1.0, elapsed_ms * 0.80) / (kCell + kGap);
        const auto move_toward = [factor, max_units](double& value, double target) {
            const double delta = target - value;
            if (std::abs(delta) <= 0.003) {
                value = target;
                return false;
            }
            const double eased = std::abs(delta) * factor;
            value += std::copysign(std::min(eased, max_units), delta);
            return true;
        };

        if (move_toward(state.visual_main, state.target_main)) keep_running = true;
        if (move_toward(state.visual_sub, state.target_sub)) keep_running = true;
    }

    if (state.pressing) {
        const double press_ms = (now - state.press_started_us) / 1000.0;
        if (press_ms >= 165.0) {
            const int main = state.pressed_main;
            const int sub = state.pressed_sub;
            state.pressing = false;
            state.animation_source = 0;
            teleport_to(main, sub);
            return G_SOURCE_REMOVE;
        }
        keep_running = true;
    }

    if (state.canvas) gtk_widget_queue_draw(state.canvas);
    if (!keep_running) {
        // Prevent unbounded coordinate growth after queued wrap movements.
        state.visual_main = state.hover_main - 1.0;
        state.visual_sub = state.hover_sub - 1.0;
        state.target_main = state.visual_main;
        state.target_sub = state.visual_sub;
        state.animation_source = 0;
        state.animation_last_us = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

void ensure_animation() {
    if (state.animation_source != 0 || !state.canvas) return;
    state.animation_last_us = g_get_monotonic_time();
    state.animation_source = gtk_widget_add_tick_callback(
        state.canvas, animation_tick, nullptr, nullptr);
}

void select_workspace(int main, int sub) {
    if (main < 1 || main > 10 || sub < 1 || sub > 10) return;
    state.hover_main = main;
    state.hover_sub = sub;
    const auto nearest_copy = [](double reference, double coordinate) {
        return coordinate + std::round((reference - coordinate) / kGridSize) * kGridSize;
    };
    state.target_main = nearest_copy(state.target_main, main - 1.0);
    state.target_sub = nearest_copy(state.target_sub, sub - 1.0);
    update_tooltip(main, sub);
    ensure_animation();
}

void schedule_teleport(int main, int sub) {
    if (state.pressing) return;
    select_workspace(main, sub);
    state.pressing = true;
    state.pressed_main = main;
    state.pressed_sub = sub;
    state.press_started_us = g_get_monotonic_time();
    ensure_animation();
}

gboolean on_motion(GtkWidget* widget, GdkEventMotion* event, gpointer) {
    int main = -1;
    int sub = -1;
    if (pointer_workspace(event->x, event->y, main, sub) &&
        (main != state.hover_main || sub != state.hover_sub)) {
        state.user_moved_selection = true;
        select_workspace(main, sub);
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

gboolean on_leave(GtkWidget* widget, GdkEventCrossing*, gpointer) {
    // Keep the current selection when the mouse leaves so keyboard navigation
    // can continue from the last pointed-at cell.
    gtk_widget_queue_draw(widget);
    return TRUE;
}

void teleport_to(int main, int sub) {
    const std::string script = std::string(g_get_home_dir()) +
                               "/.config/hypr/goto_workspace.sh";
    const std::string main_text = std::to_string(main);
    const std::string sub_text = std::to_string(sub);
    gchar* argv[] = {
        const_cast<gchar*>(script.c_str()),
        const_cast<gchar*>(main_text.c_str()),
        const_cast<gchar*>(sub_text.c_str()),
        nullptr,
    };
    GError* error = nullptr;
    g_spawn_async(nullptr, argv, nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, &error);
    if (error) g_error_free(error);

    dismiss_popup();
}

gboolean on_button(GtkWidget*, GdkEventButton* event, gpointer) {
    if (event->button != GDK_BUTTON_PRIMARY) return FALSE;
    int main = 0;
    int sub = 0;
    if (!pointer_workspace(event->x, event->y, main, sub)) return FALSE;
    schedule_teleport(main, sub);
    return TRUE;
}

gboolean on_key(GtkWidget*, GdkEventKey* event, gpointer) {
    if (event->keyval == GDK_KEY_Escape) {
        dismiss_popup();
        return TRUE;
    }
    if (state.pressing) return TRUE;

    if (state.hover_main < 1 || state.hover_sub < 1) {
        if (!workspace_coordinates(state.active_workspace, state.hover_main, state.hover_sub)) {
            state.hover_main = 1;
            state.hover_sub = 1;
        }
    }

    bool moved = true;
    switch (event->keyval) {
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            state.hover_main = state.hover_main == 1 ? 10 : state.hover_main - 1;
            state.target_main -= 1.0;
            break;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            state.hover_main = state.hover_main == 10 ? 1 : state.hover_main + 1;
            state.target_main += 1.0;
            break;
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            state.hover_sub = state.hover_sub == 1 ? 10 : state.hover_sub - 1;
            state.target_sub -= 1.0;
            break;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            state.hover_sub = state.hover_sub == 10 ? 1 : state.hover_sub + 1;
            state.target_sub += 1.0;
            break;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_space:
            schedule_teleport(state.hover_main, state.hover_sub);
            return TRUE;
        default:
            moved = false;
            break;
    }
    if (moved) {
        state.user_moved_selection = true;
        update_tooltip(state.hover_main, state.hover_sub);
        ensure_animation();
        gtk_widget_queue_draw(state.canvas);
        return TRUE;
    }
    return FALSE;
}

void cleanup() {
    invalidate_base();
    for (auto& [_, pixbuf] : state.icons) {
        if (pixbuf) g_object_unref(pixbuf);
    }
    state.icons.clear();
}

gboolean populate_state(gpointer) {
    state.refresh_source = 0;
    if (!state.window || !state.canvas) return G_SOURCE_REMOVE;
    refresh_state();
    if (!state.user_moved_selection) {
        workspace_coordinates(state.active_workspace, state.hover_main, state.hover_sub);
        state.visual_main = std::max(0, state.hover_main - 1);
        state.visual_sub = std::max(0, state.hover_sub - 1);
        state.target_main = state.visual_main;
        state.target_sub = state.visual_sub;
    }
    update_tooltip(state.hover_main, state.hover_sub);
    ensure_base_surface(state.canvas);
    gtk_widget_queue_draw(state.canvas);
    return G_SOURCE_REMOVE;
}

void schedule_population(guint delay_ms) {
    if (state.refresh_source == 0) {
        state.refresh_source = g_timeout_add(delay_ms, populate_state, nullptr);
    }
}

void show_popup() {
    state.user_moved_selection = false;
    gtk_widget_show_all(state.window);
    gtk_window_present(GTK_WINDOW(state.window));
    schedule_population(16);
}

void activate(GtkApplication* app, gpointer) {
    if (state.window) {
        if (gtk_widget_get_visible(state.window)) {
            dismiss_popup();
        } else {
            show_popup();
        }
        return;
    }

    state.app = app;
    state.window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state.window), "Workspace Field");
    gtk_window_set_default_size(GTK_WINDOW(state.window), kWindowWidth, kWindowHeight);
    gtk_window_set_resizable(GTK_WINDOW(state.window), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(state.window), FALSE);
    gtk_window_set_position(GTK_WINDOW(state.window), GTK_WIN_POS_CENTER);
    gtk_window_set_keep_above(GTK_WINDOW(state.window), TRUE);
    gtk_widget_set_app_paintable(state.window, TRUE);

    GdkScreen* screen = gtk_widget_get_screen(state.window);
    if (GdkVisual* visual = gdk_screen_get_rgba_visual(screen)) {
        gtk_widget_set_visual(state.window, visual);
    }

    state.canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(state.canvas, kWindowWidth, kWindowHeight);
    gtk_widget_add_events(state.canvas, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK |
                                          GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(state.window), state.canvas);
    state.icon_theme = gtk_icon_theme_get_default();

    g_signal_connect(state.canvas, "draw", G_CALLBACK(on_draw), nullptr);
    g_signal_connect(state.canvas, "motion-notify-event", G_CALLBACK(on_motion), nullptr);
    g_signal_connect(state.canvas, "leave-notify-event", G_CALLBACK(on_leave), nullptr);
    g_signal_connect(state.canvas, "button-press-event", G_CALLBACK(on_button), nullptr);
    g_signal_connect(state.window, "key-press-event", G_CALLBACK(on_key), nullptr);
    g_signal_connect(state.window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) {
        state.window = nullptr;
        state.canvas = nullptr;
    }), nullptr);

    // The resident instance builds this once at login and keeps it warm.
    state.hover_main = 1;
    state.hover_sub = 1;
    state.visual_main = 0.0;
    state.visual_sub = 0.0;
    state.target_main = state.visual_main;
    state.target_sub = state.visual_sub;
    if (state.resident) {
        if (!state.held) {
            g_application_hold(G_APPLICATION(app));
            state.held = true;
        }
        gtk_widget_realize(state.window);
        gtk_widget_realize(state.canvas);
        schedule_population(1);
    } else {
        show_popup();
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<char*> filtered_args;
    filtered_args.reserve(argc + 1);
    filtered_args.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        if (g_strcmp0(argv[i], "--daemon") == 0) {
            state.resident = true;
        } else {
            filtered_args.push_back(argv[i]);
        }
    }
    filtered_args.push_back(nullptr);

    g_set_prgname("workspace-field");
    GtkApplication* app = gtk_application_new("pl.aridlin.WorkspaceField", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    const int status = g_application_run(
        G_APPLICATION(app), static_cast<int>(filtered_args.size() - 1), filtered_args.data());
    cleanup();
    g_object_unref(app);
    return status;
}
