// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DiffUtils.h"

class KISMET_API FSCSDiff
{
public:
	FSCSDiff(const class UBlueprint* InBlueprint);

	void HighlightProperty(FName VarName, FPropertySoftPath Property);
	TSharedRef< SWidget > TreeWidget();

	TArray< FSCSResolvedIdentifier > GetDisplayedHierarchy() const;
private:
	TSharedPtr< class SWidget > ContainerWidget;
	TSharedPtr< class SSCSEditor > SCSEditor;
};
