// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

#include "JsonObjectWrapper.generated.h"

class FJsonObject;

/** UStruct that holds a JsonObject, can be used by structs passed to JsonObjectConverter to pass through JsonObjects directly */
USTRUCT()
struct JSONUTILITIES_API FJsonObjectWrapper
{
	GENERATED_USTRUCT_BODY()
public:

	UPROPERTY(VisibleAnywhere, Category = "JSON")
	FString JsonString;

	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);
	bool ExportTextItem(FString& ValueStr, FJsonObjectWrapper const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	void PostSerialize(const FArchive& Ar);

	TSharedPtr<FJsonObject> JsonObject;
};

template<>
struct TStructOpsTypeTraits<FJsonObjectWrapper> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithImportTextItem = true,
		WithExportTextItem = true,
		WithPostSerialize = true,
	};
};
UCLASS()
class UJsonUtilitiesDummyObject : public UObject
{
	GENERATED_BODY()
};
