// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPluginPrivatePCH.h"
#include "GameplayDebuggerModuleSettings.h"
#include "GameplayDebuggerBlueprintBaseObject.h"

UGameplayDebuggerBlueprintBaseObject::UGameplayDebuggerBlueprintBaseObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GameplayDebuggerCategoryName(TEXT("Basic"))
{
	// Empty
}

FString UGameplayDebuggerBlueprintBaseObject::GetCategoryName() const
{
	return GameplayDebuggerCategoryName;
};

void UGameplayDebuggerBlueprintBaseObject::DrawCollectedData(APlayerController* PlayerController, AActor* SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	DrawCollectedDataEvent(PlayerController, SelectedActor);
#endif
}

#if WITH_EDITOR
void UGameplayDebuggerBlueprintBaseObject::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if ENABLED_GAMEPLAY_DEBUGGER
	UGameplayDebuggerModuleSettings* Settings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
	Settings->UpdateCategories();
#endif
}
#endif
