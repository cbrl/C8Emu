#include "media_layer.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "instruction/instruction.h"
#include "util/strings.h"

#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>  // DockBuilder API
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <imgui_stdlib.h>


static auto RGBA2FloatArray(uint32_t rgba, std::span<float, 4> array) -> void {
    static constexpr float normalize = 1.0f / 255.0f;
    array[0] = static_cast<float>((rgba >> 24) & 0xFF) * normalize;
    array[1] = static_cast<float>((rgba >> 16) & 0xFF) * normalize;
    array[2] = static_cast<float>((rgba >> 8)  & 0xFF) * normalize;
    array[3] = static_cast<float>( rgba        & 0xFF) * normalize;
}

static auto FloatArray2RGBA(std::span<const float, 4> array) -> uint32_t {
    uint32_t rgba = 0;
    rgba |= static_cast<uint32_t>(array[0] * 255) << 24;
    rgba |= static_cast<uint32_t>(array[1] * 255) << 16;
    rgba |= static_cast<uint32_t>(array[2] * 255) << 8;
    rgba |= static_cast<uint32_t>(array[3] * 255);
    return rgba;
}


MediaLayer::MediaLayer() {
    //--------------------------------------------------------------------------------
    // SDL Init
    //--------------------------------------------------------------------------------
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        const std::string error = SDL_GetError();
        std::cout << "SDL_Init Error: " << error << '\n';
        throw std::runtime_error(error);
    }

    window = SDL_CreateWindow(
        "CHIP-8",
        1600,
        1200,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (!window) {
        const std::string error = SDL_GetError();
        std::cout << "SDL_CreateWindow Error: " << error << '\n';

        SDL_Quit();

        throw std::runtime_error(error);
    }

    // Initialize beeper. The emulator still runs without audio.
    (void)beeper.init_audio();


    //--------------------------------------------------------------------------------
    // OpenGL Init
    //--------------------------------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        const std::string error = SDL_GetError();
        std::cout << "SDL_GL_CreateContext Error: " << error << '\n';

        SDL_DestroyWindow(window);
        SDL_Quit();

        throw std::runtime_error(error);
    }

    SDL_GL_MakeCurrent(window, gl_context);

    // Turn VSync off. Then only the frame limit sets the pace of the main loop.
    (void)SDL_GL_SetSwapInterval(0);

    // Load OpenGL functions
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress));

    // Create the CHIP-8 display texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    //--------------------------------------------------------------------------------
    // ImGui Init
    //--------------------------------------------------------------------------------
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Enable ImGui docking features
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigDockingWithShift = false;

    ImGui::StyleColorsDark();

    // The previous editor used HLSL syntax highlighting by default
    text_editor.SetLanguage(TextEditor::Language::Hlsl());
}


MediaLayer::~MediaLayer() {
    glDeleteTextures(1, &texture);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    // SDL_Quit destroys all audio streams, so close the beeper before it
    beeper.deinit_audio();

    if (gl_context) {
        SDL_GL_DestroyContext(gl_context);
    }

    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}


void MediaLayer::set_display_scale(uint32_t scale) {
    display_scale = std::clamp<uint32_t>(scale, 1, 32);
}


void MediaLayer::process_events(c8::Chip8& chip, bool& quit) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
            case SDL_EVENT_QUIT: {
                quit = true;
            }
            break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const auto it = key_map.find(event.key.scancode);
                if (it != key_map.end()) {
                    chip.set_key(it->second, event.type == SDL_EVENT_KEY_DOWN);
                }
            }
            break;

            default: break;
        }
    }
}


void MediaLayer::render(c8::Chip8& chip, c8::Debugger& debugger) {
    // Beep only while the program runs, so a pause during a beep is silent
    if (debugger.is_running() && chip.sound_active()) {
        beeper.start_beep();
    }
    else {
        beeper.stop_beep();
    }

    begin_frame();
    render_ui(chip, debugger);
    end_frame();

    limit_frame_rate();
}


void MediaLayer::limit_frame_rate() {
    if (!frame_limit_enabled) {
        return;
    }

    const Uint64 frame_ns = static_cast<Uint64>(SDL_NS_PER_SECOND) / std::clamp(frame_limit, min_frame_limit, max_frame_limit);

    next_frame_ns += frame_ns;

    const Uint64 now = SDL_GetTicksNS();
    if (next_frame_ns > now) {
        SDL_DelayPrecise(next_frame_ns - now);
    }
    else if ((now - next_frame_ns) > frame_ns) {
        // More than one frame late. Do not try to catch up.
        next_frame_ns = now;
    }
}


void MediaLayer::update_rate_meter(const c8::MachineState& state) {
    const Uint64 now = SDL_GetTicksNS();
    const Uint64 elapsed = now - rate.start_ns;

    // A reset sets the counters back to zero. Then start a new measurement.
    const bool counters_reset = (state.total_cycles < rate.start_cycles) || (state.frame_count < rate.start_frames);

    if (!counters_reset && (elapsed < SDL_NS_PER_SECOND)) {
        return;
    }

    if (!counters_reset && (rate.start_ns != 0)) {
        const double seconds = static_cast<double>(elapsed) / static_cast<double>(SDL_NS_PER_SECOND);
        rate.ips = static_cast<double>(state.total_cycles - rate.start_cycles) / seconds;
        rate.fps = static_cast<double>(state.frame_count - rate.start_frames) / seconds;
    }

    rate.start_ns     = now;
    rate.start_cycles = state.total_cycles;
    rate.start_frames = state.frame_count;
}


void MediaLayer::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}


void MediaLayer::end_frame() {
    ImGui::Render();

    const ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));

    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}


void MediaLayer::update_display_texture(const c8::Chip8& chip) {
    const auto& display = chip.state().display;
    const auto pixels = display.pixels();

    // The display holds pixel bits. The colors belong to the frontend.
    texture_pixels.resize(pixels.size() * 4);
    for (size_t idx = 0; idx < pixels.size(); ++idx) {
        const uint32_t color = (pixels[idx] != 0) ? foreground_color : background_color;
        texture_pixels[(idx * 4) + 0] = static_cast<uint8_t>(color >> 24);
        texture_pixels[(idx * 4) + 1] = static_cast<uint8_t>(color >> 16);
        texture_pixels[(idx * 4) + 2] = static_cast<uint8_t>(color >> 8);
        texture_pixels[(idx * 4) + 3] = static_cast<uint8_t>(color);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        static_cast<GLsizei>(display.width()),
        static_cast<GLsizei>(display.height()),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        texture_pixels.data()
    );
}


void MediaLayer::load_rom_file(c8::Chip8& chip, c8::Debugger& debugger, const std::filesystem::path& file) {
    const auto loaded = c8::read_rom_file(file, chip.max_program_size())
        .and_then([&debugger](const std::vector<uint8_t>& data) { return debugger.load_program(data); });

    if (!loaded) {
        std::cout << "Error loading ROM: " << loaded.error() << '\n';
        return;
    }

    // Decompile the ROM and update the text editor
    const auto result = decompile_program(chip.program());

    for (auto idx : result.failures) {
        std::cout << std::format("Decompilation failure on instruction {} (byte offset: {})", idx, idx * 2) << std::endl;
    }

    // SetText removes all markers, so show the breakpoints again
    text_editor.SetText(std::vector<std::string_view>(result.program.begin(), result.program.end()));
    update_breakpoint_markers(chip, debugger);
}


void MediaLayer::render_ui(c8::Chip8& chip, c8::Debugger& debugger) {
    auto& state = chip.state();

    // Update the CHIP-8 display texture
    update_display_texture(chip);
    update_rate_meter(state);


    //----------------------------------------------------------------------------------
    // Menu Bar
    //----------------------------------------------------------------------------------
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::MenuItem("Open ROM")) {
            file_selector.open_selector();
        }

        if (ImGui::BeginMenu("Options")) {
            if (ImGui::BeginMenu("Quirk Preset")) {
                const auto* current = c8::find_preset(chip.quirks());

                for (const auto& preset : c8::presets) {
                    const auto name = std::string{preset.name};
                    if (ImGui::MenuItem(name.c_str(), nullptr, current == &preset)) {
                        chip.set_quirks(preset.quirks);
                        chip.set_instructions_per_frame(preset.instructions_per_frame);
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                reset_layout = true;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }


    //----------------------------------------------------------------------------------
    // Invisible Root Docking Window
    //----------------------------------------------------------------------------------
    const ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // imgui.ini restores the dock layout of the last session. Without a saved layout, use the default.
    if (reset_layout || ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        build_default_layout(dockspace_id);

        // The user can close the memory editor, so open it again
        mem_editor.Open = true;
        reset_layout = false;
    }

    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());


    //----------------------------------------------------------------------------------
    // Register Window
    //----------------------------------------------------------------------------------
    if (ImGui::Begin("Registers", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Registers");
        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

        // V Registers
        for (uint8_t i = 0; i <= 0xF; ++i) {
            auto value_str = std::format("0x{:02X}", state.v[i]);
            ImGui::PushID(i);

            ImGui::Text("v%X:", i);
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::CalcTextSize(value_str.c_str()).x);
            if (ImGui::InputText("##reg_v", &value_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                state.v[i] = str_to<uint8_t>(value_str, 16).value_or(state.v[i]);
            }

            ImGui::PopID();
        }
        ImGui::Separator();

        // I Register
        auto reg_i_str = std::format("0x{:04X}", state.i);
        ImGui::Text(" I:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(reg_i_str.c_str()).x);
        if (ImGui::InputText("##reg_i", &reg_i_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            state.i = str_to<uint16_t>(reg_i_str, 16).value_or(state.i);
        }

        // Program Counter
        auto pc_str = std::format("0x{:04X}", state.pc);
        ImGui::Text("PC:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(pc_str.c_str()).x);
        if (ImGui::InputText("##pc", &pc_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            state.pc = str_to<uint16_t>(pc_str, 16).value_or(state.pc);
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();


    //----------------------------------------------------------------------------------
    // Stack Window
    //----------------------------------------------------------------------------------
    ImGui::SetNextWindowSize({125, 325}, ImGuiCond_Appearing);
    if (ImGui::Begin("Stack", nullptr)) {
        ImGui::Text("Stack");
        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

        // Print the editable return addresses, newest first
        const int depth = std::min<int>(state.sp, c8::MachineState::stack_depth);
        for (int i = depth - 1; i >= 0; --i) {
            auto& entry = state.stack[static_cast<size_t>(i)];
            auto value_str = std::format("0x{:04X}", entry);

            ImGui::PushID(i);
            ImGui::Text("%02d:", i);
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::CalcTextSize(value_str.c_str()).x);
            if (ImGui::InputText("##stack", &value_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                entry = str_to<uint16_t>(value_str, 16).value_or(entry);
            }
            ImGui::PopID();
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();


    //----------------------------------------------------------------------------------
    // Instruction Window
    //----------------------------------------------------------------------------------
    if (ImGui::Begin("Program", nullptr, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Execution");
        ImGui::Separator();

        ImGui::InputInt("Preview Count", &instruction_count);
        instruction_count = std::clamp(instruction_count, 1, 64);
        ImGui::Separator();

        // Decode the instructions from the PC forward. Addresses wrap at the end of memory.
        const auto count = static_cast<size_t>(instruction_count);
        instructions.resize(count);

        for (size_t i = 0; i < count; ++i) {
            const auto address = static_cast<uint16_t>((state.pc + (2 * i)) & state.mask());
            instructions[i] = to_string(instruction{state.read_word(address)});

            if (i == 0) {
                ImGui::Text("0x%04X - %s", address, instructions[i].c_str());
            }
            else {
                ImGui::TextDisabled("0x%04X - %s", address, instructions[i].c_str());
            }
        }
    }
    ImGui::End();


    //----------------------------------------------------------------------------------
    // Settings
    //----------------------------------------------------------------------------------
    if (ImGui::Begin("Chip8 Settings")) {
        ImGui::Text("Settings");
        ImGui::Separator();
        ImGui::Spacing();

        // Emulation speed. The timers count at 60 Hz, so each frame runs this many instructions.
        static constexpr uint32_t ipf_step      = 1;
        static constexpr uint32_t ipf_step_fast = 10;

        auto ipf = chip.instructions_per_frame();
        ImGui::Text("Instructions/frame");
        if (ImGui::InputScalar("##ipf", ImGuiDataType_U32, &ipf, &ipf_step, &ipf_step_fast)) {
            chip.set_instructions_per_frame(ipf);
        }

        const auto target_ips = chip.instructions_per_frame() * c8::Chip8::frame_rate;
        ImGui::TextDisabled("~%u IPS target", target_ips);
        ImGui::TextDisabled("Emulated: %.0f IPS, %.0f FPS", rate.ips, rate.fps);
        ImGui::Spacing();

        const auto* preset = c8::find_preset(chip.quirks());
        const auto preset_name = preset ? std::string{preset->name} : std::string{"Custom"};
        ImGui::Text("Quirk Preset: %s", preset_name.c_str());

        bool wrap = chip.quirks().wrap_sprites;
        if (ImGui::Checkbox("Wrap Sprites", &wrap)) {
            auto quirks = chip.quirks();
            quirks.wrap_sprites = wrap;
            chip.set_quirks(quirks);
        }

        ImGui::Spacing();

        // Reset the system and load the last program again
        if (ImGui::Button("Reset System")) {
            debugger.reset();
        }

        // Pause/Resume/Step buttons
        if (debugger.is_running()) {
            if (ImGui::Button("Pause")) {
                debugger.pause();
            }
        }
        else {
            if (ImGui::Button("Resume")) {
                debugger.run();
            }
            ImGui::SameLine();
            if (ImGui::Button("Step")) {
                debugger.step_into();
            }
        }

        // Why execution stopped
        const auto& stop = debugger.last_stop();
        if (const auto message = c8::describe(stop); !message.empty()) {
            if (stop.reason == c8::StopReason::fault) {
                ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "%s", message.c_str());
            }
            else {
                ImGui::TextUnformatted(message.c_str());
            }
        }

        if (chip.waiting_for_key()) {
            ImGui::Text("Waiting for a key (v%X)", state.key_wait.reg);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Display settings
        static constexpr uint32_t scale_step = 1;
        if (ImGui::InputScalar("Display Scale", ImGuiDataType_U32, &display_scale, &scale_step)) {
            set_display_scale(display_scale);
        }

        // The host frame rate. The emulation speed does not change with it.
        static constexpr uint32_t frame_limit_step      = 1;
        static constexpr uint32_t frame_limit_step_fast = 10;

        ImGui::Checkbox("Limit Frame Rate", &frame_limit_enabled);
        ImGui::BeginDisabled(!frame_limit_enabled);
        if (ImGui::InputScalar("Max FPS", ImGuiDataType_U32, &frame_limit, &frame_limit_step, &frame_limit_step_fast)) {
            frame_limit = std::clamp(frame_limit, min_frame_limit, max_frame_limit);
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Host: %.0f FPS", ImGui::GetIO().Framerate);

        auto off_arr = std::array<float, 4>{};
        auto on_arr  = std::array<float, 4>{};

        RGBA2FloatArray(background_color, off_arr);
        RGBA2FloatArray(foreground_color, on_arr);

        if (ImGui::ColorEdit3("Background Color", off_arr.data())) {
            background_color = FloatArray2RGBA(off_arr);
        }
        if (ImGui::ColorEdit3("Foreground Color", on_arr.data())) {
            foreground_color = FloatArray2RGBA(on_arr);
        }
    }
    ImGui::End();


    //----------------------------------------------------------------------------------
    // CHIP-8 Display
    //----------------------------------------------------------------------------------
    if (ImGui::Begin("Display", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto& display = state.display;
        const auto x_size = static_cast<float>(display.width() * display_scale);
        const auto y_size = static_cast<float>(display.height() * display_scale);

        // Draw the display texture
        ImGui::BeginChild("Image", {x_size + 16.0f, y_size + 16.0f}, ImGuiChildFlags_Borders);
        ImGui::Image(static_cast<ImTextureID>(texture), ImVec2{x_size, y_size});
        ImGui::EndChild();
    }
    ImGui::End();


    //----------------------------------------------------------------------------------
    // ROM Selection
    //----------------------------------------------------------------------------------
    if (file_selector.update()) {
        load_rom_file(chip, debugger, file_selector.get_selected_file());
    }


    //----------------------------------------------------------------------------------
    // Code Editor
    //----------------------------------------------------------------------------------
    ImGui::SetNextWindowSize({400, 400}, ImGuiCond_Appearing);
    if (ImGui::Begin("Code Editor", nullptr, ImGuiWindowFlags_MenuBar)) {
        text_editor.Render("editor");

        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem("Compile")) {
                auto lines = std::vector<std::string>{};
                lines.reserve(text_editor.GetLineCount());
                for (size_t line = 0; line < text_editor.GetLineCount(); ++line) {
                    lines.push_back(text_editor.GetLineText(line));
                }

                const auto result = compile_program(lines);

                for (auto idx : result.failures) {
                    std::cout << std::format("Compilation failure on line {} (empty instruction written)", idx + 1) << std::endl;
                }

                // Store the words big-endian
                auto bytes = std::vector<uint8_t>{};
                bytes.reserve(result.program_data.size() * 2);
                for (const auto word : result.program_data) {
                    bytes.push_back(static_cast<uint8_t>(word >> 8));
                    bytes.push_back(static_cast<uint8_t>(word));
                }

                if (const auto loaded = debugger.load_program(bytes); !loaded) {
                    std::cout << "Error loading the compiled program: " << loaded.error() << '\n';
                }
            }

            if (ImGui::BeginMenu("Breakpoint")) {
                // Each editor line holds one 2-byte instruction, starting at the program origin
                const auto cursor_line = text_editor.GetMainCursorPosition().line;
                const auto address = static_cast<uint16_t>(chip.program_origin() + (cursor_line * 2));

                if (ImGui::MenuItem("Add")) {
                    debugger.add_breakpoint(address);
                    update_breakpoint_markers(chip, debugger);
                }
                if (ImGui::MenuItem("Remove")) {
                    debugger.remove_breakpoint(address);
                    update_breakpoint_markers(chip, debugger);
                }
                if (ImGui::MenuItem("Clear")) {
                    debugger.clear_breakpoints();
                    update_breakpoint_markers(chip, debugger);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
    }
    ImGui::End();


    //----------------------------------------------------------------------------------
    // Memory
    //----------------------------------------------------------------------------------
    mem_editor.DrawWindow("Memory", state.memory.data(), state.memory.size());
}


void MediaLayer::build_default_layout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    auto center = dockspace_id;
    auto left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.25f, nullptr, &center);
    auto right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.35f, nullptr, &center);

    auto left_bottom  = ImGuiID{};
    auto left_top     = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.4f, nullptr, &left_bottom);
    auto registers    = ImGuiID{};
    auto stack        = ImGui::DockBuilderSplitNode(left_bottom, ImGuiDir_Right, 0.35f, nullptr, &registers);
    auto right_bottom = ImGuiID{};
    auto right_top    = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.6f, nullptr, &right_bottom);
    auto center_bottom = ImGuiID{};
    auto center_top    = ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.4f, nullptr, &center_bottom);

    ImGui::DockBuilderDockWindow("Chip8 Settings", left_top);
    ImGui::DockBuilderDockWindow("Registers", registers);
    ImGui::DockBuilderDockWindow("Stack", stack);
    ImGui::DockBuilderDockWindow("Display", center_top);
    ImGui::DockBuilderDockWindow("Memory", center_bottom);
    ImGui::DockBuilderDockWindow("Code Editor", right_top);
    ImGui::DockBuilderDockWindow("Program", right_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}


void MediaLayer::update_breakpoint_markers(const c8::Chip8& chip, const c8::Debugger& debugger) {
    static constexpr ImU32 breakpoint_color = IM_COL32(255, 64, 64, 96);

    text_editor.ClearMarkers();

    // Each editor line holds one 2-byte instruction, so only even offsets from the origin have a line
    const auto origin = chip.program_origin();
    for (const auto address : debugger.breakpoints()) {
        if (address < origin || ((address - origin) % 2) != 0) {
            continue;
        }

        const auto line = static_cast<size_t>((address - origin) / 2);
        if (line < text_editor.GetLineCount()) {
            text_editor.AddMarker(line, breakpoint_color, breakpoint_color, "Breakpoint", "");
        }
    }
}
