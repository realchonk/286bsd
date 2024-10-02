# ABI
Caller-saved registers: AX, BX, CX, DX, ES
Callee-saved registers: SI, DI, BP, CS, DS, SS
Parameters: word-aligned, on the stack, backwards
Return: AX [0..16], DX [16..32], BX [32..48], CX [48..64]
