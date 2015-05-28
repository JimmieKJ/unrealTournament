// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/AssetUserData.h"
#include "Engine/WorldComposition.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameNetworkManager.h"
#include "SoundDefinitions.h"
#include "ParticleDefinitions.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "Particles/ParticleEventManager.h"
#include "PhysicsEngine/PhysicsSettings.h"

#define LOCTEXT_NAMESPACE "ErrorChecking"

AWorldSettings::AWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Sprite")))
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UClass> DmgType_Environmental_Object;
		FConstructorStatics()
			: DmgType_Environmental_Object(TEXT("/Engine/EngineDamageTypes/DmgTypeBP_Environmental.DmgTypeBP_Environmental_C"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bEnableWorldBoundsChecks = true;
	bEnableNavigationSystem = true;
	bEnableWorldComposition = false;
	bEnableWorldOriginRebasing = false;
#if WITH_EDITORONLY_DATA	
	bEnableHierarchicalLODSystem = false;

 	FHierarchicalSimplification LODBaseSetup;
	HierarchicalLODSetup.Add(LODBaseSetup);
#endif

	KillZ = -HALF_WORLD_MAX1;
	KillZDamageType = ConstructorStatics.DmgType_Environmental_Object.Object;

	WorldToMeters = 100.f;

	GameNetworkManagerClass = AGameNetworkManager::StaticClass();
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	TimeDilation = 1.0f;
	MatineeTimeDilation = 1.0f;
	DemoPlayTimeDilation = 1.0f;
	PackedLightAndShadowMapTextureSize = 1024;
	bHidden = false;

	DefaultColorScale = FVector(1.0f, 1.0f, 1.0f);

	bPlaceCellsOnlyAlongCameraTracks = false;
	VisibilityCellSize = 200;
	VisibilityAggressiveness = VIS_LeastAggressive;
	LevelLightingQuality = Quality_MAX;

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif // WITH_EDITORONLY_DATA
}

void AWorldSettings::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// create the emitter pool
	// we only need to do this for the persistent level's WorldSettings as sublevel actors will have their WorldSettings set to it on association
	if (GetNetMode() != NM_DedicatedServer && IsInPersistentLevel())
	{
		UWorld* World = GetWorld();
		check(World);		

		// only create once - 
		if (World->MyParticleEventManager == NULL && !GEngine->ParticleEventManagerClassPath.IsEmpty())
		{
			TSubclassOf<AParticleEventManager> ParticleEventManagerClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GEngine->ParticleEventManagerClassPath, NULL, LOAD_NoWarn, NULL));
			if (ParticleEventManagerClass != NULL)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Owner = this;
				SpawnParameters.Instigator = Instigator;
				SpawnParameters.ObjectFlags |= RF_Transient;	// We never want to save particle event managers into a map
				World->MyParticleEventManager = World->SpawnActor<AParticleEventManager>(ParticleEventManagerClass, SpawnParameters );
			}
		}
	}
}

void AWorldSettings::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GEngine->IsConsoleBuild())
	{
		GEngine->bUseConsoleInput = true;
	}
}

float AWorldSettings::GetGravityZ() const
{
	if (!bWorldGravitySet)
	{
		// try to initialize cached value
		AWorldSettings* const MutableThis = const_cast<AWorldSettings*>(this);
		MutableThis->WorldGravityZ = bGlobalGravitySet ? GlobalGravityZ : UPhysicsSettings::Get()->DefaultGravityZ;	//allows us to override DefaultGravityZ
	}

	return WorldGravityZ;
}

void AWorldSettings::NotifyBeginPlay()
{
	UWorld* World = GetWorld();
	if (!World->bBegunPlay)
	{
		for (FActorIterator It(World); It; ++It)
		{
			// Actors that have traveled seamlessly from other levels already had BeginPlay called in that level
			if (!It->IsPendingKill() && !It->HasActorBegunPlay())
			{
				It->BeginPlay();
			}
		}
		World->bBegunPlay = true;
	}
}

void AWorldSettings::NotifyMatchStarted()
{
	UWorld* World = GetWorld();
	World->bMatchStarted = true;
}

void AWorldSettings::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AWorldSettings, Pauser );
	DOREPLIFETIME( AWorldSettings, TimeDilation );
	DOREPLIFETIME( AWorldSettings, MatineeTimeDilation );
	DOREPLIFETIME( AWorldSettings, WorldGravityZ );
	DOREPLIFETIME( AWorldSettings, bHighPriorityLoading );
}

void AWorldSettings::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_ADD_OVERRIDE_GRAVITY_FLAG)
	{
		//before we had override flag we would use GlobalGravityZ != 0
		if(GlobalGravityZ != 0.0f)
		{
			bGlobalGravitySet = true;
		}
	}
}

void AWorldSettings::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* AWorldSettings::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void AWorldSettings::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

#if WITH_EDITOR

void AWorldSettings::CheckForErrors()
{
	Super::CheckForErrors();

	UWorld* World = GetWorld();
	if ( World->GetWorldSettings() != this )
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_DuplicateLevelInfo", "Duplicate level info" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::DuplicateLevelInfo));
	}

	if( World->NumLightingUnbuiltObjects > 0 )
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_RebuildLighting", "Maps need lighting rebuilt" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::RebuildLighting));
	}
}

void AWorldSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName()==TEXT("bForceNoPrecomputedLighting") && bForceNoPrecomputedLighting)
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("bForceNoPrecomputedLightingIsEnabled", "bForceNoPrecomputedLighting is now enabled, build lighting once to propagate the change (will remove existing precomputed lighting data)."));
		}

		if (PropertyThatChanged->GetName()==TEXT("bEnableWorldComposition"))
		{
			if (UWorldComposition::EnableWorldCompositionEvent.IsBound())
			{
				bEnableWorldComposition = UWorldComposition::EnableWorldCompositionEvent.Execute(GetWorld(), bEnableWorldComposition);
			}
			else
			{
				bEnableWorldComposition = false;
			}
		}
	}

	LightmassSettings.NumIndirectLightingBounces = FMath::Clamp(LightmassSettings.NumIndirectLightingBounces, 0, 100);
	LightmassSettings.IndirectLightingSmoothness = FMath::Clamp(LightmassSettings.IndirectLightingSmoothness, .25f, 10.0f);
	LightmassSettings.VolumeLightSamplePlacementScale = FMath::Clamp(LightmassSettings.VolumeLightSamplePlacementScale, .1f, 100.0f);
	LightmassSettings.IndirectLightingQuality = FMath::Clamp(LightmassSettings.IndirectLightingQuality, .1f, 10.0f);
	LightmassSettings.StaticLightingLevelScale = FMath::Clamp(LightmassSettings.StaticLightingLevelScale, .001f, 1000.0f);
	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.DirectIlluminationOcclusionFraction = FMath::Clamp(LightmassSettings.DirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.IndirectIlluminationOcclusionFraction = FMath::Clamp(LightmassSettings.IndirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.OcclusionExponent = FMath::Max(LightmassSettings.OcclusionExponent, 0.0f);
	LightmassSettings.FullyOccludedSamplesFraction = FMath::Clamp(LightmassSettings.FullyOccludedSamplesFraction, 0.0f, 1.0f);
	LightmassSettings.MaxOcclusionDistance = FMath::Max(LightmassSettings.MaxOcclusionDistance, 0.0f);
	LightmassSettings.EnvironmentIntensity = FMath::Max(LightmassSettings.EnvironmentIntensity, 0.0f);

	// Ensure texture size is power of two between 512 and 4096.
	PackedLightAndShadowMapTextureSize = FMath::Clamp<uint32>( FMath::RoundUpToPowerOfTwo( PackedLightAndShadowMapTextureSize ), 512, 4096 );

	if (GetWorld() != nullptr && GetWorld()->PersistentLevel->GetWorldSettings() == this)
	{
		if (GIsEditor)
		{
			GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeSetup"));
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
