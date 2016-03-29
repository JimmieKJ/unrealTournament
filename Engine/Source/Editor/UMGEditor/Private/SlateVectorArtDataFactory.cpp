// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "SlateVectorArtData.h"
#include "SlateVectorArtDataFactory.h"

USlateVectorArtDataFactory::USlateVectorArtDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USlateVectorArtData::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* USlateVectorArtDataFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class == USlateVectorArtData::StaticClass());

	USlateVectorArtData* SlateVectorArtData = NewObject<USlateVectorArtData>(InParent, Name, Flags);

	return SlateVectorArtData;
}

