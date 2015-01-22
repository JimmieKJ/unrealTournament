// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*====================================================================================
	ArchiveUObjectBase.h: Implements the FArchiveUObject class for serializing UObjects
======================================================================================*/

#pragma once

/**
 * Base FArchive for serializing UObjects. Supports FLazyObjectPtr and FAssetPtr serialization.
 */
class COREUOBJECT_API FArchiveUObject : public FArchive
{
public:

	// Begin FArchive Interface
	virtual FArchive& operator<< (class FLazyObjectPtr& Value) override;
	virtual FArchive& operator<< (class FAssetPtr& Value) override;
	virtual FArchive& operator<< (struct FStringAssetReference& Value) override;
	// End FArchive Interface
};
