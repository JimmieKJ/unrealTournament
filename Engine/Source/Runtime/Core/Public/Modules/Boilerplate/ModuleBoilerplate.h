// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Boilerplate that is included once for each module, even in monolithic builds
#if !defined(PER_MODULE_BOILERPLATE_ANYLINK)
#define PER_MODULE_BOILERPLATE_ANYLINK(ModuleImplClass, ModuleName)
#endif

/**
 * Override new + delete operators (and array versions) in every module.
 * This prevents the possibility of mismatched new/delete calls such as a new[] that
 * uses Unreal's allocator and a delete[] that uses the system allocator.
 */
#if USING_CODE_ANALYSIS
	#define OPERATOR_NEW_MSVC_PRAGMA MSVC_PRAGMA( warning( suppress : 28251 ) )	//	warning C28251: Inconsistent annotation for 'new': this instance has no annotations
#else
	#define OPERATOR_NEW_MSVC_PRAGMA
#endif

#define REPLACEMENT_OPERATOR_NEW_AND_DELETE \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size                        ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size                        ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ); } \
	void operator delete  ( void* Ptr )                                                 OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr )                                                 OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr, const std::nothrow_t& )                          OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr, const std::nothrow_t& )                          OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); }

// in DLL builds, these are done per-module, otherwise we just need one in the application
// visual studio cannot find cross dll data for visualizers, so these provide access
#define PER_MODULE_BOILERPLATE \
	TArray<FNameEntry const*>* GFNameTableForDebuggerVisualizers = FName::GetNameTableForDebuggerVisualizers_ST(); \
	FNameEntry*** GFNameTableForDebuggerVisualizers_MT = FName::GetNameTableForDebuggerVisualizers_MT(); \
	int32*** GSerialNumberBlocksForDebugVisualizers = FCoreDelegates::GetSerialNumberBlocksForDebugVisualizersDelegate().IsBound() ? FCoreDelegates::GetSerialNumberBlocksForDebugVisualizersDelegate().Execute() : NULL; \
	UObjectBase*** GObjectArrayForDebugVisualizers = FCoreDelegates::GetObjectArrayForDebugVisualizersDelegate().IsBound() ? FCoreDelegates::GetObjectArrayForDebugVisualizersDelegate().Execute() : NULL; \
	bool GFNameDebuggerVisualizersIsUE3=false; \
	REPLACEMENT_OPERATOR_NEW_AND_DELETE
