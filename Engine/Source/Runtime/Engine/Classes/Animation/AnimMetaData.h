// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/* 
 * Base Class for UAnimMetaData that can be implemented for each game needs
 * this data will be saved to animation asset as well as montage sections, and you can query that data and decide what to do
 *
 * Refer : GetMetaData/GetSectionMetaData
 */

#pragma once
#include "AnimMetaData.generated.h"

UCLASS(Blueprintable, abstract, const, editinlinenew, hidecategories=Object, collapsecategories)
class ENGINE_API UAnimMetaData : public UObject
{
	GENERATED_UCLASS_BODY()
};



