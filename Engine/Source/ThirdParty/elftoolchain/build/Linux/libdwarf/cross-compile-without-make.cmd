@echo off
rem set LINUX_ROOT=D:\p4\CarefullyRedist\HostWin64\Linux_x64\v2_clang-3.5.0-ld-2.23.1-glibc-2.17_arm-unknown-linux-gnueabihf\toolchain
set LINUX_ARCHITECTURE=aarch64-unknown-linux-gnueabi
rem x86_64-unknown-linux-gnu 
rem arm-unknown-linux-gnueabihf

echo Quick hack bat file for cross-compiling libdwarf without cygwin.
echo You need to have Linux toolchain installed, and its absolute path to be set in LINUX_ROOT environment variable
echo Your LINUX_MULTIARCH_ROOT='%LINUX_MULTIARCH_ROOT%'
echo Your LINUX_ARCHITECTURE='%LINUX_ARCHITECTURE%'

set OUTPUT_LIB=libdwarf.a
set CXXFLAGS= -g -O2 -D__offsetof=offsetof -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -fno-exceptions -fno-rtti -fno-short-enums -fPIC -x c

rem neeeds to be absolute
rem set M4=%LINUX_ROOT%/bin/m4.exe
set CC=%LINUX_MULTIARCH_ROOT%%LINUX_ARCHITECTURE%/bin/clang.exe
set AR_RC=%LINUX_MULTIARCH_ROOT%%LINUX_ARCHITECTURE%/bin/%LINUX_ARCHITECTURE%-ar.exe rc
set RANLIB=%LINUX_MULTIARCH_ROOT%%LINUX_ARCHITECTURE%/bin/%LINUX_ARCHITECTURE%-ranlib.exe

rem	m4 -D SRCDIR=${.CURDIR} ${.IMPSRC} > ${.TARGET}

rem %M4% -D SRCDIR=. dwarf_nametbl.m4 dwarf_pubnames.m4 > dwarf_pubnames.c
rem %M4% -D SRCDIR=. dwarf_nametbl.m4 dwarf_pubtypes.m4 > dwarf_pubtypes.c
rem %M4% -D SRCDIR=. dwarf_nametbl.m4 dwarf_weaks.m4 > dwarf_weaks.c
rem %M4% -D SRCDIR=. dwarf_nametbl.m4 dwarf_funcs.m4 > dwarf_funcs.c
rem %M4% -D SRCDIR=. dwarf_nametbl.m4 dwarf_vars.m4 > dwarf_vars.c
rem %M4% -D SRCDIR=. dwarf_nametbl.m4 dwarf_types.m4 > dwarf_types.c
rem %M4% -D SRCDIR=. dwarf_pro_nametbl.m4 dwarf_pro_pubnames.m4 > dwarf_pro_pubnames.c
rem %M4% -D SRCDIR=. dwarf_pro_nametbl.m4 dwarf_pro_weaks.m4 > dwarf_pro_weaks.c
rem %M4% -D SRCDIR=. dwarf_pro_nametbl.m4 dwarf_pro_funcs.m4 > dwarf_pro_funcs.c
rem %M4% -D SRCDIR=. dwarf_pro_nametbl.m4 dwarf_pro_types.m4 > dwarf_pro_types.c
rem %M4% -D SRCDIR=. dwarf_pro_nametbl.m4 dwarf_pro_vars.m4 > dwarf_pro_vars.c

set INCLUDE=-I..\common\%LINUX_ARCHITECTURE% -I..\libelf
set SRCS=dwarf_abbrev.c dwarf_arange.c dwarf_attr.c dwarf_attrval.c dwarf_cu.c dwarf_dealloc.c dwarf_die.c dwarf_dump.c dwarf_errmsg.c dwarf_finish.c dwarf_form.c dwarf_frame.c dwarf_funcs.c dwarf_init.c dwarf_lineno.c dwarf_loclist.c dwarf_macinfo.c dwarf_pro_arange.c dwarf_pro_attr.c dwarf_pro_die.c dwarf_pro_expr.c dwarf_pro_finish.c dwarf_pro_frame.c dwarf_pro_funcs.c dwarf_pro_init.c dwarf_pro_lineno.c dwarf_pro_macinfo.c dwarf_pro_pubnames.c dwarf_pro_reloc.c dwarf_pro_sections.c dwarf_pro_types.c dwarf_pro_vars.c dwarf_pro_weaks.c dwarf_pubnames.c dwarf_pubtypes.c dwarf_ranges.c dwarf_reloc.c dwarf_seterror.c dwarf_str.c dwarf_types.c dwarf_vars.c dwarf_weaks.c libdwarf.c libdwarf_abbrev.c libdwarf_arange.c libdwarf_attr.c libdwarf_die.c libdwarf_error.c libdwarf_elf_access.c libdwarf_elf_init.c libdwarf_frame.c libdwarf_info.c libdwarf_init.c libdwarf_lineno.c libdwarf_loc.c libdwarf_loclist.c libdwarf_macinfo.c libdwarf_nametbl.c libdwarf_ranges.c libdwarf_reloc.c libdwarf_rw.c libdwarf_sections.c libdwarf_str.c 
set OBJ=dwarf_abbrev.o dwarf_arange.o dwarf_attr.o dwarf_attrval.o dwarf_cu.o dwarf_dealloc.o dwarf_die.o dwarf_dump.o dwarf_errmsg.o dwarf_finish.o dwarf_form.o dwarf_frame.o dwarf_funcs.o dwarf_init.o dwarf_lineno.o dwarf_loclist.o dwarf_macinfo.o dwarf_pro_arange.o dwarf_pro_attr.o dwarf_pro_die.o dwarf_pro_expr.o dwarf_pro_finish.o dwarf_pro_frame.o dwarf_pro_funcs.o dwarf_pro_init.o dwarf_pro_lineno.o dwarf_pro_macinfo.o dwarf_pro_pubnames.o dwarf_pro_reloc.o dwarf_pro_sections.o dwarf_pro_types.o dwarf_pro_vars.o dwarf_pro_weaks.o dwarf_pubnames.o dwarf_pubtypes.o dwarf_ranges.o dwarf_reloc.o dwarf_seterror.o dwarf_str.o dwarf_types.o dwarf_vars.o dwarf_weaks.o libdwarf.o libdwarf_abbrev.o libdwarf_arange.o libdwarf_attr.o libdwarf_die.o libdwarf_error.o libdwarf_elf_access.o libdwarf_elf_init.o libdwarf_frame.o libdwarf_info.o libdwarf_init.o libdwarf_lineno.o libdwarf_loc.o libdwarf_loclist.o libdwarf_macinfo.o libdwarf_nametbl.o libdwarf_ranges.o libdwarf_reloc.o libdwarf_rw.o libdwarf_sections.o libdwarf_str.o 

rem echo %CC% %CXXFLAGS% %CPPFLAGS% %SRC%
%CC% %CXXFLAGS% %INCLUDE% -c -target %LINUX_ARCHITECTURE% --sysroot=%LINUX_ROOT% %SRCS%

%AR_RC% %OUTPUT_LIB% %OBJ%
%RANLIB% %OUTPUT_LIB%

echo Now replace libdwarf.a in lib/Linux/ with file from this directory
p4 open ../../../lib/Linux/%LINUX_ARCHITECTURE%/libdwarf.a
move libdwarf.a ../../../lib/Linux/%LINUX_ARCHITECTURE%/


del %OBJ%
