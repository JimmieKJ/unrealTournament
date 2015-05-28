// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#pragma once

class COREUOBJECT_API FUObjectThreadContext : public TThreadSingleton<FUObjectThreadContext>
{
	friend TThreadSingleton<FUObjectThreadContext>;

	FUObjectThreadContext();

public:
	
	/** Imports for EndLoad optimization.	*/
	int32 ImportCount;
	/** Forced exports for EndLoad optimization. */
	int32 ForcedExportCount;
	/** Count for BeginLoad multiple loads.	*/
	int32 ObjBeginLoadCount;
	/** Objects that might need preloading. */
	TArray<UObject*> ObjLoaded;
	/** List of linkers that we want to close the loaders for (to free file handles) - needs to be delayed until EndLoad is called with GObjBeginLoadCount of 0 */
	TArray<FLinkerLoad*> DelayedLinkerClosePackages;
	/** true when we are routing ConditionalPostLoad/PostLoad to objects										*/
	bool IsRoutingPostLoad;
	/* Global flag so that FObjectFinders know if they are called from inside the UObject constructors or not. */
	int32 IsInConstructor;
	/* Object that is currently being constructed with ObjectInitializer */
	UObject* ConstructedObject;
	/** Points to the main UObject currently being serialized */
	UObject* SerializedObject;
	/** Points to the main PackageLinker currently being serialized (Defined in Linker.cpp) */
	FLinkerLoad* SerializedPackageLinker;
	/** The main Import Index currently being used for serialization by CreateImports() (Defined in Linker.cpp) */
	int32 SerializedImportIndex;
	/** Points to the main Linker currently being used for serialization by CreateImports() (Defined in Linker.cpp) */
	FLinkerLoad* SerializedImportLinker;
	/** The most recently used export Index for serialization by CreateExport() */
	int32 SerializedExportIndex;
	/** Points to the most recently used Linker for serialization by CreateExport() */
	FLinkerLoad* SerializedExportLinker;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Stack to ensure that PostInitProperties is routed through Super:: calls. **/
	TArray<UObject*> PostInitPropertiesCheck;
	/** Used to verify that the Super::PostLoad chain is intact.			*/
	TArray<UObject*, TInlineAllocator<16> > DebugPostLoad;
#endif
};