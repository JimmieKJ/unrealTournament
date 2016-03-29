// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEdTypes.h"

#include "DEditorTextureParameterValue.generated.h"

UCLASS(hidecategories=Object, collapsecategories)
class UNREALED_API UDEditorTextureParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorTextureParameterValue)
	class UTexture* ParameterValue;

};

