# 8-bit MIPS nRisc

8-bit **single-cycle MIPS nRisc** processor, fully designed in Verilog HDL. It implements a simplified datapath (data, instructions, and memory in 8 bits) and comes with an embedded program that runs the **Caesar Cipher** on a message in memory.

**Developers:** Kayky Moreira Praxedes, Carlos Ernesto Cardoso dos Reis

> Complete documentation (block diagram, control signal tables, cycle-by-cycle simulation traces, and line-by-line commented Assembly code) is in the project PDF — this README is just a general overview.

---

## Embedded Program: Caesar Cipher

The algorithm embedded in the instruction memory:

1. Reads a character from the data memory.
2. Adds the shift (key, between -15 and 15).
3. Adjusts the value if it goes beyond the alphabetic range.
4. Stores the new character back into memory.

Message limit: **128 characters**, defined by the address space (7 bits).

---

## Datapath

1. PC accesses the instruction memory.
2. Instruction is decoded by the control unit.
3. Registers are read.
4. ALU performs the operation.
5. Result is written to the register file or memory.

---

## Architecture

- **Control Unit** — Generates the control signals (`ALUSrc`, `MemToReg`, `RegWrite`, `MemRead`, `MemWrite`, `Branch`, `Jump`, `NextOp`, `End`, `ALUOp`) from the OpCode and `funct` field. Complete signal table per instruction in the PDF.
- **PC (Program Counter)** — Stores the address of the next instruction.
- **Memory** — Split into data memory and instruction memory, both with 128 8-bit locations (7-bit addresses).
- **Register File** — 4 general-purpose registers (`$c0`–`$c3`), plus the special register `$re`, which holds the result of comparisons (`beq`, `slt`), defaulting to `-4` to prevent unintended branches.
- **ALU** — Performs arithmetic, logic, and comparison operations. Built hierarchically from 1-bit, 8-bit, and 16-bit adders (without using high-level language operators), explicitly showing the carry flow.
- **Signal Extender** — Converts 3-bit immediates to 8 bits.

---

## Instructions

8-bit instructions in three formats:

| Format | Fields | Usage |
|---|---|---|
| **2R** | OpCode(3) + Reg1(2) + Reg2(2) + funct(1) | `add`, `sub`, `mult`, `ld`, `st`, `beq`, `slt` |
| **1R** | OpCode(3) + Reg(2) + Immediate(3) | `addi`, `jr`, `result` |
| **0R** | OpCode(3) + don't care(4) + funct(1) | `nop`, `hlt` |

The immediate is signed and ranges from -3 to 3. *Don't care* bits default to `0`. Complete assembly and binary for each instruction, with examples, are in the PDF (Section 4).

---

## Tests

The processor was validated through individual testbenches (control unit, PC, memory, register file, ALU) and end-to-end with the embedded program, encrypting multiple messages (including cases with spaces and alphabet wraparound). Complete results in the PDF, Appendices B and C.
