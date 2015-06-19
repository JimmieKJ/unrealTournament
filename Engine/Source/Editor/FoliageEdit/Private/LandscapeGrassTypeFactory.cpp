// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "LandscapeGrassTypeFactory.h"
#include "IAssetTypeActions.h"
#include "LandscapeGrassType.h"

ULandscapeGrassTypeFactory::ULandscapeGrassTypeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = ULandscapeGrassType::StaticClass();
}

UObject* ULandscapeGrassTypeFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto NewGrassType = NewObject<ULandscapeGrassType>(InParent, Class, Name, Flags | RF_Transactional);

	return NewGrassType;
}

uint32 ULandscapeGrassTypeFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}