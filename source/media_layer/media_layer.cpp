#include "media_layer.h"
#include "chip8/chip8.h"
#include "instruction/instruction.h"
#include "util/strings.h"

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

        SDL_DestroyWindow(window);
        SDL_Quit();

        throw std::runtime_error(error);
    }

    // Initialize beeper
    if (!beeper.init_audio()) {
        //...
    }


    //--------------------------------------------------------------------------------
    // OpenGL Init
    //--------------------------------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Use VSync
    SDL_GL_SetSwapInterval(1);

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        const std::string error = SDL_GetError();
        std::cout << "SDL_GL_CreateContext Error: " << error << '\n';

        SDL_DestroyWindow(window);
        SDL_Quit();

        throw std::runtime_error(error);
    }

    SDL_GL_MakeCurrent(window, gl_context);

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


void MediaLayer::set_display_scale(uint8_t scale) {
    display_scale = scale;
}


void MediaLayer::process_events(chip8& chip, bool& quit) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
            case SDL_EVENT_QUIT: {
                quit = true;
            }
            break;

            case SDL_EVENT_KEY_DOWN: {
                const auto it = key_map.find(event.key.scancode);
                if (it != key_map.end()) {
                    chip.input.set_key_state(it->second, true);
                }
            }
            break;

            case SDL_EVENT_KEY_UP: {
                const auto it = key_map.find(event.key.scancode);
                if (it != key_map.end()) {
                    chip.input.set_key_state(it->second, false);
                }
            }
            break;

            default: break;            
        }
    }

    // Process sound output
    if (chip.timer.is_sound()) {
        beeper.start_beep();
    }
    else {
        beeper.stop_beep();
    }
}


void MediaLayer::render(chip8& chip) {
    begin_frame();
    render_ui(chip);
    end_frame();
}


void MediaLayer::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}


void MediaLayer::end_frame() {
    ImGui::Render();

    const ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}


void MediaLayer::render_ui(chip8& chip) {

    // Update the CHIP-8 display texture
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        64, 32,
        false,
        GL_RGBA,
        GL_UNSIGNED_INT_8_8_8_8,
        chip.display.data()
    );

    // ImGui::ShowDemoWindow();

    //----------------------------------------------------------------------------------
	// Menu Bar
	//----------------------------------------------------------------------------------
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::MenuItem("Open ROM")) {
			file_selector.open_selector();
		}

        if (ImGui::BeginMenu("Options")) {
            bool legacy = chip.is_legacy_mode();
            if (ImGui::Checkbox("Legacy Mode", &legacy)) {
                chip.set_legacy_mode(legacy);
            }

            ImGui::EndMenu();
        }
	}
	ImGui::EndMainMenuBar();


    //----------------------------------------------------------------------------------
    // Invisible Root Docking Window
    //----------------------------------------------------------------------------------
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());


    //----------------------------------------------------------------------------------
    // Register Window
    //----------------------------------------------------------------------------------
    if (ImGui::Begin("Registers", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Registers");
		ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

        // V Registers
		for (uint8_t i = 0; i <= 0xF; ++i) {
            auto value_str = std::format("0x{:02X}", chip.v[i]);
            ImGui::PushID(i);

            ImGui::Text("v%X:", i);
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::CalcTextSize(value_str.c_str()).x);
            if (ImGui::InputText("##reg_v", &value_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                chip.v[i] = str_to<uint8_t>(value_str, 16).value_or(chip.v[i]);
            }

            ImGui::PopID();
		}
		ImGui::Separator();

        // I Register
        auto reg_i_str = std::format("0x{:04X}", chip.i);
        ImGui::Text(" I:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(reg_i_str.c_str()).x);
        if (ImGui::InputText("##reg_i", &reg_i_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            chip.i = str_to<uint16_t>(reg_i_str, 16).value_or(chip.i);
        }

        // Program Counter
        auto pc_str = std::format("0x{:04X}", chip.pc);
        ImGui::Text("PC:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(pc_str.c_str()).x);
        if (ImGui::InputText("##pc", &pc_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            chip.pc = str_to<uint16_t>(pc_str, 16).value_or(chip.pc);
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

        // Print the editable stack contents
		for (ptrdiff_t i = chip.stack.size()-1; i >= 0; --i) {
            auto value_str = std::format("0x{:04X}", chip.stack[i]);

            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%02d:", (int)i);
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::CalcTextSize(value_str.c_str()).x);
            if (ImGui::InputText("##stack", &value_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                chip.stack[i] = str_to<uint16_t>(value_str, 16).value_or(chip.stack[i]);
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

        bool update_strings = chip.pc != last_pc;

        update_strings |= ImGui::InputInt("Preview Count", &instruction_count);
        instructions.resize(instruction_count);
        ImGui::Separator();

        // Update instruction list if PC changed or the user changed the preview count
        if (update_strings) {
            last_pc = chip.pc;

            for (size_t i = 0; i < instruction_count; ++i) {
                const auto instr = instruction{chip.memory[chip.pc + (2 * i)], chip.memory[chip.pc + (2 * i) + 1]};
                instructions[i] = to_string(instr);
            }
        }

        for (size_t i = 0; i < instruction_count; ++i) {
			if (i == 0) ImGui::Text("0x%04X - %s", chip.pc, instructions[0].data());
			else ImGui::TextDisabled("0x%04X - %s", static_cast<uint32_t>(chip.pc + (2*i)), instructions[i].data());
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

        // CHIP settings
		uint32_t clock = chip.get_clock_rate();
		ImGui::Text("Max Clock (Hz)");
		if (ImGui::InputInt("##clock", (int*)&clock)) {
			chip.set_clock_rate(clock);
		}

		ImGui::Spacing();

        // Reset and reload the ROM
		if (ImGui::Button("Reset System")) {
            const auto rom = chip.current_rom;
			chip.reset();
            if (std::filesystem::exists(rom)) {
                (void)chip.load_rom(rom);
            }
		}

        // Pause/Resume/Step buttons
        if (chip.is_paused()) {
            if (ImGui::Button("Resume")) {
                chip.resume();
            }
            ImGui::SameLine();
            if (ImGui::Button("Step")) {
                chip.run_cycle();
            }
        }
        else {
            if (ImGui::Button("Pause")) {
                chip.pause();
            }
        }
        
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

        // Display settings
        static const uint8_t scale_step = 1;
		ImGui::InputScalar("Display Scale", ImGuiDataType_U8, &display_scale, &scale_step);
			
		const uint32_t off = chip.display.get_background_color();
		const uint32_t on  = chip.display.get_foreground_color();

		auto off_arr = std::array<float, 4>{};
        auto on_arr  = std::array<float, 4>{};

        RGBA2FloatArray(off, off_arr);
        RGBA2FloatArray(on, on_arr);

		if (ImGui::ColorEdit3("Background Color", off_arr.data())) {
            chip.display.set_background_color(FloatArray2RGBA(off_arr));
		}
		if (ImGui::ColorEdit3("Foreground Color", on_arr.data())) {
            chip.display.set_foreground_color(FloatArray2RGBA(on_arr));
		}

		bool wrap = chip.display.get_wrapping();
		if (ImGui::Checkbox("Wrapping", &wrap)) {
			chip.display.set_wrapping(wrap);
		}
	}
	ImGui::End();

	//----------------------------------------------------------------------------------
	// CHIP-8 Display
	//----------------------------------------------------------------------------------
	if (ImGui::Begin("Display", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

		const auto x_size = static_cast<float>(chip.display.size_x() * display_scale);
		const auto y_size = static_cast<float>(chip.display.size_y() * display_scale);

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
		if (chip.load_rom(file_selector.get_selected_file())) {
            // Decompile the ROM and update the text editor
            const auto program_data = std::span{&chip.memory[chip.rom_start], chip.rom_end - chip.rom_start};
            const auto result = decompile_program(program_data);

            for (auto idx : result.failures) {
                std::cout << std::format("Decompilation failure on instruction {} (byte offset: {})", idx, idx * 2) << std::endl;
            }

            // SetText removes all markers, so show the breakpoints again
            text_editor.SetText(std::vector<std::string_view>(result.program.begin(), result.program.end()));
            update_breakpoint_markers(chip);
        }
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

                (void)chip.load_rom(result.program_data);
            }

            if (ImGui::BeginMenu("Breakpoint")) {
                // Breakpoints are 1-based line numbers. Editor lines are 0-based.
                const auto cursor_line = static_cast<uint16_t>(text_editor.GetMainCursorPosition().line + 1);

                if (ImGui::MenuItem("Add")) {
                    chip.add_breakpoint(cursor_line);
                    update_breakpoint_markers(chip);
                }
                if (ImGui::MenuItem("Remove")) {
                    chip.remove_breakpoint(cursor_line);
                    update_breakpoint_markers(chip);
                }
                if (ImGui::MenuItem("Clear")) {
                    chip.clear_breakpoints();
                    update_breakpoint_markers(chip);
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
	mem_editor.DrawWindow("Memory", chip.memory.data(), chip.memory.size());
}


void MediaLayer::update_breakpoint_markers(const chip8& chip) {
    static constexpr ImU32 breakpoint_color = IM_COL32(255, 64, 64, 96);

    text_editor.ClearMarkers();

    for (const auto line : chip.get_breakpoints()) {
        if (line > 0 && line <= text_editor.GetLineCount()) {
            text_editor.AddMarker(static_cast<size_t>(line) - 1, breakpoint_color, breakpoint_color, "Breakpoint", "");
        }
    }
}
