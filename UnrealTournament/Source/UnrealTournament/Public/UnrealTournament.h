// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#ifndef __UNREALTOURNAMENT_H__
#define __UNREALTOURNAMENT_H__

#include "Engine.h"
#include "ParticleDefinitions.h"

DECLARE_LOG_CATEGORY_EXTERN(UT, Log, All);

#define COLLISION_PROJECTILE ECC_GameTraceChannel1
#define COLLISION_TRACE_WEAPON ECC_GameTraceChannel2
#define COLLISION_PROJECTILE_SHOOTABLE ECC_GameTraceChannel3
#define COLLISION_TELEPORTING_OBJECT ECC_GameTraceChannel4

#include "UTATypes.h"
#include "UTTeamInterface.h"
#include "UTResetInterface.h"
#include "UTGameplayStatics.h"
#include "UTGameUserSettings.h"
#include "UTLocalPlayer.h"
#include "UTLocalMessage.h"
#include "UTPlayerState.h"
#include "UTTeamInfo.h"
#include "UTGameState.h"
#include "UTHUD.h"
#include "UTHUDWidget.h"
#include "UTDamageType.h"
#include "UTCharacter.h"
#include "UTPlayerController.h"
#include "UTProjectile.h"
#include "UTInventory.h"
#include "UTWeapon.h"
#include "UTGameSession.h"
#include "UTGameObjective.h"
#include "UTCarriedObjectMessage.h"
#include "UTCarriedObject.h"
#include "UTGameMode.h"
#include "UTTeamGameMode.h"

/** utility to find out if a particle system loops */
extern bool IsLoopingParticleSystem(const UParticleSystem* PSys);

/** utility to detach and unregister a component and all its children */
extern void UnregisterComponentTree(USceneComponent* Comp);

/** workaround for FCanvasIcon not having a constructor you can pass in the values to */
FORCEINLINE FCanvasIcon MakeCanvasIcon(UTexture* Tex, float InU, float InV, float InUL, float InVL)
{
	FCanvasIcon Result;
	Result.Texture = Tex;
	Result.U = InU;
	Result.V = InV;
	Result.UL = InUL;
	Result.VL = InVL;
	return Result;
}

#endif
