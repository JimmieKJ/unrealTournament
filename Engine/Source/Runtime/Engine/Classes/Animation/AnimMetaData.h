// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/* 
 * Base Class for UAnimMetaData that can be implemented for each game needs
 * this data will be saved to animation asset as well as montage sections, and you can query that data and decide what to do
 *
 * Refer : GetMetaData/GetSectionMetaData
 */

#pragma once
#include "AnimMetaData.generated.h"

UCLASS(Blueprintable, abstract, const, MinimalAPI, editinlinenew, hidecategories=Object, collapsecategories)
class UAnimMetaData : public UObject
{
	GENERATED_UCLASS_BODY()
};



