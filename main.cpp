#include <libinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

// GLFW and ImGui includes
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

// libinput_interface implementation
static int open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    if (fd < 0)
        std::cerr << "Failed to open: " << path << std::endl;
    return fd;
}

static void close_restricted(int fd, void *user_data)
{
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

// Human-readable event type mapping
std::string get_event_type_name(enum libinput_event_type type)
{
    switch (type)
    {
    case LIBINPUT_EVENT_DEVICE_ADDED:
        return "DEVICE_ADDED";                      // device connect
    case LIBINPUT_EVENT_DEVICE_REMOVED:
        return "DEVICE_REMOVED";                    // device remove

    case LIBINPUT_EVENT_POINTER_MOTION:
        return "POINTER_MOTION";                    // 1 finger scroll
    case LIBINPUT_EVENT_POINTER_BUTTON:
        return "POINTER_BUTTON";                    // buttons

    case LIBINPUT_EVENT_POINTER_AXIS:
        return "POINTER_AXIS";                      // 2 finger scroll
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
        return "POINTER_FINGER";                    // 2 finger scroll
        
    case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
        return "GESTURE_SWIPE_BEGIN";               // 3,4 finger scroll
    case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
        return "GESTURE_SWIPE_UPDATE";
    case LIBINPUT_EVENT_GESTURE_SWIPE_END:
        return "GESTURE_SWIPE_END";

    case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
        return "GESTURE_PINCH_BEGIN";               // 2,3,4 finger zoom
    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
        return "GESTURE_PINCH_UPDATE";
    case LIBINPUT_EVENT_GESTURE_PINCH_END:
        return "GESTURE_PINCH_END";

    case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
        return "GESTURE_HOLD_BEGIN";               // 1,2 finger tap
    case LIBINPUT_EVENT_GESTURE_HOLD_END:
        return "GESTURE_HOLD_END";

    default:
        return "UNKNOWN_EVENT_" + std::to_string(type);
    }
}

struct SwipeGesture {
    double dx = 0.0;
    double dy = 0.0;
    bool active = false;
    int fingers = 0;
};

struct PinchGesture {
    double scale = 1.0;
    double dx = 0.0;
    double dy = 0.0;
    int fingers = 0;
    bool active = false;
};


SwipeGesture swipe;
PinchGesture pinch;

std::map<std::pair<int, std::string>, std::string> gesture_bindings;
static std::map<std::pair<int, std::string>, int> selected_command_indices;

void sync_selected_commands(const std::vector<std::string>& commands) {
    for (const auto& [key, cmd] : gesture_bindings) {
        auto it = std::find(commands.begin(), commands.end(), cmd);
        if (it != commands.end()) {
            selected_command_indices[key] = std::distance(commands.begin(), it);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " /dev/input/eventX\n";
        return 1;
    }

    const char *device_path = argv[1];
    struct libinput *li = libinput_path_create_context(&interface, nullptr);
    if (!li)
    {
        std::cerr << "Failed to create libinput context\n";
        return 1;
    }

    struct libinput_device *device = libinput_path_add_device(li, device_path);
    if (!device)
    {
        std::cerr << "Failed to add device: " << device_path << "\n";
        libinput_unref(li);
        return 1;
    }

    std::cout << "Listening for events on: " << device_path << "\n";

    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    // Setup OpenGL version (3.3 core)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Touchpad Gesture Daemon", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int fd = libinput_get_fd(li);
    struct pollfd fds = {fd, POLLIN, 0};

    while (!glfwWindowShouldClose(window))
    {
        // Poll for libinput events without blocking the UI thread
        if (poll(&fds, 1, 0) > 0)
        {
            if (libinput_dispatch(li) != 0)
            {
                std::cerr << "libinput_dispatch failed\n";
                break;
            }

            struct libinput_event *event;
            while ((event = libinput_get_event(li)) != NULL)
            {
                libinput_event_type type = libinput_event_get_type(event);
                std::string type_str = get_event_type_name(type);

                switch (type)
                {
                    case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
                    {
                        swipe.active = true;
                        swipe.dx = 0.0;
                        swipe.dy = 0.0;
                        struct libinput_event_gesture *gesture_event = libinput_event_get_gesture_event(event);
                        swipe.fingers = libinput_event_gesture_get_finger_count(gesture_event);
                        #ifdef DEBUG
                        std::cout << "Swipe gesture started with " << swipe.fingers << " fingers\n";
                        #endif
                        break;
                    }

                    case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
                    {
                        struct libinput_event_gesture *gesture_event = libinput_event_get_gesture_event(event);
                        swipe.dx += libinput_event_gesture_get_dx(gesture_event);
                        swipe.dy += libinput_event_gesture_get_dy(gesture_event);
                        #ifdef DEBUG
                        std::cout << "Swipe update: dx=" << swipe.dx << ", dy=" << swipe.dy << "\n";
                        #endif
                        break;
                    }

                    case LIBINPUT_EVENT_GESTURE_SWIPE_END:
                    {
                        swipe.active = false;
                        #ifdef DEBUG
                        std::cout << "Swipe gesture (" << swipe.fingers << " fingers) ended with dx=" << swipe.dx << ", dy=" << swipe.dy << "\n";
                        #endif

                        std::string direction;
                        if (std::abs(swipe.dx) > std::abs(swipe.dy)) {
                            if (swipe.dx > 50)
                                direction = "RIGHT";
                            else if (swipe.dx < -50)
                                direction = "LEFT";
                        } else {
                            if (swipe.dy > 50)
                                direction = "DOWN";
                            else if (swipe.dy < -50)
                                direction = "UP";
                        }

                        if (!direction.empty()) {
                            std::cout << "Detected " << swipe.fingers << "-finger swipe " << direction << std::endl;

                            auto it = gesture_bindings.find({swipe.fingers, direction});
                            if (it != gesture_bindings.end()) {
                                std::cout << "Running command: " << it->second << std::endl;
                                int ret = std::system(it->second.c_str());
                                (void)ret;
                            } else {
                                std::cout << "No binding found for this gesture\n";
                            }
                        }

                        break;
                    }


                    case LIBINPUT_EVENT_POINTER_AXIS:
                    {
                        struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event(event);

                        if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
                            double v_scroll = libinput_event_pointer_get_axis_value(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
                            #ifdef DEBUG
                            std::cout << "2-finger vertical scroll: " << v_scroll << std::endl;
                            #endif
                        }

                        if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
                            double h_scroll = libinput_event_pointer_get_axis_value(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
                            #ifdef DEBUG
                            std::cout << "2-finger horizontal scroll: " << h_scroll << std::endl;
                            #endif
                        }

                        break;
                    }

                    case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
                    {
                        pinch.active = true;
                        pinch.scale = 1.0;
                        pinch.dx = 0.0;
                        pinch.dy = 0.0;
                        struct libinput_event_gesture *gesture_event = libinput_event_get_gesture_event(event);
                        pinch.fingers = libinput_event_gesture_get_finger_count(gesture_event);
                        #ifdef DEBUG
                        std::cout << "Pinch gesture started with " << pinch.fingers << " fingers\n";
                        #endif
                        break;
                    }

                    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
                    {
                        struct libinput_event_gesture *gesture_event = libinput_event_get_gesture_event(event);
                        double scale_step = libinput_event_gesture_get_scale(gesture_event);
                        pinch.scale *= scale_step;

                        pinch.dx += libinput_event_gesture_get_dx(gesture_event);
                        pinch.dy += libinput_event_gesture_get_dy(gesture_event);
                        #ifdef DEBUG
                        std::cout << "Pinch update: scale=" << pinch.scale << ", dx=" << pinch.dx << ", dy=" << pinch.dy << std::endl;
                        #endif
                        break;
                    }

                    case LIBINPUT_EVENT_GESTURE_PINCH_END:
                    {
                        pinch.active = false;
                        #ifdef DEBUG
                        std::cout << "Pinch gesture ended with total scale=" << pinch.scale
                                << ", dx=" << pinch.dx << ", dy=" << pinch.dy << "\n";
                        #endif

                        if (pinch.scale > 1.1)
                            std::cout << "Detected pinch out (zoom in)\n";
                        else if (pinch.scale < 0.9)
                            std::cout << "Detected pinch in (zoom out)\n";
                        else
                            std::cout << "Minor pinch, no zoom direction detected\n";

                        break;
                    }

                    case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
                    {
                        struct libinput_event_gesture *gesture_event = libinput_event_get_gesture_event(event);
                        int fingers = libinput_event_gesture_get_finger_count(gesture_event);
                        #ifdef DEBUG
                        std::cout << "Hold gesture started with " << fingers << " finger(s)" << std::endl;
                        #endif
                        break;
                    }
                    case LIBINPUT_EVENT_GESTURE_HOLD_END:
                    {
                        struct libinput_event_gesture *gesture_event = libinput_event_get_gesture_event(event);
                        int fingers = libinput_event_gesture_get_finger_count(gesture_event);
                        #ifdef DEBUG
                        std::cout << "Hold gesture ended with " << fingers << " finger(s)" << std::endl;
                        #endif
                        break;
                    }

                    default:
                        // For all other events, just print their type
                        #ifdef DEBUG
                        std::cout << "Event: " << type_str << std::endl;
                        #endif
                        break;
                }

                libinput_event_destroy(event);
            }

        }

        // Poll and handle GLFW events
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("Gesture Bindings");

        //Custom‐command buffer
        static std::vector<std::string> user_commands;
        static char custom_cmd[256] = "";
        ImGui::InputText("Custom Command", custom_cmd, IM_ARRAYSIZE(custom_cmd));
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            std::string cmd = std::string(custom_cmd);
            if (!cmd.empty() &&
                std::find(user_commands.begin(), user_commands.end(), cmd) == user_commands.end())
            {
                user_commands.push_back(cmd);
            }
            custom_cmd[0] = '\0'; // clear input
        }
        ImGui::Separator();

        // Default options
        const std::vector<std::string> predefined_commands = {
            "None",
            "notify-send 'Gesture Triggered'",
            // Launch terminal
            "gnome-terminal",
            // Launch Google Chrome
            "google-chrome",
            // Media controls
            "playerctl play-pause",
            "playerctl next",
            "playerctl previous"
        };

        std::vector<std::string> all_commands = predefined_commands;
        all_commands.insert(all_commands.end(), user_commands.begin(), user_commands.end());

        // Prune stale bindings and re-sync
        for (auto it = gesture_bindings.begin(); it != gesture_bindings.end(); ) {
            if (std::find(all_commands.begin(), all_commands.end(), it->second) == all_commands.end()) {
                it = gesture_bindings.erase(it);
            } else {
                ++it;
            }
        }
        selected_command_indices.clear();
        sync_selected_commands(all_commands);


        // Display
        const std::vector<std::string> directions = {"LEFT", "RIGHT", "UP", "DOWN"};

        for (int fingers : {3, 4}) {
            for (const std::string& dir : directions) {
                auto key = std::make_pair(fingers, dir);
                std::string label = std::to_string(fingers) + "F " + dir;

                int& selected = selected_command_indices[key]; // persists across frames

                if (ImGui::BeginCombo(label.c_str(), all_commands[selected].c_str())) {
                    for (int i = 0; i < (int)all_commands.size(); ++i) {
                        bool is_selected = (selected == i);
                        if (ImGui::Selectable(all_commands[i].c_str(), is_selected)) {
                            selected = i;

                            if (all_commands[i] == "None") {
                                gesture_bindings.erase(key);
                                std::cout << "Unbound " << fingers << "F " << dir << std::endl;
                            } else {
                                gesture_bindings[key] = all_commands[i];
                                std::cout << "Bound " << fingers << "F " << dir
                                        << " -> " << all_commands[i] << std::endl;
                            }
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        }

        ImGui::End();


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    libinput_unref(li);
    return 0;
}