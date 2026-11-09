#include "disassembler.h"

#include "assembler/forms.h"

#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <string_view>

namespace c8 {

namespace {

// The most bytes on one db line
constexpr size_t max_data_run = 8;

// The width of the mnemonic column. The longest mnemonic has 4 letters.
constexpr size_t mnemonic_width = 5;

// Instruction and data lines have an indent, so the labels stand out
constexpr std::string_view indent = "    ";

// A jump table has at most this many 2-byte entries, because v0 is at most 0xFF
constexpr size_t max_table_entries = 128;


// The kinds of label. When two kinds name the same address, the larger kind wins.
enum class LabelKind : uint8_t {
    data,       // dat_: a mov i target
    jump,       // lbl_: a jmp target
    table,      // tbl_: a jmp v0 target
    subroutine, // sub_: a call target
};

// The label of each address that has one
using LabelNames = std::map<uint32_t, std::string>;


auto label_name(LabelKind kind, uint32_t address) -> std::string {
    auto prefix = std::string_view{};
    switch (kind) {
        case LabelKind::data:       prefix = "dat"; break;
        case LabelKind::jump:       prefix = "lbl"; break;
        case LabelKind::table:      prefix = "tbl"; break;
        case LabelKind::subroutine: prefix = "sub"; break;
    }
    return std::format("{}_{:03x}", prefix, address);
}


auto read_word(std::span<const uint8_t> bytes, size_t offset) -> uint16_t {
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}


// The mnemonic, padded to the width of the mnemonic column
auto statement(std::string_view mnemonic) -> std::string {
    auto text = std::string{mnemonic};
    text.resize(std::max(text.size(), mnemonic_width), ' ');
    return text;
}


// The text of an instruction word. If labels is not null, a label replaces each address that has one.
auto format_word(uint16_t word, const LabelNames* labels) -> std::string {
    const auto* form = decode_form(word);
    if (form == nullptr) {
        return statement("dw") + std::format("0x{:04X}", word);
    }
    if (form->operand_count == 0) {
        return std::string{form->mnemonic};
    }

    const uint32_t x = (word >> 8) & 0xFu;
    const uint32_t y = (word >> 4) & 0xFu;
    const uint32_t address = word & 0xFFFu;

    // The address of sys is machine code, not a place in the program
    const bool use_labels = (labels != nullptr) && (form->mnemonic != "sys");

    auto text = statement(form->mnemonic);
    bool first = true;

    for (const auto kind : form->operands()) {
        if (!first) {
            text += ", ";
        }
        first = false;

        switch (kind) {
            case OperandKind::vx: text += std::format("v{:X}", x); break;
            case OperandKind::vy: text += std::format("v{:X}", y); break;
            case OperandKind::v0: text += "v0"; break;
            case OperandKind::i:  text += "i"; break;
            case OperandKind::nn: text += std::format("0x{:02X}", word & 0xFFu); break;
            case OperandKind::n:  text += std::format("0x{:X}", word & 0xFu); break;
            case OperandKind::nnn: {
                if (use_labels) {
                    if (const auto it = labels->find(address); it != labels->end()) {
                        text += it->second;
                        break;
                    }
                }
                text += std::format("0x{:03X}", address);
                break;
            }
        }
    }

    return text;
}


auto format_bytes(std::span<const uint8_t> bytes) -> std::string {
    auto text = statement("db");
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            text += ", ";
        }
        text += std::format("0x{:02X}", bytes[i]);
    }
    return text;
}


auto add_label(DisassemblyResult& result, const std::string& name, uint32_t address) -> void {
    result.labels.push_back(Label{
        .name    = name,
        .address = static_cast<uint16_t>(address),
        .line    = static_cast<uint32_t>(result.lines.size()),
    });
    result.lines.push_back(name + ':');
}


auto add_line(DisassemblyResult& result, std::string_view text, uint32_t address, size_t size, SpanKind kind) -> void {
    result.map.add(SourceSpan{
        .line    = static_cast<uint32_t>(result.lines.size()),
        .address = static_cast<uint16_t>(address),
        .size    = static_cast<uint16_t>(size),
        .kind    = kind,
    });
    result.lines.push_back(std::string{indent} + std::string{text});
}


// The role of a program byte in flow mode
enum class ByteRole : uint8_t {
    unknown,     // No path reached the byte, so it is data
    instruction, // The first byte of an instruction
    operand,     // The second byte of an instruction
};


/**
 * @class FlowAnalysis
 *
 * @brief Finds the instructions that the paths of execution from the origin reach
 */
class FlowAnalysis {
public:
    FlowAnalysis(std::span<const uint8_t> program, uint32_t origin) :
        bytes(program),
        start(origin),
        roles(program.size(), ByteRole::unknown) {

        follow(origin);
        while (!pending.empty()) {
            const auto address = pending.back();
            pending.pop_back();
            visit(address);
        }
    }

    [[nodiscard]]
    auto role(size_t offset) const -> ByteRole {
        return roles[offset];
    }

    // The label of each target. A target inside an instruction gets no label, because no line starts there.
    [[nodiscard]]
    auto label_names() const -> LabelNames {
        auto names = LabelNames{};
        for (const auto& [address, kind] : targets) {
            const auto offset = offset_of(address, 1);
            if (offset && (roles[*offset] != ByteRole::operand)) {
                names.emplace(address, label_name(kind, address));
            }
        }
        return names;
    }

private:
    // The offset of an address, if the program holds `count` bytes from that address
    [[nodiscard]]
    auto offset_of(uint32_t address, size_t count) const -> std::optional<size_t> {
        if (address < start) {
            return std::nullopt;
        }
        const size_t offset = address - start;
        if ((offset + count) > bytes.size()) {
            return std::nullopt;
        }
        return offset;
    }

    auto follow(uint32_t address) -> void {
        pending.push_back(address);
    }

    auto add_target(uint32_t address, LabelKind kind) -> void {
        const auto [it, inserted] = targets.try_emplace(address, kind);
        if (!inserted) {
            it->second = std::max(it->second, kind);
        }
    }

    auto visit(uint32_t address) -> void {
        const auto offset = offset_of(address, 2);
        if (!offset) {
            return;
        }

        // A path reached the address before, or the bytes belong to another instruction
        if ((roles[*offset] != ByteRole::unknown) || (roles[*offset + 1] != ByteRole::unknown)) {
            return;
        }

        // The path ran into data
        const uint16_t word = read_word(bytes, *offset);
        if ((word == 0x0000) || (decode_form(word) == nullptr)) {
            return;
        }

        roles[*offset]     = ByteRole::instruction;
        roles[*offset + 1] = ByteRole::operand;

        const uint32_t next   = address + 2;
        const uint32_t target = word & 0xFFFu;

        switch (word >> 12) {
            case 0x0: { // ret ends the path. cls and sys continue.
                if (word != 0x00EE) {
                    follow(next);
                }
                break;
            }
            case 0x1: { // jmp
                add_target(target, LabelKind::jump);
                follow(target);
                break;
            }
            case 0x2: { // call, then return to the next instruction
                add_target(target, LabelKind::subroutine);
                follow(target);
                follow(next);
                break;
            }
            case 0x3:
            case 0x4:
            case 0x5:
            case 0x9:
            case 0xE: { // A skip goes to the next instruction or the one after it
                follow(next);
                follow(next + 2);
                break;
            }
            case 0xA: { // mov i
                add_target(target, LabelKind::data);
                follow(next);
                break;
            }
            case 0xB: { // jmp v0
                add_target(target, LabelKind::table);
                follow_table(target);
                break;
            }
            default: {
                follow(next);
                break;
            }
        }
    }

    // Follow the entries of a jump table: each jmp in a row from the base
    auto follow_table(uint32_t base) -> void {
        // With v0 = 0, the jump goes to the base itself
        follow(base);

        for (size_t entry = 0; entry < max_table_entries; ++entry) {
            const auto address = static_cast<uint32_t>(base + (entry * 2));
            const auto offset = offset_of(address, 2);

            if (!offset || ((read_word(bytes, *offset) & 0xF000) != 0x1000)) {
                break;
            }
            follow(address);
        }
    }

    std::span<const uint8_t> bytes;
    uint32_t start;

    std::vector<ByteRole> roles;
    std::vector<uint32_t> pending;
    std::map<uint32_t, LabelKind> targets;
};


auto disassemble_flow(std::span<const uint8_t> program, uint32_t origin, DisassemblyResult& result) -> void {
    const auto flow = FlowAnalysis{program, origin};
    const auto labels = flow.label_names();

    size_t offset = 0;
    while (offset < program.size()) {
        const auto address = static_cast<uint32_t>(origin + offset);

        if (const auto it = labels.find(address); it != labels.end()) {
            add_label(result, it->second, address);
        }

        if (flow.role(offset) == ByteRole::instruction) {
            add_line(result, format_word(read_word(program, offset), &labels), address, 2, SpanKind::code);
            offset += 2;
            continue;
        }

        // A data line ends before an instruction or a label, or after max_data_run bytes
        size_t end = offset + 1;
        while ((end < program.size())
            && ((end - offset) < max_data_run)
            && (flow.role(end) != ByteRole::instruction)
            && !labels.contains(static_cast<uint32_t>(origin + end))) {
            ++end;
        }

        add_line(result, format_bytes(program.subspan(offset, end - offset)), address, end - offset, SpanKind::data);
        offset = end;
    }
}


auto disassemble_linear(std::span<const uint8_t> program, uint32_t origin, DisassemblyResult& result) -> void {
    size_t offset = 0;

    for (; (offset + 1) < program.size(); offset += 2) {
        const uint16_t word = read_word(program, offset);

        // The assembler writes an undefined word with dw, so its span is data
        const auto kind = (decode_form(word) != nullptr) ? SpanKind::code : SpanKind::data;
        add_line(result, format_word(word, nullptr), static_cast<uint32_t>(origin + offset), 2, kind);
    }

    if (offset < program.size()) {
        add_line(result, format_bytes(program.subspan(offset)), static_cast<uint32_t>(origin + offset), 1, SpanKind::data);
    }
}

} // namespace


auto DisassemblyResult::text() const -> std::string {
    size_t size = 0;
    for (const auto& line : lines) {
        size += line.size() + 1;
    }

    auto result = std::string{};
    result.reserve(size);

    for (const auto& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}


auto format_instruction(uint16_t word) -> std::string {
    return format_word(word, nullptr);
}


auto disassemble(std::span<const uint8_t> program, const DisassemblyOptions& options) -> DisassemblyResult {
    auto result = DisassemblyResult{};

    switch (options.mode) {
        case DisassemblyMode::flow:   disassemble_flow(program, options.origin, result); break;
        case DisassemblyMode::linear: disassemble_linear(program, options.origin, result); break;
    }

    return result;
}

} // namespace c8
