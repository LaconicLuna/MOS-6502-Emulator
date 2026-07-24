# MOS-6502-Emulator  
A **6502 emulator** written in **C**  
Should be a decent enough base to extend for NES, Atari, or C64 emulation  
Passes Klaus Dormann's 6502 functional test 

---

## Features  
- Full 6502 official instruction set
- All addressing modes
- Indirect JMP page-wrap bug
- Cycle Accurate (Pretty Sure 0_0)
- Simple LUT-based opcode decoder

---

## Running
Compile with your choice of C compiler:
```bash
gcc -O2 -o 6502 6502.c
```
Place '6502_functional_test.bin' in the same directory then:  
```bash
./6502
```
If all goes right you should see(Xs depend on your hardware):  
```
SUCCESS! Passed all 6502 functional tests!
Total CPU Cycles: 96241364
Executed 96241364 cycles in X.XXXX seconds!
Emulated Speed: XXX.XX MHz
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
- Illegal/Unofficial Opcodes (Maybe? probably not)
