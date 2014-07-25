// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#ifndef __UNREALTOURNAMENT_H__
#define __UNREALTOURNAMENT_H__

#include "Engine.h"
#include "ParticleDefinitions.h"

DECLARE_LOG_CATEGORY_EXTERN(UT, Log, All);

#define COLLISION_PROJECTILE ECC_GameTraceChannel1
#define COLLISION_TRACE_WEAPON ECC_GameTraceChannel2
#define COLLISION_PROJECTILE_SHOOTABLE ECC_GameTraceChannel3

#include "UTATypes.h"
#include "UTTeamInterface.h"
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

/** utility to find out if a particle system loops */
extern bool IsLoopingParticleSystem(const UParticleSystem* PSys);

#endif
