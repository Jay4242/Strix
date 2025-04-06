# X11 Grid Overlay Tool

This program creates a transparent overlay grid on your X11 desktop, which can be toggled on and off with a keyboard shortcut. It also allows you to highlight individual grid cells by typing their IDs, and automatically moves the mouse pointer to the center of the highlighted cell.

## How it works

- The program connects to the X11 display and grabs the `Ctrl + Shift + F9` key combination (including variations with NumLock and CapsLock).
- When you press `Ctrl + Shift + F9`, it toggles the visibility of a fullscreen transparent overlay window.
- The overlay window is semi-transparent and click-through (input transparent), so it does not interfere with your normal desktop usage.
- The overlay displays a grid with lines every 50 pixels, drawn in white.
- Each grid cell is labeled with a unique 2-character ID (e.g., `a0`, `a1`, ..., `z9`, `aa`, `ab`, ...).
- When the overlay is visible, you can type two alphanumeric characters to highlight the corresponding cell.
- When a cell is highlighted, the mouse pointer is automatically moved to the center of that cell and a click is triggered.
- The grid is redrawn whenever the overlay window receives an expose event (e.g., when uncovered) or when a cell is highlighted.

## Usage

1. **Install dependencies:**

   Make sure you have the X11 development libraries **and** the XTest extension development headers installed.

   On Debian/Ubuntu:

   ```
   sudo apt-get install libx11-dev libxtst-dev libxext-dev
   ```

2. **Compile the program:**

   ```
   g++ main.cpp -o grid_overlay -lX11 -lXext -lXtst
   ```

3. **Run the program:**

   ```
   ./grid_overlay
   ```

4. Press `Ctrl + Shift + F9` to toggle the grid overlay on or off.

5. When the overlay is visible, type two alphanumeric characters to highlight a specific cell by its ID. The mouse pointer will move to the center of that cell and click automatically.

6. To stop the program, terminate it from the terminal (e.g., with `Ctrl+C`).

## Dependencies

- X11 development libraries (`libX11`, `libXext`)
- XTest extension development library (`libXtst`)
- A running X11 server

## Notes

- The overlay is created using the X Shape extension to make it input-transparent.
- The opacity is set via the `_NET_WM_WINDOW_OPACITY` property to make the overlay semi-transparent.
- The program currently uses a fixed grid size of 50 pixels.
- Cell IDs are generated sequentially, starting with `a0` up to `z9`, then `aa`, `ab`, etc.
- Highlighted cells are filled in white with black text for visibility.
- When a cell is highlighted, the mouse pointer is moved to its center automatically and a click is triggered.
