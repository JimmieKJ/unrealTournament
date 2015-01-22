// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimMontage.cpp: Montage classes that contains slots
=============================================================================*/ 

#include "UnrealEd.h"



UEditorCompositeSection::UEditorCompositeSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SectionIndex = INDEX_NONE;
}

void UEditorCompositeSection::InitSection(int SectionIndexIn)
{
	SectionIndex = SectionIndexIn;
	if(UAnimMontage* Montage = Cast<UAnimMontage>(AnimObject))
	{
		if(Montage->CompositeSections.IsValidIndex(SectionIndex))
		{
			CompositeSection = Montage->CompositeSections[SectionIndex];
		}
	}
}
bool UEditorCompositeSection::ApplyChangesToMontage()
{
	if(UAnimMontage* Montage = Cast<UAnimMontage>(AnimObject))
	{
		if(Montage->CompositeSections.IsValidIndex(SectionIndex))
		{
			CompositeSection.OnChanged(CompositeSection.GetTime());
			Montage->CompositeSections[SectionIndex] = CompositeSection;
			return true;
		}
	}

	return false;
}
