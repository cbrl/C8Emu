# Debugging in C8Emu

This document describes the debugger of C8Emu.

## Source-level debugging

The Code Editor doubles as a debugger.

- Click the column left of the text to add or remove a breakpoint. A line
  without bytes puts the breakpoint on the next line with bytes.
- While the program is paused, an arrow and a highlight show the line of the PC.
- Right-click a line number for the address menu of the line (see below).
- F9 toggles the breakpoint of the cursor line. Ctrl+F10 runs to the cursor line.
- The line number of a line with a problem has the color of the problem. Hover
  it to see the messages.
- Go to Line (Ctrl+Shift+G, or a click on "Ln, Col" below the editor) moves the
  cursor to a line number. Ctrl+G is Find Next.

Breakpoints and the PC arrow follow their lines after edits to the source. A
breakpoint is removed if its line is deleted or writes no bytes after an edit.
The Log tells which breakpoints were removed.

## Run control

| Command | Key | Effect |
|---|---|---|
| Run / Pause | F5 | |
| Step Into | F11 | Run one instruction. |
| Step Over | F10 | Run a call or a key wait (FX0A) to its end. |
| Step Out | Shift+F11 | Run until the current subroutine returns. |
| Frame Advance | F6 | Run to the end of the frame. |
| Step Back | F7 | Go back one instruction. |
| Frame Back | Shift+F6 | Go back to the end of the previous frame. |
| Rewind | hold Backspace | Play the program backward. |
| Run to Cursor | Ctrl+F10 | Run until the PC reaches the cursor line. |
| Toggle Breakpoint | F9 | Add or remove the breakpoint of the cursor line. |
| Reset | Ctrl+R | Start the program again. |
| Turbo | hold Tab | Run faster. |

Step Over, Step Out, Run to Cursor, and Frame Advance run at the normal speed,
so the timers count as in real time. A breakpoint, a watchpoint, the break
condition, or a fault stops them before their goal.

## History

While Record History is on, the debugger records the state of the machine at
each frame end. The Length setting sets how many seconds the history keeps.

- **Step Back** (F7) goes back one instruction. The debugger restores a
  recorded state, then runs the instructions again until the previous
  instruction completes. Breakpoints do not stop this replay.
- **Frame Back** (Shift+F6) goes back to the end of the previous frame.
- **Rewind**: (hold Backspace) The program plays backward at the normal speed.
  Hold Tab too for a faster rewind.

A replay presses the keys at the same instructions as before, so it gives the
same states. When you go back, the history removes the states after that point.

These events clear the history. Then the history starts again at the current
state:

- Loading a program or save state, or a reset.
- Changing the platform, the quirks, or the instructions per frame.
- Editing the memory, the registers, the stack, the flag registers, or the
  random number seed.

The history does not go back in the trace or in the profiler. They keep the
instructions that ran, also after a step back.

## Save states

A save state holds the complete state of the machine and its configuration.

| Command | Key | Effect |
|---|---|---|
| Quick Save State | F2 | Save the state to the selected slot of the program. |
| Quick Load State | F4 | Load the state from the selected slot of the program. |
| State Slot | | Select a slot from 1 to 9. The menu tells which slots hold a state. |
| Save State As... | | Save the state to a file that you choose. |
| Load State... | | Load the state from a file that you choose. |

The slot files are in the `states` folder of the user data folder, for example
`%APPDATA%\cbrl\C8Emu\states\Brix.1.c8state`. The Log shows the user data
folder when C8Emu starts.

## Breakpoints

The Breakpoints panel lists all breakpoints. Each breakpoint has these
settings:

- **On**: a breakpoint that is off stays in the list, but it does not stop or log.
- **Condition**: an expression. The breakpoint stops only when the expression
  is true. The debugger checks the condition only when the PC reaches the
  address.
- **Ignore**: the number of hits that do not stop. A hit is a time that the PC
  reaches the address while the condition is true.
- **Log Message**: a breakpoint with a message is a logpoint. It writes the
  message to the Log and does not stop. `{expression}` in the message gives the
  value in hexadecimal, and `{expression:d}` gives it in decimal. `{{` and `}}`
  give a brace. Example: `x = {v0:d}, sprite at {i}`.

Logpoints write at most 100 messages in one frame.

A condition or a message with an error does not stop the program silently. The
breakpoint stops as a plain breakpoint and the panel shows the error.

The Code Editor and the Program panel show each breakpoint as a dot:

| Dot | Breakpoint |
|---|---|
| Red | A plain breakpoint |
| Orange | A breakpoint with a condition or an ignore count |
| Blue | A logpoint |
| Ring | A breakpoint that is off |

An address field takes a number, a label, or an expression (e.g., `loop+4`).

## Watchpoints

A watchpoint stops the program before an instruction reads or writes a range
of memory. Each watchpoint watches reads, writes, or both. C8Emu knows the
memory that each instruction uses:

| Instruction | Memory |
|---|---|
| `drw` (DXYN) | Reads the sprite at I. On XO-CHIP, the sprite of each selected plane. |
| `bcd` (FX33) | Writes 3 bytes at I. |
| `str vX` (FX55) | Writes v0 to vX at I. |
| `ld vX` (FX65) | Reads v0 to vX from I. |
| `str vX, vY` (5XY2) | Writes vX to vY at I (XO-CHIP). |
| `ld vX, vY` (5XY3) | Reads vX to vY from I (XO-CHIP). |
| `audio` (F002) | Reads 16 bytes at I (XO-CHIP). |

The stop message tells the address of the instruction and the watched address.

## The break condition

The Break When tab of the Breakpoints panel holds one expression. The program
stops before any instruction while the expression is true. Example: stop when
the stack is deep: `sp > 10`. The check runs before each instruction, so the
emulation is a little slower while the condition is set.

## Expressions

Conditions, log messages, the break condition, address fields, and the Watch
panel use the same expressions. The numbers are the same as in the assembler.

| Syntax | Meaning |
|---|---|
| `v0` to `vF`, `i`, `pc`, `sp` | The registers |
| `dt`, `st` | The delay timer and the sound timer |
| `frame`, `cycles` | The frames and the instructions since the reset |
| `[address]` | The byte at an address |
| `w[address]` | The big-endian word at an address |
| `2A`, `0x2A` | Hexadecimal numbers. A number without a prefix is hexadecimal. |
| `#42`, `#-1` | Decimal numbers |
| `%0010_1010` | Binary numbers |
| `label` | The value of a label or a constant of the loaded program |
| `\|\|  &&  ==  !=  <  <=  >  >=` | Logic and comparisons. True is 1, false is 0. |
| `\|  ^  &  <<  >>  +  -  *  /  %` | Bit operations and arithmetic |
| `!  ~  -` | Not, bitwise not, and negative |

The operators have the same precedence as C.

An expression never stops the emulation with an error. A division by zero
gives 0, and the arithmetic wraps at 64 bits.

After each load, all expressions compile again with the labels and the
constants of the new program. An expression that uses a name that the new
program does not define shows an error until a program defines the name again.

## Trace

The Trace panel records each executed instruction and the registers before
it. Turn on Record first. The trace keeps the newest 100,000 instructions.

- The filter shows only the rows that hold a text, for example an address, a
  label, or `drw`. The filter checks only the new rows in each frame, so a long
  trace does not slow the app.
- Follow scrolls to the newest row while the program runs.
- Copy puts the rows that pass the filter on the clipboard.
- Click an address to show its line in the Code Editor. Right-click it for the
  address menu.

## Profiler

The Profiler panel counts the executions of each address and the reads and
writes of each byte. Turn on Profile first. A load or a reset clears the counts.

- **Hot Spots**: the top addresses that run most often, with their share of
  all executions.
- **Heat Map**: all memory as cells. Red shows executions, green shows writes,
  and blue shows reads. Hover a cell to see its counts. The cells of the program
  are lighter, and a yellow frame marks the cell of the PC. Click a cell to show
  its code in the Code Editor, or its memory if no code ran in it. Right-click a
  cell for the address menu.
