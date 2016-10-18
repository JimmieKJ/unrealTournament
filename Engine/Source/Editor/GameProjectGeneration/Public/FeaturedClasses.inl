// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/** Inline file to avoid introducing many UObject headers into the global namespace */

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Camera/PlayerCameraManager.h"


/** Get a list of all featured native class types */
TArray<FNewClassInfo> FFeaturedClasses::AllNativeClasses()
{
	TArray<FNewClassInfo> Array;
	Array.Add(FNewClassInfo(FNewClassInfo::EClassType::EmptyCpp));

	AddCommonActorClasses(Array);
	AddCommonComponentClasses(Array);

	AddExtraActorClasses(Array);

	Array.Add(FNewClassInfo(UBlueprintFunctionLibrary::StaticClass()));

	// Add the extra non-UObject classes
	Array.Add(FNewClassInfo(FNewClassInfo::EClassType::SlateWidget));
	Array.Add(FNewClassInfo(FNewClassInfo::EClassType::SlateWidgetStyle));
	Array.Add(FNewClassInfo(FNewClassInfo::EClassType::UInterface));
	return Array;
}

/** Get a list of all featured Actor class types */
TArray<FNewClassInfo> FFeaturedClasses::ActorClasses()
{
	TArray<FNewClassInfo> Array;
	AddCommonActorClasses(Array);
	AddExtraActorClasses(Array);
	return Array;
}

/** Get a list of all featured Component class types */
TArray<FNewClassInfo> FFeaturedClasses::ComponentClasses()
{
	TArray<FNewClassInfo> Array;
	AddCommonComponentClasses(Array);
	AddExtraComponentClasses(Array);
	return Array;
}

/** Append the featured Actor class types that are commonly used */
void FFeaturedClasses::AddCommonActorClasses(TArray<FNewClassInfo>& Array)
{
	Array.Add(FNewClassInfo(ACharacter::StaticClass()));
	Array.Add(FNewClassInfo(APawn::StaticClass()));
	Array.Add(FNewClassInfo(AActor::StaticClass()));
}

/** Append the featured Actor class types that are less commonly used */
void FFeaturedClasses::AddExtraActorClasses(TArray<FNewClassInfo>& Array)
{
	Array.Add(FNewClassInfo(APlayerCameraManager::StaticClass()));
	Array.Add(FNewClassInfo(APlayerController::StaticClass()));
	Array.Add(FNewClassInfo(AGameMode::StaticClass()));
	Array.Add(FNewClassInfo(AWorldSettings::StaticClass()));
	Array.Add(FNewClassInfo(AHUD::StaticClass()));
	Array.Add(FNewClassInfo(APlayerState::StaticClass()));
	Array.Add(FNewClassInfo(AGameState::StaticClass()));
}

/** Append the featured Component class types that are commonly used */
void FFeaturedClasses::AddCommonComponentClasses(TArray<FNewClassInfo>& Array)
{
	Array.Add(FNewClassInfo(UActorComponent::StaticClass()));
	Array.Add(FNewClassInfo(USceneComponent::StaticClass()));
}

/** Append the featured Component class types that are less commonly used */
void FFeaturedClasses::AddExtraComponentClasses(TArray<FNewClassInfo>& Array)
{
	Array.Add(FNewClassInfo(UStaticMeshComponent::StaticClass()));
	Array.Add(FNewClassInfo(USkeletalMeshComponent::StaticClass()));
	Array.Add(FNewClassInfo(UCameraComponent::StaticClass()));
	Array.Add(FNewClassInfo(UDirectionalLightComponent::StaticClass()));
}
