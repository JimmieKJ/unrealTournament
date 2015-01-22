@echo off
set LINUX_ARCHITECTURE=arm-unknown-linux-gnueabihf
rem x86_64-unknown-linux-gnu 
rem arm-unknown-linux-gnueabihf

echo Quick hack bat file for cross-compiling libelf without cygwin.
echo You need to have Linux toolchain installed, and its absolute path to be set in LINUX_ROOT environment variable
echo Your LINUX_ROOT='%LINUX_ROOT%'
echo Your LINUX_ARCHITECTURE='%LINUX_ARCHITECTURE%'

set OUTPUT_LIB=libelf.a
set CXXFLAGS= -g -O2 -D__offsetof=offsetof -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -fno-exceptions -fno-rtti -fno-short-enums -fPIC  -x c

rem neeeds to be absolute
rem set M4=%LINUX_ROOT%/bin/m4.exe
set CC=%LINUX_ROOT%/bin/clang.exe
set AR_RC=%LINUX_ROOT%/bin/%LINUX_ARCHITECTURE%-ar.exe rc
set RANLIB=%LINUX_ROOT%/bin/%LINUX_ARCHITECTURE%-ranlib.exe

rem	m4 -D SRCDIR=${.CURDIR} ${.IMPSRC} > ${.TARGET}

rem Decided not to use M4 as (other) sources are modified by hand anyway
rem %M4% -D SRCDIR=. elf_types.m4 libelf_convert.m4 > libelf_convert.c
rem %M4% -D SRCDIR=. elf_types.m4 libelf_fsize.m4 > libelf_fsize.c
rem %M4% -D SRCDIR=. elf_types.m4 libelf_msize.m4 > libelf_msize.c

set INCLUDE=-I..\common\%LINUX_ARCHITECTURE% -isystem=.
set SRCS=elf.c elf_begin.c elf_cntl.c elf_end.c elf_errmsg.c elf_errno.c elf_data.c elf_fill.c elf_flag.c elf_getarhdr.c elf_getarsym.c elf_getbase.c elf_getident.c elf_hash.c elf_kind.c elf_memory.c elf_next.c elf_open.c elf_rand.c elf_rawfile.c elf_phnum.c elf_shnum.c elf_shstrndx.c elf_scn.c elf_strptr.c elf_update.c elf_version.c gelf_cap.c gelf_checksum.c gelf_dyn.c gelf_ehdr.c gelf_getclass.c gelf_fsize.c gelf_move.c gelf_phdr.c gelf_rel.c gelf_rela.c gelf_shdr.c gelf_sym.c gelf_syminfo.c gelf_symshndx.c gelf_xlate.c libelf_align.c libelf_allocate.c libelf_ar.c libelf_ar_util.c libelf_checksum.c libelf_data.c libelf_ehdr.c libelf_extended.c libelf_memory.c	libelf_open.c libelf_phdr.c libelf_shdr.c libelf_xlate.c libelf_convert.c libelf_fsize.c libelf_msize.c
set OBJ=elf.o elf_begin.o elf_cntl.o elf_end.o elf_errmsg.o elf_errno.o elf_data.o elf_fill.o elf_flag.o elf_getarhdr.o elf_getarsym.o elf_getbase.o elf_getident.o elf_hash.o elf_kind.o elf_memory.o elf_next.o elf_open.o elf_rand.o elf_rawfile.o elf_phnum.o elf_shnum.o elf_shstrndx.o elf_scn.o elf_strptr.o elf_update.o elf_version.o gelf_cap.o gelf_checksum.o gelf_dyn.o gelf_ehdr.o gelf_getclass.o gelf_fsize.o gelf_move.o gelf_phdr.o gelf_rel.o gelf_rela.o gelf_shdr.o gelf_sym.o gelf_syminfo.o gelf_symshndx.o gelf_xlate.o libelf_align.o libelf_allocate.o libelf_ar.o libelf_ar_util.o libelf_checksum.o libelf_data.o libelf_ehdr.o libelf_extended.o libelf_memory.o	libelf_open.o libelf_phdr.o libelf_shdr.o libelf_xlate.o libelf_convert.o libelf_fsize.o libelf_msize.o

rem echo %CC% %CXXFLAGS% %CPPFLAGS% %SRC%
%CC% %CXXFLAGS% %INCLUDE% -c -target %LINUX_ARCHITECTURE% --sysroot=%LINUX_ROOT% %SRCS%

%AR_RC% %OUTPUT_LIB% %OBJ%
%RANLIB% %OUTPUT_LIB%

echo Now replace libelf.a in lib/Linux/ with file from this directory
p4 open ../../../lib/Linux/%LINUX_ARCHITECTURE%/libelf.a
move libelf.a ../../../lib/Linux/%LINUX_ARCHITECTURE%/


del %OBJ%
