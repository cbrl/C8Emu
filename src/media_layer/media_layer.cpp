#include "media_layer.h"

#include "chip8/chip8.h"
#include "instruction/instruction.h"

#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include <iostream>


static void RGBA2FloatArray(uint32_t rgba, float array[4]) {
    static constexpr float normalize = 1.0f / 255.0f;
    array[0] = static_cast<float>((rgba >> 24) & 0xFF) * normalize;
    array[1] = static_cast<float>((rgba >> 16) & 0xFF) * normalize;
    array[2] = static_cast<float>((rgba >> 8)  & 0xFF) * normalize;
    array[3] = static_cast<float>( rgba        & 0xFF) * normalize;
}

static uint32_t FloatArray2RGBA(const float array[4]) {
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
    const int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    if (result != 0) {
        const std::string error = SDL_GetError();
        std::cout << "SDL_Init Error: " << error << '\n';
        throw std::runtime_error(error);
    }

    window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        960,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

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

    if (gl3wInit() != 0) {
        std::cout << "gl3w initialization failed\n";

        SDL_DestroyWindow(window);
        SDL_Quit();

        throw std::runtime_error("gl3w initialization failed");
    }

    // Create the CHIP-8 display texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    //--------------------------------------------------------------------------------
    // ImGui Init
    //--------------------------------------------------------------------------------
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

	ImGui::StyleColorsDark();
}


MediaLayer::~MediaLayer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

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
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
            case SDL_QUIT: {
                quit = true;
            }
            break;

            case SDL_KEYDOWN: {
                const auto it = key_map.find(event.key.keysym.scancode);
                if (it != key_map.end()) {
                    chip.input.set_key_state(it->second, true);
                }
            }
            break;

            case SDL_KEYUP: {
                const auto it = key_map.find(event.key.keysym.scancode);
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
    ImGui_ImplSDL2_NewFrame(window);
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
	// CHIP-8 Status
	//----------------------------------------------------------------------------------
	if (ImGui::Begin("CHIP-8", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

		//----------------------------------------------------------------------------------
		// Registers
		//----------------------------------------------------------------------------------
		const float reg_x = 100;
		const float reg_y = 350;

		ImGui::BeginChild("Registers", {reg_x, reg_y}, true);
		{
			ImGui::Text("Registers");
			ImGui::Separator();

			for (uint8_t i = 0; i <= 0xF; ++i) {
				ImGui::Text("v%X: 0x%02X", i, chip.v[i]);
			}
			ImGui::Separator();
			ImGui::Text(" I: 0x%04X", chip.i);
			ImGui::Text("PC: 0x%04X", chip.pc);
		}
		ImGui::EndChild();


		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		
		ImGui::BeginGroup();
		{
			const float prog_x = 200;
			const float prog_y = 200;

			//----------------------------------------------------------------------------------
			// Program Instructions
			//----------------------------------------------------------------------------------
			ImGui::BeginChild("Program", {prog_x, prog_y}, true, ImGuiWindowFlags_NoScrollbar);
			{
				ImGui::Text("Program Instructions");
				ImGui::Separator();

				static constexpr int count = 10;
				static std::vector<std::string> instructions(count);

				for (size_t i = 0; i < count; ++i) {
					const instruction instr = (chip.memory[chip.pc+(2*i)] << 8) | (chip.memory[chip.pc+(2*i)+1]);
					instructions[i] = to_string(instr);
					if (i == 0) ImGui::Text("0x%04X - %s", chip.pc, instructions[0].data());
					else ImGui::TextDisabled("0x%04X - %s", (uint32_t)(chip.pc+(2*i)), instructions[i].data());
				}
			}
			ImGui::EndChild();

			//----------------------------------------------------------------------------------
			// Settings
			//----------------------------------------------------------------------------------
			ImGui::BeginChild("Chip8 Settings", {prog_x, 0}, true, ImGuiWindowFlags_NoScrollbar);
			{
				ImGui::Text("Settings");
				ImGui::Separator();
				ImGui::Spacing();
				
				uint32_t clock = chip.get_clock_rate();
				ImGui::Text("Max Clock (Hz)");
				if (ImGui::InputInt("", (int*)&clock)) {
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
			}
			ImGui::EndChild();
		}
		ImGui::EndGroup();


		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		//----------------------------------------------------------------------------------
		// Stack
		//----------------------------------------------------------------------------------
		ImGui::BeginChild("Stack", {reg_x, reg_y}, true);
		{
			ImGui::Text("Stack");
			ImGui::Separator();

			for (ptrdiff_t i = chip.stack.size()-1; i >= 0; --i) {
				ImGui::Text("%02d: 0x%04X", (int)i, chip.stack[i]);
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();


	//----------------------------------------------------------------------------------
	// CHIP-8 Display
	//----------------------------------------------------------------------------------
	if (ImGui::Begin("Display", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

		const float x_size = chip.display.size_x() * display_scale;
		const float y_size = chip.display.size_y() * display_scale;

        // Draw the display texture
		ImGui::BeginChild("Image", {x_size + 16.0f, y_size + 16.0f}, true);
			ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2{x_size, y_size});
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

        // Display settings
		ImGui::BeginChild("Display Settings", {200, 100});
		{
            static const uint8_t scale_step = 1;
			ImGui::InputScalar("Scale", ImGuiDataType_U8, &display_scale, &scale_step);
			
			const uint32_t off = chip.display.get_background_color();
			const uint32_t on  = chip.display.get_foreground_color();

			float off_arr[4] = {};
            float on_arr[4] = {};

            RGBA2FloatArray(off, off_arr);
            RGBA2FloatArray(on, on_arr);

			if (ImGui::ColorEdit3("Background Color", off_arr)) {
                chip.display.set_background_color(FloatArray2RGBA(off_arr));
			}
			if (ImGui::ColorEdit3("Foreground Color", on_arr)) {
                chip.display.set_foreground_color(FloatArray2RGBA(on_arr));
			}

			bool wrap = chip.display.get_wrapping();
			if (ImGui::Checkbox("Wrapping", &wrap)) {
				chip.display.set_wrapping(wrap);
			}
		}
		ImGui::EndChild();

	}
	ImGui::End();


	//----------------------------------------------------------------------------------
	// ROM
	//----------------------------------------------------------------------------------
	if (file_selector.update()) {
		(void)chip.load_rom(file_selector.get_selected_file());
	}


	//----------------------------------------------------------------------------------
	// Memory
	//----------------------------------------------------------------------------------
	mem_editor.DrawWindow("Memory", chip.memory.data(), chip.memory.size());
}