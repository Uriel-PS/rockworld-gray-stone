# Rockworld Gray Stone

A 2D turn-based battle game built from scratch using C++ and OpenGL. Choose your elemental stone—Fire, Water, or Plant—and battle through three evolution stages using a precision-based timing system to maximize your damage.

## 🌟 Features

* **Tactical Turn-Based Combat:** Engage in battles against elemental enemies that scale in difficulty and size across three evolution stages.
* **Timing Attack System:** Inspired by classic RPGs, hit the moving bar at the exact right time in the green zone to get a 1.0x damage multiplier, or risk dealing minimal damage.
* **Custom Particle Engine:** Every attack (Fire Rays, Bubbles, Razor Leaves) uses a built-in particle system for dynamic visual feedback.
* **Procedural Graphics:** No external image assets are used. Every fighter, UI element, and background is drawn directly using OpenGL primitive shapes.
* **Three Difficulty Levels:** Choose between Easy, Medium, or Hard to adjust the speed of the timing bar.

## 🪨 Playable Stones

1.  **Ignithar (Fire):** Magma stone. *Skills: Fire Ray, Fire Circle*
2.  **Torrean (Water):** Porous soaked stone. *Skills: Bubbles, Waterfall*
3.  **Herbrok (Plant):** Moss-covered stone. *Skills: Razor Leaf, Giga Drain (Heals HP)*

## 🎮 Controls

**Menus:**
* `Left / Right Arrows`: Navigate options
* `ENTER / SPACE`: Confirm selection

**Battle:**
* `1 / 2` or `Up / Down`: Select your attack skill
* `ENTER / SPACE`: Lock the timing bar cursor
* `R`: Restart match (after Game Over)
* `ESC`: Return to character selection
* `F11`: Toggle Fullscreen

## 🛠️ How to Compile and Run

This project requires a C++17 compatible compiler, **GLFW3**, and **OpenGL**.

### Windows (using MSYS2/MinGW)
Open your terminal and run the following commands:
```bash
g++ rockWorld.cpp -o rockWorld -std=c++17 -O2 -lglfw3 -lopengl32 -lgdi32 -luser32
./rockWorld.exe
```

### Linux
Make sure you have `libglfw3-dev` installed, then run:
```bash
g++ rockWorld.cpp -o rockWorld -std=c++17 -O2 -lglfw -lGL
./rockWorld
```

## 📜 Credits
Developed as an open-source C++ and OpenGL learning project. Uses a public domain single-file font rendering system (`stb_easy_font`).
