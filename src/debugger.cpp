#include <cstdint>
#include <sys/wait.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>

#include "linenoise/linenoise.h"
#include "utility.h"
#include "debugger.h"
#include "registers.h"


std::vector<symbol> debugger::lookup_symbol(const std::string &name) {
    std::vector<symbol> syms;

    for (auto &sec: m_elf.sections()) {
        if (sec.get_hdr().type != elf::sht::symtab && sec.get_hdr().type != elf::sht::dynsym)
            continue;

        for (auto sym: sec.as_symtab()) {
            if (sym.get_name() == name) {
                auto &d = sym.get_data();
                syms.push_back(symbol{to_symbol_type(d.type()), sym.get_name(), d.value});
            }
        }
    }

    return syms;
}

void debugger::initialise_load_address() {
    if (m_elf.get_hdr().type == elf::et::dyn) {
        std::ifstream map("/proc/" + std::to_string(m_pid) + "/maps");

        std::string addr;
        std::getline(map, addr, '-');

        m_load_address = std::stol(addr, 0, 16);
    }
}

uint64_t debugger::offset_load_address(uint64_t addr) {
    return addr - m_load_address;
}

uint64_t debugger::offset_dwarf_address(uint64_t addr) {
    return addr + m_load_address;
}

void debugger::remove_breakpoint(std::intptr_t addr) {
    if (m_breakpoints.at(addr).is_enabled()) {
        m_breakpoints.at(addr).disable();
    }
    m_breakpoints.erase(addr);
}

void debugger::step_out() {
    auto frame_pointer = get_register_value(m_pid, reg::rbp);
    auto return_address = read_memory(frame_pointer + 8);

    bool should_remove_breakpoint = false;
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address);
        should_remove_breakpoint = true;
    }

    continue_execution();

    if (should_remove_breakpoint) {
        remove_breakpoint(return_address);
    }
}

void debugger::step_in() {
    auto line = get_line_entry_from_pc(get_offset_pc())->line;

    while (get_line_entry_from_pc(get_offset_pc())->line == line) {
        single_step_instruction_with_breakpoint_check();
    }

    auto line_entry = get_line_entry_from_pc(get_offset_pc());
    print_source(line_entry->file->path, line_entry->line);
}

void debugger::step_over() {
    auto func = get_function_from_pc(get_offset_pc());
    auto func_entry = at_low_pc(func);
    auto func_end = at_high_pc(func);

    auto line = get_line_entry_from_pc(func_entry);
    auto start_line = get_line_entry_from_pc(get_offset_pc());

    std::vector<std::intptr_t> to_delete{};

    while (line->address < func_end) {
        auto load_address = offset_dwarf_address(line->address);
        if (line->address != start_line->address && !m_breakpoints.count(load_address)) {
            set_breakpoint_at_address(load_address, "show");
            to_delete.push_back(load_address);
        }
        ++line;
    }

    auto frame_pointer = get_register_value(m_pid, reg::rbp);
    auto return_address = read_memory(frame_pointer + 8);
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address, "show");
        to_delete.push_back(return_address);
    }

    continue_execution("show");

    for (auto addr: to_delete) {
        remove_breakpoint(addr);
    }
}

void debugger::single_step_instruction() {
    ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void debugger::single_step_instruction_with_breakpoint_check() {
    if (m_breakpoints.count(get_pc())) {
        step_over_breakpoint();
    } else {
        single_step_instruction();
    }
}

uint64_t debugger::read_memory(uint64_t address) {
    return ptrace(PTRACE_PEEKDATA, m_pid, address, nullptr);
}

void debugger::write_memory(uint64_t address, uint64_t value) {
    ptrace(PTRACE_POKEDATA, m_pid, address, value);
}

uint64_t debugger::get_pc() {
    return get_register_value(m_pid, reg::rip);
}

uint64_t debugger::get_offset_pc() {
    return offset_load_address(get_pc());
}

void debugger::set_pc(uint64_t pc) {
    set_register_value(m_pid, reg::rip, pc);
}

dwarf::die debugger::get_function_from_pc(uint64_t pc) {
    for (auto &cu: m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            for (const auto &die: cu.root()) {
                if (die.tag == dwarf::DW_TAG::subprogram) {
                    if (die_pc_range(die).contains(pc)) {
                        return die;
                    }
                }
            }
        }
    }

    throw std::out_of_range{"Cannot find function"};
}

dwarf::line_table::iterator debugger::get_line_entry_from_pc(uint64_t pc) {
    for (auto &cu: m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            auto &lt = cu.get_line_table();
            auto it = lt.find_address(pc);
            if (it == lt.end()) {
                throw std::out_of_range{"Cannot find line entry"};
            } else {
                return it;
            }
        }
    }

    throw std::out_of_range{"Cannot find line entry"};
}

void debugger::print_source(const std::string &file_name, unsigned line, unsigned n_lines_context, std::string call) {
    std::ifstream file{file_name};

    auto start_line = line <= n_lines_context ? 1 : line - n_lines_context;
    auto end_line = line + n_lines_context + (line < n_lines_context ? n_lines_context - line : 0) + 1;

    char c{};
    auto current_line = 1u;

    while (current_line != start_line && file.get(c)) {
        if (c == '\n') {
            ++current_line;
        }
    }

    if(call != "show") {
        std::cout << (current_line == line ? "> " : "  ");
    }
    else std::cout << "  ";

    while (current_line <= end_line && file.get(c)) {
        std::cout << c;
        if (c == '\n') {
            ++current_line;

            if(call != "show") {
                std::cout << (current_line == line ? "> " : "  ");
            }
            else std::cout << "  ";
        }
    }

    std::cout << std::endl;
}

/*void debugger::show(){
    auto func = get_function_from_pc(get_offset_pc());
    auto func_entry = at_low_pc(func);
    auto func_end = at_high_pc(func);

    auto line_entry = get_line_entry_from_pc(func_entry);
    auto line_end = get_line_entry_from_pc(func_end-3);

    std::ifstream file{line_entry->file->path};
    auto tmp=line_end->end_sequence;
    auto tmp2=line_end->epilogue_begin;

    char c{};
    auto current_line = 1u;

    while (current_line != line_entry->line && file.get(c)) {
        if (c == '\n') {
            ++current_line;
        }
    }

    std::cout << "  ";

    while (current_line <= line_end->line + 1 && file.get(c)) {
        std::cout << c;
        if (c == '\n') {
            ++current_line;
            std::cout << "  ";
        }
    }

    std::cout << std::endl;
}*/

siginfo_t debugger::get_signal_info() {
    siginfo_t info;
    ptrace(PTRACE_GETSIGINFO, m_pid, nullptr, &info);
    return info;
}

void debugger::step_over_breakpoint() {
    if (m_breakpoints.count(get_pc())) {
        auto &bp = m_breakpoints[get_pc()];
        if (bp.is_enabled()) {
            bp.disable();
            ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
            wait_for_signal("show");
            bp.enable();
        }
    }
}

void debugger::wait_for_signal(std::string call) {
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);

    auto siginfo = get_signal_info();

    switch (siginfo.si_signo) {
        case SIGTRAP:
            handle_sigtrap(siginfo, call);
            break;
        case SIGSEGV:
            std::cout << "Yay, segfault. Reason: " << siginfo.si_code << std::endl;
            break;
        default:
            end_of_program = true;
            std::cout << "Got signal " << strsignal(siginfo.si_signo) << std::endl;
    }
}

void debugger::handle_sigtrap(siginfo_t info, std::string call) {
    switch (info.si_code) {
        case SI_KERNEL:
        case TRAP_BRKPT: {
            set_pc(get_pc() - 1);
            if(call != "show" && call != "initial"){
                std::cout << "Hit breakpoint at address 0x" << std::hex << get_pc() << std::endl;
            }
            auto offset_pc = offset_load_address(get_pc()); //rember to offset the pc for querying DWARF
            try{
                auto line_entry = get_line_entry_from_pc(offset_pc);
                if (call != "initial"){
                    print_source(line_entry->file->path, line_entry->line);
                }
            }
            catch(std::out_of_range e){
                std::cout << "End of program" << std::endl;
                end_of_program = true;
                return;
            }
            return;
        }
        case TRAP_TRACE:
            return;
        default:
            //end_of_program = true;
            return;
    }
}

void debugger::continue_execution(std::string call) {
    step_over_breakpoint();
    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);
    wait_for_signal(call);
}

void debugger::dump_registers() {
    for (const auto &rd: g_register_descriptors) {
        std::cout << rd.name << " 0x"
                  << std::setfill('0') << std::setw(16) << std::hex << get_register_value(m_pid, rd.r) << std::endl;
    }
}

void debugger::handle_command(const std::string &line) {
    auto args = split(line, ' ');
    auto command = args[0];

    if (is_prefix(command, "continue")) {
        continue_execution();
    } else if (is_prefix(command, "break")) {
        if (args[1][0] == '0' && args[1][1] == 'x') {
            std::string addr{args[1], 2};
            set_breakpoint_at_address(std::stol(addr, 0, 16));
        } else if (args[1].find(':') != std::string::npos) {
            auto file_and_line = split(args[1], ':');
            set_breakpoint_at_source_line(file_and_line[0], std::stol(file_and_line[1]));
        } else {
            set_breakpoint_at_function(args[1]);
        }
    } else if (is_prefix(command, "step")) {
        step_in();
    } else if (is_prefix(command, "next")) {
        step_over();
    } else if (is_prefix(command, "finish")) {
        step_out();
    } else if (is_prefix(command, "register")) {
        if (is_prefix(args[1], "dump")) {
            dump_registers();
        } else if (is_prefix(args[1], "read")) {
            std::cout << get_register_value(m_pid, get_register_from_name(args[2])) << std::endl;
        } else if (is_prefix(args[1], "write")) {
            std::string val{args[3], 2}; //assume 0xVAL
            set_register_value(m_pid, get_register_from_name(args[2]), std::stol(val, 0, 16));
        }
    } else if (is_prefix(command, "memory")) {
        std::string addr{args[2], 2}; //assume 0xADDRESS

        if (is_prefix(args[1], "read")) {
            std::cout << std::hex << read_memory(std::stol(addr, 0, 16)) << std::endl;
        }
        if (is_prefix(args[1], "write")) {
            std::string val{args[3], 2}; //assume 0xVAL
            write_memory(std::stol(addr, 0, 16), std::stol(val, 0, 16));
        }
    } else if (is_prefix(command, "symbol")) {
        auto syms = lookup_symbol(args[1]);
        for (auto &&s: syms) {
            std::cout << s.name << ' ' << to_string(s.type) << " 0x" << std::hex << s.addr << std::endl;
        }
    } else if (is_prefix(command, "show")) {
        if (m_breakpoints.size() == 0) {
            set_breakpoint_at_function("main", "show");
            continue_execution("initial");
        }
        auto func = get_function_from_pc(get_offset_pc());
        auto func_entry = at_low_pc(func);
        auto func_end = at_high_pc(func);

        auto line_entry = get_line_entry_from_pc(func_entry);
        auto line_end = get_line_entry_from_pc(func_end - 3);
        print_source(line_entry->file->path, line_entry->line, line_end->line, "show");
        auto &bp = m_breakpoints[get_pc()];
        bp.disable();
    } else {
        std::cerr << "Unknown command\n";
    }
}

bool is_suffix(const std::string &s, const std::string &of) {
    if (s.size() > of.size()) return false;
    auto diff = of.size() - s.size();
    return std::equal(s.begin(), s.end(), of.begin() + diff);
}

void debugger::set_breakpoint_at_function(const std::string &name, std::string call) {
    for (const auto &cu: m_dwarf.compilation_units()) {
        for (const auto &die: cu.root()) {
            if (die.has(dwarf::DW_AT::name) && at_name(die) == name) {
                auto low_pc = at_low_pc(die);
                auto entry = get_line_entry_from_pc(low_pc);
                ++entry; //skip prologue
                set_breakpoint_at_address(offset_dwarf_address(entry->address), call);
            }
        }
    }
}

void debugger::set_breakpoint_at_source_line(const std::string &file, unsigned line) {
    for (const auto &cu: m_dwarf.compilation_units()) {
        if (is_suffix(file, at_name(cu.root()))) {
            const auto &lt = cu.get_line_table();

            for (const auto &entry: lt) {
                if (entry.is_stmt && entry.line == line) {
                    set_breakpoint_at_address(offset_dwarf_address(entry.address));
                    return;
                }
            }
        }
    }
}

void debugger::set_breakpoint_at_address(std::intptr_t addr, std::string call) {
    if (call != "show"){
        std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
    }
    breakpoint bp{m_pid, addr};
    bp.enable();
    m_breakpoints[addr] = bp;
}

void debugger::run() {
    wait_for_signal();
    initialise_load_address();

    char *line = nullptr;

    while (!end_of_program && (line = linenoise("MEGAdbg> ")) != nullptr) {
        handle_command(line);
        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}
