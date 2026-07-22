#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define FLAG_C (1 << 0) // Carry
#define FLAG_Z (1 << 1) // Zero
#define FLAG_I (1 << 2) // Interrupt Disable
#define FLAG_D (1 << 3) // Decimal
#define FLAG_B (1 << 4) // Break
#define FLAG_U (1 << 5) // Unused/Always 1
#define FLAG_V (1 << 6) // Overflow
#define FLAG_N (1 << 7) // Negative

typedef struct {
  // 6502 Registers
  uint8_t a;      // Accumulator
  uint8_t x;      // X Index
  uint8_t y;      // Y Index
  uint8_t sp;     // Stack Pointer
  uint16_t pc;    // Program Counter
  uint8_t status; // Status Flags (C, Z, I, D, B, V, N)

  // RAM 64kb just cause yknow
  uint8_t ram[0x10000];
} CPU;

// CPU HELPER FUNCTIONS
static inline uint8_t cpu_read(CPU *cpu, uint16_t addr) {
  return cpu->ram[addr];
}

static inline void cpu_write(CPU *cpu, uint16_t addr, uint8_t val) {
  cpu->ram[addr] = val;
}

static void push(CPU *cpu, uint8_t val) {
  cpu_write(cpu, 0x0100 + cpu->sp, val);
  cpu->sp--;
}

static uint8_t pull(CPU *cpu) { // POP from stack
  cpu->sp++;
  return cpu_read(cpu, 0x0100 + cpu->sp);
}

static inline void update_zn(CPU *cpu, uint8_t val) {
  if (val == 0) {
    cpu->status |= FLAG_Z;
  } else {
    cpu->status &= ~FLAG_Z;
  }
  if (val & 0x80) {
    cpu->status |= FLAG_N;
  } else {
    cpu->status &= ~FLAG_N;
  }
}

void cpu_reset(CPU *cpu) {
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->sp = 0xFF;
  cpu->status = 0x24; // FLAG_U + FLAG_I
  uint16_t lo = cpu_read(cpu, 0xFFFC);
  uint16_t hi = cpu_read(cpu, 0xFFFD);
  uint16_t reset_vec = (hi << 8) | lo;
  cpu->pc = reset_vec;
}

// ================================================
//           ADDRESSING MODE HELPERS
// ================================================
uint16_t addr_imp(CPU *cpu) {
  return 0;
}

uint16_t addr_imm(CPU *cpu) {
  return cpu->pc++;
}

uint16_t addr_zp(CPU *cpu) {
  return cpu_read(cpu, cpu->pc++);
}

uint16_t addr_zpx(CPU *cpu) {
  return (cpu_read(cpu, cpu->pc++) + cpu->x) & 0xFF;
}

uint16_t addr_abs(CPU *cpu) {
  uint16_t lo = cpu_read(cpu, cpu->pc++);
  uint16_t hi = cpu_read(cpu, cpu->pc++);
  return (hi << 8) | lo;
}

uint16_t addr_absx(CPU *cpu) {
  uint16_t lo = cpu_read(cpu, cpu->pc++);
  uint16_t hi = cpu_read(cpu, cpu->pc++);
  return ((hi << 8) | lo) + cpu->x;
}

uint16_t addr_absy(CPU *cpu) {
  uint16_t lo = cpu_read(cpu, cpu->pc++);
  uint16_t hi = cpu_read(cpu, cpu->pc++);
  return ((hi << 8) | lo) + cpu->y;
}

uint16_t addr_izx(CPU *cpu) {
  uint8_t ptr = (cpu_read(cpu, cpu->pc++) + cpu->x) & 0xFF;
  uint16_t lo = cpu_read(cpu, ptr);
  uint16_t hi = cpu_read(cpu, (ptr + 1) & 0xFF);
  return (hi << 8) | lo;
}

uint16_t addr_izy(CPU *cpu) {
  uint8_t ptr = cpu_read(cpu, cpu->pc++);
  uint16_t lo = cpu_read(cpu, ptr);
  uint16_t hi = cpu_read(cpu, (ptr + 1) & 0xFF);
  return ((hi << 8) | lo) + cpu->y;
}

uint16_t addr_zpy(CPU *cpu) {
  return (cpu_read(cpu, cpu->pc++) + cpu->y) & 0xFF;
}

uint16_t addr_ind(CPU *cpu) {
  uint16_t ptr = addr_abs(cpu);
  uint8_t lo = cpu_read(cpu, ptr);
  uint8_t hi;
  if ((ptr & 0x00FF) == 0x00FF) {
    // wraps around within the same page to emulate error in original chip
    hi = cpu_read(cpu, ptr & 0xFF00);
  } else {
    hi = cpu_read(cpu, ptr + 1);
  }
  return (hi << 8) | lo;
}

uint16_t addr_rel(CPU *cpu) {
  int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
  return cpu->pc + offset;
}


// ================================================
//                 OPCODE LOGIC
// ================================================

// ================ LOAD/STORE ====================
void op_lda(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  cpu->a = temp;
  update_zn(cpu, temp);
}

void op_ldx(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  cpu->x = temp;
  update_zn(cpu, temp);
}

void op_ldy(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  cpu->y = temp;
  update_zn(cpu, temp);
}

void op_sta(CPU *cpu, uint16_t addr) {
  cpu_write(cpu, addr, cpu->a);
}

void op_stx(CPU *cpu, uint16_t addr) {
  cpu_write(cpu, addr, cpu->x);
}

void op_sty(CPU *cpu, uint16_t addr) {
  cpu_write(cpu, addr, cpu->y);
}


// ============= REGISTER TRANSFERS ===============
void op_tax(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->x = cpu->a;
  update_zn(cpu, cpu->x);
}

void op_tay(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->y = cpu->a;
  update_zn(cpu, cpu->y);
}

void op_txa(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->a = cpu->x;
  update_zn(cpu, cpu->a);
}

void op_tya(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->a = cpu->y;
  update_zn(cpu, cpu->a);
}


// ===================== STACK ====================
void op_tsx(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->x = cpu->sp;
  update_zn(cpu, cpu->x);
}

void op_txs(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->sp = cpu->x;
}

void op_pha(CPU *cpu, uint16_t unused) {
  (void)unused;
  push(cpu, cpu->a);
}

void op_php(CPU *cpu, uint16_t unused) {
  (void)unused;
  push(cpu, cpu->status | 0x30);
}

void op_pla(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->a = pull(cpu);
  update_zn(cpu, cpu->a);
}

void op_plp(CPU *cpu, uint16_t unused) {
  (void)unused;
  uint8_t p = pull(cpu);
  cpu->status = (p & ~FLAG_B) | FLAG_U;
}


// =================== LOGICAL ====================
void op_and(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  cpu->a &= temp;
  update_zn(cpu, cpu->a);
}

void op_eor(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  cpu->a ^= temp;
  update_zn(cpu, cpu->a);
}

void op_ora(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  cpu->a |= temp;
  update_zn(cpu, cpu->a);
}

void op_bit(CPU *cpu, uint16_t addr) {
  uint8_t temp = cpu_read(cpu, addr);
  if ((cpu->a & temp) == 0) {
    cpu->status |= FLAG_Z;
  } else {
    cpu->status &= ~FLAG_Z;
  }
  cpu->status = (cpu->status & 0x3F) | (temp & 0xC0);
}


// ================== ARITHMETIC ==================
void op_adc(CPU *cpu, uint16_t addr) {
  uint8_t data = cpu_read(cpu, addr);
  uint8_t carry = (cpu->status & FLAG_C) ? 1 : 0;
  if (cpu->status & FLAG_D) {
    uint16_t binary_sum = cpu->a + data + carry;
    uint16_t lo = (cpu->a & 0x0F) + (data & 0x0F) + carry;
    if (lo > 0x09) {
      lo += 0x06;
    }
    uint16_t hi = (cpu->a >> 4) + (data >> 4) + (lo > 0x0F ? 1 : 0);
    update_zn(cpu, (uint8_t)binary_sum);
    if (~(cpu->a ^ data) & (cpu->a ^ binary_sum) & 0x80) {
      cpu->status |= FLAG_V;
    } else {
      cpu->status &= ~FLAG_V;
    }
    if (hi > 0x09) {
      hi += 0x06;
    }
    if (hi > 0x0F) {
      cpu->status |= FLAG_C;
    } else {
      cpu->status &= ~FLAG_C;
    }
    cpu->a = ((hi & 0x0F) << 4) | (lo & 0x0F);
  } else {
    uint16_t sum = cpu->a + data + carry;
    if (sum > 0xFF) {
      cpu->status |= FLAG_C;
    } else {
      cpu->status &= ~FLAG_C;
    }
    if (~(cpu->a ^ data) & (cpu->a ^ sum) & 0x80) {
      cpu->status |= FLAG_V;
    } else {
      cpu->status &= ~FLAG_V;
    }
    cpu->a = (uint8_t)sum;
    update_zn(cpu, cpu->a);
  }
}

void op_sbc(CPU *cpu, uint16_t addr) {
  uint8_t data = cpu_read(cpu, addr);
  uint8_t carry = (cpu->status & FLAG_C) ? 1 : 0;
  if (cpu->status & FLAG_D) {
    uint16_t binary_diff = cpu->a - data - (1 - carry);
    int16_t lo = (cpu->a & 0x0F) - (data & 0x0F) - (1 - carry);
    int16_t hi = (cpu->a >> 4) - (data >> 4);
    if (lo < 0) {
      lo -= 0x06;
      hi--;
    }
    if (hi < 0) {
      hi -= 0x06;
    }
    if (binary_diff < 0x0100) {
      cpu->status |= FLAG_C;
    } else {
      cpu->status &= ~FLAG_C;
    }
    if ((cpu->a ^ data) & (cpu->a ^ binary_diff) & 0x80) {
      cpu->status |= FLAG_V;
    } else {
      cpu->status &= ~FLAG_V;
    }
    update_zn(cpu, (uint8_t)binary_diff);
    cpu->a = ((hi & 0x0F) << 4) | (lo & 0x0F);
  } else {
    uint8_t inv_data = ~data;
    uint16_t sum = cpu->a + inv_data + carry;
    if (sum > 0xFF) {
      cpu->status |= FLAG_C;
    } else {
      cpu->status &= ~FLAG_C;
    }
    if (~(cpu->a ^ inv_data) & (cpu->a ^ sum) & 0x80) {
      cpu->status |= FLAG_V;
    } else {
      cpu->status &= ~FLAG_V;
    }
    cpu->a = (uint8_t)sum;
    update_zn(cpu, cpu->a);
  }
}

static void do_compare(CPU *cpu, uint8_t reg, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr);
  uint16_t result = reg - val;
  if (reg >= val) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  update_zn(cpu, (uint8_t)result);
}

void op_cmp(CPU *cpu, uint16_t addr) {
  do_compare(cpu, cpu->a, addr);
}
void op_cpx(CPU *cpu, uint16_t addr) {
  do_compare(cpu, cpu->x, addr);
}
void op_cpy(CPU *cpu, uint16_t addr) {
  do_compare(cpu, cpu->y, addr);
}


// ================= INC AND DEC ==================
void op_inx(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->x++;
  update_zn(cpu, cpu->x);
}

void op_dex(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->x--;
  update_zn(cpu, cpu->x);
}

void op_iny(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->y++;
  update_zn(cpu, cpu->y);
}

void op_dey(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->y--;
  update_zn(cpu, cpu->y);
}

void op_inc(CPU *cpu, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr) + 1;
  cpu_write(cpu, addr, val);
  update_zn(cpu, val);
}

void op_dec(CPU *cpu, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr) - 1;
  cpu_write(cpu, addr, val);
  update_zn(cpu, val);
}


// ===================== SHIFTS ===================
void op_asl(CPU *cpu, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr);
  if (val & 0x80) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  val <<= 1;
  cpu_write(cpu, addr, val);
  update_zn(cpu, val);
}

void op_asl_a(CPU *cpu, uint16_t unused) {
  (void)unused;
  if (cpu->a & 0x80) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  cpu->a <<= 1;
  update_zn(cpu, cpu->a);
}

void op_lsr(CPU *cpu, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr);
  if (val & 0x01) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  val >>= 1;
  cpu_write(cpu, addr, val);
  update_zn(cpu, val);
}

void op_lsr_a(CPU *cpu, uint16_t unused) {
  (void)unused;
  if (cpu->a & 0x01) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  cpu->a >>= 1;
  update_zn(cpu, cpu->a);
}

void op_rol(CPU *cpu, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr);
  uint8_t old_c = (cpu->status & FLAG_C) ? 1 : 0;
  if (val & 0x80) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  val = (val << 1) | old_c;
  cpu_write(cpu, addr, val);
  update_zn(cpu, val);
}

void op_rol_a(CPU *cpu, uint16_t unused) {
  (void)unused;
  uint8_t old_c = (cpu->status & FLAG_C) ? 1 : 0;
  if (cpu->a & 0x80) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  cpu->a = (cpu->a << 1) | old_c;
  update_zn(cpu, cpu->a);
}

void op_ror(CPU *cpu, uint16_t addr) {
  uint8_t val = cpu_read(cpu, addr);
  uint8_t old_c = (cpu->status & FLAG_C) ? 0x80 : 0;
  if (val & 0x01) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  val = (val >> 1) | old_c;
  cpu_write(cpu, addr, val);
  update_zn(cpu, val);
}

void op_ror_a(CPU *cpu, uint16_t unused) {
  (void)unused;
  uint8_t old_c = (cpu->status & FLAG_C) ? 0x80 : 0;
  if (cpu->a & 0x01) {
    cpu->status |= FLAG_C;
  } else {
    cpu->status &= ~FLAG_C;
  }
  cpu->a = (cpu->a >> 1) | old_c;
  update_zn(cpu, cpu->a);
}


// ============== JUMP AND CALLS ==================
void op_jmp(CPU *cpu, uint16_t addr) {
  cpu->pc = addr;
}

void op_jsr(CPU *cpu, uint16_t addr) {
  uint16_t return_pc = cpu->pc - 1;
  push(cpu, (return_pc >> 8) & 0xFF);
  push(cpu, return_pc & 0xFF);
  cpu->pc = addr;
}

void op_rts(CPU *cpu, uint16_t unused) {
  (void)unused;
  uint16_t lo = pull(cpu);
  uint16_t hi = pull(cpu);
  cpu->pc = ((hi << 8) | lo) + 1;
}


// =================== BRANCH =====================
static void do_branch(CPU *cpu, uint16_t target_addr, bool condition) {
  if (condition) {
    cpu->pc = target_addr;
  }
}

void op_bcc(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_C) == 0);
}
void op_bcs(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_C) != 0);
}
void op_beq(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_Z) != 0);
}
void op_bne(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_Z) == 0);
}
void op_bmi(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_N) != 0);
}
void op_bpl(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_N) == 0);
}
void op_bvc(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_V) == 0);
}
void op_bvs(CPU *cpu, uint16_t addr) {
  do_branch(cpu, addr, (cpu->status & FLAG_V) != 0);
}


// =================== FLAGS ======================
void op_clc(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status &= ~FLAG_C;
}
void op_cld(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status &= ~FLAG_D;
}
void op_cli(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status &= ~FLAG_I;
}
void op_clv(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status &= ~FLAG_V;
}
void op_sec(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status |= FLAG_C;
}
void op_sed(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status |= FLAG_D;
}
void op_sei(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->status |= FLAG_I;
}


// =================== SYSTEM =====================
void op_nop(CPU *cpu, uint16_t unused) {
  (void)unused;
}

void op_brk(CPU *cpu, uint16_t unused) {
  (void)unused;
  cpu->pc++;
  push(cpu, (cpu->pc >> 8) & 0xFF);
  push(cpu, cpu->pc & 0xFF);
  push(cpu, cpu->status | 0x30);
  cpu->status |= FLAG_I;
  uint16_t lo = cpu_read(cpu, 0xFFFE);
  uint16_t hi = cpu_read(cpu, 0xFFFF);
  cpu->pc = (hi << 8) | lo;
}

void op_rti(CPU *cpu, uint16_t unused) {
  (void)unused;
  uint8_t p = pull(cpu);
  cpu->status = (p & ~FLAG_B) | FLAG_U;
  uint16_t lo = pull(cpu);
  uint16_t hi = pull(cpu);
  cpu->pc = (hi << 8) | lo;
}

typedef struct {
  uint16_t (*get_addr)(CPU *);   // Pointer to addressing function
  void (*exec)(CPU *, uint16_t); // Pointer to operation function
} Instruction;

const Instruction lut[256] = {
    // LOADS STORES
    [0xA9] = {addr_imm, op_lda},
    [0xA5] = {addr_zp, op_lda},
    [0xB5] = {addr_zpx, op_lda},
    [0xAD] = {addr_abs, op_lda},
    [0xBD] = {addr_absx, op_lda},
    [0xB9] = {addr_absy, op_lda},
    [0xA1] = {addr_izx, op_lda},
    [0xB1] = {addr_izy, op_lda},
    [0xA2] = {addr_imm, op_ldx},
    [0xA6] = {addr_zp, op_ldx},
    [0xB6] = {addr_zpy, op_ldx},
    [0xAE] = {addr_abs, op_ldx},
    [0xBE] = {addr_absy, op_ldx},
    [0xA0] = {addr_imm, op_ldy},
    [0xA4] = {addr_zp, op_ldy},
    [0xB4] = {addr_zpx, op_ldy},
    [0xAC] = {addr_abs, op_ldy},
    [0xBC] = {addr_absx, op_ldy},
    [0x85] = {addr_zp, op_sta},
    [0x95] = {addr_zpx, op_sta},
    [0x8D] = {addr_abs, op_sta},
    [0x9D] = {addr_absx, op_sta},
    [0x99] = {addr_absy, op_sta},
    [0x81] = {addr_izx, op_sta},
    [0x91] = {addr_izy, op_sta},
    [0x86] = {addr_zp, op_stx},
    [0x96] = {addr_zpy, op_stx},
    [0x8E] = {addr_abs, op_stx},
    [0x84] = {addr_zp, op_sty},
    [0x94] = {addr_zpx, op_sty},
    [0x8C] = {addr_abs, op_sty},

    // STACK OPERATIONS
    [0xBA] = {addr_imp, op_tsx},
    [0x9A] = {addr_imp, op_txs},
    [0x48] = {addr_imp, op_pha},
    [0x08] = {addr_imp, op_php},
    [0x68] = {addr_imp, op_pla},
    [0x28] = {addr_imp, op_plp},

    // REGISTER TRANSFERS
    [0xAA] = {addr_imp, op_tax},
    [0x8A] = {addr_imp, op_txa},
    [0xA8] = {addr_imp, op_tay},
    [0x98] = {addr_imp, op_tya},

    // LOGICAL
    [0x29] = {addr_imm, op_and},
    [0x25] = {addr_zp, op_and},
    [0x35] = {addr_zpx, op_and},
    [0x2D] = {addr_abs, op_and},
    [0x3D] = {addr_absx, op_and},
    [0x39] = {addr_absy, op_and},
    [0x21] = {addr_izx, op_and},
    [0x31] = {addr_izy, op_and},
    [0x49] = {addr_imm, op_eor},
    [0x45] = {addr_zp, op_eor},
    [0x55] = {addr_zpx, op_eor},
    [0x4D] = {addr_abs, op_eor},
    [0x5D] = {addr_absx, op_eor},
    [0x59] = {addr_absy, op_eor},
    [0x41] = {addr_izx, op_eor},
    [0x51] = {addr_izy, op_eor},
    [0x09] = {addr_imm, op_ora},
    [0x05] = {addr_zp, op_ora},
    [0x15] = {addr_zpx, op_ora},
    [0x0D] = {addr_abs, op_ora},
    [0x1D] = {addr_absx, op_ora},
    [0x19] = {addr_absy, op_ora},
    [0x01] = {addr_izx, op_ora},
    [0x11] = {addr_izy, op_ora},
    [0x24] = {addr_zp, op_bit},
    [0x2C] = {addr_abs, op_bit},

    // ARITHMETIC
    [0x69] = {addr_imm, op_adc},
    [0x65] = {addr_zp, op_adc},
    [0x75] = {addr_zpx, op_adc},
    [0x6D] = {addr_abs, op_adc},
    [0x7D] = {addr_absx, op_adc},
    [0x79] = {addr_absy, op_adc},
    [0x61] = {addr_izx, op_adc},
    [0x71] = {addr_izy, op_adc},
    [0xE9] = {addr_imm, op_sbc},
    [0xE5] = {addr_zp, op_sbc},
    [0xF5] = {addr_zpx, op_sbc},
    [0xED] = {addr_abs, op_sbc},
    [0xFD] = {addr_absx, op_sbc},
    [0xF9] = {addr_absy, op_sbc},
    [0xE1] = {addr_izx, op_sbc},
    [0xF1] = {addr_izy, op_sbc},

    // COMPARISONS
    [0xC9] = {addr_imm, op_cmp},
    [0xC5] = {addr_zp, op_cmp},
    [0xD5] = {addr_zpx, op_cmp},
    [0xCD] = {addr_abs, op_cmp},
    [0xDD] = {addr_absx, op_cmp},
    [0xD9] = {addr_absy, op_cmp},
    [0xC1] = {addr_izx, op_cmp},
    [0xD1] = {addr_izy, op_cmp},
    [0xE0] = {addr_imm, op_cpx},
    [0xE4] = {addr_zp, op_cpx},
    [0xEC] = {addr_abs, op_cpx},
    [0xC0] = {addr_imm, op_cpy},
    [0xC4] = {addr_zp, op_cpy},
    [0xCC] = {addr_abs, op_cpy},

    // INC and DEC
    [0xE8] = {addr_imp, op_inx},
    [0xCA] = {addr_imp, op_dex},
    [0xC8] = {addr_imp, op_iny},
    [0x88] = {addr_imp, op_dey},
    [0xE6] = {addr_zp, op_inc},
    [0xF6] = {addr_zpx, op_inc},
    [0xEE] = {addr_abs, op_inc},
    [0xFE] = {addr_absx, op_inc},
    [0xC6] = {addr_zp, op_dec},
    [0xD6] = {addr_zpx, op_dec},
    [0xCE] = {addr_abs, op_dec},
    [0xDE] = {addr_absx, op_dec},

    // SHIFTS
    [0x0A] = {addr_imp, op_asl_a},
    [0x06] = {addr_zp, op_asl},
    [0x16] = {addr_zpx, op_asl},
    [0x0E] = {addr_abs, op_asl},
    [0x1E] = {addr_absx, op_asl},
    [0x4A] = {addr_imp, op_lsr_a},
    [0x46] = {addr_zp, op_lsr},
    [0x56] = {addr_zpx, op_lsr},
    [0x4E] = {addr_abs, op_lsr},
    [0x5E] = {addr_absx, op_lsr},
    [0x2A] = {addr_imp, op_rol_a},
    [0x26] = {addr_zp, op_rol},
    [0x36] = {addr_zpx, op_rol},
    [0x2E] = {addr_abs, op_rol},
    [0x3E] = {addr_absx, op_rol},
    [0x6A] = {addr_imp, op_ror_a},
    [0x66] = {addr_zp, op_ror},
    [0x76] = {addr_zpx, op_ror},
    [0x6E] = {addr_abs, op_ror},
    [0x7E] = {addr_absx, op_ror},

    // JUMPS
    [0x4C] = {addr_abs, op_jmp},
    [0x6C] = {addr_ind, op_jmp},
    [0x20] = {addr_abs, op_jsr},
    [0x60] = {addr_imp, op_rts},

    // BRANCHES
    [0x90] = {addr_rel, op_bcc},
    [0xB0] = {addr_rel, op_bcs},
    [0xF0] = {addr_rel, op_beq},
    [0xD0] = {addr_rel, op_bne},
    [0x30] = {addr_rel, op_bmi},
    [0x10] = {addr_rel, op_bpl},
    [0x50] = {addr_rel, op_bvc},
    [0x70] = {addr_rel, op_bvs},

    // FLAGS
    [0x18] = {addr_imp, op_clc},
    [0x38] = {addr_imp, op_sec},
    [0x58] = {addr_imp, op_cli},
    [0x78] = {addr_imp, op_sei},
    [0xB8] = {addr_imp, op_clv},
    [0xD8] = {addr_imp, op_cld},
    [0xF8] = {addr_imp, op_sed},

    // SYSTEM
    [0xEA] = {addr_imp, op_nop},
    [0x00] = {addr_imp, op_brk},
    [0x40] = {addr_imp, op_rti},
};

void cpu_step(CPU *cpu) {
  uint8_t opcode = cpu_read(cpu, cpu->pc++);
  Instruction instr = lut[opcode];
  if (instr.exec != NULL && instr.get_addr != NULL) {
    uint16_t addr = instr.get_addr(cpu);
    instr.exec(cpu, addr);
  } else {
    printf("Unknown opcode 0x%02X at PC 0x%04X\n", opcode, cpu->pc - 1);
  }
}

int main(void) {
  CPU cpu = {0};
  FILE *f = fopen("6502_functional_test.bin", "rb");
  if (!f) {
    printf("Failed to open test binary!\n");
    return 1;
  }
  fread(cpu.ram, 1, 0x10000, f);
  fclose(f);

  // Set start address from test
  cpu.pc = 0x0400;
  uint16_t last_pc = 0;
  while (1) {
    last_pc = cpu.pc;
    cpu_step(&cpu);
    // Infinite loop detection
    if (cpu.pc == last_pc) {
      if (cpu.pc == 0x3469) {
        printf("SUCCESS! Passed all 6502 functional tests!\n");
      } else {
        printf("FAILED at PC: 0x%04X\n", cpu.pc);
      }
      break;
    }
  }
  return 0;
}
