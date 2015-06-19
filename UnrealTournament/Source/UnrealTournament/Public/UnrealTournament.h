// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __UNREALTOURNAMENT_H__
#define __UNREALTOURNAMENT_H__

#include "Engine.h"
#include "ParticleDefinitions.h"

DECLARE_LOG_CATEGORY_EXTERN(UT, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(UTNet, Log, All);

#define COLLISION_PROJECTILE ECC_GameTraceChannel1
#define COLLISION_TRACE_WEAPON ECC_GameTraceChannel2
#define COLLISION_PROJECTILE_SHOOTABLE ECC_GameTraceChannel3
#define COLLISION_TELEPORTING_OBJECT ECC_GameTraceChannel4
#define COLLISION_PAWNOVERLAP ECC_GameTraceChannel5
#define COLLISION_TRACE_WEAPONNOCHARACTER ECC_GameTraceChannel6
#define COLLISION_TRANSDISK ECC_GameTraceChannel7

#include "UTATypes.h"
#include "UTTeamInterface.h"
#include "UTResetInterface.h"
#include "UTGameplayStatics.h"
#include "UTGameUserSettings.h"
#include "UTLocalPlayer.h"
#include "UTBaseGameMode.h"
#include "UTLocalMessage.h"
#include "UTPlayerState.h"
#include "UTCharacter.h"
#include "UTBot.h"
#include "UTTeamInfo.h"
#include "UTGameState.h"
#include "UTHUD.h"
#include "UTHUDWidget.h"
#include "UTDamageType.h"
#include "UTBasePlayerController.h"
#include "UTPlayerController.h"
#include "UTProjectile.h"
#include "UTInventory.h"
#include "UTWeapon.h"
#include "UTPickup.h"
#include "UTGameSession.h"
#include "UTGameObjective.h"
#include "UTCarriedObjectMessage.h"
#include "UTCarriedObject.h"
#include "UTGameMode.h"
#include "UTTeamGameMode.h"
#include "Stat.h"
#include "StatManager.h"
#include "OnlineEntitlementsInterface.h"

/** handy response params for world-only checks */
extern FCollisionResponseParams WorldResponseParams;

/** utility to find out if a particle system loops */
extern bool IsLoopingParticleSystem(const UParticleSystem* PSys);

/** utility to detach and unregister a component and all its children */
extern void UnregisterComponentTree(USceneComponent* Comp);

/** utility to retrieve the highest priority physics volume overlapping the passed in primitive */
extern APhysicsVolume* FindPhysicsVolume(UWorld* World, const FVector& TestLoc, const FCollisionShape& Shape);
/** get GravityZ at the given location of the given world */
extern float GetLocationGravityZ(UWorld* World, const FVector& TestLoc, const FCollisionShape& Shape);

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

/** returns entitlement ID required for the given asset, if any */
extern UNREALTOURNAMENT_API FString GetRequiredEntitlementFromAsset(const FAssetData& Asset);
extern UNREALTOURNAMENT_API FString GetRequiredEntitlementFromObj(UObject* Asset);
extern UNREALTOURNAMENT_API FString GetRequiredEntitlementFromPackageName(FName PackageName);

/** returns whether any locally logged in player (via OSS) has the specified entitlement */
extern UNREALTOURNAMENT_API bool LocallyHasEntitlement(const FString& Entitlement);

/** returns asset data for all assets of the specified class 
 * do not use for Blueprints as you can only query for all blueprints period; use GetAllBlueprintAssetData() to query the blueprint's underlying class
 * if bRequireEntitlements is set, assets on disk for which no local player has the required entitlement will not be returned
 *
 * WARNING: the asset registry does a class name search not a path search so the returned assets may not actually be the class you want in the case of name conflicts
 *			if you load any returned assets always verify that you got back what you were expecting!
 */
extern UNREALTOURNAMENT_API void GetAllAssetData(UClass* BaseClass, TArray<FAssetData>& AssetList, bool bRequireEntitlements = true);
/** returns asset data for all blueprints of the specified base class in the asset registry
 * this does not actually load assets, so it's fast in a cooked build, although the first time it is run
 * in an uncooked build it will hitch while scanning the asset registry
 * if bRequireEntitlements is set, assets on disk for which no local player has the required entitlement will not be returned
 */
extern UNREALTOURNAMENT_API void GetAllBlueprintAssetData(UClass* BaseClass, TArray<FAssetData>& AssetList, bool bRequireEntitlements = true);

/** timer manipulation for UFUNCTIONs that doesn't require a timer handle */
extern UNREALTOURNAMENT_API void SetTimerUFunc(UObject* Obj, FName FuncName, float Time, bool bLooping = false);
extern UNREALTOURNAMENT_API bool IsTimerActiveUFunc(UObject* Obj, FName FuncName);
extern UNREALTOURNAMENT_API void ClearTimerUFunc(UObject* Obj, FName FuncName);

#endif
