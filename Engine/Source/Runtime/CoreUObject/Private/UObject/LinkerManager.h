// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinkerManager.h: Unreal object linker manager
=============================================================================*/

#pragma once

#include "Linker.h"

class FLinkerManager : private FSelfRegisteringExec
{
public:

	static FLinkerManager& Get();

	FORCEINLINE void GetLoaders(TSet<FLinkerLoad*>& OutLoaders)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
		OutLoaders = ObjectLoaders;
	}

	FORCEINLINE void GetLoadersAndEmpty(TSet<FLinkerLoad*>& OutLoaders)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
		OutLoaders = ObjectLoaders;
		OutLoaders.Empty();
	}

	FORCEINLINE void AddLoader(FLinkerLoad* LinkerLoad)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
		ObjectLoaders.Add(LinkerLoad);
	}

	FORCEINLINE void RemoveLoader(FLinkerLoad* LinkerLoad)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
		ObjectLoaders.Remove(LinkerLoad);
	}

	FORCEINLINE void EmptyLoaders(FLinkerLoad* LinkerLoad)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
		ObjectLoaders.Empty();
	}

	FORCEINLINE void GetLoadersWithNewImports(TSet<FLinkerLoad*>& OutLoaders)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&LoadersWithNewImportsCritical);
#endif
		OutLoaders = LoadersWithNewImports;
	}

	FORCEINLINE void GetLoadersWithNewImportsAndEmpty(TSet<FLinkerLoad*>& OutLoaders)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&LoadersWithNewImportsCritical);
#endif
		OutLoaders = LoadersWithNewImports;
		OutLoaders.Empty();
	}

	FORCEINLINE void AddLoaderWithNewImports(FLinkerLoad* LinkerLoad)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&LoadersWithNewImportsCritical);
#endif
		LoadersWithNewImports.Add(LinkerLoad);
	}

	FORCEINLINE void RemoveLoaderWithNewImports(FLinkerLoad* LinkerLoad)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&LoadersWithNewImportsCritical);
#endif
		LoadersWithNewImports.Remove(LinkerLoad);
	}

	FORCEINLINE void EmptyLoadersWithNewImports(FLinkerLoad* LinkerLoad)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjectLoadersLock(&LoadersWithNewImportsCritical);
#endif
		LoadersWithNewImports.Empty();
	}

#if UE_BUILD_DEBUG
	FORCEINLINE TArray<FLinkerLoad*>& GetLiveLinkers()
	{
		return LiveLinkers;
	}
#endif

	// FSelfRegisteringExec interface
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** Empty the loaders */
	void ResetLoaders(UObject* InPkg);

	/**
	* Dissociates all linker import and forced export object references. This currently needs to
	* happen as the referred objects might be destroyed at any time.
	*/
	void DissociateImportsAndForcedExports();

private:

	/** Map of packages to their open linkers **/
	TSet<FLinkerLoad*> ObjectLoaders;
#if THREADSAFE_UOBJECTS
	FCriticalSection ObjectLoadersCritical;
#endif

	/** List of loaders that have new imports **/
	TSet<FLinkerLoad*> LoadersWithNewImports;
#if THREADSAFE_UOBJECTS
	FCriticalSection LoadersWithNewImportsCritical;
#endif
#if UE_BUILD_DEBUG
	/** List of all the existing linker loaders **/
	TArray<FLinkerLoad*> LiveLinkers;
#endif
	
};
