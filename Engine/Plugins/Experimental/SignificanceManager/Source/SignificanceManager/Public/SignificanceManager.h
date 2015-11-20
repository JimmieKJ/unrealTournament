// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SignificanceManager.generated.h"

DECLARE_STATS_GROUP(TEXT("Significance Manager"), STATGROUP_SignificanceManager, STATCAT_Advanced);

/* The significance manager provides a framework for registering objects by tag to each have a significance
 * value calculated from which a game specific subclass and game logic can make decisions about what level
 * of detail objects should be at, tick frequency, whether to spawn effects, and other such functionality
 *
 * Each object that is registered must have a corresponding unregister event or else a dangling Object reference will
 * be left resulting in an eventual crash once the Object has been garbage collected.
 *
 * Each user of the significance manager is expected to call the Update function from the appropriate location in the
 * game code.  GameViewportClient::Tick may often serve as a good place to do this.
 */
UCLASS(config=Engine, defaultconfig)
class SIGNIFICANCEMANAGER_API USignificanceManager : public UObject
{
	GENERATED_BODY()

public:
	typedef TFunction<float(const UObject*,const FTransform&)> FSignificanceFunction;

	struct FManagedObjectInfo
	{
		FManagedObjectInfo()
			: Object(nullptr)
			, Significance(0.f)
		{
		}

		FManagedObjectInfo(const UObject* InObject, FName InTag, FSignificanceFunction InSignificanceFunction)
			: Object(InObject)
			, Tag(InTag)
			, Significance(0.f)
			, SignificanceFunction(InSignificanceFunction)
		{
		}

		const UObject* GetObject() const { return Object; }
		FName GetTag() const { return Tag; }
		float GetSignificance() const { return Significance; }

	private:
		const UObject* Object;
		FName Tag;
		float Significance;

		FSignificanceFunction SignificanceFunction;

		void UpdateSignificance(const TArray<FTransform>& ViewPoints);

		// Allow SignificanceManager to call UpdateSignificance
		friend USignificanceManager;
	};

	USignificanceManager();

	// Begin UObject overrides
	virtual void BeginDestroy() override;
	// End UObject overrides

	// Overridable function to update the managed objects' significance
	virtual void Update(const TArray<FTransform>& Viewpoints);

	// Overridable function used to register an object as managed by the significance manager
	virtual void RegisterObject(const UObject* Object, FName Tag, FSignificanceFunction SignificanceFunction);

	// Overridable function used to unregister an object as managed by the significance manager
	virtual void UnregisterObject(const UObject* Object);

	// Returns objects of specified tag, Tag must be specified or else an empty array will be returned
	const TArray<const FManagedObjectInfo*>& GetManagedObjects(FName Tag) const;

	// Returns all managed objects regardless of tag
	void GetManagedObjects(TArray<const FManagedObjectInfo*>& OutManagedObjects, bool bInSignificanceOrder = false) const;

	// Returns the significance value for a given object, returns 0 if object is not managed
	float GetSignificance(const UObject* Object) const;

	// Returns true if the object is being tracked, placing the significance value in OutSignificance (or 0 if object is not managed)
	bool QuerySignificance(const UObject* Object, float& OutSignificance) const;

	// Returns the significance manager for the specified World
	static USignificanceManager* Get(const UWorld* World);

	// Templated convenience function to return a significance manager cast to a known type
	template<class T>
	static T* Get(const UWorld* World)
	{
		return CastChecked<T>(Get(World), ECastCheckedType::NullAllowed);
	}

protected:

	const TArray<FTransform>& GetViewpoints() const { return Viewpoints; }

	// Whether the significance manager should be created on a client
	uint32 bCreateOnClient:1;

	// Whether the significance manager should be created on the server
	uint32 bCreateOnServer:1;

	// Whether the significance sort should sort high values to the end of the list
	uint32 bSortSignificanceAscending:1;

private:

	// The cached viewpoints for significance for calculating when a new object is registered
	TArray<FTransform> Viewpoints;

	// All objects being managed organized by Tag
	TMap<FName, TArray<const FManagedObjectInfo*>> ManagedObjectsByTag;

	// Reverse lookup map to find the tag for a given object
	TMap<const UObject*, FManagedObjectInfo*> ManagedObjects;

	// Game specific significance class to instantiate
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="SignificanceManager", DisplayName="Significance Manager Class"))
	FStringClassReference SignificanceManagerClassName;

	// Cached class for instantiating significance manager, only populated on CDO
	UPROPERTY()
	TSubclassOf<USignificanceManager>  SignificanceManagerClass;

	// Map of worlds to their significance manager, only populated on CDO
	UPROPERTY()
	TMap<const UWorld*, USignificanceManager*> WorldSignificanceManagers;

	// Callback function registered with global world delegates to instantiate significance manager when a game world is created
	void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

	// Callback function registered with global world delegates to cleanup significance manager when a game world is destroyed
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	// Callback function registered with HUD to supply debug info when ShowDebug SignificanceManager has been entered on the console
	void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);
};
