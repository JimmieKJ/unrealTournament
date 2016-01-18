// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "McpProfileGroup.h"
#include "JsonObjectWrapper.h"
#include "McpProfile.generated.h"

class FUniqueNetId;
class FMcpItem;
class FJsonObject;
namespace FHttpRetrySystem
{
	class FRequest;
}

class FJsonValue;
struct FMcpQueryResult;
struct FMcpProfileChangeRequest;

/**
 * Delegate type for when stats change
 *
 * @param ProfileRevision The revision number of the profile after this change has been applied
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpHandleStatsChange, int64);

/**
 * Delegate for handling changes to the inventory.
 *
 * @param ChangeFlags A set of item types which have changed
 * @param ProfileRevision The revision number of the profile after this change has been applied
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMcpHandleInventoryChange, const TSet<FString>&, int64);

// Class that handles retrieving and updating the user profile with the Mcp service
UCLASS(config=Engine)
class MCPPROFILESYS_API UMcpProfile : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	/** Getter for great justice */
	FORCEINLINE FOnlineSubsystemMcp* GetOnlineSubMcp() const { return OnlineSubMcp; }

	/** Get the profile group this profile belongs to. */
	FORCEINLINE UMcpProfileGroup& GetProfileGroup() const { check(ProfileGroup); return *ProfileGroup; }

	/** Which profile within the profile group this object refers to. Accounts can have multiple profiles for the same title. */
	FORCEINLINE const FString& GetProfileId() const { return ProfileId; }

	/** Unique number representing the change state of the profile. This is incremented any time it's changed from any source */
	FORCEINLINE int64 GetProfileRevision() const { return ProfileRevision; }

	/** returns true if this profile currently has valid data */
	FORCEINLINE bool HasValidProfileData() const { return ProfileRevision >= 0; }

	/** Query for a user's MCP profile based on the profile object's EpicID value, and load the details into this profile object */
	void ForceQueryProfile(const FMcpQueryComplete& Callback);

	/** Query for a user's MCP profile based on the profile object's EpicID value, and load the details into this profile object, use specific context. */
	void ForceQueryProfile(FBaseUrlContext& Context);

	/** Reset all of the user's persistent data.  Not for normal gameplay use, just for development use */
	void CheatResetProfile(bool bAllProfiles = true);

	/** Delegate for when XP has been updated */
	FORCEINLINE FMcpHandleStatsChange& OnStatsUpdated() { return StatsUpdatedDelegate; }

	/** Delegate for when inventory is updated */
	FORCEINLINE FMcpHandleInventoryChange& OnInventoryUpdated() { return ItemsUpdatedDelegate; }

	/** Delegate to handle a notification */
	FORCEINLINE FMcpHandleNotification& OnHandleNotification() { return HandleNotificationDelegate; }

	FString ToDebugString() const;

	TSharedPtr<const FJsonValue> GetStat(const FString& StatName) const
	{
		const TSharedPtr<FJsonValue>* StatValue = Stats.Find(StatName);
		return StatValue ? *StatValue : TSharedPtr<FJsonValue>();
	}

	TSharedPtr<const FMcpItem> GetItem(const FString& InstanceId) const;

	// Get an array of all MCPItems who have an ItemType equal to the argument
	const TSet<FString>* GetItemsByType(const FString &ItemType) const;

	// Get an array of all MCPItems who have an ItemType equal to the argument
	void GetItemsByType(const FString &ItemType, TArray< TSharedPtr<const FMcpItem> >& OutItems) const;

	TSharedPtr<const FMcpItem> FindItemByTemplate(const FString& TemplateId) const;

	/** How much MTX currency do they have in this profile */
	int32 GetMtxBalance() const;

	/** Count how many items of a particular template are present in the inventory */
	int32 GetItemCount(const FString& TemplateId) const;

	/** Array with the templateIDs for MTX items */
	static const FString MTX_TEMPLATE_IDS[4];

public:
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private: // web RPCs

	/** Delete a given profile */
	UFUNCTION(ServiceRequest(MCP))
	void DeleteProfile(UPARAM(NotReplicated) FClientUrlContext& Context);

	/** Delete all profiles for this account */
	UFUNCTION(ServiceRequest(MCP))
	void DeleteAllProfiles(UPARAM(NotReplicated) FClientUrlContext& Context);

	/** Creates the profile if it doesn't already exist then fires a FullProfileUpdate notification */
	UFUNCTION(ServiceRequest(MCP))
	void QueryProfile(UPARAM(NotReplicated) FBaseUrlContext& Context);

protected:
	friend class FMcpItem;
	friend class UMcpProfileGroup;

	virtual void Initialize(UMcpProfileGroup* ProfileGroup, const FString& ProfileId);

	/** Resets profile to default state, deletes items and stats */
	virtual void Reset();

	FORCEINLINE const TMap<FString, TSharedPtr<FJsonValue> >& GetAllStats() const { return Stats; }

	//FOR THE LOVE OF GOD AND ALL THAT IS HOLY DO NOT EDIT THE MEMBERS
	const TMap<FString, TSharedRef<FMcpItem> >& GetAllItems() const;

	/** Map of each class' latest xp value */
	TMap<FString, TSharedPtr<FJsonValue> > Stats;

	//Add FMCPItem to the assorted Items arrays
	//THIS IS SUPER DANGEROUS. DO NOT USE THIS UNLESS YOU KNOW WHAT YOU ARE DOING
	void AddItem(const TSharedRef<FMcpItem>& ItemToAdd);

	//Remove a FMcpItem from the assorted Items arrays. Returns true if successful.
	//THIS IS SUPER DANGEROUS. DO NOT USE THIS UNLESS YOU KNOW WHAT YOU ARE DOING
	bool RemoveItem(FString ItemId);

	/** Enables/disables rather this profile should create item instances. Will create/destroy existing instances as needed */
	virtual void SetShouldCreateInstances(bool bShouldCreateInstances);

	/** Creates an item instance. Can be overridden by subclasses to change the outer or add data */
	virtual UObject* CreateItemInstance(UClass* InstanceClass, FString InstanceId);

	/** Allows the profile to modify an FMcpItem after it's been initially created but before instances are made */
	virtual void ItemPostLoad(FMcpItem* Item);

	//Modify RPC's HTTP request made through this profile here
	virtual bool PrepareRpcCommand(const FString& CommandName, const TSharedRef<FOnlineHttpRequest>& HttpRequest, const TSharedRef<FJsonObject>& JsonObject);

	/** Generates profile delta commands that represent the differences in an item. Both OldItem and NewItem can be NULL, if an item is newly creates or destroyed. Returns true if any changes emitted */
	bool GenerateProfileDeltaCommand(FMcpProfileChangeRequest& Changes, TSharedPtr<const FMcpItem> OldItem, TSharedPtr<const FMcpItem> NewItem);

	/** 
	 * Computes a list of changes between the backend representation of items and the passed in item instances and stats
	 * @param Changes		Modified to create a list of changes that will be sent to the backend
	 * @param ItemInstances ItemInstances are scanned to look for new, deleted, and modified items
	 * @param ChangedStats	List of stats that may have changed. It will only send changes for stats listed in local stats, if a stat isn't listed it will not be deleted
	 */
	bool BuildProfileChanges(FMcpProfileChangeRequest& Changes, TSet<class IMcpItemAware*>& ItemInstances, TMap<FString, TSharedPtr<FJsonValue> >& ChangedStats);

private:

	/** Called when a forced profile query completes (not necessarily whenever a full update comes down). */
	void OnQueryProfileComplete(const FMcpQueryResult& Response, FMcpQueryComplete OuterCallback);

	/** Forwards net request function to the MCP */
	virtual bool CallRemoteFunction( UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, FFrame* Stack ) override;

	/** Lets the UObjectSystem know to forward service request functions */
	virtual int32 GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack ) override;

	/** Process a response from the MCP to pull out profile changes */
	void HandleMcpResponse(const TSharedPtr<FJsonObject>& JsonPayload, FMcpQueryResult& QueryResult);

	/** Process a full profile update, returns false if the parsing failed */
	bool HandleFullProfileUpdate(const TSharedPtr<FJsonObject>& ProfileObj, bool* OutStatsUpdated, TSet<FString>* OutItemTypesUpdated, int64 NewProfileRevision, int64 OldProfileRevision);

	/** Process a single delta command like add item, returns false if the parsing failed */
	bool HandleProfileDeltaCommand(const TSharedPtr<FJsonObject>& Change, bool* OutStatsUpdated, TSet<FString>* OutItemTypesUpdated, int64 NewProfileRevision, int64 OldProfileRevision);

protected:

	/** Map of template type to UClass */
	TMap<FString, UClass*> ClassMap;

	UPROPERTY()
	FString DebugName;
	
private: 

	/** Pointer to the OSS given at init time. This may be NULL in nomcp mode. */
	FOnlineSubsystemMcp* OnlineSubMcp;

	UPROPERTY()
	UMcpProfileGroup* ProfileGroup;

	/** The specific profile under this account that this McpProfile is bound to */
	UPROPERTY()
	FString ProfileId;

	/** The revision number of the profile last sent down to the client */
	UPROPERTY()
	int64 ProfileRevision;

	/** True if we are currently doing a profile query */
	UPROPERTY()
	int32 FullProfileQueryQueued;

	/** True if this profile should try to create instances */
	UPROPERTY()
	bool bShouldCreateInstances;

	/** 
	 * Overflow container for any ForceQueryProfile calls that occur while others are in flight 
	 *
	 * We hold onto the new delegates until the in flight on completes and trigger all at once
	 * it simulates the call for everyone when only one was required
	 */
	TArray<FMcpQueryComplete> PendingForceQueryDelegates;

	/** Delegate called after earning class+player xp */
	FMcpHandleStatsChange StatsUpdatedDelegate;

	/** Public delegate called after one or more schematics have been granted */
	FMcpHandleInventoryChange ItemsUpdatedDelegate;

	/** Public delegate called to handle notifications */
	FMcpHandleNotification HandleNotificationDelegate;

	//Protective wrapper for all MCPItem related storage
	TSharedPtr<struct FMcpItemsContainer> ItemsContainer;

	mutable int64 MtxBalanceCacheVersion;
	mutable int32 MtxBalanceCache;
};

/** A mirror of what an item looks like on the backend */
class MCPPROFILESYS_API FMcpItem : public TSharedFromThis<FMcpItem>
{
public:
	/** What kind of item this is, parsed off of the name */
	FString ItemType;

	/** Unique Id for this instance of the item */
	FString InstanceId;

	/** String defining what kind of thing this is */
	FString TemplateId;

	/** Quantity of this item */
	int32 Quantity;

	/** Key/Value pairs for attributes. Each value strings are json formatted */
	TMap<FString, TSharedPtr<FJsonValue> > Attributes;

	/** A game specific class implementing this item. May be NULL */
	UObject* Instance;

	/** The MCP Profile revision at the last time this FMcpItem was updated (useful for early out checks) */
	int64 LastUpdate;

	/** The MCP Profile that owns this item, can be null for temporary items */
	TWeakObjectPtr<class UMcpProfile> OwningProfile;

	FMcpItem(const FString& ItemId) :
		InstanceId(ItemId),
		Quantity(1),
		Instance(NULL),
		LastUpdate(-1)
	{
	}

	/** Helper function for getting a named attribute to avoid having to check for both a null pointer and an invalid shared pointer */
	TSharedPtr<FJsonValue> GetAttribute(const FString& Key) const;

	/** Helper for a common attribute conversion case (numeric) */
	double GetAttributeAsNumber(const FString& Key, double Default = 0.0) const;

	/** Helper for a common attribute conversion case (numeric) */
	int32 GetAttributeAsInteger(const FString& Key, int32 Default = 0) const;

	/** Helper for a common attribute conversion case (string) */
	FString GetAttributeAsString(const FString& Key, const FString& Default = FString()) const;

	/** Helper for a common attribute conversion case (bool) */
	bool GetAttributeAsBool(const FString& Key, bool Default = false) const;

	/** Initializes an item from the passed in JsonObject, will replace any data that currently exists in the item */
	bool Initialize(const TSharedRef<FJsonObject>& JsonObject, int64 ProfileRevision, class UMcpProfile* InOwningProfile);

	/** Initializes the instance from the item setup. If bCreateInstance is set will try to create one, if bForceReinitialize is set it will overwrite the instance if found */
	bool InitializeInstance(class UMcpProfile* InOwningProfile, bool bCreateInstance, bool bForceReinitialize);

	/** Converts an Item to a JsonObject */
	bool ConvertToJson(TSharedRef<FJsonObject>& JsonObject) const;
};

/**
 * You can use this class to cache values from the MCP profile that only change when the profile is updated.
 * Ex:
 * Somewhere make an instance:
 *    FMcpCacher<int32> Foo;
 * Then later, call the instance GetCached method (this will not query the profile unless it's changed)
 *    int32 AccountLevel = Foo.GetCached(GetMcpProfile(), [] (UMcpProfile* McpProfile) { return McpProfile->GetAccountLevel(); });
 */
template<typename ReturnType>
class FMcpCacher
{
public:
	FMcpCacher(const ReturnType& InitialValue)
		: CacheVersion(-1)
		, CacheValue(InitialValue)
	{
	}
	FMcpCacher()
		: CacheVersion(-1)
		, CacheValue()
	{
	}

	/** Manually invalidate the cache */
	FORCEINLINE void Invalidate() const { CacheVersion = -1; }

	/** 
	 * Check to see if the McpProfile's revision has changed. If not, return the cache value, otherwise freshen and return.
	 * NOTE: this function will never pass NULL McpProfile to the Lambda
	 */
	template<typename MCP_PROFILE_T, class LambdaType>
	inline const ReturnType& GetCached(MCP_PROFILE_T* McpProfile, LambdaType Lambda) const
	{
		if (McpProfile)
		{
			int64 CurrentRev = McpProfile->GetProfileRevision();
			if (CurrentRev != CacheVersion)
			{
				CacheValue = Lambda(McpProfile);
				CacheVersion = CurrentRev;
			}
		}
		return CacheValue;
	}

private:
	mutable int64 CacheVersion;
	mutable ReturnType CacheValue;
};


/** These structures are used to request batch updates, and match structures in ProfileChangeRequest.java */
USTRUCT()
struct MCPPROFILESYS_API FMcpAddItemRequest
{
	GENERATED_USTRUCT_BODY()

	/** The Guid of the item being added */
	UPROPERTY()
	FString ItemId;

	/** Template of item being added */
	UPROPERTY()
	FString TemplateId;

	/** Number in the stack */
	UPROPERTY()
	int32 Quantity;

	/** Json object representing the attributes */
	UPROPERTY()
	FJsonObjectWrapper Attributes;
};

USTRUCT()
struct MCPPROFILESYS_API FMcpRemoveItemRequest
{
	GENERATED_USTRUCT_BODY()

	/** The Guid of the item being added */
	UPROPERTY()
	FString ItemId;
};

USTRUCT()
struct MCPPROFILESYS_API FMcpChangeQuantityRequest
{
	GENERATED_USTRUCT_BODY()

	/** The Guid of the item being added */
	UPROPERTY()
	FString ItemId;

	/** Number to add or remove from the stack */
	UPROPERTY()
	int32 DeltaQuantity;
};

USTRUCT()
struct MCPPROFILESYS_API FMcpChangeAttributesRequest
{
	GENERATED_USTRUCT_BODY()

	/** The Guid of the item being added */
	UPROPERTY()
	FString ItemId;

	/** Json object representing the attributes that are changed */
	UPROPERTY()
	FJsonObjectWrapper Attributes;
};

USTRUCT()
struct MCPPROFILESYS_API FMcpProfileChangeRequest
{
	GENERATED_USTRUCT_BODY()

	/** The profile revision this is built on top of */
	UPROPERTY()
	int32 BaseProfileRevision;

	/** Lists of different request types */
	UPROPERTY()
	TArray<FMcpAddItemRequest> AddRequests;

	UPROPERTY()
	TArray<FMcpRemoveItemRequest> RemoveRequests;

	UPROPERTY()
	TArray<FMcpChangeQuantityRequest> ChangeQuantityRequests;

	UPROPERTY()
	TArray<FMcpChangeAttributesRequest> ChangeAttributesRequests;

	/** This is a json object with statName and statValue subfields, needs to be an opaque object because value could be any node type */
	UPROPERTY()
	TArray<FJsonObjectWrapper> ChangeStatRequests;
};