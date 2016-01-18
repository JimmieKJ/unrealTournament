// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "McpProfileSysPCH.h"
#include "OnlineIdentityMcp.h"
#include "McpProfile.h"
#include "McpProfileGroup.h"
#include "McpItemAware.h"
#include "GameServiceMcp.h"
#include "McpQueryResult.h"
#include "JsonUtilities.h"
#include "OnlineHttpRequest.h"
#include "OnlineNotification.h"

///////////////////////////////////////////////////////////////////////////////////////////
// FMcpItemsContainer

struct FMcpItemsContainer
{
	friend class UMcpProfile;
public:
	FMcpItemsContainer()
	{
	}

	//Get an MCPItem by InstanceId
	TSharedPtr<const FMcpItem> GetItem(const FString& InstanceId) const
	{
		const TSharedRef<FMcpItem>* ItemValue = Items.Find(InstanceId);
		return ItemValue ? *ItemValue : TSharedPtr<const FMcpItem>();
	}

	//Gets all items of specified type
	void GetItemsByType(const FString& ItemType, TArray< TSharedPtr<const FMcpItem> >& OutItems) const
	{
		const TSet<FString>* WantedItems = GetItemsByType(ItemType);
		if (WantedItems)
		{
			OutItems.Empty();

			for (const auto& It : *WantedItems)
			{
				const TSharedPtr<const FMcpItem>& Item = GetItem(It);
				OutItems.Add(Item);
			}
		}
	}

	//Get a set of MCPItem InstanceIds for all items of the given ItemType
	const TSet<FString>* GetItemsByType(const FString& ItemType) const
	{
		return ItemsByType.Find(ItemType);
	}

	//Add FMCPItem to the assorted Items arrays
	void AddItem(const TSharedRef<FMcpItem>& ItemToAdd)
	{
		Items.Add(ItemToAdd->InstanceId, ItemToAdd);
		TSet<FString> &TypeSet = ItemsByType.FindOrAdd(ItemToAdd->ItemType);
		TypeSet.Add(ItemToAdd->InstanceId);
	}

	//Remove a FMcpItem from the assorted Items arrays. Returns true if successful.
	bool RemoveItem(FString ItemId)
	{
		TSharedPtr<const FMcpItem> Item = GetItem(ItemId);
		if (Item.IsValid())
		{
			TSet<FString> &TypeSet = ItemsByType.FindOrAdd(Item->ItemType);
			TypeSet.Remove(Item->InstanceId);
			Items.Remove(Item->InstanceId);
			return true;
		}
		return false;
	}

	//Empty all of the items
	void EmptyItems()
	{
		Items.Empty();
		ItemsByType.Empty();
	}

protected:
	//The following functions are protected because they allow for editing the data
	//and only the MCPProfile class should be able to do that safely

	//Get an editable item. ONLY THE MCPPROFILE SHOULD BE USING THIS
	TSharedPtr<FMcpItem> GetEditableItem(const FString& InstanceId) const
	{
		const TSharedRef<FMcpItem>* ItemValue = Items.Find(InstanceId);
		return ItemValue ? *ItemValue : TSharedPtr<FMcpItem>();
	}

	//Getters for editable copies of the data
	const TMap<FString, TSharedRef<FMcpItem> >& GetAllItems() const
	{
		return Items;
	}

private:
	/** Map from item unique ID to item structure */
	TMap<FString, TSharedRef<FMcpItem> > Items;

	// Map of ItemType to array of MCPItems which have that ItemType
	TMap<FString, TSet<FString> > ItemsByType;
};

///////////////////////////////////////////////////////////////////////////

//static 
const FString UMcpProfile::MTX_TEMPLATE_IDS[] = { 
	FString(TEXT("Currency.MtxPurchased")), 
	FString(TEXT("Currency.MtxPurchaseBonus")),
	FString(TEXT("Currency.MtxGiveaway")), // earned in game
	FString(TEXT("Currency.MtxComplimentary"))
};

UMcpProfile::UMcpProfile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProfileRevision = -1;
	FullProfileQueryQueued = 0;
	OnlineSubMcp = nullptr;
	MtxBalanceCacheVersion = -1;
	MtxBalanceCache = 0;
	ProfileGroup = nullptr;
	ItemsContainer = MakeShareable(new FMcpItemsContainer());
	bShouldCreateInstances = true;
}

void UMcpProfile::Initialize(UMcpProfileGroup* ProfileGroupIn, const FString& ProfileIdIn)
{
	// profile group reference
	check(ProfileGroupIn);
	ProfileGroup = ProfileGroupIn;

	// get our profileId
	check(!ProfileIdIn.IsEmpty());
	ProfileId = ProfileIdIn;

	// copy the Online subsystem (may be null in nomcp mode)
	OnlineSubMcp = ProfileGroup->GetOnlineSubMcp();

	// build debug name since we use it for logging a lot
	DebugName = FString::Printf(TEXT("%s accountId=%s profileId=%s"), *GetProfileGroup().GetPlayerName(), *GetProfileGroup().GetGameAccountId().ToString(), *GetProfileId());
}

void UMcpProfile::Reset()
{
	ProfileRevision = -1;
	FullProfileQueryQueued = 0;
	MtxBalanceCacheVersion = -1;
	MtxBalanceCache = 0;
	ItemsContainer = MakeShareable(new FMcpItemsContainer());
	Stats.Empty();
}

int32 UMcpProfile::GetItemCount(const FString& TemplateId) const
{
	int32 Quantity = 0;
	for (const auto& It : ItemsContainer->GetAllItems())
	{
		if (It.Value->TemplateId == TemplateId)
		{
			Quantity += It.Value->Quantity;
		}
	}
	return Quantity;
}

int32 UMcpProfile::GetMtxBalance() const
{
	if (MtxBalanceCacheVersion != ProfileRevision)
	{
		MtxBalanceCacheVersion = ProfileRevision;
		MtxBalanceCache = 0;
		for (const auto& TemplateId : MTX_TEMPLATE_IDS)
		{
			MtxBalanceCache += GetItemCount(TemplateId);
		}
	}
	return MtxBalanceCache;
}

const TMap<FString, TSharedRef<FMcpItem> >& UMcpProfile::GetAllItems() const
{
	return ItemsContainer->GetAllItems();
}

TSharedPtr<const FMcpItem> UMcpProfile::GetItem(const FString& InstanceId) const
{
	return ItemsContainer->GetItem(InstanceId);
}

FString UMcpProfile::ToDebugString() const
{
	FString Output;
	Output += FString::Printf(TEXT("%s's Stats:\n"), *GetProfileGroup().GetPlayerName());
	for (const auto& It : Stats)
	{
		Output += FString::Printf(TEXT("  %s = %s\n"), *It.Key, *It.Value->AsString());
	}
	Output += FString::Printf(TEXT("%s's Inventory:\n"), *GetProfileGroup().GetPlayerName());
	for (const auto& It : ItemsContainer->GetAllItems())
	{
		Output += FString::Printf(TEXT("  %d x %s [%s]\n"), It.Value->Quantity, *It.Value->TemplateId, *It.Value->InstanceId);
	}
	return Output;
}

void UMcpProfile::CheatResetProfile(bool bAllProfiles)
{
	// these get queued up so this works without callbacks
	if (bAllProfiles)
	{
		DeleteAllProfiles(FClientUrlContext::Default);
		GetProfileGroup().RefreshAllProfiles();
	}
	else
	{
		DeleteProfile(FClientUrlContext::Default);
		ForceQueryProfile(FMcpQueryComplete());
	}
}

void UMcpProfile::ForceQueryProfile(const FMcpQueryComplete& Callback)
{
	// Send off async requests to query both the XP profile and the schematics profile
	if (GetProfileGroup().IsActingAsServer())
	{
		FDedicatedServerUrlContext Context(Callback);
		ForceQueryProfile(Context);
	}
	else
	{
		FClientUrlContext Context(Callback);
		ForceQueryProfile(Context);
	}
}

void UMcpProfile::ForceQueryProfile(FBaseUrlContext& Context)
{
	if (FullProfileQueryQueued < 2) // allow one queued and one in-flight
	{
		if (OnlineSubMcp)
		{
			UE_LOG(LogProfileSys, Verbose, TEXT("ForceQueryProfile for %s"), *DebugName);

			++FullProfileQueryQueued;

			// wrap the context callback in OnQueryProfileComplete
			Context.SetCallback(FMcpQueryComplete::CreateUObject(this, &UMcpProfile::OnQueryProfileComplete, Context.GetCallback()));

			QueryProfile(Context);
		}
		else
		{
			UE_LOG(LogProfileSys, Warning, TEXT("Unable to query profile due to no Mcp for %s"), *DebugName);

			// spoof a valid (blank) profile
			ProfileRevision = 0;

			++FullProfileQueryQueued;

			OnQueryProfileComplete(FMcpQueryResult(true), Context.GetCallback());
		}
	}
	else
	{
		UE_LOG(LogProfileSys, Display, TEXT("Caught overlapping attempts to query same user's profile, will be called when previous call completes. User %s"), *DebugName);
		PendingForceQueryDelegates.Add(Context.GetCallback());
	}

}

void UMcpProfile::OnQueryProfileComplete(const FMcpQueryResult& Response, FMcpQueryComplete OuterCallback)
{
	if (Response.bSucceeded)
	{
		UE_LOG(LogProfileSys, Verbose, TEXT("OnQueryProfileComplete succeeded for %s HTTP=%d"), *DebugName, Response.HttpResult);
	}
	else
	{
		UE_LOG(LogProfileSys, Warning, TEXT("OnQueryProfileComplete failed for %s HTTP=%d ErrorCode=%s"), *DebugName, Response.HttpResult, *Response.ErrorCode);
	}
	
	// if we've never successfully queried, treat this as a revision change
	if (ProfileRevision < 0)
	{
		--ProfileRevision;
	}

	check(FullProfileQueryQueued > 0);

	// just un-set the IsQueryingProfile command so we can force-query again if necessary
	--FullProfileQueryQueued;

	// fire the outer callback
	OuterCallback.ExecuteIfBound(Response);

	TArray<FMcpQueryComplete> DelegatesCopy = PendingForceQueryDelegates;
	PendingForceQueryDelegates.Empty();

	// fire delegates for any/all ForceQueryProfile calls made while this one was in flight
	for (const FMcpQueryComplete& ExtraDelegate : DelegatesCopy)
	{
		ExtraDelegate.ExecuteIfBound(Response);
	}
}

bool UMcpProfile::CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack )
{
	// If this is a net request function, forward it to the MCP
	if ((Function->FunctionFlags & FUNC_NetRequest))
	{
		return GetProfileGroup().TriggerMcpRpcCommand(Function, Parameters, this);
	}
	return false;
}

//
// Return whether a function should be executed remotely.
//
int32 UMcpProfile::GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack )
{
	if (Function->FunctionFlags & FUNC_NetRequest)
	{
		// Call remote
		return FunctionCallspace::Remote;
	}	

	// going to call remotely
	return FunctionCallspace::Local;
}

void UMcpProfile::HandleMcpResponse(const TSharedPtr<FJsonObject>& JsonPayload, FMcpQueryResult& QueryResult)
{
	// update the profile revision
	// NOTE: we only have 53-bits precision here (double), but unexpected for profile revision number to get anywhere near that high
	int64 OldProfileRevision = ProfileRevision;
	int64 NewProfileRevision = (int64)JsonPayload->GetNumberField(TEXT("profileRevision"));

	// see if there was any change at all
	if (OldProfileRevision != NewProfileRevision)
	{
		// get base version but try to be backwards compatible
		int64 BaseProfileRevision = (int64)JsonPayload->GetNumberField(TEXT("profileChangesBaseRevision"));
		if (BaseProfileRevision < 0)
		{
			// this was the previous assumption before profileChangesBaseRevision was introduced, it's generally safe, but can result in unnecessary Queries
			BaseProfileRevision = NewProfileRevision - 1;
			UE_LOG(LogProfileSys, Warning, TEXT("MCP-Profile: Missing new 'profileChangesBaseRevision' key. Assuming base of %lld"), BaseProfileRevision);
		}

		// process any profileChanges
		bool bAnyStatsUpdated = false;
		TSet<FString> ItemTypesUpdated;
		if (JsonPayload->HasTypedField<EJson::Array>(TEXT("profileChanges")))
		{
			const TArray< TSharedPtr<FJsonValue> >& Changes = JsonPayload->GetArrayField(TEXT("profileChanges"));
			if (Changes.Num() > 0)
			{
				// look for a full update first 
				bool bAppliedUpdate = false;
				for (const auto& It : Changes)
				{
					TSharedPtr<FJsonObject> Change = It->AsObject();
					FString ChangeType = Change->GetStringField(TEXT("changeType"));
					if (ChangeType == TEXT("fullProfileUpdate"))
					{
						if (HandleFullProfileUpdate(Change->GetObjectField(TEXT("profile")), &bAnyStatsUpdated, &ItemTypesUpdated, NewProfileRevision, OldProfileRevision))
						{
							bAppliedUpdate = true;
							ProfileRevision = NewProfileRevision;
							break; // this should be the only update if sent
						}
						else
						{
							UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Received full profile update to revision %lld from MCP but was unable to apply it."), NewProfileRevision);
						}
					}
				}

				// then check for deltas
				if (!bAppliedUpdate)
				{
					if (OldProfileRevision < 0)
					{
						// trigger a re-query on this profile
						UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Received profile deltas from MCP while in inteterminate state (rev = %lld)"), OldProfileRevision);
						ProfileRevision = -1;
					}
					else if (OldProfileRevision != BaseProfileRevision)
					{
						// trigger a re-query on this profile
						UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Received profile deltas but was not on the previous revision. Was on %lld but got revision %lld"), OldProfileRevision, NewProfileRevision);
						ProfileRevision = -1;
					}
					else
					{
						// this is a good delta, take the new revision
						ProfileRevision = NewProfileRevision;
					}

					UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: Delta profile update (revision %lld->%lld) for %s."), OldProfileRevision, NewProfileRevision, *DebugName);

					// look for delta updates
					// NOTE: even if this is an invalid delta, go ahead and apply it (profile refresh will fix us up
					for (const auto& It : Changes)
					{
						TSharedPtr<FJsonObject> Change = It->AsObject();
						FString ChangeType = Change->GetStringField(TEXT("changeType"));
						if (!HandleProfileDeltaCommand(Change, &bAnyStatsUpdated, &ItemTypesUpdated, NewProfileRevision, BaseProfileRevision))
						{
							UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Received profile delta %s %lld->%lld from MCP but was unable to apply it."), *ChangeType, BaseProfileRevision, NewProfileRevision);
						}
					}
				}

				// force a query if we got information indicating this profile is out of date
				if (ProfileRevision < 0)
				{
					ForceQueryProfile(FMcpQueryComplete());
				}
			}
		}

		// dispatch profile change delegates
		if (bAnyStatsUpdated)
		{
			StatsUpdatedDelegate.Broadcast(NewProfileRevision);
		}
		if (ItemTypesUpdated.Num() > 0)
		{
			ItemsUpdatedDelegate.Broadcast(ItemTypesUpdated, NewProfileRevision);
		}
	}

	// handle notifications
	if (JsonPayload->HasTypedField<EJson::Array>(TEXT("notifications")))
	{
		const TArray< TSharedPtr<FJsonValue> >& Notifications = JsonPayload->GetArrayField(TEXT("notifications"));
		for (const auto& It : Notifications)
		{
			const TSharedPtr<FJsonObject>& Notification = It->AsObject();
			FString PayloadType = Notification->GetStringField(TEXT("type"));
			bool bIsPrimary = Notification->GetBoolField(TEXT("primary"));

			// see if we got an explicit payload (new style)
			TSharedPtr<FJsonValue> Payload = Notification->TryGetField(TEXT("payload"));
			FOnlineNotification OnlineNote = FOnlineNotification(PayloadType, Payload.IsValid() ? Payload : It);
			OnlineNote.ToUserId = GetProfileGroup().GetGameAccountId().GetUniqueNetId();

			// if this is a primary notification, store it on the response
			if (bIsPrimary)
			{
				// log if we're replacing another one (not expected)
				if (!QueryResult.PrimaryNotification.TypeStr.IsEmpty())
				{
					UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Stomping primary notification of type %s with another of type %s (we expect only one primary per response!)."), 
						*QueryResult.PrimaryNotification.TypeStr, *OnlineNote.TypeStr);
				}

				// store the notification
				QueryResult.PrimaryNotification = OnlineNote;
			}
			else
			{
				// tell the delegate
				HandleNotificationDelegate.ExecuteIfBound(OnlineNote);
			}
		}
	}
}

static bool CompareEqual(const TMap< FString, TSharedPtr<FJsonValue> >& Lhs, const TMap< FString, TSharedPtr<FJsonValue> >& Rhs)
{
	if (Lhs.Num() != Rhs.Num())
	{
		return false;
	}

	// compare each element
	for (const auto& It : Lhs)
	{
		const FString& Key = It.Key;
		const TSharedPtr<FJsonValue>* RhsValue = Rhs.Find(Key);
		if (!RhsValue)
		{
			// not found in both objects
			return false;
		}
		const TSharedPtr<FJsonValue>& LhsValue = It.Value;
		if (LhsValue.IsValid() != RhsValue->IsValid())
		{
			return false;
		}
		if (LhsValue.IsValid())
		{
			if (!FJsonValue::CompareEqual(*LhsValue.Get(), *RhsValue->Get()))
			{
				return false;
			}
		}
	}

	return true;
}

TSharedPtr<const FMcpItem> UMcpProfile::FindItemByTemplate(const FString& TemplateId) const
{
	for (const auto& It : ItemsContainer->GetAllItems())
	{
		if (It.Value->TemplateId == TemplateId)
		{
			return It.Value;
		}
	}
	return TSharedPtr<FMcpItem>();
}

bool UMcpProfile::HandleProfileDeltaCommand(const TSharedPtr<FJsonObject>& Change, bool* OutStatsUpdated, TSet<FString>* OutItemTypesUpdated, int64 NewProfileRevision, int64 OldProfileRevision)
{
	FString ChangeType = Change->GetStringField(TEXT("changeType"));
	if (ChangeType == TEXT("itemAdded"))
	{
		TSharedPtr<FJsonObject> ItemDef = Change->GetObjectField(TEXT("item"));
		if (ItemDef.IsValid())
		{
			FString ItemId = Change->GetStringField(TEXT("itemId"));
			TSharedPtr<FMcpItem> ItemPtr =  ItemsContainer->GetEditableItem(ItemId);
			TSharedRef<FMcpItem> NewItem = ItemPtr.IsValid() ? ItemPtr.ToSharedRef() : MakeShareable(new FMcpItem(ItemId));

			if (NewItem->Initialize(ItemDef.ToSharedRef(), NewProfileRevision, this))
			{
				// add it to our map
				if (!ItemPtr.IsValid())
				{
					ItemsContainer->AddItem(NewItem);

					// log it
					UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s gained %d x %s [%s]"), *DebugName, NewItem->Quantity, *NewItem->TemplateId, *NewItem->InstanceId);
				}
				else
				{
					UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s replaced %d x %s [%s]"), *DebugName, NewItem->Quantity, *NewItem->TemplateId, *NewItem->InstanceId);
				}

				// mark this type as updated
				OutItemTypesUpdated->Add(NewItem->ItemType);
			}
			else
			{
				UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Unable to add new MCP item [%s] for %s because it did not parse correctly."), *NewItem->InstanceId, *DebugName);
				return false;
			}
		}
		else
		{
			UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: itemAdded for %s did not contain an item JSON entry."), *DebugName);
			return false;
		}
	}
	else if (ChangeType == TEXT("itemRemoved"))
	{
		FString ItemId = Change->GetStringField(TEXT("itemId"));
		TSharedPtr<FMcpItem> ItemPtr = ItemsContainer->GetEditableItem(ItemId);
		if (ItemPtr.IsValid())
		{
			ItemPtr->LastUpdate = NewProfileRevision;

			// log it
			UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s lost %d x %s"), *DebugName, ItemPtr->Quantity, *ItemPtr->TemplateId);

			// remove it
			if (ItemPtr->Instance)
			{
				IMcpItemAware* ItemAware = Cast<IMcpItemAware>(ItemPtr->Instance);
				if (ItemAware)
				{
					ItemAware->ProcessItemDestroy();
				}
				ItemPtr->Instance = NULL;
			}
			// mark this type as updated
			OutItemTypesUpdated->Add(ItemPtr->ItemType);

			RemoveItem(ItemPtr->InstanceId);
		}
		else
		{
			UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Unable to remove MCP item %s from %s because it does not exist in our cache."), *ItemId, *DebugName);
			return false;
		}
	}
	else if (ChangeType == TEXT("itemAttrChanged"))
	{
		FString ItemId = Change->GetStringField(TEXT("itemId"));
		TSharedPtr<FMcpItem> ItemPtr = ItemsContainer->GetEditableItem(ItemId);
		if (ItemPtr.IsValid())
		{
			ItemPtr->LastUpdate = NewProfileRevision;
			FString AttrName = Change->GetStringField(TEXT("attributeName"));

			// if this has an attributeValue then it's a change/add
			TSharedPtr<FJsonValue> AttrValue = Change->TryGetField(TEXT("attributeValue"));
			if (AttrValue.IsValid() && AttrValue->IsNull())
			{
				AttrValue.Reset();
			}

			// process add or remove
			if (AttrValue.IsValid())
			{
				// set the new attribute
				ItemPtr->Attributes.Add(AttrName, AttrValue);

				UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: %s, %s attribute %s changed."), *DebugName, *ItemPtr->TemplateId, *AttrName);
			}
			else
			{
				// otherwise remove the existing attribute
				ItemPtr->Attributes.Remove(AttrName);

				UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: %s, %s attribute %s was removed."), *DebugName, *ItemPtr->TemplateId, *AttrName);
			}

			// mark this type as updated
			OutItemTypesUpdated->Add(ItemPtr->ItemType);

			// update the item instance
			if (ItemPtr->Instance)
			{
				IMcpItemAware* ItemAware = Cast<IMcpItemAware>(ItemPtr->Instance);
				if (ItemAware)
				{
					// try to just update the one attribute
					if (!ItemAware->ProcessAttributeChange(AttrName, AttrValue, NewProfileRevision))
					{
						// ok try to reinitialize
						if (!ItemPtr->InitializeInstance(this, true, true))
						{
							UE_LOG(LogProfileSys, Error, TEXT("Failed to populate item %s of type %s which was mapped to class %s"), *ItemPtr->InstanceId, *ItemPtr->TemplateId, *ItemPtr->Instance->GetClass()->GetName());
							ItemPtr->Instance = NULL;
							return false;
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Unable to remove MCP item %s from %s because it does not exist in our cache."), *ItemId, *DebugName);
			return false;
		}
	}
	else if (ChangeType == TEXT("itemQuantityChanged"))
	{
		FString ItemId = Change->GetStringField(TEXT("itemId"));
		TSharedPtr<FMcpItem> ItemPtr = ItemsContainer->GetEditableItem(ItemId);
		if (ItemPtr.IsValid())
		{
			ItemPtr->LastUpdate = NewProfileRevision;
			int32 NewQuantity = (int32)Change->GetNumberField(TEXT("quantity"));

			// log it
			if (NewQuantity >= ItemPtr->Quantity)
			{
				UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: %s gained %d x %s"), *DebugName, NewQuantity - ItemPtr->Quantity, *ItemPtr->TemplateId);
			}
			else
			{
				UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: %s lost %d x %s"), *DebugName, ItemPtr->Quantity - NewQuantity, *ItemPtr->TemplateId);
			}

			// make the change
			ItemPtr->Quantity = NewQuantity;

			// mark this type as updated
			OutItemTypesUpdated->Add(ItemPtr->ItemType);

			// update the item instance quantity
			if (ItemPtr->Instance)
			{
				IMcpItemAware* ItemAware = Cast<IMcpItemAware>(ItemPtr->Instance);
				if (ItemAware)
				{
					ItemAware->UpdateQuantity(NewQuantity);
				}
			}
		}
		else
		{
			UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Unable to adjust quantity on MCP item %s from %s because it does not exist in our cache."), *ItemId, *DebugName);
			return false;
		}
	}
	else if (ChangeType == TEXT("statModified"))
	{
		*OutStatsUpdated = true;
		FString StatName = Change->GetStringField(TEXT("name"));
		TSharedPtr<FJsonValue> StatValue = Change->TryGetField(TEXT("value"));
		if (StatValue.IsValid())
		{
			Stats.Add(StatName, StatValue);
		}
		else
		{
			UE_LOG(LogProfileSys, Warning, TEXT("MCP-Profile: Unable to process statModified change to %s stat %s."), *DebugName, *StatName);
			return false;
		}
	}
	else
	{
		UE_LOG(LogProfileSys, Warning, TEXT("MCP-Profile: %s Unable to process profile change of type %s."), *DebugName, *ChangeType);
		return false;
	}

	return true;
}

bool UMcpProfile::GenerateProfileDeltaCommand(FMcpProfileChangeRequest& Changes, TSharedPtr<const FMcpItem> OldItem, TSharedPtr<const FMcpItem> NewItem)
{
	bool bChanged = false;

	if (!OldItem.IsValid() && !NewItem.IsValid())
	{
		// Doesn't exist
		return false;
	}
	if (!OldItem.IsValid())
	{
		// New item, add it
		FMcpAddItemRequest AddRequest;
		AddRequest.ItemId = NewItem->InstanceId;
		AddRequest.TemplateId = NewItem->TemplateId;
		AddRequest.Quantity = NewItem->Quantity;
		AddRequest.Attributes.JsonObject = MakeShareable(new FJsonObject());
		AddRequest.Attributes.JsonObject->Values = NewItem->Attributes;

		Changes.AddRequests.Add(AddRequest);

		return true;
	}
	if (!NewItem.IsValid())
	{
		// Item removed, delete it
		FMcpRemoveItemRequest RemoveRequest;
		RemoveRequest.ItemId = OldItem->InstanceId;

		Changes.RemoveRequests.Add(RemoveRequest);

		return true;
	}

	if (OldItem->InstanceId != NewItem->InstanceId)
	{
		UE_LOG(LogProfileSys, Error, TEXT("UMcpProfile::GenerateProfileDeltaCommand called with non matching items!"));
		return false;
	}

	if (OldItem->Quantity != NewItem->Quantity)
	{
		// Update the quantity
		FMcpChangeQuantityRequest ChangeQuantityRequest;
		ChangeQuantityRequest.ItemId = NewItem->InstanceId;
		ChangeQuantityRequest.DeltaQuantity = NewItem->Quantity - OldItem->Quantity;
		
		Changes.ChangeQuantityRequests.Add(ChangeQuantityRequest);
		bChanged = true;
		// Keep going, there could be attribute changes
	}

	TArray<FString> OldAttributeKeys;
	OldItem->Attributes.GenerateKeyArray(OldAttributeKeys);

	TArray<FString> NewAttributeKeys;
	NewItem->Attributes.GenerateKeyArray(NewAttributeKeys);

	FMcpChangeAttributesRequest ChangeAttributesRequest;
	ChangeAttributesRequest.ItemId = NewItem->InstanceId;
	ChangeAttributesRequest.Attributes.JsonObject = MakeShareable(new FJsonObject());

	// Look through attributes in new list, keep track of what was found
	for (int32 NewIndex = 0; NewIndex < NewAttributeKeys.Num(); NewIndex++)
	{
		FString AttributeKey = NewAttributeKeys[NewIndex];

		const TSharedPtr<FJsonValue>* OldValue = OldItem->Attributes.Find(AttributeKey);
		const TSharedPtr<FJsonValue>* NewValue = NewItem->Attributes.Find(AttributeKey);

		bool bChangedValue = false;

		if (!OldValue || !OldValue->IsValid())
		{
			// No old value, so send it
			if (NewValue && NewValue->IsValid())
			{
				bChangedValue = true;
			}
		}
		else if (NewValue && NewValue->IsValid())
		{
			OldAttributeKeys.Remove(AttributeKey);
			// Remove from list of old attribute keys, as we've processed it

			if (!(**NewValue == **OldValue))
			{
				bChangedValue = true;
			}
		}

		if (bChangedValue)
		{
			// Update the attribute
			ChangeAttributesRequest.Attributes.JsonObject->SetField(AttributeKey, *NewValue);

			bChanged = true;
		}		
	}

	// Look through the old attribute list, clear any attributes that were removed
	// The list now only has attributes that did not exist in NewValue
	for (int32 OldIndex = 0; OldIndex < OldAttributeKeys.Num(); OldIndex++)
	{
		FString AttributeKey = OldAttributeKeys[OldIndex];

		const TSharedPtr<FJsonValue>* OldValue = OldItem->Attributes.Find(AttributeKey);
		
		if (OldValue && OldValue->IsValid())
		{
			// Set it to a null value to clear it
			ChangeAttributesRequest.Attributes.JsonObject->SetField(AttributeKey, MakeShareable(new FJsonValueNull()));

			bChanged = true;
		}
	}

	// If our change has any attributes, add it
	if (ChangeAttributesRequest.Attributes.JsonObject->Values.Num() > 0)
	{
		Changes.ChangeAttributesRequests.Add(ChangeAttributesRequest);
	}

	return bChanged;
}

bool UMcpProfile::BuildProfileChanges(FMcpProfileChangeRequest& Changes, TSet<class IMcpItemAware*>& ItemInstances, TMap<FString, TSharedPtr<FJsonValue> >& ChangedStats)
{
	TSet<IMcpItemAware*> ProcessedInstances;

	bool bChanged = false;

	// TODO: Make sure there are no in progress updates, can't send when waiting for response

	// Set the profile revision this is based on
	Changes.BaseProfileRevision = ProfileRevision;

	if (Changes.BaseProfileRevision == -1)
	{
		// This is going to create the profile, which starts at revision 1
		Changes.BaseProfileRevision = 1;
	}

	// Look through all the items we had before
	for (const auto& It : ItemsContainer->GetAllItems())
	{
		const FMcpItem& Item = It.Value.Get();
		IMcpItemAware* Instance = Cast<IMcpItemAware>(Item.Instance);

		if (Instance && ItemInstances.Contains(Instance))
		{
			ProcessedInstances.Add(Instance);

			if (Instance->HasPendingPersistentChanges())
			{
				TSharedPtr<FMcpItem> NewItem = Instance->CreateMcpItem();

				if (NewItem.IsValid())
				{
					// We have a new item, compare it to the old item
					if (GenerateProfileDeltaCommand(Changes, It.Value, NewItem))
					{
						bChanged = true;
					}
				}
				// Clear dirty even if no changes were found
				Instance->MarkPersistentChangesResolved();
			}
		}
		else if (Instance)
		{
			// Item was deleted 
			if (GenerateProfileDeltaCommand(Changes, It.Value, NULL))
			{
				bChanged = true;
			}
		}
	}

	TSet<IMcpItemAware*> NewInstances = ItemInstances.Difference(ProcessedInstances);
	
	for (auto Instance : NewInstances)
	{
		if (Instance)
		{
			TSharedPtr<FMcpItem> NewItem = Instance->CreateMcpItem();

			if (NewItem.IsValid())
			{
				// Item was added 
				if (GenerateProfileDeltaCommand(Changes, NULL, NewItem))
				{
					bChanged = true;
				}

				// Mark local changes as resolved
				Instance->MarkPersistentChangesResolved();
			}
		}
	}

	for (auto NewStat : ChangedStats)
	{
		bool bStatChanged = false;
		TSharedPtr<FJsonValue>* OldStatValue = Stats.Find(NewStat.Key);

		if (!OldStatValue)
		{
			// Old stat doesn't exist
			bStatChanged = true;
		}
		else if (OldStatValue->IsValid() != NewStat.Value.IsValid())
		{
			// Either new or old stat value is null but other isn't
			bStatChanged = true;
		}
		else if (OldStatValue->IsValid() && NewStat.Value.IsValid())
		{
			if (!(*OldStatValue->Get() == *NewStat.Value.Get()))
			{
				// Node values don't match
				bStatChanged = true;
			}
		}

		if (bStatChanged)
		{
			// Generate this as a json object directly because statValue could be many different value types
			FJsonObjectWrapper ChangeStatRequest;
			ChangeStatRequest.JsonObject = MakeShareable(new FJsonObject());
			ChangeStatRequest.JsonObject->SetStringField(TEXT("statName"), NewStat.Key);
			ChangeStatRequest.JsonObject->SetField(TEXT("statValue"), NewStat.Value);

			Changes.ChangeStatRequests.Add(ChangeStatRequest);
			bChanged = true;
		}
	}

	return bChanged;
}

bool UMcpProfile::HandleFullProfileUpdate(const TSharedPtr<FJsonObject>& ProfileObj, bool* OutStatsUpdated, TSet<FString>* OutItemTypesUpdated, int64 NewProfileRevision, int64 OldProfileRevision)
{
	if (!ProfileObj.IsValid())
	{
		UE_LOG(LogProfileSys, Warning, TEXT("MCP-Profile: Missing profile object in JSON for %s"), *DebugName);
		return false;
	}

	// just verify these were passed in
	check(OutStatsUpdated);
	bool& bAnyStatsUpdated = *OutStatsUpdated;
	check(OutItemTypesUpdated);
	TSet<FString>& ItemTypesUpdated = *OutItemTypesUpdated;

	// log out the version number for debugging
	FString ProfileVersion = ProfileObj->GetStringField(TEXT("version"));
	int32 WipeNumber = ProfileObj->GetIntegerField(TEXT("wipeNumber"));
	UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: Full profile update (rev=%lld, version=%s@w=%d) for %s."), NewProfileRevision, *ProfileVersion, WipeNumber, *DebugName);

	// update stats
	double StatsUpdateStart = FPlatformTime::Seconds();
	const TSharedPtr<FJsonObject>& StatsContainerObj = ProfileObj->GetObjectField(TEXT("stats"));
	const TSharedPtr<FJsonObject>& StatsObj = StatsContainerObj->GetObjectField(TEXT("attributes"));
	TMap< FString, TSharedPtr<FJsonValue> >& NewStats = StatsObj->Values;
	if (!CompareEqual(NewStats, Stats))
	{
		// only mark stats as updated if they actually changed
		bAnyStatsUpdated = true;
		Stats = NewStats;
		if (NewProfileRevision >= 0)
		{
			UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: updated account stats for %s"), *DebugName);
		}
	}

	// throw all current item ids into a set
	TSet<FString> UntouchedItems;
	for (const auto& It : ItemsContainer->GetAllItems())
	{
		UntouchedItems.Add(It.Key);
	}

	// update items
	double ItemsUpdateStart = FPlatformTime::Seconds();
	const TSharedPtr<FJsonObject>& ItemsObj = ProfileObj->GetObjectField(TEXT("items"));
	for (const auto& It : ItemsObj->Values)
	{
		const TSharedPtr<FJsonObject>& ItemDef = It.Value->AsObject();

		// remove this entry from the untouched list since we've seen it now
		if (UntouchedItems.Remove(It.Key) > 0)
		{
			// this is an existing item, see if it changed
			TSharedPtr<FMcpItem> Item = ItemsContainer->GetEditableItem(It.Key);

			if (Item.IsValid())
			{
				// make sure the template is the same
				if (Item->TemplateId != ItemDef->GetStringField(TEXT("templateId")))
				{
					// no matter what happens, this item type has changed
					ItemTypesUpdated.Add(Item->ItemType);

					// template changed, we need to remake it (this is very unusual)
					FString OldTemplateId = Item->TemplateId;
					if (Item->Initialize(ItemDef.ToSharedRef(), NewProfileRevision, this))
					{
						UE_LOG(LogProfileSys, Warning, TEXT("MCP-Profile: %s recreated %d x %s (template change from %s)"), *DebugName, Item->Quantity, *Item->TemplateId, *OldTemplateId);

						// mark the new item type as changed too (possibly different post-Parse)
						ItemTypesUpdated.Add(Item->ItemType);
					}
					else
					{
						// remove the item
						UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: %s, %s changed template type but was unable to parse. Removing item %s."), *DebugName, *OldTemplateId, *It.Key);
						Item->Instance = NULL;

						RemoveItem(Item->InstanceId);
					}
				}
				else
				{
					// see if any attributes changed
					TMap<FString, TSharedPtr<FJsonValue> >& NewAttributes = ItemDef->GetObjectField(TEXT("attributes"))->Values;
					if (!CompareEqual(NewAttributes, Item->Attributes))
					{
						// log the change
						UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s, %s attributes were updated."), *DebugName, *Item->TemplateId);

						// replace all attributes
						Item->LastUpdate = NewProfileRevision;
						Item->Attributes = NewAttributes;

						// mark this type as updated
						ItemTypesUpdated.Add(Item->ItemType);

						// reinit the instance if a change was found
						if (Item->Instance)
						{
							IMcpItemAware* ItemAware = Cast<IMcpItemAware>(Item->Instance);
							if (ItemAware)
							{
								if (!ItemAware->PopulateFromMcpItem(*Item))
								{
									UE_LOG(LogProfileSys, Error, TEXT("Failed to populate item %s of type %s which was mapped to class %s"), *Item->InstanceId, *Item->TemplateId, *Item->Instance->GetClass()->GetName());
									Item->Instance = NULL;
								}
							}
						}
					}

					// see if the quantity changed
					int32 NewQuantity = (int32)ItemDef->GetNumberField(TEXT("quantity"));
					if (Item->Quantity != NewQuantity)
					{
						// log it
						if (NewQuantity >= Item->Quantity)
						{
							UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s gained %d x %s"), *DebugName, NewQuantity - Item->Quantity, *Item->TemplateId);
						}
						else
						{
							UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s lost %d x %s"), *DebugName, Item->Quantity - NewQuantity, *Item->TemplateId);
						}

						// make the change
						Item->LastUpdate = NewProfileRevision;
						Item->Quantity = NewQuantity;

						// mark this type as updated
						ItemTypesUpdated.Add(Item->ItemType);

						// update the item instance quantity
						if (Item->Instance)
						{
							IMcpItemAware* ItemAware = Cast<IMcpItemAware>(Item->Instance);
							if (ItemAware)
							{
								ItemAware->UpdateQuantity(NewQuantity);
							}
						}
					}
				}
			}
		}
		else
		{
			// this is a new item, we should add it
			TSharedRef<FMcpItem> NewItem = MakeShareable(new FMcpItem(It.Key));
			if (NewItem->Initialize(ItemDef.ToSharedRef(), NewProfileRevision, this))
			{
				// add it to our map
				AddItem(NewItem);

				// log it
				if (OldProfileRevision >= 0)
				{
					UE_LOG(LogProfileSys, Log, TEXT("MCP-Profile: %s gained %d x %s"), *DebugName, NewItem->Quantity, *NewItem->TemplateId);
				}

				// mark this type as updated
				ItemTypesUpdated.Add(NewItem->ItemType);
			}
			else
			{
				// unable to add the new item
				UE_LOG(LogProfileSys, Error, TEXT("MCP-Profile: Unable to add new MCP item for %s because it did not parse correctly."), *DebugName);
			}
		}
	}

	// remove any items still in the id set
	double DestroyUntouchedStart = FPlatformTime::Seconds();
	for (const auto& It: UntouchedItems)
	{
		//Get the item we are gonna remove
		TSharedPtr<FMcpItem> Item = ItemsContainer->GetEditableItem(*It);
		if (Item.IsValid())
		{
			//Remove it
			ItemsContainer->RemoveItem(*It);
			Item->LastUpdate = NewProfileRevision;
			if (Item->Instance)
			{
				IMcpItemAware* ItemAware = Cast<IMcpItemAware>(Item->Instance);
				if (ItemAware)
				{
					ItemAware->ProcessItemDestroy();
				}
				Item->Instance = NULL;
			}
			ItemTypesUpdated.Add(Item->ItemType);
		}
	}

	double EndTime = FPlatformTime::Seconds();
	UE_LOG(LogProfileSys, Display, TEXT("HandleFullProfileUpdate Complete:"));
	UE_LOG(LogProfileSys, Display, TEXT("\tTotalTime: %0.2f"), EndTime - StatsUpdateStart);
	UE_LOG(LogProfileSys, Display, TEXT("\tStatUpdate: %0.2f"), ItemsUpdateStart - StatsUpdateStart);
	UE_LOG(LogProfileSys, Display, TEXT("\tItemUpdate: %0.2f"), DestroyUntouchedStart - ItemsUpdateStart);
	UE_LOG(LogProfileSys, Display, TEXT("\tDestroyOld: %0.2f"), EndTime - DestroyUntouchedStart);

	return true;
}

// static
void UMcpProfile::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMcpProfile* This = CastChecked<UMcpProfile>(InThis);
	for (auto& It : This->ItemsContainer->GetAllItems())
	{
		// Make sure to pass in the reference to the pointer so it can be cleared
		UObject*& Instance = It.Value->Instance;
		if (Instance)
		{
			Collector.AddReferencedObject(Instance, This);
		}
	}
	for (auto& It : This->ClassMap)
	{
		UClass*& ClassRef = It.Value;
		Collector.AddReferencedObject(ClassRef, This);
	}
}

const TSet<FString>* UMcpProfile::GetItemsByType(const FString& ItemType) const
{
	return ItemsContainer->GetItemsByType(ItemType);
}

void UMcpProfile::GetItemsByType(const FString &ItemType, TArray< TSharedPtr<const FMcpItem> >& OutItems) const
{
	ItemsContainer->GetItemsByType(ItemType, OutItems);
}

void UMcpProfile::AddItem(const TSharedRef <FMcpItem>& ItemToAdd)
{
	return ItemsContainer->AddItem(ItemToAdd);
}

bool UMcpProfile::RemoveItem(FString ItemId)
{
	return ItemsContainer->RemoveItem(ItemId);
}

void UMcpProfile::SetShouldCreateInstances(bool InShouldCreateInstances)
{
	if (InShouldCreateInstances != bShouldCreateInstances)
	{
		bShouldCreateInstances = InShouldCreateInstances;

		for (const auto& It : ItemsContainer->GetAllItems())
		{
			// This will create/destroy instances as needed
			It.Value->InitializeInstance(this, bShouldCreateInstances, false);
		}
	}
}

UObject* UMcpProfile::CreateItemInstance(UClass* InstanceClass, FString InstanceId)
{
	UObject *Instance = NewObject<UObject>(this, InstanceClass);
	return Instance;
}

void UMcpProfile::ItemPostLoad(FMcpItem* Item)
{

}

bool UMcpProfile::PrepareRpcCommand(const FString& CommandName, const TSharedRef<FOnlineHttpRequest>& HttpRequest, const TSharedRef<FJsonObject>& JsonObject)
{
	FString JsonStr;

	// serialize the JSON to string	
	TSharedRef<TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
	bool bResult = FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
	if (!bResult)
	{
		return false;
	}

	// just put the payload on the http request
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonStr);
	return true;
}

//////////////////////////////////////////
// FMcpItem

bool FMcpItem::Initialize(const TSharedRef<FJsonObject>& JsonObject, int64 ProfileRevision, class UMcpProfile* InOwningProfile)
{
	LastUpdate = ProfileRevision;

	// get the basic attributes out
	TemplateId = JsonObject->GetStringField(TEXT("templateId"));
	Quantity = JsonObject->GetNumberField(TEXT("quantity"));
	Attributes = JsonObject->GetObjectField(TEXT("attributes"))->Values;
	OwningProfile = InOwningProfile;
	if (TemplateId.Contains(TEXT(".")))
	{
		// Split on . if it's there
		FString TypeString;
		TemplateId.Split(TEXT("."), &TypeString, NULL);
		ItemType = TypeString;
	}
	else
	{
		// otherwise just use the full template name as the type
		ItemType = TemplateId;
	}

	InOwningProfile->ItemPostLoad(this);

	return InitializeInstance(InOwningProfile, InOwningProfile->bShouldCreateInstances, true);
}

bool FMcpItem::InitializeInstance(class UMcpProfile* InOwningProfile, bool bCreateInstance, bool bForceReinitialize)
{
	// first check to see if there's a class registered for this specific template id
	UClass* InstanceClass = InOwningProfile->ClassMap.FindRef(TemplateId);
	if (!InstanceClass)
	{
		// see if there's a registered instance class for this type
		InstanceClass = InOwningProfile->ClassMap.FindRef(ItemType);
	}

	// create the instance class if a class definition is found
	if (InstanceClass && bCreateInstance)
	{
		// make a new class if we have to
		if (!Instance || Instance->GetClass() != InstanceClass)
		{
			// This is a new one, so reinitialize
			bForceReinitialize = true;
			Instance = InOwningProfile->CreateItemInstance(InstanceClass, InstanceId);
		}

		// populate it
		IMcpItemAware* ItemAware = Cast<IMcpItemAware>(Instance);
		if (ItemAware && bForceReinitialize)
		{
			if (!ItemAware->PopulateFromMcpItem(*this))
			{
				UE_LOG(LogProfileSys, Error, TEXT("Failed to populate item %s of type %s which was mapped to class %s"), *InstanceId, *TemplateId, *InstanceClass->GetName());
				Instance = NULL;

				return false;
			}
		}
	}
	else
	{
		// remove our reference to any existing class
		Instance = NULL;
	}

	return true;
}

bool FMcpItem::ConvertToJson(TSharedRef<FJsonObject>& JsonObject) const
{
	JsonObject->SetStringField(TEXT("templateId"), TemplateId);
	JsonObject->SetNumberField(TEXT("quantity"), Quantity);

	TSharedRef<FJsonObject> AttributeObject = MakeShareable(new FJsonObject());
	AttributeObject->Values = Attributes;
	JsonObject->SetObjectField(TEXT("attributes"), AttributeObject);

	return true;
}

/** Helper function for getting a named attribute to avoid having to check for both a null pointer and an invalid shared pointer */
TSharedPtr<FJsonValue> FMcpItem::GetAttribute(const FString& Key) const
{
	const TSharedPtr<FJsonValue>* Value = Attributes.Find(Key);
	return Value != nullptr ? *Value : TSharedPtr<FJsonValue>();
}

/** Helper for a common attribute conversion case (numeric) */
double FMcpItem::GetAttributeAsNumber(const FString& Key, double Default) const
{
	TSharedPtr<FJsonValue> Attr = GetAttribute(Key);
	return Attr.IsValid() ? Attr->AsNumber() : Default;
}

/** Helper for a common attribute conversion case (numeric) */
int32 FMcpItem::GetAttributeAsInteger(const FString& Key, int32 Default) const
{
	TSharedPtr<FJsonValue> Attr = GetAttribute(Key);
	return Attr.IsValid() ? (int32)Attr->AsNumber() : Default;
}

/** Helper for a common attribute conversion case (string) */
FString FMcpItem::GetAttributeAsString(const FString& Key, const FString& Default) const
{
	TSharedPtr<FJsonValue> Attr = GetAttribute(Key);
	return Attr.IsValid() ? Attr->AsString() : Default;
}

/** Helper for a common attribute conversion case (bool) */
bool FMcpItem::GetAttributeAsBool(const FString& Key, bool Default) const
{
	TSharedPtr<FJsonValue> Attr = GetAttribute(Key);
	return Attr.IsValid() ? Attr->AsBool() : Default;
}

