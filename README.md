# X11 Grid Overlay Tool

This program creates a transparent overlay grid on your X11 desktop, which can be toggled on and off with a keyboard shortcut.

## How it works

- The program connects to the X11 display and grabs the `Ctrl + Shift + F9` key combination (including variations with NumLock and CapsLock).
- When you press `Ctrl + Shift + F9`, it toggles the visibility of a fullscreen transparent overlay window.
- The overlay window is semi-transparent and click-through (input transparent), so it does not interfere with your normal desktop usage.
- The overlay displays a grid with lines every 50 pixels, drawn in white.
- The grid is redrawn whenever the overlay window receives an expose event (e.g., when uncovered).

## Usage

1. Compile the program with a command like:

   ```
   g++ main.cpp -o grid_overlay -lX11 -lXext
   ```

2. Run the program:

   ```
   ./grid_overlay
   ```

3. Press `Ctrl + Shift + F9` to toggle the grid overlay on or off.

4. To stop the program, terminate it from the terminal (e.g., with `Ctrl+C`).

## Dependencies

- X11 development libraries (`libX11`, `libXext`)
- A running X11 server

## Notes

- The overlay is created using the X Shape extension to make it input-transparent.
- The opacity is set via the `_NET_WM_WINDOW_OPACITY` property to make the overlay semi-transparent.
- The program currently uses a fixed grid size of 50 pixels.
