// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GatherableTextData.h"

class FPropertyLocalizationDataGatherer
{
public:
	COREUOBJECT_API FPropertyLocalizationDataGatherer(TArray<FGatherableTextData>& InGatherableTextDataArray)
		: GatherableTextDataArray(InGatherableTextDataArray)
	{
	}

	COREUOBJECT_API void GatherLocalizationDataFromPropertiesOfDataStructure(UStruct* const Structure, void* const Data, const bool ForceIsEditorOnly = false);
	COREUOBJECT_API void GatherLocalizationDataFromChildTextProperies(const FString& PathToParent, UProperty* const Property, void* const ValueAddress, const bool ForceIsEditorOnly = false);
	COREUOBJECT_API void GatherLocalizationDataFromTextProperty(const FString& PathToParent, UTextProperty* const TextProperty, void* const ValueAddress, const bool ForceIsEditorOnly = false);

private:
	TArray<FGatherableTextData>& GatherableTextDataArray;
	TArray<UObject*> ProcessedObjects;
};