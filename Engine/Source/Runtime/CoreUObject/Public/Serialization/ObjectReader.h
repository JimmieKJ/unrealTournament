// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Serialization/MemoryArchive.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "UObject/PropertyPortFlags.h"

class FAssetPtr;
class FLazyObjectPtr;
struct FStringAssetReference;

/**
 * UObject Memory Reader Archive. Reads from InBytes, writes to Obj.
 */
class FObjectReader : public FMemoryArchive
{
public:
	FObjectReader(UObject* Obj, TArray<uint8>& InBytes, bool bIgnoreClassRef = false, bool bIgnoreArchetypeRef = false)
		: Bytes(InBytes)
	{
		ArIsLoading = true;
		ArIsPersistent = false;
		ArIgnoreClassRef = bIgnoreClassRef;
		ArIgnoreArchetypeRef = bIgnoreArchetypeRef;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && !(ArPortFlags & PPF_DuplicateForPIE))
		{
			SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(Obj));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		Obj->Serialize(*this);
	}

	//~ Begin FArchive Interface

	int64 TotalSize()
	{
		return (int64)Bytes.Num();
	}

	void Serialize(void* Data, int64 Num)
	{
		if (Num && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Num <= TotalSize())
			{
				FMemory::Memcpy(Data, &Bytes[Offset], Num);
				Offset += Num;
			}
			else
			{
				ArIsError = true;
			}
		}
	}

	COREUOBJECT_API virtual FArchive& operator<<( class FName& N ) override;
	COREUOBJECT_API virtual FArchive& operator<<( class UObject*& Res ) override;
	COREUOBJECT_API virtual FArchive& operator<<( FLazyObjectPtr& LazyObjectPtr ) override;
	COREUOBJECT_API virtual FArchive& operator<<( FAssetPtr& AssetPtr ) override;
	COREUOBJECT_API virtual FArchive& operator<<(FStringAssetReference& AssetPtr) override;
	COREUOBJECT_API virtual FArchive& operator<<(struct FWeakObjectPtr& Value) override;
	COREUOBJECT_API virtual FString GetArchiveName() const override;
	//~ End FArchive Interface



protected:
	FObjectReader(TArray<uint8>& InBytes)
		: Bytes(InBytes)
	{
		ArIsLoading = true;
		ArIsPersistent = false;
		ArIgnoreClassRef = false;
		ArIgnoreArchetypeRef = false;
	}

	const TArray<uint8>& Bytes;
};
