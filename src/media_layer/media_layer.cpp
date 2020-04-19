#include "media_layer.h"

#include "chip8/chip8.h"
#include "instruction/instruction.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include <iostream>


void RGBA2FloatArray(uint32_t rgba, float array[4]) {
    array[0] = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
    array[1] = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
    array[2] = static_cast<float>((rgba >> 8)  & 0xFF) / 255.0f;
    array[3] = static_cast<float>( rgba        & 0xFF) / 255.0f;
}

uint32_t FloatArray2RGBA(const float array[4]) {
    uint32_t rgba = 0;
    rgba |= static_cast<uint32_t>(array[0] * 255) << 24;
    rgba |= static_cast<uint32_t>(array[1] * 255) << 16;
    rgba |= static_cast<uint32_t>(array[2] * 255) << 8;
    rgba |= static_cast<uint32_t>(array[3] * 255);
    return rgba;
}


static const std::map<SDL_Scancode, Keys> key_map = {
    {SDL_SCANCODE_KP_0, Keys::Key0},
    {SDL_SCANCODE_KP_1, Keys::Key1},
    {SDL_SCANCODE_KP_2, Keys::Key2},
    {SDL_SCANCODE_KP_3, Keys::Key3},
    {SDL_SCANCODE_KP_4, Keys::Key4},
    {SDL_SCANCODE_KP_5, Keys::Key5},
    {SDL_SCANCODE_KP_6, Keys::Key6},
    {SDL_SCANCODE_KP_7, Keys::Key7},
    {SDL_SCANCODE_KP_8, Keys::Key8},
    {SDL_SCANCODE_KP_9, Keys::Key9},
    {SDL_SCANCODE_A,    Keys::KeyA},
    {SDL_SCANCODE_B,    Keys::KeyB},
    {SDL_SCANCODE_C,    Keys::KeyC},
    {SDL_SCANCODE_D,    Keys::KeyD},
    {SDL_SCANCODE_E,    Keys::KeyE},
    {SDL_SCANCODE_F,    Keys::KeyF},
};


MediaLayer::MediaLayer() {
    //--------------------------------------------------------------------------------
    // SDL Init
    //--------------------------------------------------------------------------------
    const int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    if (result != 0) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << '\n';
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
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << '\n';
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
        std::cout << "SDL_GL_CreateContext Error: " << SDL_GetError() << '\n';
    }

    SDL_GL_MakeCurrent(window, gl_context);

    if (gl3wInit() != 0) {
        std::cout << "gl3w initialization failed\n";
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
}


MediaLayer::~MediaLayer() {
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
	// CHIP-8 Status
	//----------------------------------------------------------------------------------
	if (ImGui::Begin("CHIP-8", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

		float reg_x = 100;
		float reg_y = 350;

		//----------------------------------------------------------------------------------
		// Registers
		//----------------------------------------------------------------------------------
		ImGui::BeginChild("Registers", {reg_x, reg_y}, true);
			ImGui::Text("Registers");
			ImGui::Separator();

			for (uint8_t i = 0; i <= 0xF; ++i) {
				ImGui::Text("v%x: 0x%02X", i, chip.v[i]);
			}
			ImGui::Separator();
			ImGui::Text(" I: 0x%04X", chip.i);
			ImGui::Text("PC: 0x%04X", chip.pc);
		ImGui::EndChild();


		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		
		ImGui::BeginGroup();
		{
			//----------------------------------------------------------------------------------
			// Program Instructions
			//----------------------------------------------------------------------------------
			float prog_x = 200;
			float prog_y = 200;
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
			ImGui::BeginChild("Chip8 Settings", {prog_x, 0}, true);
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

				if (ImGui::Button("Reset System")) {
					chip.reset();
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

		ImGui::BeginChild("Image", {x_size + 16.0f, y_size + 16.0f}, true);
			ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2{x_size, y_size});
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

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
	if (ImGui::Begin("ROM", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		
		static std::string filename;
		static std::filesystem::path path;

		static const std::function<void(std::filesystem::directory_iterator)> list_files = [](std::filesystem::directory_iterator it) {
			for (auto& d : it) {
				auto d_path = d.path();
				if (d.is_directory()) {
					bool node_open = ImGui::TreeNode(d_path.filename().string().c_str());
					if (node_open) {
						list_files(std::filesystem::directory_iterator(d_path));
						ImGui::TreePop();
					}
				}
				else if (d_path.extension() == ".ch8") {
					if (ImGui::Selectable(d_path.filename().string().c_str())) {
						path = d_path;
						filename = path.filename().string();
					}
				}
			}
		};
		
		if (ImGui::BeginChild("ROM_List", ImVec2(350, 250), true, ImGuiWindowFlags_NoSavedSettings)) {
			bool node_open = ImGui::TreeNodeEx("./", ImGuiTreeNodeFlags_DefaultOpen);
			if (node_open) {
				list_files(std::filesystem::directory_iterator("./"));
				ImGui::TreePop();
			}
		}
		ImGui::EndChild();

		ImGui::PushItemWidth(-1);
		ImGui::InputText("", &filename, ImGuiInputTextFlags_ReadOnly);
		ImGui::PopItemWidth();
		if (ImGui::Button("Load")) {
			(void)chip.load_rom(path);
		}
	}
	ImGui::End();
}