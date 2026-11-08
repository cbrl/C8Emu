#include "file_dialogs.h"

#include "frontend/rom_files.h"

#include <array>

#include <nfd.h>
#include <nfd_sdl3.h>

namespace c8 {

namespace {

// NFD takes the extensions without dots, separated by commas
constexpr auto rom_filters = std::array{
    nfdu8filteritem_t{.name = "CHIP-8 ROMs", .spec = "ch8,c8,rom"},
};

auto nfd_error() -> std::string {
    const char* error = NFD_GetError();
    return ((error != nullptr) && (*error != '\0')) ? std::string{error} : std::string{"Unknown error"};
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


auto FileDialogs::open_rom(SDL_Window* parent, const std::filesystem::path& folder)
    -> std::expected<std::optional<std::filesystem::path>, std::string> {

    if (!initialized) {
        return std::unexpected(init_error);
    }

    const auto folder_text = folder.empty() ? std::string{} : path_to_utf8(folder);

    auto args = nfdopendialogu8args_t{
        .filterList  = rom_filters.data(),
        .filterCount = static_cast<nfdfiltersize_t>(rom_filters.size()),
        .defaultPath = folder_text.empty() ? nullptr : folder_text.c_str(),
        .title       = "Open ROM",
    };

    // Without a parent, the dialog still works. It is only not modal to the window.
    (void)NFD_GetNativeWindowFromSDLWindow(parent, &args.parentWindow);

    nfdu8char_t* chosen = nullptr;
    switch (NFD_OpenDialogU8_With(&chosen, &args)) {
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

} // namespace c8
