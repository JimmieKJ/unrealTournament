// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "AssetRegistryModule.h"
#include "UTWorldSettings.h"
#include "UTLevelSummary.h"

class FUTModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FUTModule, UnrealTournament, "UnrealTournament");
 
DEFINE_LOG_CATEGORY(UT);
DEFINE_LOG_CATEGORY(UTNet);

// init editor hooks
#if WITH_EDITOR

#include "SlateBasics.h"
#include "UTDetailsCustomization.h"

static void AddLevelSummaryAssetTags(const UWorld* InWorld, TArray<UObject::FAssetRegistryTag>& OutTags)
{
	// add level summary data to the asset registry as part of the world
	AUTWorldSettings* Settings = Cast<AUTWorldSettings>(InWorld->GetWorldSettings());
	if (Settings != NULL && Settings->GetLevelSummary() != NULL)
	{
		Settings->GetLevelSummary()->GetAssetRegistryTags(OutTags);
	}
}

void FUTModule::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("UTWeapon", FOnGetDetailCustomizationInstance::CreateStatic(&FUTDetailsCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("UTWeaponAttachment", FOnGetDetailCustomizationInstance::CreateStatic(&FUTDetailsCustomization::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	FWorldDelegates::GetAssetTags.AddStatic(&AddLevelSummaryAssetTags);
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
		if (PSys->Emitters[i]->GetLODLevel(0)->RequiredModule->EmitterLoops <= 0 && PSys->Emitters[i]->GetLODLevel(0)->RequiredModule->bEnabled)
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

	World->OverlapMultiByChannel(Hits, TestLoc, FQuat::Identity, ECC_Pawn, Shape, Params);

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

static TMap<FName, FString> HackedEntitlementTable = []()
{
	TMap<FName, FString> Result;
	Result.Add(TEXT("BP_Round_HelmetGoggles"), TEXT("91afa66fbf744726af33dba391657296"));
	Result.Add(TEXT("BP_Round_HelmetGoggles_C"), TEXT("91afa66fbf744726af33dba391657296"));
	Result.Add(TEXT("BP_SkullMask"), TEXT("606862e8a0ec4f5190f67c6df9d4ea81"));
	Result.Add(TEXT("BP_SkullMask_C"), TEXT("606862e8a0ec4f5190f67c6df9d4ea81"));
	Result.Add(TEXT("BP_BaseballHat"), TEXT("8747335f79dd4bec8ddc03214c307950"));
	Result.Add(TEXT("BP_BaseballHat_C"), TEXT("8747335f79dd4bec8ddc03214c307950"));
	Result.Add(TEXT("BP_CardboardHat"), TEXT("9a1ad6c3c10e438f9602c14ad1b67bfa"));
	Result.Add(TEXT("BP_CardboardHat_C"), TEXT("9a1ad6c3c10e438f9602c14ad1b67bfa"));
	Result.Add(TEXT("DM-Lea"), TEXT("0d5e275ca99d4cf0b03c518a6b279e26"));
	return Result;
}();

FString GetRequiredEntitlementFromAsset(const FAssetData& Asset)
{
	// FIXME: total temp hack since we don't have any way to embed entitlement IDs with the asset yet...
	FString* Found = HackedEntitlementTable.Find(Asset.AssetName);
	return (Found != NULL) ? *Found : FString();
}
FString GetRequiredEntitlementFromObj(UObject* Asset)
{
	if (Asset == NULL)
	{
		return FString();
	}
	else
	{
		// FIXME: total temp hack since we don't have any way to embed entitlement IDs with the asset yet...
		FString* Found = HackedEntitlementTable.Find(Asset->GetFName());
		return (Found != NULL) ? *Found : FString();
	}
}
FString GetRequiredEntitlementFromPackageName(FName PackageName)
{
	FAssetData Asset;
	Asset.AssetName = PackageName;
	return GetRequiredEntitlementFromAsset(Asset);
}

bool LocallyHasEntitlement(const FString& Entitlement)
{
	if (Entitlement.IsEmpty())
	{
		// no entitlement required
		return true;
	}
	else
	{
		if (IOnlineSubsystem::Get() != NULL)
		{
			IOnlineIdentityPtr IdentityInterface = IOnlineSubsystem::Get()->GetIdentityInterface();
			IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
			if (IdentityInterface.IsValid() && EntitlementInterface.IsValid())
			{
				for (int32 i = 0; i < MAX_LOCAL_PLAYERS; i++)
				{
					TSharedPtr<FUniqueNetId> Id = IdentityInterface->GetUniquePlayerId(i);
					if (Id.IsValid())
					{
						if (EntitlementInterface->GetItemEntitlement(*Id.Get(), Entitlement).IsValid())
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	}
}

void GetAllAssetData(UClass* BaseClass, TArray<FAssetData>& AssetList, bool bRequireEntitlements)
{
	// calling this with UBlueprint::StaticClass() is probably a bug where the user intended to call GetAllBlueprintAssetData()
	if (BaseClass == UBlueprint::StaticClass())
	{
#if DO_GUARD_SLOW
		UE_LOG(UT, Fatal, TEXT("GetAllAssetData() should not be used for blueprints; call GetAllBlueprintAssetData() instead"));
#else
		UE_LOG(UT, Error, TEXT("GetAllAssetData() should not be used for blueprints; call GetAllBlueprintAssetData() instead"));
#endif
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);

#if WITH_EDITOR
	// HACK: workaround for terrible registry performance when scanning; limit search paths to improve perf a bit
	RootPaths.Remove(TEXT("/Engine/"));
	RootPaths.Remove(TEXT("/Game/"));
	RootPaths.Remove(TEXT("/Paper2D/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Maps/"));
	RootPaths.Add(TEXT("/Game/Maps/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Blueprints/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Pickups/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Weapons/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Character/"));
	// Cooked data has the asset data already set up
	AssetRegistry.ScanPathsSynchronous(RootPaths);
#endif

	FARFilter ARFilter;
	if (BaseClass != NULL)
	{
		ARFilter.ClassNames.Add(BaseClass->GetFName());
		// Add any old names to the list in case things haven't been resaved
		TArray<FName> OldNames = FLinkerLoad::FindPreviousNamesForClass(BaseClass->GetPathName(), false);
		ARFilter.ClassNames.Append(OldNames);
	}
	ARFilter.bRecursivePaths = true;
	ARFilter.bIncludeOnlyOnDiskAssets = true;
	ARFilter.bRecursiveClasses = true;

	AssetRegistry.GetAssets(ARFilter, AssetList);

	// query entitlements for any assets and remove those that are not usable
	if (bRequireEntitlements)
	{
		for (int32 i = AssetList.Num() - 1; i >= 0; i--)
		{
			if (!LocallyHasEntitlement(GetRequiredEntitlementFromAsset(AssetList[i])))
			{
				AssetList.RemoveAt(i);
			}
		}
	}
}

void GetAllBlueprintAssetData(UClass* BaseClass, TArray<FAssetData>& AssetList, bool bRequireEntitlements)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);

#if WITH_EDITOR
	// HACK: workaround for terrible registry performance when scanning; limit search paths to improve perf a bit
	RootPaths.Remove(TEXT("/Engine/"));
	RootPaths.Remove(TEXT("/Game/"));
	RootPaths.Remove(TEXT("/Paper2D/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Maps/"));
	RootPaths.Add(TEXT("/Game/Maps/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Blueprints/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Pickups/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Weapons/"));
	RootPaths.Add(TEXT("/Game/RestrictedAssets/Character/"));
	// Cooked data has the asset data already set up
	AssetRegistry.ScanPathsSynchronous(RootPaths);
#endif

	FARFilter ARFilter;
	ARFilter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());

	/*for (int32 PathIndex = 0; PathIndex < RootPaths.Num(); PathIndex++)
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

	// query entitlements for any assets and remove those that are not usable
	if (bRequireEntitlements)
	{
		for (int32 i = AssetList.Num() - 1; i >= 0; i--)
		{
			if (!LocallyHasEntitlement(GetRequiredEntitlementFromAsset(AssetList[i])))
			{
				AssetList.RemoveAt(i);
			}
		}
	}
}

void SetTimerUFunc(UObject* Obj, FName FuncName, float Time, bool bLooping)
{
	if (Obj != NULL)
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(Obj);
		if (World != NULL)
		{
			UFunction* const Func = Obj->FindFunction(FuncName);
			if (Func == NULL)
			{
				UE_LOG(UT, Warning, TEXT("SetTimer: Object %s does not have a function named '%s'"), *Obj->GetName(), *FuncName.ToString());
			}
			else if (Func->ParmsSize > 0)
			{
				// User passed in a valid function, but one that takes parameters
				// FTimerDynamicDelegate expects zero parameters and will choke on execution if it tries
				// to execute a mismatched function
				UE_LOG(UT, Warning, TEXT("SetTimer passed a function (%s) that expects parameters."), *FuncName.ToString());
			}
			else
			{
				FTimerDynamicDelegate Delegate;
				Delegate.BindUFunction(Obj, FuncName);

				FTimerHandle Handle = World->GetTimerManager().K2_FindDynamicTimerHandle(Delegate);
				World->GetTimerManager().SetTimer(Handle, Delegate, Time, bLooping);
			}
		}
	}
}
bool IsTimerActiveUFunc(UObject* Obj, FName FuncName)
{
	if (Obj != NULL)
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(Obj);
		if (World != NULL)
		{
			UFunction* const Func = Obj->FindFunction(FuncName);
			if (Func == NULL)
			{
				UE_LOG(UT, Warning, TEXT("IsTimerActive: Object %s does not have a function named '%s'"), *Obj->GetName(), *FuncName.ToString());
				return false;
			}
			else
			{
				FTimerDynamicDelegate Delegate;
				Delegate.BindUFunction(Obj, FuncName);

				FTimerHandle Handle = World->GetTimerManager().K2_FindDynamicTimerHandle(Delegate);
				return World->GetTimerManager().IsTimerActive(Handle);
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}
void ClearTimerActiveUFunc(UObject* Obj, FName FuncName)
{
	if (Obj != NULL)
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(Obj);
		if (World != NULL)
		{
			UFunction* const Func = Obj->FindFunction(FuncName);
			if (Func == NULL)
			{
				UE_LOG(UT, Warning, TEXT("ClearTimer: Object %s does not have a function named '%s'"), *Obj->GetName(), *FuncName.ToString());
			}
			else
			{
				FTimerDynamicDelegate Delegate;
				Delegate.BindUFunction(Obj, FuncName);

				FTimerHandle Handle = World->GetTimerManager().K2_FindDynamicTimerHandle(Delegate);
				return World->GetTimerManager().ClearTimer(Handle);
			}
		}
	}
}