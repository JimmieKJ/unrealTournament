// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncPackage.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/Guid.h"

struct FAsyncPackageDesc
{
	/** Handle for the caller */
	int32 RequestID;
	/** Name of the UPackage to create. */
	FName Name;
	/** Name of the package to load. */
	FName NameToLoad;
	/** GUID of the package to load, or the zeroed invalid GUID for "don't care" */
	FGuid Guid;
	/** Delegate called on completion of loading */
	FLoadPackageAsyncDelegate PackageLoadedDelegate;
	/** The flags that should be applied to the package */
	EPackageFlags PackageFlags;
	/** Package loading priority. Higher number is higher priority. */
	TAsyncLoadPriority Priority;
	/** PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32 PIEInstanceID;


	FAsyncPackageDesc(int32 InRequestID, const FName& InName, FName InPackageToLoadFrom = NAME_None, const FGuid& InGuid = FGuid(), FLoadPackageAsyncDelegate InCompletionDelegate = FLoadPackageAsyncDelegate(), EPackageFlags InPackageFlags = PKG_None, int32 InPIEInstanceID = INDEX_NONE, TAsyncLoadPriority InPriority = 0)
		: RequestID(InRequestID)
		, Name(InName)
		, NameToLoad(InPackageToLoadFrom)
		, Guid(InGuid)
		, PackageLoadedDelegate(InCompletionDelegate)
		, PackageFlags(InPackageFlags)
		, Priority(InPriority)
		, PIEInstanceID(InPIEInstanceID)
	{
		if (NameToLoad == NAME_None)
		{
			NameToLoad = Name;
		}
	}
};
