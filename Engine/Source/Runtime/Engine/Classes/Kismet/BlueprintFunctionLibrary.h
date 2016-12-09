// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/StringAssetReference.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "BlueprintFunctionLibrary.generated.h"

// This class is a base class for any function libraries exposed to blueprints.
// Methods in subclasses are expected to be static, and no methods should be added to this base class.
UCLASS(Abstract, MinimalAPI)
class UBlueprintFunctionLibrary : public UObject
{
	GENERATED_UCLASS_BODY()

	// UObject interface
	ENGINE_API virtual int32 GetFunctionCallspace(UFunction* Function, void* Parms, FFrame* Stack) override;
	// End of UObject interface

	UFUNCTION(BlueprintPure, Category="StringAssetReference", CustomThunk, meta = (Keywords = "construct build", NativeMakeFunc))
	static FStringAssetReference MakeStringAssetReference(const FString& AssetLongPathname);

	/**
	 * Custom MakeStringAssetReference blueprint node.
	 */
	static FStringAssetReference Generic_MakeStringAssetReference(FFrame& StackFrame, const FString& AssetLongPathname);

	DECLARE_FUNCTION(execMakeStringAssetReference)
	{
		P_GET_PROPERTY(UStrProperty, Z_Param_AssetLongPathname);
		P_FINISH;

		*(FStringAssetReference*)Z_Param__Result = UBlueprintFunctionLibrary::Generic_MakeStringAssetReference(Stack, Z_Param_AssetLongPathname);
	}

protected:

	//bool	CheckAuthority(UObject* 

};
