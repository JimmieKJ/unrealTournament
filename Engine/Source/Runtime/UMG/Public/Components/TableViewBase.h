// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "TableViewBase.generated.h"

/** The base class for all wrapped table views */
UCLASS(Abstract)
class UMG_API UTableViewBase : public UWidget
{
	GENERATED_UCLASS_BODY()

	/** Delegate for constructing a UWidget based on a UObject */
	DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(UWidget*, FOnGenerateRowUObject, UObject*, Item);
};
