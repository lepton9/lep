#include "../include/regalloc.h"

// FIX: this file is deprecated

const char* selReg(const int regI, const int size) {
  switch (size) {
    case 8: return regs[regI].r64;
    case 4: return regs[regI].r32;
    case 2: return regs[regI].r16;
    case 1: return regs[regI].r8;
    default: return regs[regI].r64;
  }
}

void useReg(int regI) {
  regs[regI].in_use = 1;
}

void freeReg(int regI) {
  regs[regI].in_use = 0;
}

