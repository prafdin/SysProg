#pragma once

#include <utility>
#include <string>
#include <linux/types.h>
#include <unordered_map>

#include <fcntl.h>

#include "breakpoint.h"
#include "utility.h"
#include "libelfin/dwarf/dwarf++.hh"
#include "libelfin/elf/elf++.hh"


class debugger {
public:
    debugger(std::string prog_name, pid_t pid)
            : m_prog_name{std::move(prog_name)}, m_pid{pid} {
        auto fd = open(m_prog_name.c_str(), O_RDONLY);

        m_elf = elf::elf{elf::create_mmap_loader(fd)};
        m_dwarf = dwarf::dwarf{dwarf::elf::create_loader(m_elf)};
    }

    void run();

    void set_breakpoint_at_address(std::intptr_t addr, std::string call = "break");

    void set_breakpoint_at_function(const std::string &name, std::string call = "break");

    void set_breakpoint_at_source_line(const std::string &file, unsigned line);

    void dump_registers();

    void print_source(const std::string &file_name, unsigned line, unsigned n_lines_context = 2, std::string = "step");

    void show();

    auto lookup_symbol(const std::string &name) -> std::vector<symbol>;

    void single_step_instruction();

    void single_step_instruction_with_breakpoint_check();

    void step_in();

    void step_over();

    void step_out();

    void remove_breakpoint(std::intptr_t addr);

private:
    bool end_of_program = false;

    void handle_command(const std::string &line);

    void continue_execution(std::string call = "break");

    uint64_t get_pc();

    uint64_t get_offset_pc();

    void set_pc(uint64_t pc);

    void step_over_breakpoint();

    void wait_for_signal(std::string call = "break");

    siginfo_t get_signal_info();

    void handle_sigtrap(siginfo_t info, std::string call = "break");

    void initialise_load_address();

    uint64_t offset_load_address(uint64_t addr);

    uint64_t offset_dwarf_address(uint64_t addr);

    dwarf::die get_function_from_pc(uint64_t pc);

    dwarf::line_table::iterator get_line_entry_from_pc(uint64_t pc);

    uint64_t read_memory(uint64_t address);

    void write_memory(uint64_t address, uint64_t value);

    std::string m_prog_name;
    pid_t m_pid;
    uint64_t m_load_address = 0;
    std::unordered_map<std::intptr_t, breakpoint> m_breakpoints;
    dwarf::dwarf m_dwarf;
    elf::elf m_elf;
};

