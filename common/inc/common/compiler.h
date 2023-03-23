#ifndef COMMON_COMPILER_H
#define COMMON_COMPILER_H

#if defined(__INTELLISENSE__)
#define REGARG(arg, reg) arg
#else
#define REGARG(arg, reg) arg asm(reg)
#endif

#endif // COMMON_COMPILER_H
