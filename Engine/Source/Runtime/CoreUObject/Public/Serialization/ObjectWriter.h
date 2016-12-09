// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Serialization/MemoryWriter.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "UObject/PropertyPortFlags.h"

class FAssetPtr;
class FLazyObjectPtr;
struct FStringAssetReference;

/**
 * UObject Memory Writer Archive.
 */
class FObjectWriter : public FMemoryWriter
{

public:
	FObjectWriter(UObject* Obj, TArray<uint8>& InBytes, bool bIgnoreClassRef = false, bool bIgnoreArchetypeRef = false, bool bDoDelta = true, uint32 AdditionalPortFlags = 0)
		: FMemoryWriter(InBytes)
	{
		ArIgnoreClassRef = bIgnoreClassRef;
		ArIgnoreArchetypeRef = bIgnoreArchetypeRef;
		ArNoDelta = !bDoDelta;
		ArPortFlags |= AdditionalPortFlags;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && !(ArPortFlags & PPF_DuplicateForPIE))
		{
			SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(Obj));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		Obj->Serialize(*this);
	}

	//~ Begin FArchive Interface
	COREUOBJECT_API virtual FArchive& operator<<( class FName& N ) override;
	COREUOBJECT_API virtual FArchive& operator<<( class UObject*& Res ) override;
	COREUOBJECT_API virtual FArchive& operator<<( FLazyObjectPtr& LazyObjectPtr ) override;
	COREUOBJECT_API virtual FArchive& operator<<( FAssetPtr& AssetPtr ) override;
	COREUOBJECT_API virtual FArchive& operator<<(FStringAssetReference& AssetPtr) override;
	COREUOBJECT_API virtual FArchive& operator<<(struct FWeakObjectPtr& Value) override;
	COREUOBJECT_API virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

protected:
	FObjectWriter(TArray<uint8>& InBytes)
		: FMemoryWriter(InBytes)
	{
		ArIgnoreClassRef = false;
		ArIgnoreArchetypeRef = false;
	}
};
