# X11 Grid Overlay Tool

This program creates a transparent overlay grid on your X11 desktop, which can be toggled on and off with a keyboard shortcut. It allows you to highlight individual grid cells by typing their IDs, and automatically moves the mouse pointer to the center of the highlighted cell or subcell, triggering a click.

## How it works

- The program connects to the X11 display and grabs the `Ctrl + h` and `Ctrl + t` key combinations (including variations with NumLock and CapsLock).
- When you **hold Ctrl and press both `h` and `t` simultaneously**, it toggles the visibility of a fullscreen transparent overlay window.
- The overlay window is semi-transparent and input-transparent, so it does not interfere with your normal desktop usage.
- The overlay displays a grid with lines every 50 pixels, drawn in white.
- Each grid cell is labeled with a unique 2-character ID (e.g., `a0`, `a1`, ..., `z9`, `aa`, `ab`, ...).
- When the overlay is visible, you can type **two alphanumeric characters** to highlight the corresponding main cell.
- When a main cell is highlighted, a 3x3 subgrid appears inside it, with subcells labeled `a0`, `b0`, `c0`, `a1`, ..., `c2`.
- You can then type **two more characters** (total of 4 typed characters) to select a subcell within the highlighted main cell.
- The mouse pointer will move to the center of the highlighted cell or subcell and automatically click.
- After a subcell click, the overlay automatically hides.
- **Alternatively, after selecting a main cell (typing 2 characters), you can press Enter (or Return) to immediately click the center of that main cell and hide the overlay, without selecting a subcell.**
- The grid is redrawn whenever the overlay window receives an expose event (e.g., when uncovered) or when a cell/subcell is highlighted.
- Press **Escape** to cancel and hide the overlay without clicking.

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

4. Press **Ctrl+h+t simultaneously** (hold Ctrl and press both `h` and `t` at the same time) to toggle the grid overlay on or off.

5. When the overlay is visible:
   - Type **two alphanumeric characters** (e.g., `b3`) to select a main grid cell. The pointer will move and click in the center of that cell.
   - After selecting a main cell, a 3x3 subgrid appears inside it.
   - Type **two more characters** (e.g., `a1`) to select a subcell within that main cell. The pointer will move and click in the center of the subcell, and the overlay will automatically hide.
   - **Alternatively, after typing two characters for the main cell, press Enter (or Return) to immediately click the center of that main cell and hide the overlay, skipping subcell selection.**
   - Press **Escape** to cancel and hide the overlay without clicking.

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
- Subcells within a main cell are labeled from `a0` (top-left) to `c2` (bottom-right).
- Highlighted cells and subcells are filled in white with black or orange text for visibility.
- When a cell or subcell is highlighted, the mouse pointer is moved to its center automatically and a click is triggered.
- After a subcell click, or after pressing Enter on a main cell, the overlay automatically hides and resets.
- The overlay grabs focus when shown, and releases it when hidden.
