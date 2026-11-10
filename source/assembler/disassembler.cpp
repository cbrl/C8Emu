#include "disassembler.h"

#include "assembler/forms.h"
#include "chip8/analysis.h"

#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <string_view>

namespace c8 {

namespace {

// The most bytes on one db line
constexpr size_t max_data_run = 8;

// The width of the mnemonic column. Most mnemonics have 4 letters or fewer.
constexpr size_t mnemonic_width = 5;

// Instruction and data lines have an indent, so the labels stand out
constexpr std::string_view indent = "    ";

// The label of each address that has one
using LabelNames = std::map<uint32_t, std::string>;


auto label_name(TargetKind kind, uint32_t address) -> std::string {
    auto prefix = std::string_view{};
    switch (kind) {
        case TargetKind::data:       prefix = "dat"; break;
        case TargetKind::jump:       prefix = "lbl"; break;
        case TargetKind::table:      prefix = "tbl"; break;
        case TargetKind::subroutine: prefix = "sub"; break;
    }
    return std::format("{}_{:03x}", prefix, address);
}


auto read_word(std::span<const uint8_t> bytes, size_t offset) -> uint16_t {
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}


// The mnemonic, padded to the width of the mnemonic column. A longer mnemonic gets one space.
auto statement(std::string_view mnemonic) -> std::string {
    auto text = std::string{mnemonic};
    text.resize(std::max(text.size() + 1, mnemonic_width), ' ');
    return text;
}


// The text of an instruction. F000 NNNN reads its address from next_word.
// If labels is not null, a label replaces each address that has one.
auto format_word(uint16_t word, uint16_t next_word, Variant variant, const LabelNames* labels) -> std::string {
    const auto* form = form_for_word(word, variant);
    if (form == nullptr) {
        return statement("dw") + std::format("0x{:04X}", word);
    }
    if (form->operand_count == 0) {
        return std::string{form->mnemonic};
    }

    const uint32_t x = (word >> 8) & 0xFu;
    const uint32_t y = (word >> 4) & 0xFu;

    // The address of sys is machine code, not a place in the program
    const bool use_labels = (labels != nullptr) && (form->op != isa::Op::sys);

    const auto address_text = [&](uint32_t address, std::string_view number_format) {
        if (use_labels) {
            if (const auto it = labels->find(address); it != labels->end()) {
                return it->second;
            }
        }
        return std::vformat(number_format, std::make_format_args(address));
    };

    auto text = statement(form->mnemonic);
    auto separator = std::string_view{};

    for (const auto kind : form->operands()) {
        text += separator;
        separator = (kind == OperandKind::long_kw) ? " " : ", ";

        switch (kind) {
            case OperandKind::vx:      text += std::format("v{:X}", x); break;
            case OperandKind::vy:      text += std::format("v{:X}", y); break;
            case OperandKind::v0:      text += "v0"; break;
            case OperandKind::i:       text += "i"; break;
            case OperandKind::long_kw: text += "long"; break;
            case OperandKind::nn:      text += std::format("0x{:02X}", word & 0xFFu); break;
            case OperandKind::n:       text += std::format("0x{:X}", word & 0xFu); break;
            case OperandKind::nx:      text += std::format("0x{:X}", x); break;
            case OperandKind::nnn:     text += address_text(word & 0xFFFu, "0x{:03X}"); break;
            case OperandKind::nnnn:    text += address_text(next_word, "0x{:04X}"); break;
        }
    }

    return text;
}


// The platform line at the start of the text. CHIP-8 needs none.
auto add_platform_line(DisassemblyResult& result, Variant variant) -> void {
    if (variant != Variant::chip8) {
        result.lines.push_back(std::format("{}platform {}", indent, variant_id(variant)));
    }
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


// The label of each target. A target inside an instruction gets no label, because no line starts there.
auto label_names(const FlowAnalysis& flow) -> LabelNames {
    auto names = LabelNames{};
    for (const auto& [address, kind] : flow.targets()) {
        const auto offset = flow.offset_of(address, 1);
        if (offset && (flow.role(*offset) != ByteRole::operand)) {
            names.emplace(address, label_name(kind, address));
        }
    }
    return names;
}


auto disassemble_flow(std::span<const uint8_t> program, uint32_t origin, Variant variant, DisassemblyResult& result) -> void {
    const auto flow = FlowAnalysis{program, origin, variant};
    const auto labels = label_names(flow);

    size_t offset = 0;
    while (offset < program.size()) {
        const auto address = static_cast<uint32_t>(origin + offset);

        if (const auto it = labels.find(address); it != labels.end()) {
            add_label(result, it->second, address);
        }

        if (flow.role(offset) == ByteRole::instruction) {
            const uint16_t word = read_word(program, offset);
            const size_t size = isa::instruction_size(isa::decode(word, variant));
            const uint16_t next_word = (size == 4) ? read_word(program, offset + 2) : uint16_t{0};

            // A word that executes but has no form of its own (0230 on Hires CHIP-8) is written with dw
            const auto kind = (form_for_word(word, variant) != nullptr) ? SpanKind::code : SpanKind::data;
            add_line(result, format_word(word, next_word, variant, &labels), address, size, kind);
            offset += size;
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


auto disassemble_linear(std::span<const uint8_t> program, uint32_t origin, Variant variant, DisassemblyResult& result) -> void {
    size_t offset = 0;

    while ((offset + 1) < program.size()) {
        const uint16_t word = read_word(program, offset);
        const auto* form = form_for_word(word, variant);

        // F000 NNNN needs its second word. Without it, F000 is a data word.
        const bool long_form = (form != nullptr) && (form->size() == 4);
        if (long_form && ((offset + 3) >= program.size())) {
            add_line(result, statement("dw") + std::format("0x{:04X}", word), static_cast<uint32_t>(origin + offset), 2, SpanKind::data);
            offset += 2;
            continue;
        }

        const size_t size = long_form ? 4 : 2;
        const uint16_t next_word = long_form ? read_word(program, offset + 2) : uint16_t{0};

        // The assembler writes an undefined word with dw, so its span is data
        const auto kind = (form != nullptr) ? SpanKind::code : SpanKind::data;
        add_line(result, format_word(word, next_word, variant, nullptr), static_cast<uint32_t>(origin + offset), size, kind);
        offset += size;
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


auto format_instruction(uint16_t word, uint16_t next_word, Variant variant) -> std::string {
    return format_word(word, next_word, variant, nullptr);
}


auto disassemble(std::span<const uint8_t> program, const DisassemblyOptions& options) -> DisassemblyResult {
    auto result = DisassemblyResult{};
    add_platform_line(result, options.variant);

    switch (options.mode) {
        case DisassemblyMode::flow:   disassemble_flow(program, options.origin, options.variant, result); break;
        case DisassemblyMode::linear: disassemble_linear(program, options.origin, options.variant, result); break;
    }

    return result;
}

} // namespace c8
