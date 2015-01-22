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

; DO NOT EDIT - AUTOMATICALLY GENERATED FROM .s FILE
.code 
	ALIGN 8
	PUBLIC __TBB_machine_fetchadd1
__TBB_machine_fetchadd1:
	mov rax,rdx
	lock xadd [rcx],al
	ret
.code 
	ALIGN 8
	PUBLIC __TBB_machine_fetchstore1
__TBB_machine_fetchstore1:
	mov rax,rdx
	lock xchg [rcx],al
	ret
.code 
	ALIGN 8
	PUBLIC __TBB_machine_cmpswp1
__TBB_machine_cmpswp1:
	mov rax,r8
	lock cmpxchg [rcx],dl
	ret
.code 
	ALIGN 8
	PUBLIC __TBB_machine_fetchadd2
__TBB_machine_fetchadd2:
	mov rax,rdx
	lock xadd [rcx],ax
	ret
.code 
	ALIGN 8
	PUBLIC __TBB_machine_fetchstore2
__TBB_machine_fetchstore2:
	mov rax,rdx
	lock xchg [rcx],ax
	ret
.code 
	ALIGN 8
	PUBLIC __TBB_machine_cmpswp2
__TBB_machine_cmpswp2:
	mov rax,r8
	lock cmpxchg [rcx],dx
	ret
.code
        ALIGN 8
        PUBLIC __TBB_machine_pause
__TBB_machine_pause:
L1:
        dw 090f3H; pause
        add ecx,-1
        jne L1
        ret
end

