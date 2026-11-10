# The C8Emu assembler

This document describes the source language of the C8Emu assembler. The
Syntax Reference panel in the app shows the same instruction table.

## Lines

Each line has this form. All parts are optional.

```
label:  instruction operands   ; comment
```

- Letter case has no effect: `MOV V1, 0x2A` and `mov v1 0x2a` are the same.
- Spaces, tabs, or commas separate the operands.
- A comment starts with `;` and continues to the end of the line.
- A line can define one label. A label can stand alone on its line. It then
  applies to the next instruction or data line.

## Numbers

| Form | Base | Note |
|---|---|---|
| `2A`, `0x2A`, `FF` | hexadecimal | A number without a prefix is hexadecimal. |
| `#42`, `#-1` | decimal | A negative number is stored as two's complement. |
| `%0010_1010` | binary | A `_` can separate the digits after a prefix. |
| `%..#.#.#.` | binary | A `.` is 0 and a `#` is 1. Use this form for sprite rows. |

A number without a prefix is hexadecimal, so `mov v0, 10` sets v0 to 16.
Write `#10` for decimal 10.

A `-` just before a number without a prefix is an error. The number `-10` would
mean -0x10, but is easy to mistake as the decimal number -10. Write `#-10` for
decimal -10, or `-0x10` for hexadecimal.

## Expressions

An operand can be an expression. The operators have the same precedence as in C.
This table starts with the highest precedence:

| Operators | Meaning |
|---|---|
| `-` `+` `~` | negative, positive, bitwise NOT (before one value) |
| `*` `/` `%` | multiplication, division, remainder |
| `+` `-` | addition, subtraction |
| `<<` `>>` | shifts |
| `&` | bitwise AND |
| `^` | bitwise XOR |
| `\|` | bitwise OR |

Parentheses group the parts of an expression.

A space separates two operands. So an expression has no spaces, or it is in
parentheses:

```
mov v0, x+1          ; one operand: x + 1
mov v0, (x + 1)      ; the same
db 1 #-1             ; two bytes: 1 and -1
```

Division and the remainder round toward zero. A division by zero is an
error. A value that does not fit in its field is an error. For example, the
`nn` field of `mov vX, nn` takes 0 to 0xFF, or #-128 to #-1.

## Labels and constants

A label has the address of the line it identifies:

```
loop:
    add v0, 1
    jmp loop
```

A constant gets its value from `equ`:

```
WIDTH  equ #64
RIGHT  equ WIDTH-8
```

A label or a constant can be used before the line that defines it. A
constant can use labels and other constants. A constant that uses its own
value, also through other constants, is an error.

A name starts with a letter, `_`, or `.`. It holds only letters, digits,
`_`, and `.`. A name cannot be:

- a register: `v0` to `vF`, or `i`
- an instruction name, a directive name, or `long`
- a word that reads as a hex number, for example `face`, `add`, or `bead`

## Directives

| Directive | Effect |
|---|---|
| `db value, ...` | Write bytes. |
| `dw value, ...` | Write 16-bit words, high byte first. A label gives its address. |
| `ds count [fill]` | Write `count` bytes of the `fill` value. Without a fill, the bytes are 0. |
| `org address` | Move forward to an address. Zeros fill the gap. |
| `align size` | Move forward to a multiple of `size`, a power of two. Zeros fill the gap. |
| `name equ value` | Define a constant. |
| `platform name` | Select the platform: `chip8`, `hires`, `schip`, or `xochip`. |

The assembler makes two passes. `org`, `ds`, and `align` need their value in
the first pass, so their operand can use only the labels and constants on the
lines before them.

A label on an `org` or an `align` line gets the address after the move.

A line with only a number writes it as a word. So `1234` writes the bytes
0x12 and 0x34.

### The platform directive

The platform sets the instructions and the memory size. The `platform` line
must come before the first instruction, data, or `org` line. An instruction
that the platform does not have is an error. The message tells which
platforms have it.

Without a `platform` line, the current platform selection of the machine
applies. "Assemble & Load" changes the platform of the machine to the platform
of the source.

## Instructions

`vX` and `vY` are registers. `nnn` is a 12-bit address, `nn` is a byte, `n`
is a 4-bit value, and `nnnn` is a 16-bit address. An address can be a label.

### CHIP-8

| Syntax | Opcode | Effect |
|---|---|---|
| `sys nnn` | 0NNN | Call a machine code routine. The emulator ignores it. |
| `cls` | 00E0 | Clear the display. |
| `ret` | 00EE | Return from a subroutine. |
| `jmp nnn` | 1NNN | Jump to nnn. |
| `call nnn` | 2NNN | Call the subroutine at nnn. |
| `se vX, nn` | 3XNN | Skip the next instruction if vX == nn. |
| `sne vX, nn` | 4XNN | Skip the next instruction if vX != nn. |
| `se vX, vY` | 5XY0 | Skip the next instruction if vX == vY. |
| `mov vX, nn` | 6XNN | vX = nn. |
| `add vX, nn` | 7XNN | vX += nn. VF does not change. |
| `mov vX, vY` | 8XY0 | vX = vY. |
| `or vX, vY` | 8XY1 | vX \|= vY. |
| `and vX, vY` | 8XY2 | vX &= vY. |
| `xor vX, vY` | 8XY3 | vX ^= vY. |
| `add vX, vY` | 8XY4 | vX += vY. VF = 1 on a carry. |
| `sub vX, vY` | 8XY5 | vX -= vY. VF = 0 on a borrow. |
| `shr vX, vY` | 8XY6 | vX = vY >> 1. VF = the bit shifted out. |
| `subn vX, vY` | 8XY7 | vX = vY - vX. VF = 0 on a borrow. |
| `shl vX, vY` | 8XYE | vX = vY << 1. VF = the bit shifted out. |
| `sne vX, vY` | 9XY0 | Skip the next instruction if vX != vY. |
| `mov i, nnn` | ANNN | I = nnn. |
| `jmp v0, nnn` | BNNN | Jump to nnn + v0. |
| `rnd vX, nn` | CXNN | vX = a random byte & nn. |
| `drw vX, vY, n` | DXYN | Draw n rows of the sprite at I. VF = 1 on a collision. |
| `skp vX` | EX9E | Skip the next instruction if the key in vX is held. |
| `sknp vX` | EXA1 | Skip the next instruction if the key in vX is not held. |
| `gdly vX` | FX07 | vX = the delay timer. |
| `key vX` | FX0A | Wait for a key press, then vX = the key. |
| `sdly vX` | FX15 | The delay timer = vX. |
| `ssnd vX` | FX18 | The sound timer = vX. |
| `add i, vX` | FX1E | I += vX. |
| `font vX` | FX29 | I = the address of the small font sprite for the digit in vX. |
| `bcd vX` | FX33 | Write the 3 decimal digits of vX at I, I+1, and I+2. |
| `str vX` | FX55 | Write v0 to vX at I. |
| `ld vX` | FX65 | Read v0 to vX from I. |

`shr vX` and `shl vX` with one register mean `shr vX, vX` and `shl vX, vX`.

The quirks of the machine change some effects. The Syntax Reference panel
tells which.

### SUPER-CHIP

These instructions are also on XO-CHIP.

| Syntax | Opcode | Effect |
|---|---|---|
| `scd n` | 00CN | Scroll the display down n pixels. |
| `scr` | 00FB | Scroll the display right 4 pixels. |
| `scl` | 00FC | Scroll the display left 4 pixels. |
| `exit` | 00FD | Stop the program. |
| `low` | 00FE | Select lores mode (64x32) and clear the display. |
| `high` | 00FF | Select hires mode (128x64) and clear the display. |
| `drw vX, vY, 0` | DXY0 | Draw a 16x16 sprite (32 bytes). |
| `hfont vX` | FX30 | I = the address of the big font sprite for the digit in vX. |
| `sflags vX` | FX75 | Save v0 to vX in the flag registers. |
| `lflags vX` | FX85 | Load v0 to vX from the flag registers. |

### XO-CHIP

| Syntax | Opcode | Effect |
|---|---|---|
| `scu n` | 00DN | Scroll the display up n pixels. |
| `str vX, vY` | 5XY2 | Write vX to vY at I. I does not change. |
| `ld vX, vY` | 5XY3 | Read vX to vY from I. I does not change. |
| `mov i, long nnnn` | F000 NNNN | I = nnnn. The instruction has 4 bytes. |
| `plane n` | FN01 | Select the planes (0 to 3) that draw, clear, and scroll. |
| `audio` | F002 | Copy the 16-byte audio pattern at I to the sound buffer. |
| `pitch vX` | FX3A | Set the pitch of the audio pattern to vX. |

## An example

This program moves a ball to the right.

```
; Move a ball
platform chip8

SPEED   equ #1
TOP     equ #5

start:
    cls
    mov v0, #10         ; x
    mov v1, TOP         ; y
    mov i, ball
loop:
    drw v0, v1, BALL_ROWS   ; Draw
    mov v2, 1
    sdly v2
wait:
    gdly v2             ; Wait for the next frame
    se v2, 0
    jmp wait
    drw v0, v1, BALL_ROWS   ; Erase
    add v0, SPEED
    jmp loop

ball:
    db %.##.....
    db %.##.....
BALL_ROWS equ ball_end-ball
ball_end:
```
