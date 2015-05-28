// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "EditorClassUtils.h"

#define LOCTEXT_NAMESPACE "WorldFactory"

UWorldFactory::UWorldFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = UEditorEngine::IsUsingWorldAssets();
	SupportedClass = UWorld::StaticClass();
	WorldType = EWorldType::Inactive;
	bInformEngineOfWorld = false;
	FeatureLevel = ERHIFeatureLevel::Num;
}

bool UWorldFactory::ConfigureProperties()
{
	return true;
}

UObject* UWorldFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	// Create a new world.
	const bool bAddToRoot = false;
	UWorld* NewWorld = UWorld::CreateWorld(WorldType, bInformEngineOfWorld, Name, Cast<UPackage>(InParent), bAddToRoot, FeatureLevel);
	NewWorld->SetFlags(Flags);
	NewWorld->ThumbnailInfo = NewObject<UWorldThumbnailInfo>(NewWorld);

	return NewWorld;
}

FText UWorldFactory::GetToolTip() const
{
	return ULevel::StaticClass()->GetToolTipText();
}

FString UWorldFactory::GetToolTipDocumentationPage() const
{
	return FEditorClassUtils::GetDocumentationPage(ULevel::StaticClass());
}

FString UWorldFactory::GetToolTipDocumentationExcerpt() const
{
	return FEditorClassUtils::GetDocumentationExcerpt(ULevel::StaticClass());
}

#undef LOCTEXT_NAMESPACE
