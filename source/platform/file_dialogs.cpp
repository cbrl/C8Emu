#include "file_dialogs.h"

#include "frontend/rom_files.h"

#include <array>
#include <span>

#include <nfd.h>
#include <nfd_sdl3.h>

namespace c8 {

namespace {

// NFD takes the extensions without dots, separated by commas
constexpr auto rom_filters = std::array{
    nfdu8filteritem_t{.name = "CHIP-8 ROMs", .spec = "ch8,c8h,sc8,xo8,c8,rom"},
};

constexpr auto source_filters = std::array{
    nfdu8filteritem_t{.name = "CHIP-8 assembly", .spec = "c8s,asm,s"},
    nfdu8filteritem_t{.name = "Text files", .spec = "txt"},
};

constexpr auto state_filters = std::array{
    nfdu8filteritem_t{.name = "C8Emu save states", .spec = "c8state"},
};

auto filters(FileKind kind) -> std::span<const nfdu8filteritem_t> {
    switch (kind) {
        case FileKind::rom:    return rom_filters;
        case FileKind::source: return source_filters;
        case FileKind::state:  return state_filters;
    }
    return source_filters;
}

auto open_title(FileKind kind) -> const char* {
    switch (kind) {
        case FileKind::rom:    return "Open ROM";
        case FileKind::source: return "Open Source";
        case FileKind::state:  return "Load State";
    }
    return "Open";
}

auto save_title(FileKind kind) -> const char* {
    switch (kind) {
        case FileKind::rom:    return "Export ROM";
        case FileKind::source: return "Save Source";
        case FileKind::state:  return "Save State";
    }
    return "Save";
}

auto nfd_error() -> std::string {
    const char* error = NFD_GetError();
    return ((error != nullptr) && (*error != '\0')) ? std::string{error} : std::string{"Unknown error"};
}

// Convert the result of an NFD dialog
auto to_result(nfdresult_t result, nfdu8char_t* chosen) -> FileDialogs::Result {
    switch (result) {
        case NFD_OKAY: {
            auto path = path_from_utf8(chosen);
            NFD_FreePathU8(chosen);
            return path;
        }
        case NFD_CANCEL: {
            return std::nullopt;
        }
        default: {
            return std::unexpected(nfd_error());
        }
    }
}

} // namespace


FileDialogs::FileDialogs() {
    // Under Wayland, NFD needs the display of SDL. On other systems, this does nothing.
    if (!NFD_SetDisplayPropertiesFromSDL()) {
        init_error = SDL_GetError();
        return;
    }

    if (NFD_Init() != NFD_OKAY) {
        init_error = nfd_error();
        return;
    }

    initialized = true;
}


FileDialogs::~FileDialogs() {
    if (initialized) {
        NFD_Quit();
    }
}


auto FileDialogs::open_file(SDL_Window* parent, FileKind kind, const std::filesystem::path& folder) -> Result {
    if (!initialized) {
        return std::unexpected(init_error);
    }

    const auto folder_text = folder.empty() ? std::string{} : path_to_utf8(folder);
    const auto list = filters(kind);

    auto args = nfdopendialogu8args_t{
        .filterList  = list.data(),
        .filterCount = static_cast<nfdfiltersize_t>(list.size()),
        .defaultPath = folder_text.empty() ? nullptr : folder_text.c_str(),
        .title       = open_title(kind),
    };

    // Without a parent, the dialog still works. It is only not modal to the window.
    (void)NFD_GetNativeWindowFromSDLWindow(parent, &args.parentWindow);

    nfdu8char_t* chosen = nullptr;
    const auto result = NFD_OpenDialogU8_With(&chosen, &args);
    return to_result(result, chosen);
}


auto FileDialogs::save_file(SDL_Window* parent, FileKind kind, const std::filesystem::path& folder, const std::string& name)
    -> Result {

    if (!initialized) {
        return std::unexpected(init_error);
    }

    const auto folder_text = folder.empty() ? std::string{} : path_to_utf8(folder);
    const auto list = filters(kind);

    auto args = nfdsavedialogu8args_t{
        .filterList  = list.data(),
        .filterCount = static_cast<nfdfiltersize_t>(list.size()),
        .defaultPath = folder_text.empty() ? nullptr : folder_text.c_str(),
        .defaultName = name.empty() ? nullptr : name.c_str(),
        .title       = save_title(kind),
    };

    (void)NFD_GetNativeWindowFromSDLWindow(parent, &args.parentWindow);

    nfdu8char_t* chosen = nullptr;
    const auto result = NFD_SaveDialogU8_With(&chosen, &args);
    return to_result(result, chosen);
}

} // namespace c8
