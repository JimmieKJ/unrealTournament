// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"
#include "SoundDefinitions.h"
#include "PlatformFeatures.h"
#include "LatentActions.h"
#include "IInputInterface.h"
#include "SlateBasics.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/Character.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/DamageType.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/DecalComponent.h"
#include "LandscapeProxy.h"
#include "MessageLog.h"

#define LOCTEXT_NAMESPACE "GameplayStatics"

static const int UE4_SAVEGAME_FILE_TYPE_TAG = 0x53415647;		// "sAvG"
static const int UE4_SAVEGAME_FILE_VERSION = 1;

//////////////////////////////////////////////////////////////////////////
// UGameplayStatics

UGameplayStatics::UGameplayStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGameInstance* UGameplayStatics::GetGameInstance(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->GetGameInstance() : nullptr;
}

APlayerController* UGameplayStatics::GetPlayerController(UObject* WorldContextObject, int32 PlayerIndex ) 
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		uint32 Index = 0;
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (Index == PlayerIndex)
			{
				return PlayerController;
			}
			Index++;
		}
	}
	return nullptr;
}

ACharacter* UGameplayStatics::GetPlayerCharacter(UObject* WorldContextObject, int32 PlayerIndex)
{
	APlayerController* PC = GetPlayerController(WorldContextObject, PlayerIndex);
	return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
}

APawn* UGameplayStatics::GetPlayerPawn(UObject* WorldContextObject, int32 PlayerIndex)
{
	APlayerController* PC = GetPlayerController(WorldContextObject, PlayerIndex);
	return PC ? PC->GetPawnOrSpectator() : nullptr;
}

APlayerCameraManager* UGameplayStatics::GetPlayerCameraManager(UObject* WorldContextObject, int32 PlayerIndex)
{
	APlayerController* const PC = GetPlayerController(WorldContextObject, PlayerIndex);
	return PC ? PC->PlayerCameraManager : nullptr;
}

APlayerController* UGameplayStatics::CreatePlayer(UObject* WorldContextObject, int32 ControllerId, bool bSpawnPawn)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	FString Error;

	ULocalPlayer* LocalPlayer = World ? World->GetGameInstance()->CreateLocalPlayer(ControllerId, Error, bSpawnPawn) : NULL;

	if (Error.Len() > 0)
	{
		UE_LOG(LogPlayerManagement, Error, TEXT("Failed to Create Player: %s"), *Error);
	}

	return (LocalPlayer ? LocalPlayer->PlayerController : nullptr);
}

void UGameplayStatics::RemovePlayer(APlayerController* PlayerController, bool bDestroyPawn)
{
	if (PlayerController)
	{
		if (UWorld* World = PlayerController->GetWorld())
		{
			if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
			{
				APawn* PlayerPawn = (bDestroyPawn ? PlayerController->GetPawn() : nullptr);
				if (World->GetGameInstance()->RemoveLocalPlayer(LocalPlayer) && PlayerPawn)
				{
					PlayerPawn->Destroy();
				}
			}
		}
	}
}

AGameMode* UGameplayStatics::GetGameMode(UObject* WorldContextObject)
{
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->GetAuthGameMode() : NULL;
}

AGameState* UGameplayStatics::GetGameState(UObject* WorldContextObject)
{
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->GameState : nullptr;
}

class UClass* UGameplayStatics::GetObjectClass(const UObject* Object)
{
	return Object ? Object->GetClass() : nullptr;
}

float UGameplayStatics::GetGlobalTimeDilation(UObject* WorldContextObject)
{
	UWorld* const World = GEngine->GetWorldFromContextObject( WorldContextObject );
	return World ? World->GetWorldSettings()->TimeDilation : 1.f;
}

void UGameplayStatics::SetGlobalTimeDilation(UObject* WorldContextObject, float TimeDilation)
{
	UWorld* const World = GEngine->GetWorldFromContextObject( WorldContextObject );
	if(World != nullptr)
	{
		if (TimeDilation < 0.0001f || TimeDilation > 20.f)
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Time Dilation must be between 0.0001 and 20.  Clamping value to that range."));
			TimeDilation = FMath::Clamp(TimeDilation, 0.0001f, 20.0f);
		}
		World->GetWorldSettings()->TimeDilation = TimeDilation;
	}
}

bool UGameplayStatics::SetGamePaused(UObject* WorldContextObject, bool bPaused)
{
	UGameInstance* const GameInstance = GetGameInstance( WorldContextObject );
	APlayerController* const PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	return PC ? PC->SetPause(bPaused) : false;
}

bool UGameplayStatics::IsGamePaused(UObject* WorldContextObject)
{
	UWorld* const World = GEngine->GetWorldFromContextObject( WorldContextObject );
	return World ? World->IsPaused() : false;
}

/** @RETURN True if weapon trace from Origin hits component VictimComp.  OutHitResult will contain properties of the hit. */
static bool ComponentIsDamageableFrom(UPrimitiveComponent* VictimComp, FVector const& Origin, AActor const* IgnoredActor, const TArray<AActor*>& IgnoreActors, ECollisionChannel TraceChannel, FHitResult& OutHitResult)
{
	static FName NAME_ComponentIsVisibleFrom = FName(TEXT("ComponentIsVisibleFrom"));
	FCollisionQueryParams LineParams(NAME_ComponentIsVisibleFrom, true, IgnoredActor);
	LineParams.AddIgnoredActors( IgnoreActors );

	// Do a trace from origin to middle of box
	UWorld* const World = VictimComp->GetWorld();
	check(World);

	FVector const TraceEnd = VictimComp->Bounds.Origin;
	FVector TraceStart = Origin;
	if (Origin == TraceEnd)
	{
		// tiny nudge so LineTraceSingle doesn't early out with no hits
		TraceStart.Z += 0.01f;
	}
	bool const bHadBlockingHit = World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, LineParams);
	//::DrawDebugLine(World, TraceStart, TraceEnd, FLinearColor::Red, true);

	// If there was a blocking hit, it will be the last one
	if (bHadBlockingHit)
	{
		if (OutHitResult.Component == VictimComp)
		{
			// if blocking hit was the victim component, it is visible
			return true;
		}
		else
		{
			// if we hit something else blocking, it's not
			UE_LOG(LogDamage, Log, TEXT("Radial Damage to %s blocked by %s (%s)"), *GetNameSafe(VictimComp), *GetNameSafe(OutHitResult.GetActor()), *GetNameSafe(OutHitResult.Component.Get()) );
			return false;
		}
	}
		
	// didn't hit anything, assume nothing blocking the damage and victim is consequently visible
	// but since we don't have a hit result to pass back, construct a simple one, modeling the damage as having hit a point at the component's center.
	FVector const FakeHitLoc = VictimComp->GetComponentLocation();
	FVector const FakeHitNorm = (Origin - FakeHitLoc).GetSafeNormal();		// normal points back toward the epicenter
	OutHitResult = FHitResult(VictimComp->GetOwner(), VictimComp, FakeHitLoc, FakeHitNorm);
	return true;
}

bool UGameplayStatics::ApplyRadialDamage(UObject* WorldContextObject, float BaseDamage, const FVector& Origin, float DamageRadius, TSubclassOf<UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController, bool bDoFullDamage, ECollisionChannel DamagePreventionChannel )
{
	float DamageFalloff = bDoFullDamage ? 0.f : 1.f;
	return ApplyRadialDamageWithFalloff(WorldContextObject, BaseDamage, 0.f, Origin, 0.f, DamageRadius, DamageFalloff, DamageTypeClass, IgnoreActors, DamageCauser, InstigatedByController, DamagePreventionChannel);
}

bool UGameplayStatics::ApplyRadialDamageWithFalloff(UObject* WorldContextObject, float BaseDamage, float MinimumDamage, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff, TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController, ECollisionChannel DamagePreventionChannel)
{
	static FName NAME_ApplyRadialDamage = FName(TEXT("ApplyRadialDamage"));
	FCollisionQueryParams SphereParams(NAME_ApplyRadialDamage, false, DamageCauser);

	SphereParams.AddIgnoredActors(IgnoreActors);

	// query scene to see what we hit
	TArray<FOverlapResult> Overlaps;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(DamageOuterRadius), SphereParams);

	// collate into per-actor list of hit components
	TMap<AActor*, TArray<FHitResult> > OverlapComponentMap;
	for (int32 Idx=0; Idx<Overlaps.Num(); ++Idx)
	{
		FOverlapResult const& Overlap = Overlaps[Idx];
		AActor* const OverlapActor = Overlap.GetActor();

		if ( OverlapActor && 
			OverlapActor->bCanBeDamaged && 
			OverlapActor != DamageCauser &&
			Overlap.Component.IsValid() )
		{
			FHitResult Hit;
			if (DamagePreventionChannel == ECC_MAX || ComponentIsDamageableFrom(Overlap.Component.Get(), Origin, DamageCauser, IgnoreActors, DamagePreventionChannel, Hit))
			{
				TArray<FHitResult>& HitList = OverlapComponentMap.FindOrAdd(OverlapActor);
				HitList.Add(Hit);
			}
		}
	}

	// make sure we have a good damage type
	TSubclassOf<UDamageType> const ValidDamageTypeClass = DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());

	bool bAppliedDamage = false;

	// call damage function on each affected actors
	for (TMap<AActor*, TArray<FHitResult> >::TIterator It(OverlapComponentMap); It; ++It)
	{
		AActor* const Victim = It.Key();
		TArray<FHitResult> const& ComponentHits = It.Value();

		FRadialDamageEvent DmgEvent;
		DmgEvent.DamageTypeClass = ValidDamageTypeClass;
		DmgEvent.ComponentHits = ComponentHits;
		DmgEvent.Origin = Origin;
		DmgEvent.Params = FRadialDamageParams(BaseDamage, MinimumDamage, DamageInnerRadius, DamageOuterRadius, DamageFalloff);

		Victim->TakeDamage(BaseDamage, DmgEvent, InstigatedByController, DamageCauser);

		bAppliedDamage = true;
	}

	return bAppliedDamage;
}

void UGameplayStatics::ApplyPointDamage(AActor* DamagedActor, float BaseDamage, FVector const& HitFromDirection, FHitResult const& HitInfo, AController* EventInstigator, AActor* DamageCauser, TSubclassOf<UDamageType> DamageTypeClass)
{
	if (DamagedActor && BaseDamage != 0.f)
	{
		// make sure we have a good damage type
		TSubclassOf<UDamageType> const ValidDamageTypeClass = DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());
		FPointDamageEvent PointDamageEvent(BaseDamage, HitInfo, HitFromDirection, ValidDamageTypeClass);

		DamagedActor->TakeDamage(BaseDamage, PointDamageEvent, EventInstigator, DamageCauser);
	}
}

void UGameplayStatics::ApplyDamage(AActor* DamagedActor, float BaseDamage, AController* EventInstigator, AActor* DamageCauser, TSubclassOf<UDamageType> DamageTypeClass)
{
	if ( DamagedActor && (BaseDamage != 0.f) )
	{
		// make sure we have a good damage type
		TSubclassOf<UDamageType> const ValidDamageTypeClass = DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());
		FDamageEvent DamageEvent(ValidDamageTypeClass);

		DamagedActor->TakeDamage(BaseDamage, DamageEvent, EventInstigator, DamageCauser);
	}
}

bool UGameplayStatics::CanSpawnObjectOfClass(TSubclassOf<UObject> ObjectClass, bool bAllowAbstract)
{
	bool bBlueprintType = true;
#if WITH_EDITOR
	{
		static const FName BlueprintTypeName(TEXT("BlueprintType"));
		static const FName NotBlueprintTypeName(TEXT("NotBlueprintType"));
		const UClass* ParentClass = ObjectClass;
		while (ParentClass)
		{
			// Climb up the class hierarchy and look for "BlueprintType" and "NotBlueprintType" to see if this class is allowed.
			if (ParentClass->GetBoolMetaData(BlueprintTypeName))
			{
				bBlueprintType = true;
				break;
			}
			else if (ParentClass->GetBoolMetaData(NotBlueprintTypeName))
			{
				bBlueprintType = false;
				break;
			}
			ParentClass = ParentClass->GetSuperClass();
		}
	}
#endif // WITH_EDITOR

	bool bForbiddenSpawn = false;
#if WITH_EDITOR
	static const FName DontUseGenericSpawnObjectName(TEXT("DontUseGenericSpawnObject"));
	const UClass* ParentClass = ObjectClass;
	while (ParentClass)
	{
		if (ParentClass->HasMetaData(DontUseGenericSpawnObjectName))
		{
			bForbiddenSpawn = true;
			break;
		}
		ParentClass = ParentClass->GetSuperClass();
	}
#endif // WITH_EDITOR

	return (nullptr != *ObjectClass)
		&& bBlueprintType
		&& !bForbiddenSpawn
		&& (bAllowAbstract || !ObjectClass->HasAnyClassFlags(CLASS_Abstract))
		&& !ObjectClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
		&& !ObjectClass->IsChildOf(AActor::StaticClass())
		&& !ObjectClass->IsChildOf(UActorComponent::StaticClass());
}

UObject* UGameplayStatics::SpawnObject(TSubclassOf<UObject> ObjectClass, UObject* Outer)
{
	if (!CanSpawnObjectOfClass(ObjectClass))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error(FText::Format(LOCTEXT("SpawnObjectWrongClass", "SpawnObject wrong class: {0}'"), FText::FromString(GetNameSafe(*ObjectClass))));
#endif // WITH_EDITOR
		UE_LOG(LogScript, Error, TEXT("UGameplayStatics::SpawnObject wrong class: %s"), *GetPathNameSafe(*ObjectClass));
		return nullptr;
	}

	if (!Outer)
	{
		UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::SpawnObject null outer"));
		return nullptr;
	}

	if (ObjectClass->ClassWithin && !Outer->IsA(ObjectClass->ClassWithin))
	{
		UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::SpawnObject outer %s is not %s"), *GetPathNameSafe(Outer), *GetPathNameSafe(ObjectClass->ClassWithin));
		return nullptr;
	}

	return NewObject<UObject>(Outer, ObjectClass, NAME_None, RF_StrongRefOnFrame);
}

AActor* UGameplayStatics::BeginSpawningActorFromBlueprint(UObject* WorldContextObject, UBlueprint const* Blueprint, const FTransform& SpawnTransform, bool bNoCollisionFail)
{
	if (Blueprint && Blueprint->GeneratedClass)
	{
		if( Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()) )
		{
			ESpawnActorCollisionHandlingMethod const CollisionHandlingOverride = bNoCollisionFail ? ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding : ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return BeginDeferredActorSpawnFromClass(WorldContextObject, *Blueprint->GeneratedClass, SpawnTransform, CollisionHandlingOverride);
		}
		else
		{
			UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::BeginSpawningActorFromBlueprint: %s is not an actor class"), *Blueprint->GeneratedClass->GetName() );
		}
	}
	return nullptr;
}

// deprecated
AActor* UGameplayStatics::BeginSpawningActorFromClass(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, bool bNoCollisionFail, AActor* Owner)
{
	ESpawnActorCollisionHandlingMethod const CollisionHandlingOverride = bNoCollisionFail ? ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding : ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return BeginDeferredActorSpawnFromClass(WorldContextObject, ActorClass, SpawnTransform, CollisionHandlingOverride, Owner);
}

AActor* UGameplayStatics::BeginDeferredActorSpawnFromClass(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingMethod, AActor* Owner)
{
	if (UClass* Class = *ActorClass)
	{
		// If the WorldContextObject is a Pawn we will use that as the instigator.
		// Otherwise if the WorldContextObject is an Actor we will share its instigator.
		// If the value is set via the exposed parameter on SpawnNode it will be overwritten anyways, so this is safe to specify here
		APawn* AutoInstigator = Cast<APawn>(WorldContextObject);
		if (AutoInstigator)
		{
			if (AActor* ContextActor = Cast<AActor>(WorldContextObject))
			{
				AutoInstigator = ContextActor->Instigator;
			}
		}

		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
		{
			return World->SpawnActorDeferred<AActor>(Class, SpawnTransform, Owner, AutoInstigator, CollisionHandlingMethod);
		}
		else
		{
			UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::BeginSpawningActorFromClass: %s can not be spawned in NULL world"), *Class->GetName());		
		}
	}
	else
	{
		UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::BeginSpawningActorFromClass: can not spawn an actor from a NULL class"));
	}
	return nullptr;
}

AActor* UGameplayStatics::FinishSpawningActor(AActor* Actor, const FTransform& SpawnTransform)
{
	if (Actor)
	{
		Actor->FinishSpawning(SpawnTransform);
	}

	return Actor;
}

void UGameplayStatics::LoadStreamLevel(UObject* WorldContextObject, FName LevelName,bool bMakeVisibleAfterLoad,bool bShouldBlockOnLoad,FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FStreamLevelAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FStreamLevelAction* NewAction = new FStreamLevelAction(true, LevelName, bMakeVisibleAfterLoad, bShouldBlockOnLoad, LatentInfo, World);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UGameplayStatics::UnloadStreamLevel(UObject* WorldContextObject, FName LevelName,FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FStreamLevelAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FStreamLevelAction* NewAction = new FStreamLevelAction(false, LevelName, false, false, LatentInfo, World );
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction );
		}
	}
}

ULevelStreaming* UGameplayStatics::GetStreamingLevel(UObject* WorldContextObject, FName InPackageName)
{
	if (InPackageName != NAME_None)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
		{
			const FString SearchPackageName = FStreamLevelAction::MakeSafeLevelName(InPackageName, World);

			for (ULevelStreaming* LevelStreaming : World->StreamingLevels)
			{
				// We check only suffix of package name, to handle situations when packages were saved for play into a temporary folder
				// Like Saved/Autosaves/PackageName
				if (LevelStreaming && LevelStreaming->GetWorldAssetPackageName().EndsWith(SearchPackageName, ESearchCase::IgnoreCase))
				{
					return LevelStreaming;
				}
			}
		}
	}
	
	return NULL;
}

void UGameplayStatics::FlushLevelStreaming(UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		World->FlushLevelStreaming();
	}
}

void UGameplayStatics::CancelAsyncLoading()
{
	::CancelAsyncLoading();
}

void UGameplayStatics::OpenLevel(UObject* WorldContextObject, FName LevelName, bool bAbsolute, FString Options)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	const ETravelType TravelType = (bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative);
	FWorldContext &WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	FString Cmd = LevelName.ToString();
	if (Options.Len() > 0)
	{
		Cmd += FString(TEXT("?")) + Options;
	}
	FURL TestURL(&WorldContext.LastURL, *Cmd, TravelType);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		if (!GEngine->MakeSureMapNameIsValid(TestURL.Map))
		{
			UE_LOG(LogLevel, Warning, TEXT("WARNING: The map '%s' does not exist."), *TestURL.Map);
		}
	}

	GEngine->SetClientTravel( World, *Cmd, TravelType );
}

FString UGameplayStatics::GetCurrentLevelName(UObject* WorldContextObject, bool bRemovePrefixString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		FString LevelName = World->GetMapName();
		if (bRemovePrefixString)
		{
			LevelName.RemoveFromStart(World->StreamingLevelsPrefix);
		}
		return LevelName;
	}
	return FString();
}

FVector UGameplayStatics::GetActorArrayAverageLocation(const TArray<AActor*>& Actors)
{
	FVector LocationSum(0,0,0); // sum of locations
	int32 ActorCount = 0; // num actors
	// iterate over actors
	for(int32 ActorIdx=0; ActorIdx<Actors.Num(); ActorIdx++)
	{
		AActor* A = Actors[ActorIdx];
		// Check actor is non-null, not deleted, and has a root component
		if (A && !A->IsPendingKill() && A->GetRootComponent())
		{
			LocationSum += A->GetActorLocation();
			ActorCount++;
		}
	}

	// Find average
	FVector Average(0,0,0);
	if(ActorCount > 0)
	{
		Average = LocationSum/((float)ActorCount);
	}
	return Average;
}

void UGameplayStatics::GetActorArrayBounds(const TArray<AActor*>& Actors, bool bOnlyCollidingComponents, FVector& Center, FVector& BoxExtent)
{
	FBox ActorBounds(0);
	// Iterate over actors and accumulate bouding box
	for(int32 ActorIdx=0; ActorIdx<Actors.Num(); ActorIdx++)
	{
		AActor* A = Actors[ActorIdx];
		// Check actor is non-null, not deleted
		if(A && !A->IsPendingKill())
		{
			ActorBounds += A->GetComponentsBoundingBox(!bOnlyCollidingComponents);
		}
	}

	// if a valid box, get its center and extent
	Center = BoxExtent = FVector::ZeroVector;
	if(ActorBounds.IsValid)
	{
		Center = ActorBounds.GetCenter();
		BoxExtent = ActorBounds.GetExtent();
	}
}

void UGameplayStatics::GetAllActorsOfClass(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);

	// We do nothing if not class provided, rather than giving ALL actors!
	if (ActorClass && World)
	{
		for(TActorIterator<AActor> It(World, ActorClass); It; ++It)
		{
			AActor* Actor = *It;
			if(!Actor->IsPendingKill())
			{
				OutActors.Add(Actor);
			}
		}
	}
}

void UGameplayStatics::GetAllActorsWithInterface(UObject* WorldContextObject, TSubclassOf<UInterface> Interface, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	// We do nothing if not class provided, rather than giving ALL actors!
	if (Interface && World)
	{
		for(FActorIterator It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && !Actor->IsPendingKill() && Actor->GetClass()->ImplementsInterface(Interface))
			{
				OutActors.Add(Actor);
			}
		}
	}
}


void UGameplayStatics::PlayWorldCameraShake(UObject* WorldContextObject, TSubclassOf<class UCameraShake> Shake, FVector Epicenter, float InnerRadius, float OuterRadius, float Falloff, bool bOrientShakeTowardsEpicenter)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(World != nullptr)
	{
		APlayerCameraManager::PlayWorldCameraShake(World, Shake, Epicenter, InnerRadius, OuterRadius, Falloff, bOrientShakeTowardsEpicenter);
	}
}

UParticleSystemComponent* CreateParticleSystem(UParticleSystem* EmitterTemplate, UWorld* World, AActor* Actor, bool bAutoDestroy)
{
	UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>((Actor ? Actor : (UObject*)World));
	PSC->bAutoDestroy = bAutoDestroy;
	PSC->bAllowAnyoneToDestroyMe = true;
	PSC->SecondsBeforeInactive = 0.0f;
	PSC->bAutoActivate = false;
	PSC->SetTemplate(EmitterTemplate);
	PSC->bOverrideLODMethod = false;

	return PSC;
}

UParticleSystemComponent* UGameplayStatics::SpawnEmitterAtLocation(UObject* WorldContextObject, UParticleSystem* EmitterTemplate, FVector SpawnLocation, FRotator SpawnRotation, bool bAutoDestroy)
{
	if (EmitterTemplate)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
		{
			UParticleSystemComponent* PSC = CreateParticleSystem(EmitterTemplate, World, World->GetWorldSettings(), bAutoDestroy);
			PSC->SetAbsolute(true, true, true);
			PSC->SetWorldLocationAndRotation(SpawnLocation, SpawnRotation);
			PSC->SetRelativeScale3D(FVector(1.f));

			PSC->RegisterComponentWithWorld(World);

			PSC->ActivateSystem(true);
			return PSC;
		}
	}
	return nullptr;
}

UParticleSystemComponent* UGameplayStatics::SpawnEmitterAtLocation(UWorld* World, UParticleSystem* EmitterTemplate, const FTransform& SpawnTransform, bool bAutoDestroy)
{
	UParticleSystemComponent* PSC = nullptr;
	if (World && EmitterTemplate)
	{
		PSC = CreateParticleSystem(EmitterTemplate, World, World->GetWorldSettings(), bAutoDestroy);

		PSC->SetAbsolute(true, true, true);
		PSC->SetWorldTransform(SpawnTransform);

		PSC->RegisterComponentWithWorld(World);

		PSC->ActivateSystem(true);
	}
	return PSC;
}


UParticleSystemComponent* UGameplayStatics::SpawnEmitterAttached(UParticleSystem* EmitterTemplate, USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bAutoDestroy)
{
	UParticleSystemComponent* PSC = nullptr;
	if (EmitterTemplate)
	{
		if (AttachToComponent == nullptr)
		{
			UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::SpawnEmitterAttached: NULL AttachComponent specified!"));
		}
		else if (AttachToComponent->GetWorld() && AttachToComponent->GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			PSC = CreateParticleSystem(EmitterTemplate, AttachToComponent->GetWorld(), AttachToComponent->GetOwner(), bAutoDestroy);
			PSC->AttachTo(AttachToComponent, AttachPointName);
			if (LocationType == EAttachLocation::KeepWorldPosition)
			{
				PSC->SetWorldLocationAndRotation(Location, Rotation);
			}
			else
			{
				PSC->SetRelativeLocationAndRotation(Location, Rotation);
			}
			PSC->SetRelativeScale3D(FVector(1.f));
			PSC->ActivateSystem(true);
		}
	}
	return PSC;
}

void UGameplayStatics::BreakHitResult(const FHitResult& Hit, bool& bBlockingHit, bool& bInitialOverlap, float& Time, FVector& Location, FVector& ImpactPoint, FVector& Normal, FVector& ImpactNormal, UPhysicalMaterial*& PhysMat, AActor*& HitActor, UPrimitiveComponent*& HitComponent, FName& HitBoneName, int32& HitItem, FVector& TraceStart, FVector& TraceEnd)
{
	bBlockingHit = Hit.bBlockingHit;
	bInitialOverlap = Hit.bStartPenetrating;
	Time = Hit.Time;
	Location = Hit.Location;
	ImpactPoint = Hit.ImpactPoint;
	Normal = Hit.Normal;
	ImpactNormal = Hit.ImpactNormal;	
	PhysMat = Hit.PhysMaterial.Get();
	HitActor = Hit.GetActor();
	HitComponent = Hit.GetComponent();
	HitBoneName = Hit.BoneName;
	HitItem = Hit.Item;
	TraceStart = Hit.TraceStart;
	TraceEnd = Hit.TraceEnd;
}

EPhysicalSurface UGameplayStatics::GetSurfaceType(const struct FHitResult& Hit)
{
	UPhysicalMaterial* const HitPhysMat = Hit.PhysMaterial.Get();
	return UPhysicalMaterial::DetermineSurfaceType( HitPhysMat );
}

bool UGameplayStatics::AreAnyListenersWithinRange(UObject* WorldContextObject, FVector Location, float MaximumRange)
{
	if (!GEngine || !GEngine->UseSound())
	{
		return false;
	}
	
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld)
	{
		return false;
	}
	
	// If there is no valid world from the world context object then there certainly are no listeners
	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		return AudioDevice->LocationIsAudible(Location, MaximumRange);
	}	

	return false;
}

void UGameplayStatics::SetGlobalPitchModulation(UObject* WorldContextObject, float PitchModulation, float TimeSec)
{
	if (!GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->GlobalPitchScale.Set(PitchModulation, TimeSec);
	}
}


void UGameplayStatics::PlaySound2D(UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundConcurrency* ConcurrencySettings)
{
	if (!Sound || !GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// TODO - Audio Threading. This call would be a task call to dispatch to the audio thread
	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		FActiveSound NewActiveSound;
		NewActiveSound.Sound = Sound;

		NewActiveSound.VolumeMultiplier = VolumeMultiplier;
		NewActiveSound.PitchMultiplier = PitchMultiplier;

		NewActiveSound.RequestedStartTime = FMath::Max(0.f, StartTime);

		NewActiveSound.bIsUISound = true;
		NewActiveSound.ConcurrencySettings = ConcurrencySettings;

		AudioDevice->AddNewActiveSound(NewActiveSound);
	}
}

UAudioComponent* UGameplayStatics::CreateSound2D(UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundConcurrency* ConcurrencySettings)
{
	if (!Sound || !GEngine || !GEngine->UseSound())
	{
		return nullptr;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(Sound, ThisWorld, ThisWorld->GetWorldSettings(), false, false, nullptr, nullptr, ConcurrencySettings);
	if (AudioComponent)
	{
		const bool bIsInGameWorld = AudioComponent->GetWorld()->IsGameWorld();
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		AudioComponent->SetPitchMultiplier(PitchMultiplier);
		AudioComponent->bAllowSpatialization = false;
		AudioComponent->bIsUISound = true;
		AudioComponent->bAutoDestroy = true;
		AudioComponent->SubtitlePriority = 10000.f; // Fixme: pass in? Do we want that exposed to blueprints though?		
	}
	return AudioComponent;
}

UAudioComponent* UGameplayStatics::SpawnSound2D(UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundConcurrency* ConcurrencySettings)
{
	UAudioComponent* AudioComponent = CreateSound2D(WorldContextObject, Sound, VolumeMultiplier, PitchMultiplier, StartTime, ConcurrencySettings);
	if (AudioComponent)
	{
		AudioComponent->Play(StartTime);
	}
	return AudioComponent;
}

void UGameplayStatics::PlaySoundAtLocation(UObject* WorldContextObject, class USoundBase* Sound, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings, class USoundConcurrency* ConcurrencySettings)
{
	if (!Sound || !GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		if (Sound->IsAudibleSimple(AudioDevice, Location, AttenuationSettings))
		{
			const bool bIsInGameWorld = ThisWorld->IsGameWorld();

			FActiveSound NewActiveSound;
			NewActiveSound.World = ThisWorld;
			NewActiveSound.Sound = Sound;

			NewActiveSound.VolumeMultiplier = VolumeMultiplier;
			NewActiveSound.PitchMultiplier = PitchMultiplier;

			NewActiveSound.RequestedStartTime = FMath::Max(0.f, StartTime);

			NewActiveSound.bLocationDefined = true;
			NewActiveSound.Transform.SetTranslation(Location);
			NewActiveSound.Transform.SetRotation(FQuat(Rotation));

			NewActiveSound.bIsUISound = !bIsInGameWorld;
			NewActiveSound.bHandleSubtitles = true;
			NewActiveSound.SubtitlePriority = 10000.f; // Fixme: pass in? Do we want that exposed to blueprints though?

			const FAttenuationSettings* AttenuationSettingsToApply = (AttenuationSettings ? &AttenuationSettings->Attenuation : Sound->GetAttenuationSettingsToApply());

			NewActiveSound.bHasAttenuationSettings = (bIsInGameWorld && AttenuationSettingsToApply);
			if (NewActiveSound.bHasAttenuationSettings)
			{
				NewActiveSound.AttenuationSettings = *AttenuationSettingsToApply;
			}

			NewActiveSound.ConcurrencySettings = ConcurrencySettings;

			// TODO - Audio Threading. This call would be a task call to dispatch to the audio thread
			AudioDevice->AddNewActiveSound(NewActiveSound);
		}
		else
		{
			// Don't play a sound for short sounds that start out of range of any listener
			UE_LOG(LogAudio, Log, TEXT("Sound not played for out of range Sound %s"), *Sound->GetName());
		}
	}
}

UAudioComponent* UGameplayStatics::SpawnSoundAtLocation(UObject* WorldContextObject, class USoundBase* Sound, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings, class USoundConcurrency* ConcurrencySettings)
{
	if (!Sound || !GEngine || !GEngine->UseSound())
	{
		return nullptr;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	const bool bIsInGameWorld = ThisWorld->IsGameWorld();

	UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(Sound, ThisWorld, ThisWorld->GetWorldSettings(), false, false, &Location, AttenuationSettings, ConcurrencySettings);

	if (AudioComponent)
	{
		AudioComponent->SetWorldLocationAndRotation(Location, Rotation);
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		AudioComponent->SetPitchMultiplier(PitchMultiplier);
		AudioComponent->bAllowSpatialization	= bIsInGameWorld;
		AudioComponent->bIsUISound				= !bIsInGameWorld;
		AudioComponent->bAutoDestroy			= true;
		AudioComponent->SubtitlePriority		= 10000.f; // Fixme: pass in? Do we want that exposed to blueprints though?
		AudioComponent->Play(StartTime);
	}

	return AudioComponent;
}

class UAudioComponent* UGameplayStatics::SpawnSoundAttached(class USoundBase* Sound, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bStopWhenAttachedToDestroyed, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings, class USoundConcurrency* ConcurrencySettings)
{
	if (!Sound)
	{
		return nullptr;
	}

	if (!AttachToComponent)
	{
		UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::PlaySoundAttached: NULL AttachComponent specified! Trying to spawn sound [%s],"), *Sound->GetName());
		return nullptr;
	}

	// Location used to check whether to create a component if out of range
	FVector TestLocation = Location;
	if (LocationType != EAttachLocation::KeepWorldPosition)
	{
		TestLocation = AttachToComponent->GetRelativeTransform().TransformPosition(Location);
	}

	UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(Sound, AttachToComponent->GetWorld(), AttachToComponent->GetOwner(), false, bStopWhenAttachedToDestroyed, &TestLocation, AttenuationSettings, ConcurrencySettings);
	if (AudioComponent)
	{
		const bool bIsInGameWorld = AudioComponent->GetWorld()->IsGameWorld();

		AudioComponent->AttachTo(AttachToComponent, AttachPointName);
		if (LocationType == EAttachLocation::KeepWorldPosition)
		{
			AudioComponent->SetWorldLocationAndRotation(Location, Rotation);
		}
		else
		{
			AudioComponent->SetRelativeLocationAndRotation(Location, Rotation);
		}
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		AudioComponent->SetPitchMultiplier(PitchMultiplier);
		AudioComponent->bAllowSpatialization	= bIsInGameWorld;
		AudioComponent->bIsUISound				= !bIsInGameWorld;
		AudioComponent->bAutoDestroy			= true;
		AudioComponent->SubtitlePriority		= 10000.f; // Fixme: pass in? Do we want that exposed to blueprints though?
		AudioComponent->Play(StartTime);
	}

	return AudioComponent;
}

void UGameplayStatics::PlayDialogue2D(UObject* WorldContextObject, class UDialogueWave* Dialogue, const FDialogueContext& Context, float VolumeMultiplier, float PitchMultiplier, float StartTime)
{
	if (Dialogue)
	{
		PlaySound2D(WorldContextObject, Dialogue->GetWaveFromContext(Context), VolumeMultiplier, PitchMultiplier, StartTime);
	}
}

UAudioComponent* UGameplayStatics::SpawnDialogue2D(UObject* WorldContextObject, class UDialogueWave* Dialogue, const FDialogueContext& Context, float VolumeMultiplier, float PitchMultiplier, float StartTime)
{
	if (Dialogue)
	{
		return SpawnSound2D(WorldContextObject, Dialogue->GetWaveFromContext(Context), VolumeMultiplier, PitchMultiplier, StartTime);
	}
	return nullptr;
}

void UGameplayStatics::PlayDialogueAtLocation(UObject* WorldContextObject, class UDialogueWave* Dialogue, const FDialogueContext& Context, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings)
{
	if (Dialogue)
	{
		PlaySoundAtLocation(WorldContextObject, Dialogue->GetWaveFromContext(Context), Location, Rotation, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}
}

UAudioComponent* UGameplayStatics::SpawnDialogueAtLocation(UObject* WorldContextObject, class UDialogueWave* Dialogue, const FDialogueContext& Context, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings)
{
	if (Dialogue)
	{
		return SpawnSoundAtLocation(WorldContextObject, Dialogue->GetWaveFromContext(Context), Location, Rotation, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}
	return nullptr;
}

class UAudioComponent* UGameplayStatics::SpawnDialogueAttached(class UDialogueWave* Dialogue, const FDialogueContext& Context, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bStopWhenAttachedToDestroyed, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings)
{
	if (Dialogue)
	{
		return SpawnSoundAttached(Dialogue->GetWaveFromContext(Context), AttachToComponent, AttachPointName, Location, Rotation, LocationType, bStopWhenAttachedToDestroyed, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}
	return nullptr;
}

void UGameplayStatics::SetBaseSoundMix(UObject* WorldContextObject, USoundMix* InSoundMix)
{
	if (!InSoundMix || !GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->SetBaseSoundMix(InSoundMix);
	}
}

void UGameplayStatics::PushSoundMixModifier(UObject* WorldContextObject, USoundMix* InSoundMixModifier)
{
	if (!InSoundMixModifier || !GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->PushSoundMixModifier(InSoundMixModifier);
	}
}

void UGameplayStatics::PopSoundMixModifier(UObject* WorldContextObject, USoundMix* InSoundMixModifier)
{
	if (InSoundMixModifier == nullptr || GEngine == nullptr || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (ThisWorld == nullptr || !ThisWorld->bAllowAudioPlayback)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->PopSoundMixModifier(InSoundMixModifier);
	}
}

void UGameplayStatics::ClearSoundMixModifiers(UObject* WorldContextObject)
{
	if (!GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->ClearSoundMixModifiers();
	}
}

void UGameplayStatics::ActivateReverbEffect(UObject* WorldContextObject, class UReverbEffect* ReverbEffect, FName TagName, float Priority, float Volume, float FadeTime)
{
	if (ReverbEffect == nullptr || !GEngine || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->ActivateReverbEffect(ReverbEffect, TagName, Priority, Volume, FadeTime);
	}
}

void UGameplayStatics::DeactivateReverbEffect(UObject* WorldContextObject, FName TagName)
{
	if (GEngine == nullptr || !GEngine->UseSound())
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice())
	{
		AudioDevice->DeactivateReverbEffect(TagName);
	}
}

UDecalComponent* CreateDecalComponent(class UMaterialInterface* DecalMaterial, FVector DecalSize, UWorld* World, AActor* Actor, float LifeSpan)
{
	const FMatrix DecalInternalTransform = FRotationMatrix(FRotator(0.f, 90.0f, -90.0f));

	UDecalComponent* DecalComp = NewObject<UDecalComponent>((Actor ? Actor : (UObject*)World));
	DecalComp->bAllowAnyoneToDestroyMe = true;
	DecalComp->DecalMaterial = DecalMaterial;
	DecalComp->RelativeScale3D = DecalInternalTransform.TransformVector(DecalSize);
	DecalComp->bAbsoluteScale = true;
	DecalComp->RegisterComponentWithWorld(World);

	if (LifeSpan > 0.f)
	{
		DecalComp->SetLifeSpan(LifeSpan);
	}

	return DecalComp;
}

UDecalComponent* UGameplayStatics::SpawnDecalAtLocation(UObject* WorldContextObject, class UMaterialInterface* DecalMaterial, FVector DecalSize, FVector Location, FRotator Rotation, float LifeSpan)
{
	if (DecalMaterial)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
		{
			UDecalComponent* DecalComp = CreateDecalComponent(DecalMaterial, DecalSize, World, World->GetWorldSettings(), LifeSpan);
			DecalComp->SetWorldLocationAndRotation(Location, Rotation);
			return DecalComp;
		}
	}
	return nullptr;
}

UDecalComponent* UGameplayStatics::SpawnDecalAttached(class UMaterialInterface* DecalMaterial, FVector DecalSize, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, float LifeSpan)
{
	if (DecalMaterial)
	{
		if (!AttachToComponent)
		{
			UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::SpawnDecalAttached: NULL AttachComponent specified!"));
		}
		else
		{
			UPrimitiveComponent* AttachToPrimitive = Cast<UPrimitiveComponent>(AttachToComponent);
			if (!AttachToPrimitive || AttachToPrimitive->bReceivesDecals)
			{
				if (AttachToPrimitive && Cast<AWorldSettings>(AttachToPrimitive->GetOwner()))
				{
					// special case: don't attach to component when it's owned by invisible WorldSettings (decals on BSP brush)
					return SpawnDecalAtLocation(AttachToPrimitive->GetOwner(), DecalMaterial, DecalSize, Location, Rotation, LifeSpan);
				}
				else
				{
					UDecalComponent* DecalComp = CreateDecalComponent(DecalMaterial, DecalSize, AttachToComponent->GetWorld(), AttachToComponent->GetOwner(), LifeSpan);
					DecalComp->AttachTo(AttachToComponent, AttachPointName);
					if (LocationType == EAttachLocation::KeepWorldPosition)
					{
						DecalComp->SetWorldLocationAndRotation(Location, Rotation);
					}
					else
					{
						DecalComp->SetRelativeLocationAndRotation(Location, Rotation);
					}
					return DecalComp;
				}
			}
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////

USaveGame* UGameplayStatics::CreateSaveGameObject(TSubclassOf<USaveGame> SaveGameClass)
{
	// Don't save if no class or if class is the abstract base class.
	if (*SaveGameClass && (*SaveGameClass != USaveGame::StaticClass()))
	{
		return NewObject<USaveGame>(GetTransientPackage(), SaveGameClass);
	}
	return nullptr;
}

USaveGame* UGameplayStatics::CreateSaveGameObjectFromBlueprint(UBlueprint* SaveGameBlueprint)
{
	if (SaveGameBlueprint && SaveGameBlueprint->GeneratedClass && SaveGameBlueprint->GeneratedClass->IsChildOf(USaveGame::StaticClass()))
	{
		return NewObject<USaveGame>(GetTransientPackage(), SaveGameBlueprint->GeneratedClass);
	}
	return nullptr;
}


bool UGameplayStatics::SaveGameToSlot(USaveGame* SaveGameObject, const FString& SlotName, const int32 UserIndex)
{
	ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
	// If we have a system and an object to save and a save name...
	if(SaveSystem && SaveGameObject && (SlotName.Len() > 0))
	{
		TArray<uint8> ObjectBytes;
		FMemoryWriter MemoryWriter(ObjectBytes, true);

		// write file type tag. identifies this file type and indicates it's using proper versioning
		// since older UE4 versions did not version this data.
		int32 FileTypeTag = UE4_SAVEGAME_FILE_TYPE_TAG;
		MemoryWriter << FileTypeTag;

		// Write version for this file format
		int32 SavegameFileVersion = UE4_SAVEGAME_FILE_VERSION;
		MemoryWriter << SavegameFileVersion;

		// Write out engine and UE4 version information
		int32 PackageFileUE4Version = GPackageFileUE4Version;
		MemoryWriter << PackageFileUE4Version;
		FEngineVersion SavedEngineVersion = FEngineVersion::Current();
		MemoryWriter << SavedEngineVersion;

		// Write the class name so we know what class to load to
		FString SaveGameClassName = SaveGameObject->GetClass()->GetName();
		MemoryWriter << SaveGameClassName;

		// Then save the object state, replacing object refs and names with strings
		FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, false);
		SaveGameObject->Serialize(Ar);

		// Stuff that data into the save system with the desired file name
		return SaveSystem->SaveGame(false, *SlotName, UserIndex, ObjectBytes);
	}
	return false;
}

bool UGameplayStatics::DoesSaveGameExist(const FString& SlotName, const int32 UserIndex)
{
	if (ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem())
	{
		return SaveSystem->DoesSaveGameExist(*SlotName, UserIndex);
	}
	return false;
}

bool UGameplayStatics::DeleteGameInSlot(const FString& SlotName, const int32 UserIndex)
{
	if (ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem())
	{
		return SaveSystem->DeleteGame(false, *SlotName, UserIndex);
	}
	return false;
}

USaveGame* UGameplayStatics::LoadGameFromSlot(const FString& SlotName, const int32 UserIndex)
{
	USaveGame* OutSaveGameObject = NULL;

	ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
	// If we have a save system and a valid name..
	if(SaveSystem && (SlotName.Len() > 0))
	{
		// Load raw data from slot
		TArray<uint8> ObjectBytes;
		bool bSuccess = SaveSystem->LoadGame(false, *SlotName, UserIndex, ObjectBytes);
		if(bSuccess)
		{
			FMemoryReader MemoryReader(ObjectBytes, true);

			int32 FileTypeTag;
			MemoryReader << FileTypeTag;

			int32 SavegameFileVersion;
			if (FileTypeTag != UE4_SAVEGAME_FILE_TYPE_TAG)
			{
				// this is an old saved game, back up the file pointer to the beginning and assume version 1
				MemoryReader.Seek(0);
				SavegameFileVersion = 1;

				// Note for 4.8 and beyond: if you get a crash loading a pre-4.8 version of your savegame file and 
				// you don't want to delete it, try uncommenting these lines and changing them to use the version 
				// information from your previous build. Then load and resave your savegame file.
				//MemoryReader.SetUE4Ver(MyPreviousUE4Version);				// @see GPackageFileUE4Version
				//MemoryReader.SetEngineVer(MyPreviousEngineVersion);		// @see FEngineVersion::Current()
			}
			else
			{
				// Read version for this file format
				MemoryReader << SavegameFileVersion;

				// Read engine and UE4 version information
				int32 SavedUE4Version;
				MemoryReader << SavedUE4Version;

				FEngineVersion SavedEngineVersion;
				MemoryReader << SavedEngineVersion;

				MemoryReader.SetUE4Ver(SavedUE4Version);
				MemoryReader.SetEngineVer(SavedEngineVersion);
			}
			
			// Get the class name
			FString SaveGameClassName;
			MemoryReader << SaveGameClassName;

			// Try and find it, and failing that, load it
			UClass* SaveGameClass = FindObject<UClass>(ANY_PACKAGE, *SaveGameClassName);
			if(SaveGameClass == NULL)
			{
				SaveGameClass = LoadObject<UClass>(NULL, *SaveGameClassName);
			}

			// If we have a class, try and load it.
			if(SaveGameClass != NULL)
			{
				OutSaveGameObject = NewObject<USaveGame>(GetTransientPackage(), SaveGameClass);

				FObjectAndNameAsStringProxyArchive Ar(MemoryReader, true);
				OutSaveGameObject->Serialize(Ar);
			}
		}
	}

	return OutSaveGameObject;
}

float UGameplayStatics::GetWorldDeltaSeconds(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->GetDeltaSeconds() : 0.f;
}

float UGameplayStatics::GetRealTimeSeconds(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->GetRealTimeSeconds() : 0.f;
}

float UGameplayStatics::GetAudioTimeSeconds(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->GetAudioTimeSeconds() : 0.f;
}

void UGameplayStatics::GetAccurateRealTime(UObject* WorldContextObject, int32& Seconds, float& PartialSeconds)
{
	double TimeSeconds = FPlatformTime::Seconds();
	Seconds = floor(TimeSeconds);
	PartialSeconds = TimeSeconds - double(Seconds);
}

void UGameplayStatics::EnableLiveStreaming(bool Enable)
{
	if (IDVRStreamingSystem* StreamingSystem = IPlatformFeaturesModule::Get().GetStreamingSystem())
	{
		StreamingSystem->EnableStreaming(Enable);
	}
}

FString UGameplayStatics::GetPlatformName()
{
	// the string that BP users care about is actually the platform name that we'd name the .ini file directory (Windows, not WindowsEditor)
	return FPlatformProperties::IniPlatformName();
}

bool UGameplayStatics::BlueprintSuggestProjectileVelocity(UObject* WorldContextObject, FVector& OutTossVelocity, FVector StartLocation, FVector EndLocation, float LaunchSpeed, float OverrideGravityZ, ESuggestProjVelocityTraceOption::Type TraceOption, float CollisionRadius, bool bFavorHighArc, bool bDrawDebug)
{
	// simple pass-through to the C++ interface
	return UGameplayStatics::SuggestProjectileVelocity(WorldContextObject, OutTossVelocity, StartLocation, EndLocation, LaunchSpeed, bFavorHighArc, CollisionRadius, OverrideGravityZ, TraceOption, FCollisionResponseParams::DefaultResponseParam, TArray<AActor*>(), bDrawDebug);
}

// Based on analytic solution to ballistic angle of launch http://en.wikipedia.org/wiki/Trajectory_of_a_projectile#Angle_required_to_hit_coordinate_.28x.2Cy.29
bool UGameplayStatics::SuggestProjectileVelocity(UObject* WorldContextObject, FVector& OutTossVelocity, FVector Start, FVector End, float TossSpeed, bool bFavorHighArc, float CollisionRadius, float OverrideGravityZ, ESuggestProjVelocityTraceOption::Type TraceOption, const FCollisionResponseParams& ResponseParam, const TArray<AActor*>& ActorsToIgnore, bool bDrawDebug)
{
	const FVector FlightDelta = End - Start;
	const FVector DirXY = FlightDelta.GetSafeNormal2D();
	const float DeltaXY = FlightDelta.Size2D();

	const float DeltaZ = FlightDelta.Z;

	const float TossSpeedSq = FMath::Square(TossSpeed);

	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);

	const float GravityZ = (OverrideGravityZ != 0.f) ? -OverrideGravityZ : -World->GetGravityZ();


	// v^4 - g*(g*x^2 + 2*y*v^2)
	const float InsideTheSqrt = FMath::Square(TossSpeedSq) - GravityZ * ( (GravityZ * FMath::Square(DeltaXY)) + (2.f * DeltaZ * TossSpeedSq) );
	if (InsideTheSqrt < 0.f)
	{
		// sqrt will be imaginary, therefore no solutions
		return false;
	}

	// if we got here, there are 2 solutions: one high-angle and one low-angle.

	const float SqrtPart = FMath::Sqrt(InsideTheSqrt);

	// this is the tangent of the firing angle for the first (+) solution
	const float TanSolutionAngleA = (TossSpeedSq + SqrtPart) / (GravityZ * DeltaXY);
	// this is the tangent of the firing angle for the second (-) solution
	const float TanSolutionAngleB = (TossSpeedSq - SqrtPart) / (GravityZ * DeltaXY);
	
	// mag in the XY dir = sqrt( TossSpeedSq / (TanSolutionAngle^2 + 1) );
	const float MagXYSq_A = TossSpeedSq / (FMath::Square(TanSolutionAngleA) + 1.f);
	const float MagXYSq_B = TossSpeedSq / (FMath::Square(TanSolutionAngleB) + 1.f);

	bool bFoundAValidSolution = false;

	// trace if desired
	if (TraceOption == ESuggestProjVelocityTraceOption::DoNotTrace)
	{
		// choose which arc
		const float FavoredMagXYSq = bFavorHighArc ? FMath::Min(MagXYSq_A, MagXYSq_B) : FMath::Max(MagXYSq_A, MagXYSq_B);
		const float ZSign = bFavorHighArc ?
							(MagXYSq_A < MagXYSq_B) ? FMath::Sign(TanSolutionAngleA) : FMath::Sign(TanSolutionAngleB) :
							(MagXYSq_A > MagXYSq_B) ? FMath::Sign(TanSolutionAngleA) : FMath::Sign(TanSolutionAngleB);

		// finish calculations
		const float MagXY = FMath::Sqrt(FavoredMagXYSq);
		const float MagZ = FMath::Sqrt(TossSpeedSq - FavoredMagXYSq);		// pythagorean

		// final answer!
		OutTossVelocity = (DirXY * MagXY) + (FVector::UpVector * MagZ * ZSign);
		bFoundAValidSolution = true;

	 	if (bDrawDebug)
	 	{
	 		static const float StepSize = 0.125f;
	 		FVector TraceStart = Start;
	 		for ( float Step=0.f; Step<1.f; Step+=StepSize )
	 		{
	 			const float TimeInFlight = (Step+StepSize) * DeltaXY/MagXY;
	 
	 			// d = vt + .5 a t^2
				const FVector TraceEnd = Start + OutTossVelocity*TimeInFlight + FVector(0.f, 0.f, 0.5f * -GravityZ * FMath::Square(TimeInFlight) - CollisionRadius);
	 
	 			DrawDebugLine( World, TraceStart, TraceEnd, (bFoundAValidSolution ? FColor::Yellow : FColor::Red), true );
	 			TraceStart = TraceEnd;
	 		}
	 	}
	}
	else
	{
		// need to trace to validate

		// sort potential solutions by priority
		float PrioritizedSolutionsMagXYSq[2];
		PrioritizedSolutionsMagXYSq[0] = bFavorHighArc ? FMath::Min(MagXYSq_A, MagXYSq_B) : FMath::Max(MagXYSq_A, MagXYSq_B);
		PrioritizedSolutionsMagXYSq[1] = bFavorHighArc ? FMath::Max(MagXYSq_A, MagXYSq_B) : FMath::Min(MagXYSq_A, MagXYSq_B);

		float PrioritizedSolutionZSign[2];
		PrioritizedSolutionZSign[0] = bFavorHighArc ?
										(MagXYSq_A < MagXYSq_B) ? FMath::Sign(TanSolutionAngleA) : FMath::Sign(TanSolutionAngleB) :
										(MagXYSq_A > MagXYSq_B) ? FMath::Sign(TanSolutionAngleA) : FMath::Sign(TanSolutionAngleB);
		PrioritizedSolutionZSign[1] = bFavorHighArc ?
										(MagXYSq_A > MagXYSq_B) ? FMath::Sign(TanSolutionAngleA) : FMath::Sign(TanSolutionAngleB) :
										(MagXYSq_A < MagXYSq_B) ? FMath::Sign(TanSolutionAngleA) : FMath::Sign(TanSolutionAngleB);

		FVector PrioritizedProjVelocities[2];

		// try solutions in priority order
		int32 ValidSolutionIdx = INDEX_NONE;
		for (int32 CurrentSolutionIdx=0; (CurrentSolutionIdx<2); ++CurrentSolutionIdx)
		{
			const float MagXY = FMath::Sqrt( PrioritizedSolutionsMagXYSq[CurrentSolutionIdx] );
			const float MagZ = FMath::Sqrt( TossSpeedSq - PrioritizedSolutionsMagXYSq[CurrentSolutionIdx] );		// pythagorean
			const float ZSign = PrioritizedSolutionZSign[CurrentSolutionIdx];

			PrioritizedProjVelocities[CurrentSolutionIdx] = (DirXY * MagXY) + (FVector::UpVector * MagZ * ZSign);

			// iterate along the arc, doing stepwise traces
			bool bFailedTrace = false;
			static const float StepSize = 0.125f;
			FVector TraceStart = Start;
			for ( float Step=0.f; Step<1.f; Step+=StepSize )
			{
				const float TimeInFlight = (Step+StepSize) * DeltaXY/MagXY;

				// d = vt + .5 a t^2
				const FVector TraceEnd = Start + PrioritizedProjVelocities[CurrentSolutionIdx]*TimeInFlight + FVector(0.f, 0.f, 0.5f * -GravityZ * FMath::Square(TimeInFlight) - CollisionRadius);

				if ( (TraceOption == ESuggestProjVelocityTraceOption::OnlyTraceWhileAsceding) && (TraceEnd.Z < TraceStart.Z) )
				{
					// falling, we are done tracing
					if (!bDrawDebug)
					{
						// if we're drawing, we continue stepping without the traces
						// else we can just trivially end the iteration loop
						break;
					}
				}
				else
				{
					// note: this will automatically fall back to line test if radius is small enough
					static const FName NAME_SuggestProjVelTrace = FName(TEXT("SuggestProjVelTrace"));

					FCollisionQueryParams QueryParams(NAME_SuggestProjVelTrace, true);
					QueryParams.AddIgnoredActors(ActorsToIgnore);
					if (World->SweepTestByChannel(TraceStart, TraceEnd, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(CollisionRadius), QueryParams, ResponseParam))
					{
						// hit something, failed
						bFailedTrace = true;

						if (bDrawDebug)
						{
							// draw failed segment in red
							DrawDebugLine( World, TraceStart, TraceEnd, FColor::Red, true );
						}

						break;
					}

				}

				if (bDrawDebug)
				{
					DrawDebugLine( World, TraceStart, TraceEnd, FColor::Yellow, true );
				}

				// advance
				TraceStart = TraceEnd;
			}

			if (bFailedTrace == false)
			{
				// passes all traces along the arc, we have a valid solution and can be done
				ValidSolutionIdx = CurrentSolutionIdx;
				break;
			}
		}

		if (ValidSolutionIdx != INDEX_NONE)
		{
			OutTossVelocity = PrioritizedProjVelocities[ValidSolutionIdx];
			bFoundAValidSolution = true;
		}
	}

	return bFoundAValidSolution;
}

FIntVector UGameplayStatics::GetWorldOriginLocation(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World ? World->OriginLocation : FIntVector::ZeroValue;
}

void UGameplayStatics::SetWorldOriginLocation(UObject* WorldContextObject, FIntVector NewLocation)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if ( World )
	{
		World->RequestNewWorldOrigin(NewLocation);
	}
}

int32 UGameplayStatics::GrassOverlappingSphereCount(UObject* WorldContextObject, const UStaticMesh* Mesh, FVector CenterPosition, float Radius)
{
	int32 Count = 0;

	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World)
	{
		const FSphere Sphere(CenterPosition, Radius);

		// check every landscape
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			ALandscapeProxy* L = *It;
			if (L)
			{
				for (UHierarchicalInstancedStaticMeshComponent const* HComp : L->FoliageComponents)
				{
					if (HComp && (HComp->StaticMesh == Mesh))
					{
						Count += HComp->GetOverlappingSphereCount(Sphere);
					}
				}
			}
		}
	}

	return Count;
}


bool UGameplayStatics::DeprojectScreenToWorld(APlayerController const* Player, const FVector2D& ScreenPosition, FVector& WorldPosition, FVector& WorldDirection)
{
	ULocalPlayer* const LP = Player ? Player->GetLocalPlayer() : nullptr;
	if (LP && LP->ViewportClient)
	{
		// get the projection data
		FSceneViewProjectionData ProjectionData;
		if (LP->GetProjectionData(LP->ViewportClient->Viewport, eSSP_FULL, /*out*/ ProjectionData))
		{
			FMatrix const InvViewProjMatrix = ProjectionData.ComputeViewProjectionMatrix().InverseFast();
			FSceneView::DeprojectScreenToWorld(ScreenPosition, ProjectionData.GetConstrainedViewRect(), InvViewProjMatrix, /*out*/ WorldPosition, /*out*/ WorldDirection);
			return true;
		}
	}

	// something went wrong, zero things and return false
	WorldPosition = FVector::ZeroVector;
	WorldDirection = FVector::ZeroVector;
	return false;
}

bool UGameplayStatics::ProjectWorldToScreen(APlayerController const* Player, const FVector& WorldPosition, FVector2D& ScreenPosition)
{
	ULocalPlayer* const LP = Player ? Player->GetLocalPlayer() : nullptr;
	if (LP && LP->ViewportClient)
	{
		// get the projection data
		FSceneViewProjectionData ProjectionData;
		if (LP->GetProjectionData(LP->ViewportClient->Viewport, eSSP_FULL, /*out*/ ProjectionData))
		{
			FMatrix const ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
			return FSceneView::ProjectWorldToScreen(WorldPosition, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition);
		}
	}

	ScreenPosition = FVector2D::ZeroVector;
	return false;
}

bool UGameplayStatics::GrabOption( FString& Options, FString& Result )
{
	FString QuestionMark(TEXT("?"));

	if( Options.Left(1).Equals(QuestionMark, ESearchCase::CaseSensitive) )
	{
		// Get result.
		Result = Options.Mid(1, MAX_int32);
		if (Result.Contains(QuestionMark, ESearchCase::CaseSensitive))
		{
			Result = Result.Left(Result.Find(QuestionMark, ESearchCase::CaseSensitive));
		}

		// Update options.
		Options = Options.Mid(1, MAX_int32);
		if (Options.Contains(QuestionMark, ESearchCase::CaseSensitive))
		{
			Options = Options.Mid(Options.Find(QuestionMark, ESearchCase::CaseSensitive), MAX_int32);
		}
		else
		{
			Options = FString();
		}

		return true;
	}

	return false;
}

void UGameplayStatics::GetKeyValue( const FString& Pair, FString& Key, FString& Value )
{
	const int32 EqualSignIndex = Pair.Find(TEXT("="), ESearchCase::CaseSensitive);
	if( EqualSignIndex != INDEX_NONE )
	{
		Key = Pair.Left(EqualSignIndex);
		Value = Pair.Mid(EqualSignIndex + 1, MAX_int32);
	}
	else
	{
		Key = Pair;
		Value = TEXT("");
	}
}

FString UGameplayStatics::ParseOption( FString Options, const FString& Key )
{
	FString ReturnValue;
	FString Pair, PairKey, PairValue;
	while( GrabOption( Options, Pair ) )
	{
		GetKeyValue( Pair, PairKey, PairValue );
		if (Key == PairKey)
		{
			ReturnValue = MoveTemp(PairValue);
			break;
		}
	}
	return ReturnValue;
}

bool UGameplayStatics::HasOption( FString Options, const FString& Key )
{
	FString Pair, PairKey, PairValue;
	while( GrabOption( Options, Pair ) )
	{
		GetKeyValue( Pair, PairKey, PairValue );
		if (Key == PairKey)
		{
			return true;
		}
	}
	return false;
}

int32 UGameplayStatics::GetIntOption( const FString& Options, const FString& Key, int32 DefaultValue)
{
	const FString InOpt = ParseOption( Options, Key );
	if ( !InOpt.IsEmpty() )
	{
		return FCString::Atoi(*InOpt);
	}
	return DefaultValue;
}

#undef LOCTEXT_NAMESPACE