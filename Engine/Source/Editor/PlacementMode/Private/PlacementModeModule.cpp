// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PlacementModePrivatePCH.h"
#include "IPlacementModeModule.h"
#include "PlacementMode.h"
#include "AssetToolsModule.h"
#include "KismetEditorUtilities.h"
#include "Classes/ActorFactories/ActorFactoryPlanarReflection.h"

struct FPlacementCategory : FPlacementCategoryInfo
{
	FPlacementCategory(const FPlacementCategoryInfo& SourceInfo)
		: FPlacementCategoryInfo(SourceInfo)
	{

	}

	FPlacementCategory(FPlacementCategory&& In)
		: FPlacementCategoryInfo(MoveTemp(In))
		, Items(MoveTemp(In.Items))
	{}

	FPlacementCategory& operator=(FPlacementCategory&& In)
	{
		FPlacementCategoryInfo::operator=(MoveTemp(In));
		Items = MoveTemp(In.Items);
		return *this;
	}

	TMap<uint32, TSharedPtr<FPlaceableItem>> Items;
};

static TOptional<FLinearColor> GetBasicShapeColorOverride()
{
	// Get color for basic shapes.  It should appear like all the other basic types
	static TOptional<FLinearColor> BasicShapeColorOverride;
	
	if( !BasicShapeColorOverride.IsSet() )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		TSharedPtr<IAssetTypeActions> AssetTypeActions;
		AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UClass::StaticClass()).Pin();
		if (AssetTypeActions.IsValid())
		{
			BasicShapeColorOverride = TOptional<FLinearColor>(AssetTypeActions->GetTypeColor());
		}
	}
	return BasicShapeColorOverride;
}

class FPlacementModeModule : public IPlacementModeModule
{
public:
	
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override
	{

		TArray< FString > RecentlyPlacedAsStrings;
		GConfig->GetArray(TEXT("PlacementMode"), TEXT("RecentlyPlaced"), RecentlyPlacedAsStrings, GEditorPerProjectIni);

		//FString ActivePaletteName;
		//GConfig->GetString( TEXT( "PlacementMode" ), TEXT( "ActivePalette" ), ActivePaletteName, GEditorPerProjectIni );

		for (int Index = 0; Index < RecentlyPlacedAsStrings.Num(); Index++)
		{
			RecentlyPlaced.Add(FActorPlacementInfo(RecentlyPlacedAsStrings[Index]));
		}


		FEditorModeRegistry::Get().RegisterMode<FPlacementMode>(
			FBuiltinEditorModes::EM_Placement,
			NSLOCTEXT("PlacementMode", "DisplayName", "Place"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.PlacementMode", "LevelEditor.PlacementMode.Small"),
			true, 0);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw( this, &FPlacementModeModule::OnAssetRemoved );
		AssetRegistryModule.Get().OnAssetRenamed().AddRaw( this, &FPlacementModeModule::OnAssetRenamed );
		AssetRegistryModule.Get().OnAssetAdded().AddRaw( this, &FPlacementModeModule::OnAssetAdded );

		TOptional<FLinearColor> BasicShapeColorOverride = GetBasicShapeColorOverride();


		RegisterPlacementCategory(
			FPlacementCategoryInfo(
				NSLOCTEXT( "PlacementMode", "RecentlyPlaced", "Recently Placed" ),
				FBuiltInPlacementCategories::RecentlyPlaced(),
				TEXT("PMRecentlyPlaced"),
				TNumericLimits<int32>::Lowest(),
				false
			)
		);

		{
			int32 SortOrder = 0;
			FName CategoryName = FBuiltInPlacementCategories::Basic();
			RegisterPlacementCategory(
				FPlacementCategoryInfo(
					NSLOCTEXT( "PlacementMode", "Basic", "Basic" ),
					CategoryName,
					TEXT("PMBasic"),
					10
				)
			);

			FPlacementCategory* Category = Categories.Find(CategoryName);
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryEmptyActor::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryCharacter::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryPawn::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryPointLight::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryPlayerStart::StaticClass(), SortOrder+=10)) );
			// Cube
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr,*UActorFactoryBasicShape::BasicCube.ToString())), FName("ClassThumbnail.Cube"), BasicShapeColorOverride, SortOrder+=10, NSLOCTEXT("PlacementMode", "Cube", "Cube") )) );
			// Sphere
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr,*UActorFactoryBasicShape::BasicSphere.ToString())), FName("ClassThumbnail.Sphere"), BasicShapeColorOverride, SortOrder+=10, NSLOCTEXT("PlacementMode", "Sphere", "Sphere") )) );
			// Cylinder
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr,*UActorFactoryBasicShape::BasicCylinder.ToString())), FName("ClassThumbnail.Cylinder"), BasicShapeColorOverride, SortOrder+=10, NSLOCTEXT("PlacementMode", "Cylinder", "Cylinder") )) );
			// Cone
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr,*UActorFactoryBasicShape::BasicCone.ToString())), FName("ClassThumbnail.Cone"), BasicShapeColorOverride, SortOrder+=10, NSLOCTEXT("PlacementMode", "Cone", "Cone") )) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryTriggerBox::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryTriggerSphere::StaticClass(), SortOrder+=10)) );
		}

		{
			int32 SortOrder = 0;
			FName CategoryName = FBuiltInPlacementCategories::Lights();
			RegisterPlacementCategory(
				FPlacementCategoryInfo(
					NSLOCTEXT( "PlacementMode", "Lights", "Lights" ),
					CategoryName,
					TEXT("PMLights"),
					20
				)
			);

			FPlacementCategory* Category = Categories.Find(CategoryName);
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryDirectionalLight::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryPointLight::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactorySpotLight::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactorySkyLight::StaticClass(), SortOrder+=10)) );
		}

		{
			int32 SortOrder = 0;
			FName CategoryName = FBuiltInPlacementCategories::Visual();
			RegisterPlacementCategory(
				FPlacementCategoryInfo(
					NSLOCTEXT( "PlacementMode", "VisualEffects", "Visual Effects" ),
					CategoryName,
					TEXT("PMVisual"),
					30
				)
			);

			UActorFactory* PPFactory = GEditor->FindActorFactoryByClassForActorClass( UActorFactoryBoxVolume::StaticClass(), APostProcessVolume::StaticClass() );

			FPlacementCategory* Category = Categories.Find(CategoryName);
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(PPFactory, FAssetData(APostProcessVolume::StaticClass()), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryAtmosphericFog::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryExponentialHeightFog::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactorySphereReflectionCapture::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBoxReflectionCapture::StaticClass(), SortOrder+=10)) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryPlanarReflection::StaticClass(), SortOrder += 10)));
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryDeferredDecal::StaticClass(), SortOrder+=10)) );
		}

		RegisterPlacementCategory(
			FPlacementCategoryInfo(
				NSLOCTEXT( "PlacementMode", "Volumes", "Volumes" ),
				"Volumes",
				TEXT("PMVolumes"),
				40
			)
		);

		{
			int32 SortOrder = 0;
			FName CategoryName = FBuiltInPlacementCategories::AllClasses();
			RegisterPlacementCategory(
				FPlacementCategoryInfo(
					NSLOCTEXT( "PlacementMode", "AllClasses", "All Classes" ),
					CategoryName,
					TEXT("PMAllClasses"),
					50
				)
			);

			FPlacementCategory* Category = Categories.Find(CategoryName);
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryEmptyActor::StaticClass())) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryCharacter::StaticClass())) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryPawn::StaticClass())) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCube.ToString())), FName("ClassThumbnail.Cube"), GetBasicShapeColorOverride())) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicSphere.ToString())), FName("ClassThumbnail.Sphere"), GetBasicShapeColorOverride())) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCylinder.ToString())), FName("ClassThumbnail.Cylinder"), GetBasicShapeColorOverride())) );
			Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCone.ToString())), FName("ClassThumbnail.Cone"), GetBasicShapeColorOverride())) );
		}

	}

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void PreUnloadCallback() override
	{
		FEditorModeRegistry::Get().UnregisterMode( FBuiltinEditorModes::EM_Placement );

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRenamed().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll( this );
	}

	DECLARE_DERIVED_EVENT( FPlacementModeModule, IPlacementModeModule::FOnRecentlyPlacedChanged, FOnRecentlyPlacedChanged );
	virtual FOnRecentlyPlacedChanged& OnRecentlyPlacedChanged() override { return RecentlyPlacedChanged; }

	DECLARE_DERIVED_EVENT( FPlacementModeModule, IPlacementModeModule::FOnAllPlaceableAssetsChanged, FOnAllPlaceableAssetsChanged );
	virtual FOnAllPlaceableAssetsChanged& OnAllPlaceableAssetsChanged() override { return AllPlaceableAssetsChanged; }

	/**
	 * Add the specified assets to the recently placed items list
	 */
	virtual void AddToRecentlyPlaced( const TArray< UObject* >& PlacedObjects, UActorFactory* FactoryUsed = NULL ) override
	{
		FString FactoryPath;
		if (FactoryUsed != NULL)
		{
			FactoryPath = FactoryUsed->GetPathName();
		}

		TArray< UObject* > FilteredPlacedObjects;
		for (UObject* PlacedObject : PlacedObjects)
		{
			// Don't include null placed objects that just have factories.
			if (PlacedObject == NULL)
			{
				continue;
			}

			// Don't add brush builders to the recently placed.
			if (PlacedObject->IsA(UBrushBuilder::StaticClass()))
			{
				continue;
			}

			FilteredPlacedObjects.Add(PlacedObject);
		}

		// Don't change the recently placed if nothing passed the filter.
		if (FilteredPlacedObjects.Num() == 0)
		{
			return;
		}

		bool Changed = false;
		for (int Index = 0; Index < FilteredPlacedObjects.Num(); Index++)
		{
			Changed |= RecentlyPlaced.Remove(FActorPlacementInfo(FilteredPlacedObjects[Index]->GetPathName(), FactoryPath)) > 0;
		}

		for (int Index = 0; Index < FilteredPlacedObjects.Num(); Index++)
		{
			if (FilteredPlacedObjects[Index] != NULL)
			{
				RecentlyPlaced.Insert(FActorPlacementInfo(FilteredPlacedObjects[Index]->GetPathName(), FactoryPath), 0);
				Changed = true;
			}
		}

		for (int Index = RecentlyPlaced.Num() - 1; Index >= 20; Index--)
		{
			RecentlyPlaced.RemoveAt(Index);
			Changed = true;
		}

		if (Changed)
		{
			TArray< FString > RecentlyPlacedAsStrings;
			for (int Index = 0; Index < RecentlyPlaced.Num(); Index++)
			{
				RecentlyPlacedAsStrings.Add(RecentlyPlaced[Index].ToString());
			}

			GConfig->SetArray(TEXT("PlacementMode"), TEXT("RecentlyPlaced"), RecentlyPlacedAsStrings, GEditorPerProjectIni);
			RecentlyPlacedChanged.Broadcast(RecentlyPlaced);
		}
	}

	void OnAssetRemoved(const FAssetData& /*InRemovedAssetData*/)
	{
		RecentlyPlacedChanged.Broadcast(RecentlyPlaced);
		AllPlaceableAssetsChanged.Broadcast();
	}

	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
	{
		for (auto& RecentlyPlacedItem : RecentlyPlaced)
		{
			if (RecentlyPlacedItem.ObjectPath == OldObjectPath)
			{
				RecentlyPlacedItem.ObjectPath = AssetData.ObjectPath.ToString();
				break;
			}
		}

		RecentlyPlacedChanged.Broadcast(RecentlyPlaced);
		AllPlaceableAssetsChanged.Broadcast();
	}

	void OnAssetAdded(const FAssetData& AssetData)
	{
		AllPlaceableAssetsChanged.Broadcast();
	}

	/**
	 * Add the specified asset to the recently placed items list
	 */
	virtual void AddToRecentlyPlaced( UObject* Asset, UActorFactory* FactoryUsed = NULL ) override
	{
		TArray< UObject* > Assets;
		Assets.Add( Asset );
		AddToRecentlyPlaced( Assets, FactoryUsed );
	}

	/**
	 * Get a copy of the recently placed items list
	 */
	virtual const TArray< FActorPlacementInfo >& GetRecentlyPlaced() const override
	{
		return RecentlyPlaced;
	}

	/** @return the event that is broadcast whenever the placement mode enters a placing session */
	DECLARE_DERIVED_EVENT( FPlacementModeModule, IPlacementModeModule::FOnStartedPlacingEvent, FOnStartedPlacingEvent );
	virtual FOnStartedPlacingEvent& OnStartedPlacing() override
	{
		return StartedPlacingEvent;
	}
	virtual void BroadcastStartedPlacing( const TArray< UObject* >& Assets ) override
	{
		StartedPlacingEvent.Broadcast( Assets );
	}

	/** @return the event that is broadcast whenever the placement mode exits a placing session */
	DECLARE_DERIVED_EVENT( FPlacementModeModule, IPlacementModeModule::FOnStoppedPlacingEvent, FOnStoppedPlacingEvent );
	virtual FOnStoppedPlacingEvent& OnStoppedPlacing() override
	{
		return StoppedPlacingEvent;
	}
	virtual void BroadcastStoppedPlacing( bool bWasSuccessfullyPlaced ) override
	{
		StoppedPlacingEvent.Broadcast( bWasSuccessfullyPlaced );
	}

public:

	virtual bool RegisterPlacementCategory(const FPlacementCategoryInfo& Info)
	{
		if (Categories.Contains(Info.UniqueHandle))
		{
			return false;
		}

		Categories.Add(Info.UniqueHandle, Info);
		return true;
	}

	virtual const FPlacementCategoryInfo* GetRegisteredPlacementCategory(FName CategoryName) const override
	{
		return Categories.Find(CategoryName);
	}

	virtual void UnregisterPlacementCategory(FName Handle)
	{
		Categories.Remove(Handle);
	}

	virtual void GetSortedCategories(TArray<FPlacementCategoryInfo>& OutCategories) const
	{
		TArray<FName> SortedNames;
		Categories.GenerateKeyArray(SortedNames);

		SortedNames.Sort([&](const FName& A, const FName& B){
			return Categories[A].SortOrder < Categories[B].SortOrder;
		});

		OutCategories.Reset(Categories.Num());
		for (const FName& Name : SortedNames)
		{
			OutCategories.Add(Categories[Name]);
		}
	}

	virtual TOptional<FPlacementModeID> RegisterPlaceableItem(FName CategoryName, const TSharedRef<FPlaceableItem>& InItem)
	{
		FPlacementCategory* Category = Categories.Find(CategoryName);
		if (Category && !Category->CustomGenerator)
		{
			FPlacementModeID ID = CreateID(CategoryName);
			Category->Items.Add(ID.UniqueID, InItem);
			return ID;
		}
		return TOptional<FPlacementModeID>();
	}

	virtual void UnregisterPlaceableItem(FPlacementModeID ID)
	{
		FPlacementCategory* Category = Categories.Find(ID.Category);
		if (Category)
		{
			Category->Items.Remove(ID.UniqueID);
		}
	}

	virtual void GetItemsForCategory(FName CategoryName, TArray<TSharedPtr<FPlaceableItem>>& OutItems) const
	{
		const FPlacementCategory* Category = Categories.Find(CategoryName);
		if (Category)
		{
			for (auto& Pair : Category->Items)
			{
				OutItems.Add(Pair.Value);
			}
		}
	}

	virtual void GetFilteredItemsForCategory(FName CategoryName, TArray<TSharedPtr<FPlaceableItem>>& OutItems, TFunctionRef<bool(const TSharedPtr<FPlaceableItem>&)> Filter) const
	{
		const FPlacementCategory* Category = Categories.Find(CategoryName);
		if (Category)
		{
			for (auto& Pair : Category->Items)
			{
				if (Filter(Pair.Value))
				{
					OutItems.Add(Pair.Value);
				}
			}
		}
	}

	virtual void RegenerateItemsForCategory(FName Category) override
	{
		if (Category == FBuiltInPlacementCategories::RecentlyPlaced())
		{
			RefreshRecentlyPlaced();
		}
		else if (Category == FBuiltInPlacementCategories::Volumes())
		{
			RefreshVolumes();
		}
		else if (Category == FBuiltInPlacementCategories::AllClasses())
		{
			RefreshAllPlaceableClasses();
		}

		// @todo: Broadcast so clients can update dynamic lists?
	}

	void RefreshRecentlyPlaced()
	{
		FName CategoryName = FBuiltInPlacementCategories::RecentlyPlaced();

		FPlacementCategory* Category = Categories.Find(CategoryName);
		if (!Category)
		{
			return;
		}

		Category->Items.Reset();


		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( TEXT( "AssetRegistry" ) );

		for (const FActorPlacementInfo& RecentlyPlacedItem : RecentlyPlaced)
		{
			UObject* Asset = FindObject<UObject>(nullptr, *RecentlyPlacedItem.ObjectPath);

			// If asset is pending delete, it will not be marked as RF_Standalone, in which case we skip it
			if (Asset != nullptr && Asset->HasAnyFlags(RF_Standalone))
			{
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*RecentlyPlacedItem.ObjectPath);

				if (AssetData.IsValid())
				{
					UActorFactory* Factory = FindObject<UActorFactory>(nullptr, *RecentlyPlacedItem.Factory);
					Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(Factory, AssetData) ));
				}
			}
		}

		// Broadcast?
	}

	void RefreshVolumes()
	{
		FName CategoryName = FBuiltInPlacementCategories::Volumes();

		FPlacementCategory* Category = Categories.Find(CategoryName);
		if (!Category)
		{
			return;
		}

		Category->Items.Reset();

		// Add loaded classes
		for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			const UClass* Class = *ClassIt;

			if (!Class->HasAllClassFlags(CLASS_NotPlaceable) &&
				!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
				Class->IsChildOf(AVolume::StaticClass()) &&
				Class->ClassGeneratedBy == nullptr )
			{
				UActorFactory* Factory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), Class);
				Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(Factory, FAssetData(Class)) ));
			}
		}

		// Broadcast?
	}

	void RefreshAllPlaceableClasses()
	{
		FName CategoryName = FBuiltInPlacementCategories::AllClasses();

		// Unregister old stuff
		FPlacementCategory* Category = Categories.Find(CategoryName);
		if (!Category)
		{
			return;
		}

		Category->Items.Reset();

		// Make a map of UClasses to ActorFactories that support them
		const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
		TMap<UClass*, UActorFactory*> ActorFactoryMap;
		for ( int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
		{
			UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

			if ( ActorFactory )
			{
				ActorFactoryMap.Add(ActorFactory->GetDefaultActorClass(FAssetData()), ActorFactory);
			}
		}

		FAssetData NoAssetData;
		FText UnusedErrorMessage;

		// Add loaded classes
		for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			// Don't offer skeleton classes
			bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(*ClassIt);

			if ( !ClassIt->HasAllClassFlags(CLASS_NotPlaceable) &&
				!ClassIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
				ClassIt->IsChildOf(AActor::StaticClass()) &&
				( !ClassIt->IsChildOf(ABrush::StaticClass()) || ClassIt->IsChildOf(AVolume::StaticClass()) ) &&
				!bIsSkeletonClass )
			{
				UActorFactory* ActorFactory = ActorFactoryMap.FindRef(*ClassIt);

				const bool IsVolume = ClassIt->IsChildOf(AVolume::StaticClass());

				if ( IsVolume )
				{
					ActorFactory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), *ClassIt);
				}
				else if (ActorFactory && !ActorFactory->CanCreateActorFrom(NoAssetData, UnusedErrorMessage))
				{
					continue;
				}

				Category->Items.Add(CreateID(), MakeShareable( new FPlaceableItem(ActorFactory, FAssetData(*ClassIt)) ));
			}
		}

		// Broadcast?
	}

private:

	uint32 CreateID()
	{
		static uint32 IDCounter = 0;
		uint32 Next = ++IDCounter;
		checkf(Next, TEXT("uint32 overflow. If this occurs, something's gone seriously wrong."))
		return Next;
	}

	FPlacementModeID CreateID(FName InCategory)
	{
		FPlacementModeID NewID;
		NewID.UniqueID = CreateID();
		NewID.Category = InCategory;

		
		return NewID;
	}

private:

	TMap<FName, FPlacementCategory> Categories;

	TArray< FActorPlacementInfo >	RecentlyPlaced;
	FOnRecentlyPlacedChanged		RecentlyPlacedChanged;

	FOnAllPlaceableAssetsChanged	AllPlaceableAssetsChanged;

	FOnStartedPlacingEvent			StartedPlacingEvent;
	FOnStoppedPlacingEvent			StoppedPlacingEvent;

	TArray< TSharedPtr<FExtender> > ContentPaletteFiltersExtenders;
	TArray< TSharedPtr<FExtender> > PaletteExtenders;
};

IMPLEMENT_MODULE( FPlacementModeModule, PlacementMode );

