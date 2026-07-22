# MOS-6502-Emulator  
A 6502 emulator written in C  
Should be a decent enough base to extend for NES, Atari, or C64 emulation  
Passes Klaus Dormann's 6502 functional test 

---

## Features  
- Full 6502 official instruction set
- All addressing modes
- Indirect JMP page-wrap bug
- Simple LUT-based opcode decoder

---

## Running
Compile with your choice of C compiler:
```
gcc -O2 -o emulator main.c
```
Place '6502_functional_test.bin' in the same directory then:  
```
./emulator
```
If all goes right you should see:  
```
SUCCESS! Passed all 6502 functional tests!
```

---

## Rescources:  
Main references I used:

- 6502 Instruction Set Guide  
  https://6502.org/users/obelisk/6502/instructions.html

- Klaus Dormann’s 6502 Functional Tests  
  https://github.com/Klaus2m5/6502_65C02_functional_tests/tree/master

---

## Future Improvements:
- Cycle accurate timing
- Illegal/Unofficial Opcodes (Maybe?)
