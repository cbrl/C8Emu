#include "serialize.h"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>

namespace c8 {

namespace {

// The save state file. A change of the file layout or of the machine state layout needs a new version.
constexpr auto save_magic = std::string_view{"C8EMU-STATE"};
constexpr uint8_t save_version = 1;

// The largest name in a save state file
constexpr uint32_t max_name_size = 1024;


// Little-endian values into a byte vector
class Writer {
public:
    explicit Writer(std::vector<uint8_t>& target) noexcept : out(target) {
    }

    auto u8(uint8_t value) -> void {
        out.push_back(value);
    }

    auto u16(uint16_t value) -> void {
        u8(static_cast<uint8_t>(value));
        u8(static_cast<uint8_t>(value >> 8));
    }

    auto u32(uint32_t value) -> void {
        u16(static_cast<uint16_t>(value));
        u16(static_cast<uint16_t>(value >> 16));
    }

    auto u64(uint64_t value) -> void {
        u32(static_cast<uint32_t>(value));
        u32(static_cast<uint32_t>(value >> 32));
    }

    auto flag(bool value) -> void {
        u8(value ? 1 : 0);
    }

    auto bytes(std::span<const uint8_t> data) -> void {
        out.insert(out.end(), data.begin(), data.end());
    }

    auto text(std::string_view value) -> void {
        for (const char c : value) {
            out.push_back(static_cast<uint8_t>(c));
        }
    }

private:
    std::vector<uint8_t>& out;
};


// Little-endian values from bytes. A read past the end gives 0 and sets the failed state.
class Reader {
public:
    explicit Reader(std::span<const uint8_t> data) noexcept : in(data) {
    }

    auto u8() -> uint8_t {
        if (pos >= in.size()) {
            failed = true;
            return 0;
        }
        return in[pos++];
    }

    auto u16() -> uint16_t {
        const auto low = u8();
        return static_cast<uint16_t>(low | (u8() << 8));
    }

    auto u32() -> uint32_t {
        const auto low = u16();
        return static_cast<uint32_t>(low | (static_cast<uint32_t>(u16()) << 16));
    }

    auto u64() -> uint64_t {
        const auto low = u32();
        return low | (static_cast<uint64_t>(u32()) << 32);
    }

    auto flag() -> bool {
        return u8() != 0;
    }

    auto bytes(size_t count) -> std::span<const uint8_t> {
        if ((in.size() - pos) < count) {
            failed = true;
            pos = in.size();
            return {};
        }
        const auto result = in.subspan(pos, count);
        pos += count;
        return result;
    }

    // Read exactly `out.size()` bytes into `out`
    auto fill(std::span<uint8_t> out) -> void {
        const auto data = bytes(out.size());
        if (!data.empty()) {
            std::ranges::copy(data, out.begin());
        }
    }

    auto expect(std::string_view text) -> bool {
        const auto data = bytes(text.size());
        return !failed && std::ranges::equal(data, text, {}, {}, [](char c) { return static_cast<uint8_t>(c); });
    }

    [[nodiscard]]
    auto ok() const noexcept -> bool {
        return !failed;
    }

    [[nodiscard]]
    auto at_end() const noexcept -> bool {
        return pos == in.size();
    }

private:
    std::span<const uint8_t> in;
    size_t pos = 0;
    bool failed = false;
};


auto write_state(Writer& w, const MachineState& s) -> void {
    w.u32(static_cast<uint32_t>(s.memory.size()));
    w.bytes(s.memory);

    w.bytes(s.v);
    w.u16(s.i);
    w.u16(s.pc);
    for (const auto entry : s.stack) {
        w.u16(entry);
    }
    w.u8(s.sp);
    w.u8(s.delay_timer);
    w.u8(s.sound_timer);
    w.u16(s.keys);

    w.u8(static_cast<uint8_t>(s.key_wait.phase));
    w.u8(s.key_wait.reg);
    w.u8(s.key_wait.key);
    w.u16(s.key_wait.pc);
    w.u16(s.key_wait.ignored);

    w.flag(s.vblank_wait);
    w.u32(s.cycle_in_frame);
    w.u64(s.frame_count);
    w.u64(s.total_cycles);
    w.u32(s.rng);

    w.u8(static_cast<uint8_t>(s.display.width()));
    w.u8(static_cast<uint8_t>(s.display.height()));
    w.bytes(s.display.pixels());

    w.u8(s.planes);
    w.bytes(s.audio_pattern);
    w.flag(s.audio_pattern_loaded);
    w.u8(s.pitch);
    w.bytes(s.flags);
    w.flag(s.exited);
}


auto read_state(Reader& r) -> std::expected<MachineState, std::string> {
    const auto error = [](std::string message) { return std::unexpected(std::move(message)); };
    auto s = MachineState{};

    const auto memory_bytes = r.u32();
    if ((memory_bytes != memory_size(Variant::chip8)) && (memory_bytes != memory_size(Variant::xochip))) {
        return error(std::format("The memory size {} is not valid", memory_bytes));
    }
    const auto memory = r.bytes(memory_bytes);
    s.memory.assign(memory.begin(), memory.end());

    r.fill(s.v);
    s.i = r.u16();
    s.pc = r.u16();
    for (auto& entry : s.stack) {
        entry = r.u16();
    }
    s.sp = r.u8();
    s.delay_timer = r.u8();
    s.sound_timer = r.u8();
    s.keys = r.u16();

    const auto phase = r.u8();
    if (phase > static_cast<uint8_t>(KeyWaitPhase::wait_release)) {
        return error("The key wait phase is not valid");
    }
    s.key_wait.phase = static_cast<KeyWaitPhase>(phase);
    s.key_wait.reg = static_cast<uint8_t>(r.u8() & 0xF);
    s.key_wait.key = static_cast<uint8_t>(r.u8() & 0xF);
    s.key_wait.pc = r.u16();
    s.key_wait.ignored = r.u16();

    s.vblank_wait = r.flag();
    s.cycle_in_frame = r.u32();
    s.frame_count = r.u64();
    s.total_cycles = r.u64();
    s.rng = r.u32();

    const size_t width = r.u8();
    const size_t height = r.u8();
    if ((width == 0) || (width > Display::max_width) || (height == 0) || (height > Display::max_height)) {
        return error(std::format("The display size {}x{} is not valid", width, height));
    }
    s.display.assign(width, height, r.bytes(width * height));

    s.planes = static_cast<uint8_t>(r.u8() & Display::all_planes);
    r.fill(s.audio_pattern);
    s.audio_pattern_loaded = r.flag();
    s.pitch = r.u8();
    r.fill(s.flags);
    s.exited = r.flag();

    if (!r.ok()) {
        return error("The machine state is incomplete");
    }
    return s;
}


auto write_quirks(Writer& w, const Quirks& q) -> void {
    w.flag(q.shift_uses_vy);
    w.u8(static_cast<uint8_t>(q.load_store));
    w.flag(q.jump_uses_vx);
    w.flag(q.vf_reset);
    w.flag(q.wrap_sprites);
    w.flag(q.display_wait);
    w.flag(q.key_wait_release);
}


auto read_quirks(Reader& r) -> std::expected<Quirks, std::string> {
    auto q = Quirks{};
    q.shift_uses_vy = r.flag();

    const auto load_store = r.u8();
    if (load_store > static_cast<uint8_t>(LoadStoreQuirk::unchanged)) {
        return std::unexpected(std::string{"The load/store quirk is not valid"});
    }
    q.load_store = static_cast<LoadStoreQuirk>(load_store);

    q.jump_uses_vx = r.flag();
    q.vf_reset = r.flag();
    q.wrap_sprites = r.flag();
    q.display_wait = r.flag();
    q.key_wait_release = r.flag();
    return q;
}

} // namespace


auto serialize_state(const MachineState& state, std::vector<uint8_t>& out) -> void {
    out.clear();
    out.reserve(state.memory.size() + state.display.size() + 128);

    auto writer = Writer{out};
    write_state(writer, state);
}


auto serialize_state(const MachineState& state) -> std::vector<uint8_t> {
    auto out = std::vector<uint8_t>{};
    serialize_state(state, out);
    return out;
}


auto deserialize_state(std::span<const uint8_t> bytes) -> std::expected<MachineState, std::string> {
    auto reader = Reader{bytes};
    auto state = read_state(reader);
    if (state && !reader.at_end()) {
        return std::unexpected(std::string{"The machine state has extra data"});
    }
    return state;
}


auto serialize_save_state(const SaveState& save) -> std::vector<uint8_t> {
    auto out = std::vector<uint8_t>{};
    auto w = Writer{out};

    w.text(save_magic);
    w.u8(save_version);

    w.u8(static_cast<uint8_t>(save.variant));
    write_quirks(w, save.quirks);
    w.u32(save.instructions_per_frame);
    w.u16(save.origin);

    const auto name = std::string_view{save.name}.substr(0, max_name_size);
    w.u32(static_cast<uint32_t>(name.size()));
    w.text(name);

    w.u32(static_cast<uint32_t>(save.program.size()));
    w.bytes(save.program);

    // The machine state fills the rest of the file
    auto state = std::vector<uint8_t>{};
    serialize_state(save.state, state);
    w.bytes(state);

    return out;
}


auto deserialize_save_state(std::span<const uint8_t> bytes) -> std::expected<SaveState, std::string> {
    const auto error = [](std::string message) { return std::unexpected(std::move(message)); };

    auto r = Reader{bytes};
    if (!r.expect(save_magic)) {
        return error("The file is not a C8Emu save state");
    }
    if (const auto version = r.u8(); version != save_version) {
        return error(std::format("The save state has version {}. This C8Emu reads only version {}.", version, save_version));
    }

    auto save = SaveState{};

    const auto variant = r.u8();
    if (variant > static_cast<uint8_t>(Variant::xochip)) {
        return error("The platform of the save state is not valid");
    }
    save.variant = static_cast<Variant>(variant);

    auto quirks = read_quirks(r);
    if (!quirks) {
        return error(std::move(quirks.error()));
    }
    save.quirks = *quirks;
    save.instructions_per_frame = r.u32();
    save.origin = r.u16();

    const auto name_size = r.u32();
    if (name_size > max_name_size) {
        return error("The program name of the save state is too long");
    }
    for (const auto byte : r.bytes(name_size)) {
        save.name.push_back(static_cast<char>(byte));
    }

    const auto program_size = r.u32();
    if (program_size > memory_size(Variant::xochip)) {
        return error("The program of the save state is too large");
    }
    const auto program = r.bytes(program_size);
    save.program.assign(program.begin(), program.end());

    if (!r.ok()) {
        return error("The save state is incomplete");
    }

    auto state = read_state(r);
    if (!state) {
        return error(std::move(state.error()));
    }
    if (!r.at_end()) {
        return error("The save state has extra data");
    }

    save.state = std::move(*state);
    return save;
}

} // namespace c8
