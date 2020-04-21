#include "file_selector.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include <functional>

FileSelector::FileSelector(std::filesystem::path root_dir) {
	if (std::filesystem::exists(root_dir)) {
		set_path(root_dir);
	}
	else {
		set_path(std::filesystem::current_path());
	}
}


void FileSelector::open_selector() {
	open = true;
}


std::filesystem::path FileSelector::get_selected_file() {
	return selected_file;
}


void FileSelector::set_path(const std::filesystem::path& path) {
	current_path = std::filesystem::absolute(path);
	nav_bar_text = current_path.string();

	curr_file_list.clear();
	curr_dir_list.clear();
	update_file_list = true;
}


bool FileSelector::update() {
	bool file_selected = false;

	if (open) {
		ImGui::OpenPopup("FileSelector");
		open = false;
	}

	if (ImGui::BeginPopupModal("FileSelector", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

		//----------------------------------------------------------------------------------
		// Nav Bar
		//----------------------------------------------------------------------------------
		if (ImGui::BeginChild("NavBar", {425, 40}, true, ImGuiWindowFlags_NoSavedSettings)) {
			if (ImGui::Button("^")) {
				set_path(current_path.parent_path());
			}

			ImGui::SameLine();

			std::string path_string = current_path.string();
			ImGui::PushItemWidth(350);
			ImGui::InputText("", &nav_bar_text);
			ImGui::PopItemWidth();

			ImGui::SameLine();

			if (ImGui::Button("Go")) {
				if (std::filesystem::exists(nav_bar_text))
					set_path(nav_bar_text);
			}
		}
		ImGui::EndChild();


		//----------------------------------------------------------------------------------
		// File List
		//----------------------------------------------------------------------------------
		if (ImGui::BeginChild("FileList", {425, 300}, true, ImGuiWindowFlags_NoSavedSettings)) {
			list_files(std::filesystem::directory_iterator{current_path});
		}
		ImGui::EndChild();

		if (ImGui::BeginChild("FileBar", {425, 40}, true, ImGuiWindowFlags_NoSavedSettings)) {
			ImGui::Text("File:");
			ImGui::SameLine();

			// Filename Box
			std::string selected_string = selected_file.string();
			ImGui::PushItemWidth(250);
			ImGui::InputText("", &selected_string, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
			
			ImGui::SameLine();

			// OK/Cancel Buttons
			ImGui::BeginGroup();
			if (ImGui::Button("Open")) {
				if (std::filesystem::exists(selected_file)) {
					file_selected = true;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndGroup();
		}
		ImGui::EndChild();

		ImGui::EndPopup();
	}

	return file_selected;
}


void FileSelector::list_files(std::filesystem::directory_iterator it) {
	static const auto folder_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	static const float text_offset = -16.0f;

	// Update files
	if (update_file_list) {
		update_file_list = false;

		std::vector<std::filesystem::path> folders;
		std::vector<std::filesystem::path> files;

		for (const auto p : it) {
			const auto p_path = p.path();
			if (p.is_directory()) {
				folders.emplace_back(p_path);
			}
			else {
				files.emplace_back(p_path);
			}
		}

		auto cmp = [](const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
			return lhs.filename().string() < rhs.filename().string();
		};

		std::sort(folders.begin(), folders.end(), cmp);
		std::sort(files.begin(), files.end(), cmp);

		for (const auto& folder : folders) {
			curr_dir_list.emplace_back("[Dir]  " + folder.filename().string(), folder);
		}
		for (const auto& file : files) {
			curr_file_list.emplace_back("[File]  " + file.filename().string(), file);
		}
	}

	// Up folder
	if (current_path.has_parent_path() && (current_path.parent_path() != current_path)) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + text_offset);

		if (ImGui::TreeNodeEx("[Dir]  ..", folder_flags)) {
			if (ImGui::IsItemActivated()) {
				set_path(current_path.parent_path());
			}
			ImGui::TreePop();
		}
	}

	// List folders
	for (const auto& [dir_name, dir_path] : curr_dir_list) {
		//ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.88f, 0.0f, 1.0f});
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + text_offset);

		if (ImGui::TreeNodeEx(dir_name.c_str(), folder_flags)) {
			if (ImGui::IsItemActivated()) {
				set_path(dir_path);
			}
			ImGui::TreePop();
		}
		//ImGui::PopStyleColor();
	}

	// List files
	for (const auto& [file_name, file_path] : curr_file_list) {
		std::error_code err; //err passed to std::filesystem::equivalent to suppress potential run-time exception
		const bool selected = std::filesystem::equivalent(file_path, selected_file, err);
		const auto flags = ImGuiTreeNodeFlags_Leaf | (selected ? ImGuiTreeNodeFlags_Selected : 0);

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + text_offset);

		if (ImGui::TreeNodeEx(file_name.c_str(), flags)) {
			if (ImGui::IsItemActivated()) {
				selected_file = file_path;
			}
			ImGui::TreePop();
		}
	}
}
