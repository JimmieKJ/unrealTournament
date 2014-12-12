// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "AssetRegistryModule.h"

class FUTModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FUTModule, UnrealTournament, "UnrealTournament");
 
DEFINE_LOG_CATEGORY(UT);

// init editor hooks
#if WITH_EDITOR

#include "SlateBasics.h"
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

void GetAllBlueprintAssetData(UClass* BaseClass, TArray<FAssetData>& AssetList)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);

#if WITH_EDITOR
	// Cooked data has the asset data already set up
	AssetRegistry.ScanPathsSynchronous(RootPaths);
#endif

	FARFilter ARFilter;
	ARFilter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());

	/*for (int PathIndex = 0; PathIndex < RootPaths.Num(); PathIndex++)
	{
		ARFilter.PackagePaths.Add(FName(*RootPaths[PathIndex]));
	}*/

	ARFilter.bRecursivePaths = true;
	ARFilter.bIncludeOnlyOnDiskAssets = true;

	if (BaseClass == NULL)
	{
		AssetRegistry.GetAssets(ARFilter, AssetList);
	}
	else
	{
		// TODO: the below filtering is torturous because the asset registry does not contain full inheritance information for blueprints
		// nor does it return full class paths when you request a class tree

		TArray<FAssetData> LocalAssetList;
		AssetRegistry.GetAssets(ARFilter, LocalAssetList);

		TSet<FString> UnloadedBaseClassPaths;
		// first pass: determine the inheritance that we can trivially verify are the correct class because their parent is in memory
		for (int32 i = 0; i < LocalAssetList.Num(); i++)
		{
			const FString* LoadedParentClass = LocalAssetList[i].TagsAndValues.Find("ParentClass");
			if (LoadedParentClass != NULL && !LoadedParentClass->IsEmpty())
			{
				UClass* Class = FindObject<UClass>(ANY_PACKAGE, **LoadedParentClass);
				if (Class == NULL)
				{
					// apparently you have to 'load' native classes once for FindObject() to reach them
					// figure out if this parent is such a class and if so, allow LoadObject()
					FString ParentPackage = *LoadedParentClass;
					ConstructorHelpers::StripObjectClass(ParentPackage);
					if (ParentPackage.StartsWith(TEXT("/Script/")))
					{
						ParentPackage = ParentPackage.LeftChop(ParentPackage.Len() - ParentPackage.Find(TEXT(".")));
						if (FindObject<UPackage>(NULL, *ParentPackage) != NULL)
						{
							Class = LoadObject<UClass>(NULL, **LoadedParentClass, NULL, LOAD_NoWarn | LOAD_Quiet);
						}
					}
				}
				if (Class != NULL)
				{
					if (Class->IsChildOf(BaseClass))
					{
						AssetList.Add(LocalAssetList[i]);
						const FString* GenClassPath = LocalAssetList[i].TagsAndValues.Find("GeneratedClass");
						if (GenClassPath != NULL)
						{
							UnloadedBaseClassPaths.Add(*GenClassPath);
						}
					}
					LocalAssetList.RemoveAt(i);
					i--;
				}
			}
			else
			{
				// asset info is missing; fail
				LocalAssetList.RemoveAt(i);
				i--;
			}
		}
		// now go through the remainder and match blueprints against an unloaded parent
		// if we find no new matching assets, the rest must be the wrong super
		bool bFoundAny = false;
		do 
		{
			bFoundAny = false;
			for (int32 i = 0; i < LocalAssetList.Num(); i++)
			{
				if (UnloadedBaseClassPaths.Find(*LocalAssetList[i].TagsAndValues.Find("ParentClass")))
				{
					AssetList.Add(LocalAssetList[i]);
					const FString* GenClassPath = LocalAssetList[i].TagsAndValues.Find("GeneratedClass");
					if (GenClassPath != NULL)
					{
						UnloadedBaseClassPaths.Add(*GenClassPath);
					}
					LocalAssetList.RemoveAt(i);
					i--;
					bFoundAny = true;
				}
			}
		} while (bFoundAny && LocalAssetList.Num() > 0);
	}
}