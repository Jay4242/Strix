#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cctype>

Display *display;
Window root;
Window overlay = 0;
bool overlayVisible = false;

std::string typed_chars = "";
std::string highlighted_cell = "";

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

            // Highlight if matches
            if (cell_id == highlighted_cell) {
                XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
                XFillRectangle(display, win, gc, x, y, grid_size, grid_size);
                XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
            } else {
                XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
            }

            // Draw vertical lines
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

            XDrawString(display, win, gc, cx, cy, cell_id.c_str(), 2);

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

    // Do NOT make input transparent, so we can receive key events
    // XShapeCombineRectangles(display, overlay, ShapeInput, 0, 0, nullptr, 0, ShapeSet, 0);

    XSelectInput(display, overlay, ExposureMask | KeyPressMask);
    XMapRaised(display, overlay);
    XFlush(display);

    // Set input focus to overlay so it receives key events
    XSetInputFocus(display, overlay, RevertToParent, CurrentTime);

    draw_grid(overlay, width, height);
}

void destroy_overlay() {
    if (overlay) {
        // Clear focus back to root
        XSetInputFocus(display, root, RevertToParent, CurrentTime);

        XDestroyWindow(display, overlay);
        overlay = 0;
        highlighted_cell = "";
        typed_chars = "";
    }
}

void grab_key_with_modifiers(Display *disp, Window win, int keycode, int base_mods) {
    unsigned int modifiers[] = {
        0,
        Mod2Mask,            // NumLock
        LockMask,            // CapsLock
        Mod2Mask | LockMask  // Both
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

    int modifiers_mask = ControlMask | ShiftMask;
    KeyCode keycode = XKeysymToKeycode(display, XK_F9);

    grab_key_with_modifiers(display, root, keycode, modifiers_mask);

    XSelectInput(display, root, KeyPressMask);

    while (true) {
        XEvent ev;
        XNextEvent(display, &ev);

        if (ev.type == KeyPress) {
            XKeyEvent xkey = ev.xkey;
            KeySym keysym = XLookupKeysym(&xkey, 0);

            if ((xkey.state & modifiers_mask) == modifiers_mask && xkey.keycode == keycode) {
                overlayVisible = !overlayVisible;
                if (overlayVisible) {
                    create_overlay();
                } else {
                    destroy_overlay();
                }
            } else if (overlayVisible && ev.xkey.window == overlay) {
                // accumulate typed characters when overlay is visible and has focus
                char buf[32];
                int len = XLookupString(&xkey, buf, sizeof(buf), &keysym, nullptr);
                if (len == 1 && std::isalnum(buf[0])) {
                    char c = std::tolower(buf[0]);
                    typed_chars += c;
                    if (typed_chars.length() > 2) {
                        typed_chars = typed_chars.substr(typed_chars.length() - 2);
                    }
                    if (typed_chars.length() == 2) {
                        highlighted_cell = typed_chars;
                        int screen = DefaultScreen(display);
                        int width = DisplayWidth(display, screen);
                        int height = DisplayHeight(display, screen);
                        draw_grid(overlay, width, height);
                        move_pointer_to_cell(highlighted_cell, width, height);
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
