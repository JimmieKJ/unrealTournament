// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintNodeSpawnerUtils.h"

/*******************************************************************************
 * BlueprintActionDatabaseRegistrarImpl
 ******************************************************************************/

namespace BlueprintActionDatabaseRegistrarImpl
{
	static UObject const* ResolveClassKey(UClass const* ClassKey);
	static UObject const* ResolveActionKey(UObject const* UserPassedKey);
}

//------------------------------------------------------------------------------
static UObject const* BlueprintActionDatabaseRegistrarImpl::ResolveClassKey(UClass const* ClassKey)
{
	UObject const* ResolvedKey = ClassKey;
	if (UBlueprintGeneratedClass const* BlueprintClass = Cast<UBlueprintGeneratedClass>(ClassKey))
	{
		ResolvedKey = CastChecked<UBlueprint>(BlueprintClass->ClassGeneratedBy);
	}
	return ResolvedKey;
}

//------------------------------------------------------------------------------
static UObject const* BlueprintActionDatabaseRegistrarImpl::ResolveActionKey(UObject const* UserPassedKey)
{
	UObject const* ResolvedKey = nullptr;
	if (UClass const* Class = Cast<UClass>(UserPassedKey))
	{
		ResolvedKey = ResolveClassKey(Class);
	}
	// both handled in the IsAsset() case
// 	else if (UUserDefinedEnum const* EnumAsset = Cast<UUserDefinedEnum>(UserPassedKey))
// 	{
// 		ResolvedKey = EnumAsset;
// 	}
// 	else if (UUserDefinedStruct const* StructAsset = Cast<UUserDefinedStruct>(StructOwner))
// 	{
// 		ResolvedKey = StructAsset;
// 	}
	else if (UserPassedKey->IsAsset())
	{
		ResolvedKey = UserPassedKey;
	}
	else if (UField const* MemberField = Cast<UField>(UserPassedKey))
	{
		ResolvedKey = ResolveClassKey(MemberField->GetOwnerClass());
	}

	return ResolvedKey;
}

/*******************************************************************************
 * FBlueprintActionDatabaseRegistrar
 ******************************************************************************/

//------------------------------------------------------------------------------
FBlueprintActionDatabaseRegistrar::FBlueprintActionDatabaseRegistrar(FActionRegistry& Database, FUnloadedActionRegistry& UnloadedDatabase, FPrimingQueue& PrimingQueue, TSubclassOf<UEdGraphNode> DefaultKey)
	: GeneratingClass(DefaultKey)
	, ActionDatabase(Database)
	, UnloadedActionDatabase(UnloadedDatabase)
	, ActionKeyFilter(nullptr)
	, ActionPrimingQueue(PrimingQueue)
{
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner)
{
	UField const* ActionKey = GeneratingClass;
	// if this spawner wraps some member function/property, then we want it 
	// recorded under that class (so we can update the action if the class 
	// changes... like, if the member is deleted, or if one is added)
	if (UField const* MemberField = FBlueprintNodeSpawnerUtils::GetAssociatedField(NodeSpawner))
	{
		ActionKey = MemberField;
	}
	return AddActionToDatabase(ActionKey, NodeSpawner);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(UClass const* ClassOwner, UBlueprintNodeSpawner* NodeSpawner)
{
	// @TODO: assert that AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner) 
	//        wouldn't come up with a different key (besides GeneratingClass)

	// ResolveActionKey() is used on ClassOwner (in AddActionToDatabase), to 
	// convert it into a proper key
	return AddActionToDatabase(ClassOwner, NodeSpawner);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(UEnum const* EnumOwner, UBlueprintNodeSpawner* NodeSpawner)
{
	// @TODO: assert that AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner) 
	//        wouldn't come up with a different key (besides GeneratingClass)

	// ResolveActionKey() is used on EnumOwner (in AddActionToDatabase), to 
	// convert it into a proper key
	return AddActionToDatabase(EnumOwner, NodeSpawner);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(UScriptStruct const* StructOwner, UBlueprintNodeSpawner* NodeSpawner)
{
	// @TODO: assert that AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner) 
	//        wouldn't come up with a different key (besides GeneratingClass)

	// ResolveActionKey() is used on StructOwner (in AddActionToDatabase), to 
	// convert it into a proper key
	return AddActionToDatabase(StructOwner, NodeSpawner);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(UField const* FieldOwner, UBlueprintNodeSpawner* NodeSpawner)
{
	// @TODO: assert that AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner) 
	//        wouldn't come up with a different key (besides GeneratingClass)

	// ResolveActionKey() is used on FieldOwner (in AddActionToDatabase), to 
	// convert it into a proper key
	return AddActionToDatabase(FieldOwner, NodeSpawner);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(UObject const* AssetOwner, UBlueprintNodeSpawner* NodeSpawner)
{
	// @TODO: assert that AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner) 
	//        wouldn't come up with a different key (besides GeneratingClass)

	// cannot record an action under any ol' object (we want to associate them 
	// with asset/class outers that are subject to change; so that we can 
	// refresh/rebuild corresponding actions when that happens).
	check(AssetOwner->IsAsset());
	return AddActionToDatabase(AssetOwner, NodeSpawner);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddBlueprintAction(FAssetData const& AssetDataOwner, UBlueprintNodeSpawner* NodeSpawner)
{
	bool bReturnResult = false;

	// @TODO: assert that AddBlueprintAction(UBlueprintNodeSpawner* NodeSpawner) 
	//        wouldn't come up with a different key (besides GeneratingClass)
	if(AssetDataOwner.IsAssetLoaded())
	{
		bReturnResult = AddBlueprintAction(AssetDataOwner.GetAsset(), NodeSpawner);
	}
	else
	{
		bReturnResult = AddBlueprintAction(NodeSpawner->NodeClass, NodeSpawner);
		if(bReturnResult)
		{
			TArray<UBlueprintNodeSpawner*>& ActionList = UnloadedActionDatabase.FindOrAdd(AssetDataOwner.ObjectPath);
			ActionList.Add(NodeSpawner);
		}
	}
	return bReturnResult;
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::IsOpenForRegistration(UObject const* OwnerKey)
{
	UObject const* ActionKey = BlueprintActionDatabaseRegistrarImpl::ResolveActionKey(OwnerKey);
	if (ActionKey == nullptr)
	{
		ActionKey = GeneratingClass;
	}
	return (ActionKey != nullptr) && ((ActionKeyFilter == nullptr) || (ActionKeyFilter == ActionKey));
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::IsOpenForRegistration(FAssetData const& AssetKey)
{
	UObject const* OwnerKey = GeneratingClass;
	if (AssetKey.IsAssetLoaded())
	{
		OwnerKey = AssetKey.GetAsset();
	}
	return IsOpenForRegistration(OwnerKey);
}

//------------------------------------------------------------------------------
bool FBlueprintActionDatabaseRegistrar::AddActionToDatabase(UObject const* ActionKey, UBlueprintNodeSpawner* NodeSpawner)
{
	ensureMsg(NodeSpawner->NodeClass == GeneratingClass, TEXT("We expect a nodes to add only spawners for its own type... Maybe a sub-class is adding nodes it shouldn't?"));
	if (IsOpenForRegistration(ActionKey))
	{
		ActionKey = BlueprintActionDatabaseRegistrarImpl::ResolveActionKey(ActionKey);
		if (ActionKey == nullptr)
		{
			ActionKey = GeneratingClass;
		}
		FBlueprintActionDatabase::FActionList& ActionList = ActionDatabase.FindOrAdd(ActionKey);
		
		int32* QueuedIndex = ActionPrimingQueue.Find(ActionKey);
		if (QueuedIndex == nullptr)
		{
			int32 PrimingIndex = ActionList.Num();
			ActionPrimingQueue.Add(ActionKey, PrimingIndex);
		}

		ActionList.Add(NodeSpawner);
		return true;
	}
	return false;
}