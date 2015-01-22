// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "FbxLibs.h"
#include "FbxImporter.h"
#include "FbxExporter.h"

// Loading the DLL's for FBx is currently only needed for VC11 compiled software
#if PLATFORM_WINDOWS && _MSC_VER >= 1700
	HMODULE	FBxHandle = 0;
#endif

//-------------------------------------------------------------------------
// Memory management callback functions used by the FBX SDK
//-------------------------------------------------------------------------
void* MyMalloc(size_t pSize)       
{
	return FMemory::Malloc(pSize);
}

void* MyCalloc(size_t pCount,size_t pSize)
{
	void* Alloc = FMemory::Malloc(pCount*pSize);
	return FMemory::Memzero(Alloc, pCount*pSize);
}

void* MyRealloc(void* pData, size_t pSize)
{
	return FMemory::Realloc(pData, pSize);
}

void  MyFree(void* pData)
{
	FMemory::Free(pData);
}


void LoadFBxLibraries()
{
#define FBX_DELAY_LOAD 0
#if PLATFORM_WINDOWS && _MSC_VER == 1700 && FBX_DELAY_LOAD
	#if PLATFORM_64BITS
		FString RootFBxPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/FBx/Win64/");
		FBxHandle = LoadLibraryW(*(RootFBxPath + "libfbxsdk.dll"));
	#else
		static_assert(false, TEXT("FBX importing currently not supported in 32 bit versions"));
	#endif // PLATFORM_64BITS
#endif // PLATFORM_WINDOWS && _MSC_VER == 1700

	// Specify global memory handler callbacks to be used by the FBX SDK
	FbxSetMallocHandler(	&MyMalloc);
	FbxSetCallocHandler(	&MyCalloc);
	FbxSetReallocHandler(	&MyRealloc);
	FbxSetFreeHandler(		&MyFree);

}

void UnloadFBxLibraries()
{
	UnFbx::FFbxImporter::DeleteInstance();
	UnFbx::FFbxExporter::DeleteInstance();

	// Hack: After we have freed our fbx sdk instance we need to set back to the default fbx memory handlers. 
	// This is required because there are some allocations made in the FBX dllmain before it is possible to set up our custom allocators
	// If this is not done, memory created by one allocator will be freed by another
	FbxSetMallocHandler(	FbxGetDefaultMallocHandler() );
	FbxSetCallocHandler(	FbxGetDefaultCallocHandler() );
	FbxSetReallocHandler(	FbxGetDefaultReallocHandler() );
	FbxSetFreeHandler(		FbxGetDefaultFreeHandler() );

#if PLATFORM_WINDOWS && _MSC_VER == 1700
	FreeLibrary(FBxHandle);

	FBxHandle = 0;
#endif
}