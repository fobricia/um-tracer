#include <iostream>
#include <Windows.h>
#include <fstream>
#include <filesystem>
#include "vmtracer.hpp"
#include "vmp2.hpp"
#include "cli-parser.hpp"

extern "C" void __lconstbzx(void);
extern "C" u64 __mbase;

#define NT_HEADER(x) \
	reinterpret_cast<PIMAGE_NT_HEADERS64>( \
		reinterpret_cast<PIMAGE_DOS_HEADER>(x)->e_lfanew + x)

inline std::vector<vmp2::entry_t> traces;
inline vmp2::file_header trace_header;

int __cdecl main(int argc, const char** argv)
{
    argparse::argument_parser_t parser(
        "um-tracer", "usermode virtual instruction tracer");

    parser.add_argument()
        .names({ "--bin", "--vmpbin" })
        .description("path to a binary protected with VMProtect 2")
        .required(true);

    parser.add_argument()
        .names({ "--table", "--vmtable" })
        .description("relative virtual address to the vm handler table")
        .required(true);

    parser.add_argument()
        .names({ "--base", "--imagebase" })
        .description("image base from OptionalHeader::ImageBase")
        .required(true);

    parser.add_argument()
        .names({ "--out", "--output" })
        .description("name and/or path to output file")
        .required(true);

    parser.enable_help();
    auto err = parser.parse(argc, argv);

    if (err)
    {
        std::cout << err << std::endl;
        return -1;
    }

    if (parser.exists("help"))
    {
        parser.print_help();
        return 0;
    }

    /*
        the vm_handlers are encrypted/encoded with a basic
        math operation... typically a NOT, XOR, NEG, etc...

        You can determine what type of encryption your binary
        is using by first finding where the LEA r12, vm_handlers
        is located, then follow the usage of r12 until you see
        MOV GP, [r12 + rax * 8], then follow the usage of the GP...

        For example:
        .vmp1:00000001401D1015                 lea     r12, vm_handlers
        .vmp1:00000001401D0C0A                 mov     rdx, [r12+rax*8]
        .vmp1:00000001401D0C10                 ror     rdx, 25h

        Note:
        R12 and RAX always seem to be used for this vm handler index...
        You could signature scan for LEA r12, ? ? ? ? and find the vm handler
        table really easily by manually inspecting each result...
    */

    vm::decrypt_handler_t _decrypt_handler = 
        [](u64 val) -> u64
    {
        return val ^ 0x7F3D2149;
    };

    vm::encrypt_handler_t _encrypt_handler = 
        [](u64 val) -> u64
    {
        return val ^ 0x7F3D2149;
    };

    vm::handler::edit_entry_t _edit_entry =
        [](u64* entry_ptr, u64 val) -> void
    {
        DWORD old_prot;
        VirtualProtect(entry_ptr, sizeof val, 
            PAGE_EXECUTE_READWRITE, &old_prot);

        *entry_ptr = val;
        VirtualProtect(entry_ptr, sizeof val,
            old_prot, &old_prot);
    };

    const auto module_base =
        reinterpret_cast<std::uintptr_t>(
            LoadLibraryExA(parser.get<std::string>("bin").c_str(),
                NULL, DONT_RESOLVE_DLL_REFERENCES));

    // used in LCONSTBZX hook...
    __mbase = module_base;

    const auto handler_table_rva = std::strtoul(
        parser.get<std::string>("table").c_str(), nullptr, 16);

    const auto image_base = std::strtoull(
        parser.get<std::string>("base").c_str(), nullptr, 16);

    const auto handler_table_ptr = 
        reinterpret_cast<std::uintptr_t*>(
            module_base + handler_table_rva);

    /*
        the VM handler table is an array of 256 QWORD's... each encrypted differently per-binary...
        each one of these is an encrypted RVA to a virtual instruction...

        .vmp1:00000001401D25D3 vm_handlers     dq 3A28FA000000028h, 3A40E4000000028h, 3A2F5C000000028h
        .vmp1:00000001401D25D3                 dq 3A1096000000028h, 3A3DBC000000028h, 3A1DDA000000028h
        .vmp1:00000001401D25D3                 dq 3A6032000000028h, 2 dup(3A40E4000000028h), 3A2B5A000000028h
        .vmp1:00000001401D25D3                 dq 3A4004000000028h, 3A2810000000028h, 3A446A000000028h
        .vmp1:00000001401D25D3                 dq 3A39B6000000028h, 3A6728000000028h, 3A6032000000028h
        .vmp1:00000001401D25D3                 dq 3A34F0000000028h, 3A46F2000000028h, 3A0170000000028h
        .vmp1:00000001401D25D3                 dq 3A0952000000028h, 3A4004000000028h, 3A494E000000028h
        .vmp1:00000001401D25D3                 dq 3A35C2000000028h, 3A4A1E000000028h, 3A37D8000000028h
        .vmp1:00000001401D25D3                 dq 3A1482000000028h, 3A6492000000028h, 3A2948000000028h
        .vmp1:00000001401D25D3                 dq 3A2D1C000000028h, 2 dup(3A6ABE000000028h), 3A068A000000028h
        .vmp1:00000001401D25D3                 dq 3A3F52000000028h, 3A118E000000028h, 3A27BE000000028h

        // .... many more ...
    */

    vm::handler::table_t handler_table(handler_table_ptr, _edit_entry);

    // set all vm handler callbacks to just 
    // print the rolling decrypt key and handler idx...
    for (auto idx = 0u; idx < 256; ++idx)
    {
        handler_table.set_callback(idx,
            [](vm::registers* regs, u8 handler_idx) -> void
            {
                vmp2::entry_t entry;
                entry.decrypt_key = regs->rbx;
                entry.handler_idx = handler_idx;
                entry.vip = regs->rsi;

                memcpy(&entry.regs, &regs->r15, sizeof entry.regs);
                memcpy(&entry.vregs, (void*)regs->rdi, sizeof entry.vregs);

                for (auto idx = 0u; idx < sizeof(entry.vsp) / 8; ++idx)
                    entry.vsp.qword[idx] = reinterpret_cast<u64*>(regs->rbp)[idx];

                traces.push_back(entry);
                std::printf("> TID = %d, handler idx = %d, decryption key = 0x%p\n", 
                    GetCurrentThreadId(), handler_idx, regs->rbx);
            }
        );
    }

    vm::tracer_t tracer(
        module_base, 
        image_base, 
        _decrypt_handler,
        _encrypt_handler,
        &handler_table
    );

    // change vm handler 0x55 (LCONSTBZX) to our implimentation of it...
    auto _meta_data = handler_table.get_meta_data(0x55);
    _meta_data.virt = reinterpret_cast<u64>(&__lconstbzx);
    handler_table.set_meta_data(0x55, _meta_data);

    std::ofstream vmp2_file(parser.get<std::string>("out"), std::ios::binary);
    memcpy(&trace_header.magic, "VMP2", sizeof "VMP2" - 1);
    trace_header.epoch_time = time(nullptr);
    trace_header.entry_offset = sizeof trace_header;
    trace_header.advancement = vmp2::exec_type_t::forward;
    trace_header.version = vmp2::version_t::v1;
    trace_header.module_base = module_base;

    // patch vm handler table...
    tracer.start();

    // call entry point...
    auto result = reinterpret_cast<int (*)()>(
        NT_HEADER(module_base)->OptionalHeader.AddressOfEntryPoint + module_base)();

    std::printf("result = %d\n", result);

    // unpatch vm handler table...
    tracer.stop();

    // write vmp2 file to disk...
    trace_header.entry_count = traces.size();
    vmp2_file.write((char*)&trace_header, sizeof trace_header);

    for (auto& trace : traces)
        vmp2_file.write((char*)&trace, sizeof trace);

    vmp2_file.close();
    std::printf("> finished vm trace...\n");
    std::getchar();
}