// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SignificanceManagerPrivate.h"
#include "SignificanceManager.h"
#include "DisplayDebugHelpers.h"

IMPLEMENT_MODULE( FDefaultModuleImpl, SignificanceManager );

DECLARE_CYCLE_STAT(TEXT("Update Total"), STAT_SignificanceManager_Update, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Significance Update"), STAT_SignificanceManager_SignificanceUpdate, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Significance Sort"), STAT_SignificanceManager_SignificanceSort, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Unregister Object"), STAT_SignificanceManager_UnregisterObject, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Significance Check"), STAT_SignificanceManager_SignificanceCheck, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Register Object"), STAT_SignificanceManager_RegisterObject, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Initial Significance Update"), STAT_SignificanceManager_InitialSignificanceUpdate, STATGROUP_SignificanceManager);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Managed Objects"), STAT_SignificanceManager_NumObjects, STATGROUP_SignificanceManager);


bool CompareBySignificanceAscending(const USignificanceManager::FManagedObjectInfo& A, const USignificanceManager::FManagedObjectInfo& B) 
{ 
	return A.GetSignificance() < B.GetSignificance(); 
}

bool CompareBySignificanceDescending(const USignificanceManager::FManagedObjectInfo& A, const USignificanceManager::FManagedObjectInfo& B) 
{ 
	return A.GetSignificance() > B.GetSignificance(); 
}

typedef bool CompareFunctionType(const USignificanceManager::FManagedObjectInfo&,const USignificanceManager::FManagedObjectInfo&);

CompareFunctionType& PickCompareBySignificance(const bool bAscending)
{
	return (bAscending ? CompareBySignificanceAscending : CompareBySignificanceDescending);
}

USignificanceManager::USignificanceManager()
	: Super()
{
	SignificanceManagerClassName = FStringClassReference(GetClass()); 

	bCreateOnClient = true;
	bCreateOnServer = true;
	bSortSignificanceAscending = false;

	// Only put this in place for the actual USignificanceManager CDO
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass() == USignificanceManager::StaticClass())
	{
		FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &USignificanceManager::OnWorldInit);
		FWorldDelegates::OnWorldCleanup.AddUObject(this, &USignificanceManager::OnWorldCleanup);
		if (!IsRunningDedicatedServer())
		{
			AHUD::OnShowDebugInfo.AddUObject(this, &USignificanceManager::OnShowDebugInfo);
		}
	}
}

/** Gets the instance of USignificanceManager that holds references to world significance managers */
static USignificanceManager* GetSignificanceManagerSingleton()
{
	static struct FSingletonInit
	{
		FName SingletonName;
		FSingletonInit()
			: SingletonName(TEXT("SignificanceSingleton"))
		{
			// Construct root set object to hold manager references that exist outside of disregard for GC pool.
			USignificanceManager* Singleton = Cast<USignificanceManager>(StaticFindObjectFast(USignificanceManager::StaticClass(), GetTransientPackage(), SingletonName));
			if (!Singleton)
			{
				Singleton = NewObject<USignificanceManager>(GetTransientPackage(), SingletonName);
			}
			Singleton->AddToRoot();
		}
	} SingletonInit;
	// Always find in memory (should be very quick). Don't store static references, this breaks hot-reload!
	return CastChecked<USignificanceManager>(StaticFindObjectFast(USignificanceManager::StaticClass(), GetTransientPackage(), SingletonInit.SingletonName));
}

USignificanceManager* USignificanceManager::Get(const UWorld* World)
{ 
	return GetSignificanceManagerSingleton()->WorldSignificanceManagers.FindRef(World);
}

void USignificanceManager::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (World->IsGameWorld())
	{
		if (*SignificanceManagerClass == nullptr)
		{
			SignificanceManagerClass = LoadClass<USignificanceManager>(nullptr, *SignificanceManagerClassName.ToString());
		}

		if (*SignificanceManagerClass != nullptr)
		{
			USignificanceManager* ManagerToCreateDefault = SignificanceManagerClass->GetDefaultObject<USignificanceManager>();
			if ((ManagerToCreateDefault->bCreateOnServer && !IsRunningClientOnly()) || (ManagerToCreateDefault->bCreateOnClient && !IsRunningDedicatedServer()))
			{
				GetSignificanceManagerSingleton()->WorldSignificanceManagers.Add(World, NewObject<USignificanceManager>(World, SignificanceManagerClass));
			}
		}
	}
}

void USignificanceManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	GetSignificanceManagerSingleton()->WorldSignificanceManagers.Remove(World);
}

void USignificanceManager::BeginDestroy()
{
	Super::BeginDestroy();

	for (const TPair<const UObject*, FManagedObjectInfo*>& ObjectToObjectInfoPair : ManagedObjects)
	{
		delete ObjectToObjectInfoPair.Value;
	}
	ManagedObjects.Reset();
	ManagedObjectsByTag.Reset();
}

void USignificanceManager::RegisterObject(const UObject* Object, const FName Tag, FSignificanceFunction SignificanceFunction)
{
	INC_DWORD_STAT(STAT_SignificanceManager_NumObjects);
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_RegisterObject);

	check(Object);
	checkf(!ManagedObjects.Contains(Object), TEXT("'%s' already added to significance manager. Original Tag: '%s' New Tag: '%s'"), *Object->GetName(), *ManagedObjects.FindChecked(Object)->GetTag().ToString(), *Tag.ToString());

	FManagedObjectInfo* ObjectInfo = new FManagedObjectInfo(Object, Tag, SignificanceFunction);
	
	// Calculate initial significance
	if (Viewpoints.Num())
	{
		SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_InitialSignificanceUpdate);
		ObjectInfo->UpdateSignificance(Viewpoints);
	}

	ManagedObjects.Add(Object, ObjectInfo);
	TArray<const FManagedObjectInfo*>& ManagedObjectInfos = ManagedObjectsByTag.FindOrAdd(Tag);

	if (ManagedObjectInfos.Num() > 0)
	{
		// Insert in to the sorted list
		int32 LowIndex = 0;
		int32 HighIndex = ManagedObjectInfos.Num() - 1;
		auto CompareFunction = PickCompareBySignificance(bSortSignificanceAscending);
		while (true)
		{
			int32 MidIndex = LowIndex + (HighIndex - LowIndex) / 2;
			if (CompareFunction(*ObjectInfo, *ManagedObjectInfos[MidIndex]))
			{
				if (LowIndex == MidIndex)
				{
					ManagedObjectInfos.Insert(ObjectInfo, LowIndex);
					break;
				}
				else
				{
					HighIndex = MidIndex - 1;
				}
			}
			else if (LowIndex == HighIndex)
			{
				ManagedObjectInfos.Insert(ObjectInfo, LowIndex + 1);
				break;
			}
			else
			{
				LowIndex = MidIndex + 1;
			}
		}
	}
	else
	{
		ManagedObjectInfos.Add(ObjectInfo);
	}
}

void USignificanceManager::UnregisterObject(const UObject* Object)
{
	DEC_DWORD_STAT(STAT_SignificanceManager_NumObjects);
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_UnregisterObject);

	FManagedObjectInfo* ObjectInfo;
	if (ManagedObjects.RemoveAndCopyValue(Object, ObjectInfo))
	{
		TArray<const FManagedObjectInfo*>& ObjectsWithTag = ManagedObjectsByTag.FindChecked(ObjectInfo->GetTag());
		if (ObjectsWithTag.Num() == 1)
		{
			check(ObjectsWithTag[0] == ObjectInfo);
			ManagedObjectsByTag.Remove(ObjectInfo->GetTag());
		}
		else
		{
			ObjectsWithTag.RemoveSingle(ObjectInfo);
		}
		delete ObjectInfo;
	}
}

const TArray<const USignificanceManager::FManagedObjectInfo*>& USignificanceManager::GetManagedObjects(const FName Tag) const
{
	if (const TArray<const FManagedObjectInfo*>* ObjectsWithTag = ManagedObjectsByTag.Find(Tag))
	{
		return *ObjectsWithTag;
	}

	static const TArray<const FManagedObjectInfo*> EmptySet;
	return EmptySet;
}

void USignificanceManager::GetManagedObjects(TArray<const USignificanceManager::FManagedObjectInfo*>& OutManagedObjects, bool bInSignificanceOrder) const
{
	OutManagedObjects.Reserve(ManagedObjects.Num());
	for (const TPair<FName, TArray<const FManagedObjectInfo*>>& TagToObjectInfoArrayPair : ManagedObjectsByTag)
	{
		OutManagedObjects.Append(TagToObjectInfoArrayPair.Value);
	}
	if (bInSignificanceOrder)
	{
		OutManagedObjects.Sort(PickCompareBySignificance(bSortSignificanceAscending));
	}
}

float USignificanceManager::GetSignificance(const UObject* Object) const
{
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceCheck);
	float Significance = 0.f;
	if (FManagedObjectInfo* const* Info = ManagedObjects.Find(Object))
	{
		Significance = (*Info)->GetSignificance();
	}
	return Significance;
}

bool USignificanceManager::QuerySignificance(const UObject* Object, float& OutSignificance) const
{
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceCheck);
	if (FManagedObjectInfo* const* Info = ManagedObjects.Find(Object))
	{
		OutSignificance = (*Info)->GetSignificance();
		return true;
	}
	else
	{
		OutSignificance = 0.0f;
		return false;
	}
}

void USignificanceManager::FManagedObjectInfo::UpdateSignificance(const TArray<FTransform>& InViewpoints)
{
	if (InViewpoints.Num())
	{
		Significance = TNumericLimits<float>::Lowest();
		for (const FTransform& Viewpoint : InViewpoints)
		{
			const float ViewpointSignificance = SignificanceFunction(Object, Viewpoint);
			if (ViewpointSignificance > Significance)
			{
				Significance = ViewpointSignificance;
			}
		}
	}
	else
	{
		Significance = 0.f;
	}
}

void USignificanceManager::Update(const TArray<FTransform>& InViewpoints)
{
	Viewpoints = InViewpoints;

	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_Update);

	{
		SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceUpdate);
		//TODO: Consider parallel for
		for (TPair<const UObject*, FManagedObjectInfo*>& ObjectToObjectInfoPair : ManagedObjects)
		{
			const UObject* Object = ObjectToObjectInfoPair.Key;
			FManagedObjectInfo* ObjectInfo = ObjectToObjectInfoPair.Value;

			checkSlow(Object->IsValidLowLevel());

			ObjectInfo->UpdateSignificance(Viewpoints);
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceSort);
		for (TPair<FName, TArray<const FManagedObjectInfo*>>& TagToObjectInfoArrayPair : ManagedObjectsByTag)
		{
			TagToObjectInfoArrayPair.Value.Sort(PickCompareBySignificance(bSortSignificanceAscending));
		}
	}
}

static int32 GSignificanceManagerObjectsToShow = 15;
static FAutoConsoleVariableRef CVarSignificanceManagerObjectsToShow(
	TEXT("SignificanceManager.ObjectsToShow"),
	GSignificanceManagerObjectsToShow,
	TEXT("How many objects to display when ShowDebug SignificanceManager is enabled.\n"),
	ECVF_Cheat
	);

static FAutoConsoleVariable CVarSignificanceManagerFilterTag(
	TEXT("SignificanceManager.FilterTag"),
	TEXT(""),
	TEXT("Only display objects with the specified filter tag.  If None objects with any will be displayed.\n"),
	ECVF_Cheat
	);


void USignificanceManager::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	static const FName NAME_SignificanceManager("SignificanceManager");
	if (Canvas && HUD->ShouldDisplayDebug(NAME_SignificanceManager))
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			if (USignificanceManager* SignificanceManager = Get(HUD->GetWorld()))
			{
				SignificanceManager->OnShowDebugInfo(HUD, Canvas, DisplayInfo, YL, YPos);
			}
		}
		else
		{
			FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
			DisplayDebugManager.SetFont(GEngine->GetSmallFont());
			DisplayDebugManager.SetDrawColor(FColor::Red);
			DisplayDebugManager.DrawString(FString::Printf(TEXT("SIGNIFICANCE MANAGER - %d Managed Objects"), ManagedObjects.Num()));

			const FName SignificanceManagerTag(*CVarSignificanceManagerFilterTag->GetString());
			TArray<const FManagedObjectInfo*> AllObjects;
			const TArray<const FManagedObjectInfo*>& ObjectsToShow = (SignificanceManagerTag.IsNone() ? AllObjects : GetManagedObjects(SignificanceManagerTag));
			if (SignificanceManagerTag.IsNone())
			{
				GetManagedObjects(AllObjects, true);
			}

			DisplayDebugManager.SetDrawColor(FColor::White);
			const int32 NumObjectsToShow = FMath::Min(GSignificanceManagerObjectsToShow, ObjectsToShow.Num());
			for (int32 Index = 0; Index < NumObjectsToShow; ++Index)
			{
				const FManagedObjectInfo* ObjectInfo = ObjectsToShow[Index];
				const FString Str = FString::Printf(TEXT("%6.3f - %s (%s)"), ObjectInfo->GetSignificance(), *ObjectInfo->GetObject()->GetName(), *ObjectInfo->GetTag().ToString());
				DisplayDebugManager.DrawString(Str);
			}
		}
	}
}
