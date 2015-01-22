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

.686
.model flat,c
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchadd1
__TBB_machine_fetchadd1:
	mov edx,4[esp]
	mov eax,8[esp]
	lock xadd [edx],al
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchstore1
__TBB_machine_fetchstore1:
	mov edx,4[esp]
	mov eax,8[esp]
	lock xchg [edx],al
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_cmpswp1
__TBB_machine_cmpswp1:
	mov edx,4[esp]
	mov ecx,8[esp]
	mov eax,12[esp]
	lock cmpxchg [edx],cl
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchadd2
__TBB_machine_fetchadd2:
	mov edx,4[esp]
	mov eax,8[esp]
	lock xadd [edx],ax
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchstore2
__TBB_machine_fetchstore2:
	mov edx,4[esp]
	mov eax,8[esp]
	lock xchg [edx],ax
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_cmpswp2
__TBB_machine_cmpswp2:
	mov edx,4[esp]
	mov ecx,8[esp]
	mov eax,12[esp]
	lock cmpxchg [edx],cx
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchadd4
__TBB_machine_fetchadd4:
	mov edx,4[esp]
	mov eax,8[esp]
	lock xadd [edx],eax
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchstore4
__TBB_machine_fetchstore4:
	mov edx,4[esp]
	mov eax,8[esp]
	lock xchg [edx],eax
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_cmpswp4
__TBB_machine_cmpswp4:
	mov edx,4[esp]
	mov ecx,8[esp]
	mov eax,12[esp]
	lock cmpxchg [edx],ecx
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchadd8
__TBB_machine_fetchadd8:
	push ebx
	push edi
	mov edi,12[esp]
	mov eax,[edi]
	mov edx,4[edi]
__TBB_machine_fetchadd8_loop:
	mov ebx,16[esp]
	mov ecx,20[esp]
	add ebx,eax
	adc ecx,edx
	lock cmpxchg8b qword ptr [edi]
	jnz __TBB_machine_fetchadd8_loop
	pop edi
	pop ebx
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_fetchstore8
__TBB_machine_fetchstore8:
	push ebx
	push edi
	mov edi,12[esp]
	mov ebx,16[esp]
	mov ecx,20[esp]
	mov eax,[edi]
	mov edx,4[edi]
__TBB_machine_fetchstore8_loop:
	lock cmpxchg8b qword ptr [edi]
	jnz __TBB_machine_fetchstore8_loop
	pop edi
	pop ebx
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_cmpswp8
__TBB_machine_cmpswp8:
	push ebx
	push edi
	mov edi,12[esp]
	mov ebx,16[esp]
	mov ecx,20[esp]
	mov eax,24[esp]
	mov edx,28[esp]
	lock cmpxchg8b qword ptr [edi]
	pop edi
	pop ebx
	ret
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_load8
__TBB_machine_Load8:
	; If location is on stack, compiler may have failed to align it correctly, so we do dynamic check.
	mov ecx,4[esp]
	test ecx,7
	jne load_slow
	; Load within a cache line
	sub esp,12
	fild qword ptr [ecx]
	fistp qword ptr [esp]
	mov eax,[esp]
	mov edx,4[esp]
	add esp,12
	ret
load_slow:
	; Load is misaligned. Use cmpxchg8b.
	push ebx
	push edi
	mov edi,ecx
	xor eax,eax
	xor ebx,ebx
	xor ecx,ecx
	xor edx,edx
	lock cmpxchg8b qword ptr [edi]
	pop edi
	pop ebx
	ret
EXTRN __TBB_machine_store8_slow:PROC
.code 
	ALIGN 4
	PUBLIC c __TBB_machine_store8
__TBB_machine_Store8:
	; If location is on stack, compiler may have failed to align it correctly, so we do dynamic check.
	mov ecx,4[esp]
	test ecx,7
	jne __TBB_machine_store8_slow ;; tail call to tbb_misc.cpp
	fild qword ptr 8[esp]
	fistp qword ptr [ecx]
	ret
end
