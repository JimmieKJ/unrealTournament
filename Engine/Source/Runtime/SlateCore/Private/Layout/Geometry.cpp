// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "LayoutGeometry.h"

FGeometry::FGeometry() 
	: Size(0.0f, 0.0f)
	, Scale(1.0f)
	, AbsolutePosition(0.0f, 0.0f)
{
}

FGeometry& FGeometry::operator=(const FGeometry& RHS)
{
	// HACK to allow us to make FGeometry public members immutable to catch misuse.
	if (this != &RHS)
	{
		FMemory::Memcpy(*this, RHS);
	}
	return *this;
}

FString FGeometry::ToString() const
{
	return FString::Printf(TEXT("[Abs=%s, Scale=%.2f, Size=%s]"), *AbsolutePosition.ToString(), Scale, *Size.ToString());
}
