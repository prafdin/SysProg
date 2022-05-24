#pragma once
#include <cstdint>
#include <sys/personality.h>
#include <unistd.h>
#include <vector>

#include "registers.h"
#include "libelfin/elf/data.hh"

enum class symbol_type {
    notype,            // No type (e.g., absolute symbol)
    object,            // Data object
    func,              // Function entry point
    section,           // Symbol is associated with a section
    file,              // Source file associated with the
};

std::string to_string (symbol_type st);

symbol_type to_symbol_type(elf::stt sym);

struct symbol {
    symbol_type type;
    std::string name;
    std::uintptr_t addr;
};

uint64_t get_register_value(pid_t pid, reg r);

void set_register_value(pid_t pid, reg r, uint64_t value);

uint64_t get_register_value_from_dwarf_register(pid_t pid, unsigned regnum);

std::string get_register_name(reg r);

reg get_register_from_name(const std::string &name);

std::vector<std::string> split(const std::string &s, char delimiter);

bool is_prefix(const std::string& s, const std::string& of);
