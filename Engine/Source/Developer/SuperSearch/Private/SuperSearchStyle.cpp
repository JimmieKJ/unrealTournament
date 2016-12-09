// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SuperSearchStyle.h"

const FName FSuperSearchStyle::TypeName(TEXT("FSuperSearchStyle"));

FSuperSearchStyle::FSuperSearchStyle()
	: ComboBoxStyle()
	, ForegroundColor()
	, TextBlockStyle()
	, SearchBoxStyle()
	, SearchEnginePlacement(ESuperSearchEnginePlacement::Left)
{ }

void FSuperSearchStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	TextBlockStyle.GetResources(OutBrushes);
	ComboBoxStyle.GetResources(OutBrushes);
	SearchBoxStyle.GetResources(OutBrushes);
}

const FSuperSearchStyle& FSuperSearchStyle::GetDefault()
{
	static FSuperSearchStyle Default;
	return Default;
}
