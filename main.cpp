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

Display *display;
Window root;
Window overlay = 0;
bool overlayVisible = false;

std::string typed_chars = "";
std::string highlighted_cell = "";
std::string highlighted_subcell = "";

bool toggle_in_progress = false;  // prevent repeated toggling while keys held

KeyCode keycode_h;
KeyCode keycode_t;

unsigned long orange_pixel;  // pixel value for orange text
unsigned long dark_pixel;    // pixel value for dark text

void click_pointer();

void destroy_overlay();

void hide_overlay_without_click() {
    if (overlay) {
        XSetInputFocus(display, root, RevertToParent, CurrentTime);

        XDestroyWindow(display, overlay);
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
        XWarpPointer(display, None, root, 0, 0, 0, 0, found_x, found_y);
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

    // Subgrid is 3x3 inside main cell
    int sub_size = grid_size / 3;
    int sub_x = -1, sub_y = -1;

    for (int sy = 0; sy < 3; ++sy) {
        for (int sx = 0; sx < 3; ++sx) {
            std::string scid;
            scid += 'a' + sx; // 'a','b','c'
            scid += '0' + sy; // '0','1','2'

            if (scid == subcell_id) {
                sub_x = cell_x + sx * sub_size + sub_size / 2;
                sub_y = cell_y + sy * sub_size + sub_size / 2;
                break;
            }
        }
        if (sub_x != -1) break;
    }

    if (sub_x != -1 && sub_y != -1) {
        XWarpPointer(display, None, root, 0, 0, 0, 0, sub_x, sub_y);
        XFlush(display);
    }
}

void draw_grid(Window win, int width, int height) {
    GC gc = XCreateGC(display, win, 0, nullptr);
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));

    int grid_size = 50; // pixels between lines

    int id_counter = 0;
    for (int y = 0; y < height; y += grid_size) {
        for (int x = 0; x < width; x += grid_size) {
            std::string cell_id;

            if (id_counter < 260) { // 26 * 10
                cell_id += 'a' + (id_counter / 10) % 26;
                cell_id += '0' + (id_counter % 10);
            } else {
                cell_id += 'a' + ((id_counter - 260) / 26) % 26;
                cell_id += 'a' + ((id_counter - 260) % 26);
            }

            bool is_highlighted = (cell_id == highlighted_cell);

            // Highlight main cell
            if (is_highlighted) {
                XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                XFillRectangle(display, win, gc, x, y, grid_size, grid_size);
            }

            // Draw vertical lines
            XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
            XDrawLine(display, win, gc, x, y, x, y + grid_size);
            // Draw horizontal lines
            XDrawLine(display, win, gc, x, y, x + grid_size, y);

            // Draw text
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

            XSetForeground(display, gc, orange_pixel);
            XDrawString(display, win, gc, cx, cy, cell_id.c_str(), 2);

            // Draw subgrid if this is the highlighted cell and either:
            // - subcell mode active (highlighted_subcell != "")
            // - or user has typed exactly 2 chars (main cell selected, waiting for subcell)
            bool show_subgrid = false;
            if (is_highlighted) {
                if (highlighted_subcell != "" || (typed_chars.length() == 2 && highlighted_cell == typed_chars)) {
                    show_subgrid = true;
                }
            }

            if (show_subgrid) {
                int sub_size = grid_size / 3;
                for (int sy = 0; sy < 3; ++sy) {
                    for (int sx = 0; sx < 3; ++sx) {
                        int sub_x = x + sx * sub_size;
                        int sub_y = y + sy * sub_size;

                        std::string scid;
                        scid += 'a' + sx;
                        scid += '0' + sy;

                        bool sub_highlight = (highlighted_subcell != "" && scid == highlighted_subcell);

                        if (sub_highlight) {
                            XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                            XFillRectangle(display, win, gc, sub_x, sub_y, sub_size, sub_size);
                        }

                        // Draw subgrid lines
                        XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                        XDrawLine(display, win, gc, sub_x, sub_y, sub_x + sub_size, sub_y);
                        XDrawLine(display, win, gc, sub_x, sub_y, sub_x, sub_y + sub_size);

                        // Draw subcell text
                        int scx = sub_x + sub_size / 2;
                        int scy = sub_y + sub_size / 2;

                        if (font) {
                            XTextExtents(font, scid.c_str(), 2, &direction, &ascent, &descent, &overall);
                            scx -= overall.width / 2;
                            scy += (ascent - descent) / 2;
                        }

                        // Use dark color for subgrid text inside highlighted cell
                        if (is_highlighted) {
                            XSetForeground(display, gc, dark_pixel);
                        } else {
                            XSetForeground(display, gc, orange_pixel);
                        }
                        XDrawString(display, win, gc, scx, scy, scid.c_str(), 2);
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

    // Make window transparent
    unsigned long opacity = 0x40000000; // semi-transparent
    Atom property = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
    Atom cardinal_atom = XInternAtom(display, "CARDINAL", False);
    XChangeProperty(display, overlay, property, cardinal_atom, 32, PropModeReplace,
                    (unsigned char *)&opacity, 1);

    XSelectInput(display, overlay, ExposureMask | KeyPressMask);
    XMapRaised(display, overlay);
    XFlush(display);

    XSetInputFocus(display, overlay, RevertToParent, CurrentTime);

    draw_grid(overlay, width, height);
}

void click_pointer() {
    XTestFakeButtonEvent(display, 1, True, CurrentTime);
    XFlush(display);
    usleep(10000);
    XTestFakeButtonEvent(display, 1, False, CurrentTime);
    XFlush(display);
}

void destroy_overlay() {
    if (overlay) {
        XSetInputFocus(display, root, RevertToParent, CurrentTime);

        XDestroyWindow(display, overlay);
        overlay = 0;
        highlighted_cell = "";
        highlighted_subcell = "";
        typed_chars = "";

        overlayVisible = false;  // fix: update toggle state after destroying overlay

        click_pointer();
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
        if (result != 0) {
            // success
        } else {
            std::cerr << "Warning: failed to grab key with modifiers mask " << (base_mods | mod) << "\n";
        }
    }
}

int main() {
    display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Cannot open display\n";
        return 1;
    }

    root = DefaultRootWindow(display);

    // Allocate orange color
    Colormap cmap = DefaultColormap(display, DefaultScreen(display));
    XColor color;
    if (!XParseColor(display, cmap, "#FFA500", &color) || !XAllocColor(display, cmap, &color)) {
        std::cerr << "Failed to allocate orange color, using black instead\n";
        orange_pixel = BlackPixel(display, DefaultScreen(display));
    } else {
        orange_pixel = color.pixel;
    }

    // Allocate dark color (dark gray)
    XColor dark_color;
    if (!XParseColor(display, cmap, "#333333", &dark_color) || !XAllocColor(display, cmap, &dark_color)) {
        std::cerr << "Failed to allocate dark color, using black instead\n";
        dark_pixel = BlackPixel(display, DefaultScreen(display));
    } else {
        dark_pixel = dark_color.pixel;
    }

    int ctrl_mask = ControlMask;

    keycode_h = XKeysymToKeycode(display, XK_h);
    keycode_t = XKeysymToKeycode(display, XK_t);

    grab_key_with_modifiers(display, root, keycode_h, ctrl_mask);
    grab_key_with_modifiers(display, root, keycode_t, ctrl_mask);

    XSelectInput(display, root, KeyPressMask);

    while (true) {
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
                    } else {
                        destroy_overlay();
                    }
                    toggle_in_progress = true;
                }
            } else {
                toggle_in_progress = false;
            }

            if (overlayVisible && ev.xkey.window == overlay) {
                if (keysym == XK_Escape) {
                    hide_overlay_without_click();
                    continue;
                }

                // New: handle Enter/Return key
                if ((keysym == XK_Return || keysym == XK_KP_Enter) && typed_chars.length() >= 2) {
                    int screen = DefaultScreen(display);
                    int width = DisplayWidth(display, screen);
                    int height = DisplayHeight(display, screen);

                    highlighted_subcell = "";  // clear subcell highlight
                    draw_grid(overlay, width, height);
                    move_pointer_to_cell(highlighted_cell, width, height);
                    destroy_overlay();
                    continue;
                }

                char buf[32];
                int len = XLookupString(&xkey, buf, sizeof(buf), &keysym, nullptr);
                if (len == 1 && std::isalnum(buf[0])) {
                    char c = std::tolower(buf[0]);
                    typed_chars += c;
                    if (typed_chars.length() > 4) {
                        typed_chars = typed_chars.substr(typed_chars.length() - 4);
                    }

                    int screen = DefaultScreen(display);
                    int width = DisplayWidth(display, screen);
                    int height = DisplayHeight(display, screen);

                    if (typed_chars.length() == 2) {
                        highlighted_cell = typed_chars;
                        highlighted_subcell = "";
                        draw_grid(overlay, width, height);
                        move_pointer_to_cell(highlighted_cell, width, height);
                    } else if (typed_chars.length() == 4) {
                        std::string subid = typed_chars.substr(2,2);
                        highlighted_subcell = subid;
                        draw_grid(overlay, width, height);
                        move_pointer_to_subcell(highlighted_cell, highlighted_subcell, width, height);
                        // After subcell selection, reset state and hide overlay
                        destroy_overlay();
                    }
                }
            }
        } else if (ev.type == Expose && overlayVisible) {
            int screen = DefaultScreen(display);
            int width = DisplayWidth(display, screen);
            int height = DisplayHeight(display, screen);
            draw_grid(overlay, width, height);
        }
    }

    XCloseDisplay(display);
    return 0;
}
