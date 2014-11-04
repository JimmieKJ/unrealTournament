// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

class FUTModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FUTModule, UnrealTournament, "UnrealTournament");
 
DEFINE_LOG_CATEGORY(UT);

// init editor hooks
#if WITH_EDITOR

#include "Slate.h"
#include "UTDetailsCustomization.h"

void FUTModule::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("UTWeapon", FOnGetDetailCustomizationInstance::CreateStatic(&FUTDetailsCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("UTWeaponAttachment", FOnGetDetailCustomizationInstance::CreateStatic(&FUTDetailsCustomization::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

#else

void FUTModule::StartupModule()
{
}
#endif

FCollisionObjectQueryParams WorldObjectQueryParams = []()
{
	FCollisionObjectQueryParams Result;
	Result.AddObjectTypesToQuery(ECC_WorldDynamic);
	Result.AddObjectTypesToQuery(ECC_WorldStatic);
	return Result;
}();
FCollisionResponseParams WorldResponseParams = []()
{
	FCollisionResponseParams Result(ECR_Ignore);
	Result.CollisionResponse.WorldStatic = ECR_Block;
	Result.CollisionResponse.WorldDynamic = ECR_Block;
	return Result;
}();

#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleLODLevel.h"

bool IsLoopingParticleSystem(const UParticleSystem* PSys)
{
	for (int32 i = 0; i < PSys->Emitters.Num(); i++)
	{
		if (PSys->Emitters[i]->GetLODLevel(0)->RequiredModule->EmitterLoops <= 0)
		{
			return true;
		}
	}
	return false;
}

void UnregisterComponentTree(USceneComponent* Comp)
{
	if (Comp != NULL)
	{
		TArray<USceneComponent*> Children;
		Comp->GetChildrenComponents(true, Children);
		Comp->DetachFromParent();
		Comp->UnregisterComponent();
		for (USceneComponent* Child : Children)
		{
			Child->UnregisterComponent();
		}
	}
}

APhysicsVolume* FindPhysicsVolume(UWorld* World, const FVector& TestLoc, const FCollisionShape& Shape)
{
	APhysicsVolume* NewVolume = World->GetDefaultPhysicsVolume();

	// check for all volumes that overlap the component
	TArray<FOverlapResult> Hits;
	static FName NAME_PhysicsVolumeTrace = FName(TEXT("PhysicsVolumeTrace"));
	FComponentQueryParams Params(NAME_PhysicsVolumeTrace, NULL);

	World->OverlapMulti(Hits, TestLoc, FQuat::Identity, ECC_Pawn, Shape, Params);

	for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
	{
		const FOverlapResult& Link = Hits[HitIdx];
		APhysicsVolume* const V = Cast<APhysicsVolume>(Link.GetActor());
		if (V != NULL && (V->Priority > NewVolume->Priority))
		{
			NewVolume = V;
		}
	}

	return NewVolume;
}