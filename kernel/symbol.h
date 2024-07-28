//
// Created by Jannik on 28.07.2024.
//

#ifndef NIGHTOS_SYMBOL_H
#define NIGHTOS_SYMBOL_H

struct kernel_symbol {
    unsigned long value;
    const char *name;
};

int register_symbol(const char *name, unsigned long value);
struct kernel_symbol *find_symbol(const char *name);

#define EXPORT_SYMBOL(sym)                                \
    static const char __ksymtab_##sym_name[]              \
    __attribute__((section("__ksymtab_strings")))         \
    = #sym;                                               \
    static const struct kernel_symbol __ksymtab_##sym     \
    __attribute__((section("__ksymtab"))) = {             \
        (unsigned long)&sym, __ksymtab_##sym_name         \
    }

#endif //NIGHTOS_SYMBOL_H
