#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cctype>
#include <cstring>
#include <cassert>
#include <array>
#include <cstdlib>
#include <chrono>
#include <csignal>

static void fatal(const std::string& msg) {
    std::cerr << "Fatal error: " << msg << std::endl;
    std::exit(1);
}

Display *display;
Window root;
Window overlay = 0;
bool overlayVisible = false;
std::chrono::steady_clock::time_point overlay_start_time;

std::string typed_chars = "";
std::string highlighted_cell = "";
std::string highlighted_subcell = "";

bool toggle_in_progress = false;  // prevent repeated toggling while keys held
int overlay_timeout_seconds = 30; // timeout in seconds
int overlay_timeout_ms = overlay_timeout_seconds * 1000; // derived milliseconds

KeyCode keycode_h;
KeyCode keycode_t;

unsigned long orange_pixel;       // pixel value for orange text (subcells)
unsigned long bright_pixel;       // pixel value for bright main cell text
unsigned long dark_pixel;         // pixel value for dark text

enum ClickMode { LEFT_CLICK, RIGHT_CLICK, MIDDLE_CLICK, DOUBLE_CLICK };
ClickMode current_click_mode = LEFT_CLICK;

volatile std::sig_atomic_t keepRunning = 1;

void signal_handler(int signum) {
    (void)signum; // unused
    keepRunning = 0;
}

void click_pointer(ClickMode mode);

void destroy_overlay(bool should_click = false);

void hide_overlay_without_click() {
    if (overlay) {
        (void)XSetInputFocus(display, root, RevertToParent, CurrentTime);

        (void)XDestroyWindow(display, overlay);
        overlay = 0;
        highlighted_cell = "";
        highlighted_subcell = "";
        typed_chars = "";

        overlayVisible = false;
    }
}

void move_pointer_to_cell(const std::string& cell_id, int width, int height) {
    int grid_size = 50;
    int id_counter = 0;
    int found_x = -1, found_y = -1;

    for (int y = 0; y < height; y += grid_size) {
        for (int x = 0; x < width; x += grid_size) {
            std::string cid;
            if (id_counter < 260) { // 26 * 10
                cid += 'a' + (id_counter / 10) % 26;
                cid += '0' + (id_counter % 10);
            } else {
                cid += 'a' + ((id_counter - 260) / 26) % 26;
                cid += 'a' + ((id_counter - 260) % 26);
            }

            if (cid == cell_id) {
                found_x = x + grid_size / 2;
                found_y = y + grid_size / 2;
                break;
            }
            id_counter++;
        }
        if (found_x != -1) break;
    }

    if (found_x != -1 && found_y != -1) {
        int status = XWarpPointer(display, None, root, 0, 0, 0, 0, found_x, found_y);
        if (status == BadValue) {
            std::cerr << "XWarpPointer failed for cell " << cell_id << "\n";
        }
        XFlush(display);
    }
}

void move_pointer_to_subcell(const std::string& main_cell_id, const std::string& subcell_id, int width, int height) {
    int grid_size = 50;
    int id_counter = 0;
    int cell_x = -1, cell_y = -1;

    // Find main cell position
    for (int y = 0; y < height; y += grid_size) {
        for (int x = 0; x < width; x += grid_size) {
            std::string cid;
            if (id_counter < 260) {
                cid += 'a' + (id_counter / 10) % 26;
                cid += '0' + (id_counter % 10);
            } else {
                cid += 'a' + ((id_counter - 260) / 26) % 26;
                cid += 'a' + ((id_counter - 260) % 26);
            }

            if (cid == main_cell_id) {
                cell_x = x;
                cell_y = y;
                break;
            }
            id_counter++;
        }
        if (cell_x != -1) break;
    }

    if (cell_x == -1 || cell_y == -1) return;

    int sub_size = grid_size / 3;
    int sub_x = -1, sub_y = -1;

    std::array<char, 9> subcell_keys = {'g', 'c', 'r', 'h', 't', 'n', 'm', 'w', 'v'};

    for (int sy = 0; sy < 3; ++sy) {
        for (int sx = 0; sx < 3; ++sx) {
            int index = sy * 3 + sx;
            std::string scid;
            scid += subcell_keys[index];

            if (scid == subcell_id) {
                sub_x = cell_x + sx * sub_size + sub_size / 2;
                sub_y = cell_y + sy * sub_size + sub_size / 2;
                break;
            }
        }
        if (sub_x != -1) break;
    }

    if (sub_x != -1 && sub_y != -1) {
        int status = XWarpPointer(display, None, root, 0, 0, 0, 0, sub_x, sub_y);
        if (status == BadValue) {
            std::cerr << "XWarpPointer failed for subcell " << subcell_id << "\n";
        }
        XFlush(display);
    }
}

void draw_grid(Window win, int width, int height) {
    GC gc = XCreateGC(display, win, 0, nullptr);
    if (!gc) {
        fatal("Failed to create graphics context");
    }

    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));

    int grid_size = 50;

    int id_counter = 0;
    for (int y = 0; y < height; y += grid_size) {
        for (int x = 0; x < width; x += grid_size) {
            std::string cell_id;

            if (id_counter < 260) {
                cell_id += 'a' + (id_counter / 10) % 26;
                cell_id += '0' + (id_counter % 10);
            } else {
                cell_id += 'a' + ((id_counter - 260) / 26) % 26;
                cell_id += 'a' + ((id_counter - 260) % 26);
            }

            bool is_highlighted = (cell_id == highlighted_cell);

            if (is_highlighted) {
                XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                XFillRectangle(display, win, gc, x, y, grid_size, grid_size);
            }

            XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
            XDrawLine(display, win, gc, x, y, x, y + grid_size);
            XDrawLine(display, win, gc, x, y, x + grid_size, y);

            int cx = x + grid_size / 2;
            int cy = y + grid_size / 2;

            int direction, ascent, descent;
            XCharStruct overall;
            XFontStruct *font = XQueryFont(display, XGContextFromGC(gc));
            if (font) {
                XTextExtents(font, cell_id.c_str(), 2, &direction, &ascent, &descent, &overall);
                cx -= overall.width / 2;
                cy += (ascent - descent) / 2;
            }

            // Use bright color for main cell text
            XSetForeground(display, gc, bright_pixel);
            XDrawString(display, win, gc, cx, cy, cell_id.c_str(), 2);

            bool show_subgrid = false;
            if (is_highlighted) {
                if (highlighted_subcell != "" || (typed_chars.length() == 2 && highlighted_cell == typed_chars)) {
                    show_subgrid = true;
                }
            }

            if (show_subgrid) {
                int sub_size = grid_size / 3;
                std::array<char, 9> subcell_keys = {'g', 'c', 'r', 'h', 't', 'n', 'm', 'w', 'v'};
                for (int sy = 0; sy < 3; ++sy) {
                    for (int sx = 0; sx < 3; ++sx) {
                        int sub_x = x + sx * sub_size;
                        int sub_y = y + sy * sub_size;

                        int index = sy * 3 + sx;
                        std::string scid;
                        scid += subcell_keys[index];

                        bool sub_highlight = (highlighted_subcell != "" && scid == highlighted_subcell);

                        if (sub_highlight) {
                            XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                            XFillRectangle(display, win, gc, sub_x, sub_y, sub_size, sub_size);
                        }

                        XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                        XDrawLine(display, win, gc, sub_x, sub_y, sub_x + sub_size, sub_y);
                        XDrawLine(display, win, gc, sub_x, sub_y, sub_x, sub_y + sub_size);

                        int scx = sub_x + sub_size / 2;
                        int scy = sub_y + sub_size / 2;

                        if (font) {
                            XTextExtents(font, scid.c_str(), 1, &direction, &ascent, &descent, &overall);
                            scx -= overall.width / 2;
                            scy += (ascent - descent) / 2;
                        }

                        if (is_highlighted) {
                            XSetForeground(display, gc, dark_pixel);
                        } else {
                            XSetForeground(display, gc, orange_pixel);
                        }
                        XDrawString(display, win, gc, scx, scy, scid.c_str(), 1);
                    }
                }
            }

            id_counter++;
        }
    }

    XFreeGC(display, gc);
}

void create_overlay() {
    int screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    overlay = XCreateWindow(display, root, 0, 0, width, height, 0,
                            DefaultDepth(display, screen),
                            InputOutput,
                            DefaultVisual(display, screen),
                            CWOverrideRedirect | CWBackPixel | CWBorderPixel,
                            &attrs);
    if (!overlay) {
        fatal("Failed to create overlay window");
    }

    unsigned long opacity = 0x80000000;
    Atom property = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
    Atom cardinal_atom = XInternAtom(display, "CARDINAL", False);
    (void)XChangeProperty(display, overlay, property, cardinal_atom, 32, PropModeReplace,
                    (unsigned char *)&opacity, 1);

    (void)XSelectInput(display, overlay, ExposureMask | KeyPressMask);
    (void)XMapRaised(display, overlay);
    XFlush(display);

    (void)XSetInputFocus(display, overlay, RevertToParent, CurrentTime);

    draw_grid(overlay, width, height);
}

void click_pointer(ClickMode mode) {
    int button = 1;  // default left
    int click_count = 1;

    switch (mode) {
        case LEFT_CLICK: button = 1; click_count = 1; break;
        case RIGHT_CLICK: button = 3; click_count = 1; break;
        case MIDDLE_CLICK: button = 2; click_count = 1; break;
        case DOUBLE_CLICK: button = 1; click_count = 2; break;
    }

    for (int i = 0; i < click_count; ++i) {
        if (!XTestFakeButtonEvent(display, button, True, CurrentTime)) {
            std::cerr << "Failed to fake button press\n";
        }
        XFlush(display);
        usleep(10000);
        if (!XTestFakeButtonEvent(display, button, False, CurrentTime)) {
            std::cerr << "Failed to fake button release\n";
        }
        XFlush(display);
        if (click_count == 2) usleep(100000);  // small delay between double clicks
    }
}

void destroy_overlay(bool should_click) {
    if (overlay) {
        (void)XSetInputFocus(display, root, RevertToParent, CurrentTime);

        (void)XDestroyWindow(display, overlay);
        overlay = 0;
        highlighted_cell = "";
        highlighted_subcell = "";
        typed_chars = "";

        overlayVisible = false;

        if (should_click) {
            click_pointer(current_click_mode);
        }
    }
}

void grab_key_with_modifiers(Display *disp, Window win, int keycode, int base_mods) {
    unsigned int modifiers[] = {
        0,
        Mod2Mask,
        LockMask,
        Mod2Mask | LockMask
    };

    for (unsigned int mod : modifiers) {
        int result = XGrabKey(disp, keycode, base_mods | mod, win, True, GrabModeAsync, GrabModeAsync);
        if (result == 0) {
            std::cerr << "Warning: failed to grab key with modifiers mask " << (base_mods | mod) << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Unable to open X display\n";
        return 1;
    }

    root = DefaultRootWindow(display);

    Colormap cmap = DefaultColormap(display, DefaultScreen(display));
    XColor color;
    if (!XParseColor(display, cmap, "#FFA500", &color) || !XAllocColor(display, cmap, &color)) {
        std::cerr << "Failed to allocate orange color, using black instead\n";
        orange_pixel = BlackPixel(display, DefaultScreen(display));
    } else {
        orange_pixel = color.pixel;
    }

    XColor bright_color;
    if (!XParseColor(display, cmap, "#FFFF00", &bright_color) || !XAllocColor(display, cmap, &bright_color)) {
        std::cerr << "Failed to allocate bright yellow color, using white instead\n";
        bright_pixel = WhitePixel(display, DefaultScreen(display));
    } else {
        bright_pixel = bright_color.pixel;
    }

    XColor dark_color;
    if (!XParseColor(display, cmap, "#333333", &dark_color) || !XAllocColor(display, cmap, &dark_color)) {
        std::cerr << "Failed to allocate dark color, using black instead\n";
        dark_pixel = BlackPixel(display, DefaultScreen(display));
    } else {
        dark_pixel = dark_color.pixel;
    }

    unsigned int ctrl_mask = ControlMask;

    keycode_h = XKeysymToKeycode(display, XK_h);
    keycode_t = XKeysymToKeycode(display, XK_t);

    grab_key_with_modifiers(display, root, keycode_h, ctrl_mask);
    grab_key_with_modifiers(display, root, keycode_t, ctrl_mask);

    (void)XSelectInput(display, root, KeyPressMask);

    // Parse command‑line arguments for overlay timeout (seconds)
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--timeout" && i + 1 < argc) {
            overlay_timeout_seconds = std::stoi(argv[i + 1]);
            overlay_timeout_ms = overlay_timeout_seconds * 1000;
            ++i; // skip the value we just consumed
        }
    }

    std::signal(SIGINT, signal_handler);
    while (keepRunning) {
        // Check timeout before blocking for events
        if (overlayVisible) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - overlay_start_time).count() > overlay_timeout_ms) {
                hide_overlay_without_click();
                // Continue to next iteration to avoid processing stale events
                continue;
            }
        }

        // Process pending events if any; otherwise wait briefly
        if (XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);

            if (ev.type == KeyPress) {
                XKeyEvent xkey = ev.xkey;
                KeySym keysym = XLookupKeysym(&xkey, 0);

                char keys_return[32];
                XQueryKeymap(display, keys_return);

                bool ctrl_held = (xkey.state & ctrl_mask) == ctrl_mask;
                bool h_down = (keys_return[keycode_h / 8] & (1 << (keycode_h % 8))) != 0;
                bool t_down = (keys_return[keycode_t / 8] & (1 << (keycode_t % 8))) != 0;

                if (ctrl_held && h_down && t_down) {
                    if (!toggle_in_progress) {
                        overlayVisible = !overlayVisible;
                        if (overlayVisible) {
                            create_overlay();
                            overlay_start_time = std::chrono::steady_clock::now();
                        } else {
                            destroy_overlay();
                        }
                        toggle_in_progress = true;
                    }
                } else {
                    toggle_in_progress = false;
                }

                if (overlayVisible && ev.xkey.window == overlay) {
                    // Change click mode with Ctrl+number
                    if (ctrl_held) {
                        if (keysym == XK_1) {
                            current_click_mode = LEFT_CLICK;
                            continue;
                        } else if (keysym == XK_2) {
                            current_click_mode = RIGHT_CLICK;
                            continue;
                        } else if (keysym == XK_3) {
                            current_click_mode = MIDDLE_CLICK;
                            continue;
                        } else if (keysym == XK_4) {
                            current_click_mode = DOUBLE_CLICK;
                            continue;
                        }
                    }

                    if (keysym == XK_Escape) {
                        hide_overlay_without_click();
                        continue;
                    }

                    if ((keysym == XK_Return || keysym == XK_KP_Enter)) {
                        int screen = DefaultScreen(display);
                        int width = DisplayWidth(display, screen);
                        int height = DisplayHeight(display, screen);

                        if (typed_chars.length() >= 2) {
                            highlighted_subcell = "";
                            draw_grid(overlay, width, height);
                            move_pointer_to_cell(highlighted_cell, width, height);
                        }
                        destroy_overlay(true);
                        continue;
                    }

                    char buf[32];
                    int len = XLookupString(&xkey, buf, sizeof(buf), &keysym, nullptr);
                    if (len == 1 && std::isalnum(buf[0])) {
                        char c = std::tolower(buf[0]);
                        typed_chars += c;
                        if (typed_chars.length() > 3) {
                            typed_chars = typed_chars.substr(typed_chars.length() - 3);
                        }

                        int screen = DefaultScreen(display);
                        int width = DisplayWidth(display, screen);
                        int height = DisplayHeight(display, screen);

                        if (typed_chars.length() == 2) {
                            highlighted_cell = typed_chars;
                            highlighted_subcell = "";
                            draw_grid(overlay, width, height);
                            move_pointer_to_cell(highlighted_cell, width, height);
                        } else if (typed_chars.length() == 3) {
                            std::array<char, 9> subcell_keys = {'g', 'c', 'r', 'h', 't', 'n', 'm', 'w', 'v'};
                            std::string subid;
                            bool valid_subcell = false;
                            for (char key : subcell_keys) {
                                if (typed_chars[2] == key) {
                                    subid += key;
                                    valid_subcell = true;
                                    break;
                                }
                            }

                            if (valid_subcell) {
                                highlighted_subcell = subid;
                                draw_grid(overlay, width, height);
                                move_pointer_to_subcell(highlighted_cell, highlighted_subcell, width, height);
                                destroy_overlay(true);
                            } else {
                                typed_chars = typed_chars.substr(0, 2); // remove invalid char
                            }
                        }
                    }
                }
            } else if (ev.type == Expose && overlayVisible) {
                int screen = DefaultScreen(display);
                int width = DisplayWidth(display, screen);
                int height = DisplayHeight(display, screen);
                draw_grid(overlay, width, height);
            }
        } else {
            // No events pending; sleep briefly to avoid busy loop
            usleep(10000);
        }
    }

    XCloseDisplay(display);
    return 0;
}
