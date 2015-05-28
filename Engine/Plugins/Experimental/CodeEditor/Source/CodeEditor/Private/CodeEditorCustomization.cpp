// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CodeEditorPrivatePCH.h"
#include "CodeEditorCustomization.h"

UCodeEditorCustomization::UCodeEditorCustomization(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FControlCustomization& UCodeEditorCustomization::GetControl(const FName& ControlCustomizationName)
{
	static FControlCustomization Default;

	return Default;
}

const FTextCustomization& UCodeEditorCustomization::GetText(const FName& TextCustomizationName)
{
	static FTextCustomization Default;

	return Default;
}