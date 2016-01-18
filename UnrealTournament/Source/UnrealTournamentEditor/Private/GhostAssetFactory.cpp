// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournamentEditor.h"
#include "GhostAssetFactory.h"
#include "UTGhostData.h"

#define LOCTEXT_NAMESPACE "GhostAssetFactory"

UGhostAssetFactory::UGhostAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = false;
	SupportedClass = UUTGhostData::StaticClass();
}


FText UGhostAssetFactory::GetDisplayName() const
{
	return LOCTEXT("GhostAssetFactoryFactoryDescription", "Ghost Recording");
}


bool UGhostAssetFactory::ConfigureProperties()
{
	return true;
}

UObject* UGhostAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UUTGhostData>(InParent, Name, Flags);
}

#undef LOCTEXT_NAMESPACE


