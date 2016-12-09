// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ImageComparer.h"

enum class EComparisonResultType
{
	Added,
	Missing,
	Compared
};

class FScreenComparisonModel
{
public:
	FScreenComparisonModel(EComparisonResultType InType)
		: Type(InType)
	{
	}

	EComparisonResultType GetType() const { return Type; }

	FString Folder;

	TOptional<FImageComparisonResult> ComparisonResult;

private:

	EComparisonResultType Type;
};
