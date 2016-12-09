// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#if !defined(FMEMORY_INLINE_FUNCTION_DECORATOR)
	#define FMEMORY_INLINE_FUNCTION_DECORATOR 
#endif

#if !defined(FMEMORY_INLINE_GMalloc)
	#define FMEMORY_INLINE_GMalloc GMalloc
#endif

struct FMemory;
struct FScopedMallocTimer;

FMEMORY_INLINE_FUNCTION_DECORATOR void* FMemory::Malloc(SIZE_T Count, uint32 Alignment)
{
	if (!FMEMORY_INLINE_GMalloc)
	{
		return MallocExternal(Count, Alignment);
	}
	DoGamethreadHook(0);
	FScopedMallocTimer Timer(0);
	return FMEMORY_INLINE_GMalloc->Malloc(Count, Alignment);
}

FMEMORY_INLINE_FUNCTION_DECORATOR void* FMemory::Realloc(void* Original, SIZE_T Count, uint32 Alignment)
{
	if (!FMEMORY_INLINE_GMalloc)
	{
		return ReallocExternal(Original, Count, Alignment);
	}
	DoGamethreadHook(1);
	FScopedMallocTimer Timer(1);
	return FMEMORY_INLINE_GMalloc->Realloc(Original, Count, Alignment);
}

FMEMORY_INLINE_FUNCTION_DECORATOR void FMemory::Free(void* Original)
{
	if (!Original)
	{
		FScopedMallocTimer Timer(3);
		return;
	}

	if (!FMEMORY_INLINE_GMalloc)
	{
		FreeExternal(Original);
		return;
	}
	DoGamethreadHook(2);
	FScopedMallocTimer Timer(2);
	FMEMORY_INLINE_GMalloc->Free(Original);
}

FMEMORY_INLINE_FUNCTION_DECORATOR SIZE_T FMemory::GetAllocSize(void* Original)
{
	if (!FMEMORY_INLINE_GMalloc)
	{
		return GetAllocSizeExternal(Original);
	}

	SIZE_T Size = 0;
	return FMEMORY_INLINE_GMalloc->GetAllocationSize(Original, Size) ? Size : 0;
}

FMEMORY_INLINE_FUNCTION_DECORATOR SIZE_T FMemory::QuantizeSize(SIZE_T Count, uint32 Alignment)
{
	if (!FMEMORY_INLINE_GMalloc)
	{
		return Count;
	}
	return FMEMORY_INLINE_GMalloc->QuantizeSize(Count, Alignment);
}

