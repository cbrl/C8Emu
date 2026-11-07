#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "instruction/instruction.h"
#include "util/strings.h"

TEST_CASE("instruction decodes its arguments", "[instruction]")
{
  const auto instr = instruction {0xD123};

  CHECK(instr.opcode == Opcodes::drw_vx_vy_n);
  CHECK(instr.x == 0x1);
  CHECK(instr.y == 0x2);
  CHECK(instr.n == 0x3);
  CHECK(static_cast<uint16_t>(instr) == 0xD123);
}

TEST_CASE("decompiled programs compile to the same bytes", "[instruction]")
{
  // cls; mov v1 0x2A; mov i 0x123; drw v1 v2 0x5; jmp 0x200
  const auto bytes =
      std::array<uint8_t, 10> {0x00, 0xE0, 0x61, 0x2A, 0xA1, 0x23, 0xD1, 0x25, 0x12, 0x00};

  const auto decompiled = decompile_program(bytes);
  REQUIRE(decompiled.failures.empty());
  REQUIRE(decompiled.program.size() == bytes.size() / 2);

  const auto compiled = compile_program(decompiled.program);
  REQUIRE(compiled.failures.empty());

  const auto expected = std::vector<uint16_t> {0x00E0, 0x612A, 0xA123, 0xD125, 0x1200};
  CHECK(compiled.program_data == expected);
}

TEST_CASE("str_to parses integers", "[strings]")
{
  CHECK(str_to<uint16_t>("1F", 16) == std::optional<uint16_t> {0x1F});
  CHECK(str_to<uint8_t>("300") == std::nullopt);
  CHECK(str_to<int>("") == std::nullopt);
}
