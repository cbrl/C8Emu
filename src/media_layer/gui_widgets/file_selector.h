#pragma once

#include <filesystem>
#include <string>
#include <vector>

class FileSelector final {
public:
	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	FileSelector(std::filesystem::path root_dir = std::filesystem::current_path());
	FileSelector(const FileSelector&) = default;
	FileSelector(FileSelector&&) = default;


	//----------------------------------------------------------------------------------
	// Destructors
	//----------------------------------------------------------------------------------
	~FileSelector() = default;


	//----------------------------------------------------------------------------------
	// Operators
	//----------------------------------------------------------------------------------
	FileSelector& operator=(const FileSelector&) = default;
	FileSelector& operator=(FileSelector&&) = default;


	//----------------------------------------------------------------------------------
	// Member Functions
	//----------------------------------------------------------------------------------

	void open_selector();

	[[nodiscard]]
	bool update();

	[[nodiscard]]
	std::filesystem::path get_selected_file();

private:

	void set_path(const std::filesystem::path& path);

	void list_files(std::filesystem::directory_iterator it);


	//----------------------------------------------------------------------------------
	// Member Variables
	//----------------------------------------------------------------------------------
	bool open = false;
	bool update_file_list = true;

	std::filesystem::path current_path;
	std::filesystem::path selected_file;

	// The current text in the nav bar
	std::string nav_bar_text;

	// The current list of files/folders in the file window
	std::vector<std::pair<std::string, std::filesystem::path>> curr_dir_list;
	std::vector<std::pair<std::string, std::filesystem::path>> curr_file_list;
};