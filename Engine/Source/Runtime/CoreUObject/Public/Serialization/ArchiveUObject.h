// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Base FArchive for serializing UObjects. Supports FLazyObjectPtr and FAssetPtr serialization.
 */
class COREUOBJECT_API FArchiveUObject : public FArchive
{
public:

	using FArchive::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<< (class FLazyObjectPtr& Value) override;
	virtual FArchive& operator<< (class FAssetPtr& Value) override;
	virtual FArchive& operator<< (struct FStringAssetReference& Value) override;
	virtual FArchive& operator<< (struct FWeakObjectPtr& Value) override;
	//~ End FArchive Interface
};
