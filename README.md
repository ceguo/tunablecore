## Instructions

## Instruction Format

32-bit instructions. 4 bytes.
- Byte 0: opcode
- Byte 1: higher 4 bits for subfunction; lower 4 bits for Rz
- Byte 2:
    - (R-type) higher 4 bits for Rx; lower 4 bits for Ry
    - (I-type) all 8 bits for the upper 8 bits of the immediate value
- Byte 3:
    - (R-type) not used
    - (I-type) all 8 bits for the lower 8 bits of the immediate value

## Opcode
General format: `ooooiitt`
- Lower 2 bits `tt` for operations:
    - tt = 00: R-type instruction
        - ooooii = 000000: (nop)
        - ooooii = 000001: (add) Rz <- Rx + Ry
        - ooooii = 000010: (sub) Rz <- Rx - Ry
        - ooooii = 000011: (mul) Rz <- Rx * Ry
        - ooooii = 000100: (div) Rz <- Rx / Ry
        - ooooii = 001000: (load) Rz <- M[Rx + Ry]
        - ooooii = 001001: (store) M[Rx + Ry] <- Rz
    - tt = 01: I-type instruction
        - ii = 00: Imm as unsigned memory address
            - oooo = 0010: (jz) go to (PC+Imm) if Rz = 0
            - oooo = 0011: (jp) go to (PC+Imm) if Rz > 0
        - ii = 01: Imm as signed integer
            - oooo = 0000: (li) load Rz with (extended) Imm
