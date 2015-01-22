// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "TranslationEditorPrivatePCH.h"
#include "TranslationUnit.h"

UTranslationUnit::UTranslationUnit( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

}

void UTranslationUnit::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	TranslationUnitPropertyChangedEvent.Broadcast(Name);
}