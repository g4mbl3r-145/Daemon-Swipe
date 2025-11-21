# Touchpad Gesture Daemon

A Linux daemon with an interactive GUI for mapping multi-finger touchpad gestures to shell commands. Built with `libinput`, `GLFW`, `OpenGL`, and `Dear ImGui`.

## 🚀 Features

- Detects swipe gestures (3-finger and 4-finger) in all directions
- Recognizes pinch (zoom in/out) and hold gestures
- Tracks scroll and pointer movement
- Configurable gesture-to-command bindings via ImGui GUI
- Built-in presets (`playerctl`, `notify-send`, etc.)
- Custom shell command support
- Simple CLI usage with interactive GUI
- Lightweight and easy to use

---

## 📦 Dependencies

### Required Libraries

- `libinput`
- `libudev`
- `libx11`
- `libgl1-mesa-dev`
- `libglfw3`/ `libglfw3-dev`
- `libpthread`

### 3rd Party

- [GLFW](https://www.glfw.org/)
- [Dear ImGui](https://github.com/ocornut/imgui)

## Install on Debian/Ubuntu

- Install required system libraries

```bash
sudo apt install libinput-dev libudev-dev libglfw3-dev libx11-dev libgl1-mesa-dev
```

- Clone the Repo

```bash
git clone https://github.com/S0r4-0/TouchDaemon
cd TouchDaemon
```

- Clone ImGui Repo

```bash
git clone https://github.com/ocornut/imgui.git
```

- Edit your device path (replace `/dev/input/eventX` in [build.sh](./build.sh))
- You can find the correct path using: `libinput list-devices | grep -iA10 "Touchpad"`

---

## ▶️ Usage

### 🔨 Build & Run

Use the provided script to build and run the project:

- 🚀 **Release mode** (default):

  ```bash
  ./build.sh
  ```

- 🐞 **Debug mode**:
  
  ```bash
  ./build.sh debug
  ```

---

## ⚙️ How It Works

- Initializes a GUI window using ImGui
- Listens to `libinput` gesture events
- Identifies swipes, pinches, and holds
- Lets you assign commands to each gesture
- Runs assigned shell commands on gesture detection

---

## 🖼️ UI Overview

- Dropdown selectors to map gestures
- Text box for adding custom commands
- UI handles cleanup of unused mappings

![UI Interface - 1](assets/UI-1.png)

![UI Interface - 2](assets/UI-2.png)

---

## 🔗 Example Bindings

| Gesture             | Action Command                 |
|---------------------|---------------------------------|
| 3-finger swipe up   | `gnome-terminal`               |
| 4-finger swipe left | `playerctl previous`           |
| 4-finger swipe down | `notify-send 'Swipe Detected'` |

---

## 📝 License

This project is licensed under the [MIT License](./LICENSE).
