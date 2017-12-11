cmake_minimum_required(VERSION 3.5)
project(vasm)

set(VASM_CPU "x86" CACHE STRING "vasm target CPU")
set(VASM_SYNTAX "std" CACHE STRING "vasm assembler syntax")

# vasm
set(vasm_sources
    vasm.c
    atom.c
    expr.c
    symtab.c
    symbol.c
    error.c
    parse.c
    reloc.c
    hugeint.c
    cond.c
    supp.c
    cpus/${VASM_CPU}/cpu.c
    syntax/${VASM_SYNTAX}/syntax.c
    output_test.c
    output_elf.c
    output_bin.c
    output_vobj.c
    output_hunk.c
    output_aout.c
    output_tos.c
    output_srec.c
    )
set(vasm_exe vasm${VASM_CPU}_${VASM_SYNTAX})
add_executable(${vasm_exe} ${vasm_sources})
target_include_directories(
    ${vasm_exe}
    PRIVATE
    .
    cpus/${VASM_CPU}
    syntax/${VASM_SYNTAX}
    )
if(UNIX)
  target_link_libraries(${vasm_exe} m)
endif()

# vobjdump
set(vobjdump_sources
    vobjdump.c
    )
set(vobjdump_exe vobjdump)
add_executable(${vobjdump_exe} ${vobjdump_sources})
target_include_directories(
    ${vobjdump_exe}
    PRIVATE
    .
    cpus/${VASM_CPU}
    syntax/${VASM_SYNTAX}
    )

