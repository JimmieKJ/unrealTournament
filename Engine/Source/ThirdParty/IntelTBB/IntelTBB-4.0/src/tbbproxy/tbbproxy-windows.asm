; Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
;
; The source code contained or described herein and all documents related
; to the source code ("Material") are owned by Intel Corporation or its
; suppliers or licensors.  Title to the Material remains with Intel
; Corporation or its suppliers and licensors.  The Material is protected
; by worldwide copyright laws and treaty provisions.  No part of the
; Material may be used, copied, reproduced, modified, published, uploaded,
; posted, transmitted, distributed, or disclosed in any way without
; Intel's prior express written permission.
;
; No license under any patent, copyright, trade secret or other
; intellectual property right is granted to or conferred upon you by
; disclosure or delivery of the Materials, either expressly, by
; implication, inducement, estoppel or otherwise.  Any license under such
; intellectual property rights must be express and approved by Intel in
; writing.

#include "tbb/tbb_config.h"

// __TBB_STRING macro defined in "tbb_stddef.h". However, we cannot include "tbb_stddef.h"
// because it contains a lot of C/C++ definitions. So, we have to define __TBB_STRING here:
#define __TBB_STRING_AUX( x ) #x
#define __TBB_STRING( x ) __TBB_STRING_AUX( x )

// Eliminate difference between IA-32 and Intel 64: AWORD is a type of pointer, LANG is language
// specification for extern directive.
#ifdef ARCH_ia32
    #define AWORD dword
    #define LANG  c
#else
    #define AWORD qword
    #define LANG
#endif

#ifdef ARCH_ia32
    // These directives are required for IA32 architecture only.
    .686
    .model flat, syscall
#endif

/*
    Symbol names.
*/

// Note: masm for IA-32 does not like symbols defined as "name:" in data sections,
// so we have to define symbols with "name label type" directive instead.

fname macro sym:req
    align sizeof AWORD
    Ln_&sym& label byte
    byte "&sym&", 0
endm

.const        // Symbol names are constants.
#define __TBB_SYMBOL( sym ) fname sym
#include __TBB_STRING( __TBB_LST )

/*
    Symbol descriptors.
*/

extern LANG __tbb_internal_runtime_loader_stub : AWORD

fsymbol macro sym:req
    Ls_&sym& label AWORD
    AWORD __tbb_internal_runtime_loader_stub
    AWORD Ln_&sym&
    dword sizeof AWORD
    dword 1
endm

.data
align sizeof AWORD
public LANG __tbb_internal_runtime_loader_symbols
__tbb_internal_runtime_loader_symbols label AWORD
#define __TBB_SYMBOL( sym ) fsymbol sym
#include __TBB_STRING( __TBB_LST )
AWORD 0, 0        // Terminator of the __tbb_internal_runtime_loader_symbols array.
dword 0, 0

/*
    Generate functions.
*/

// Helper assembler macro to handle different naming conventions on IA-32 and Intel 64:
// IA-32: C++ names preserved, C names require leading underscore.
// Intel 64: All names preserved.
mangle  macro name:req
    #ifdef ARCH_ia32
        if @instr( 1, name, <?> )
            exitm @catstr( name )
        else
            exitm @catstr( <_>, name )
        endif
    #else
        exitm @catstr( name )
    #endif
endm

function macro sym:req
    mangle( sym )  proc
        jmp AWORD ptr Ls_&sym&
    mangle( sym )  endp
endm

.code
#define __TBB_SYMBOL( sym ) function sym
#include __TBB_STRING( __TBB_LST )

end

// end of file //
