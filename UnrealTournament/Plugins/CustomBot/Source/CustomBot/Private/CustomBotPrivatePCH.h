// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//----------------------------------------------------------------------//
// Engine headers
//----------------------------------------------------------------------//

#include "Core.h"
#include "CoreUObject.h"
#include "InputCore.h"
#include "EngineDefines.h"
#include "EngineSettings.h"
#include "EngineStats.h"
#include "EngineLogs.h"
#include "EngineGlobals.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

#include "SlateBasics.h"
#include "Engine/NetDriver.h"

#include "UnrealEngine.h"

//----------------------------------------------------------------------//
// Required UT
//----------------------------------------------------------------------//

class UAudioComponent;
class USphereComponent;


#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/LocalMessage.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "Engine/Canvas.h"
#include "Engine/LocalPlayer.h"
#include "Curves/CurveLinearColor.h"
#include "AssetData.h"

class SUWindowsDesktop;
class AUTPickup;

#include "UTTeamInterface.h"
#include "UTCharacter.h"
#include "UTWeapon.h"
#include "UTLocalPlayer.h"
#include "UTBaseGameMode.h"
#include "UTBot.h"
#include "UTGameMode.h"

#include "UTCustomBot.h"
