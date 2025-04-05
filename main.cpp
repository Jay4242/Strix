#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <iostream>

Display *display;
Window root;
Window overlay = 0;
bool overlayVisible = false;

void draw_grid(Window win, int width, int height) {
    GC gc = XCreateGC(display, win, 0, nullptr);
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));

    int grid_size = 50; // pixels between lines
    for (int x = 0; x < width; x += grid_size) {
        XDrawLine(display, win, gc, x, 0, x, height);
    }
    for (int y = 0; y < height; y += grid_size) {
        XDrawLine(display, win, gc, 0, y, width, y);
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

    // Make window input transparent (click-through)
    XShapeCombineRectangles(display, overlay, ShapeInput, 0, 0, nullptr, 0, ShapeSet, 0);

    XSelectInput(display, overlay, ExposureMask);
    XMapRaised(display, overlay);
    XFlush(display);

    draw_grid(overlay, width, height);
}

void destroy_overlay() {
    if (overlay) {
        XDestroyWindow(display, overlay);
        overlay = 0;
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
            if ((xkey.state & modifiers_mask) == modifiers_mask && xkey.keycode == keycode) {
                overlayVisible = !overlayVisible;
                if (overlayVisible) {
                    create_overlay();
                } else {
                    destroy_overlay();
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
