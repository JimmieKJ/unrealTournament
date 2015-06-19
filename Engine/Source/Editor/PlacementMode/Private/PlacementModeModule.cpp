// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PlacementModePrivatePCH.h"
#include "IPlacementModeModule.h"
#include "PlacementMode.h"

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

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw( this, &FPlacementModeModule::OnAssetRemoved );
		AssetRegistryModule.Get().OnAssetRenamed().AddRaw( this, &FPlacementModeModule::OnAssetRenamed );
	}

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override
	{
		FEditorModeRegistry::Get().UnregisterMode( FBuiltinEditorModes::EM_Placement );

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRenamed().RemoveAll( this );
	}

	DECLARE_DERIVED_EVENT( FPlacementModeModule, IPlacementModeModule::FOnRecentlyPlacedChanged, FOnRecentlyPlacedChanged );
	virtual FOnRecentlyPlacedChanged& OnRecentlyPlacedChanged() override { return RecentlyPlacedChanged; }

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
private:

	TArray< FActorPlacementInfo >	RecentlyPlaced;
	FOnRecentlyPlacedChanged		RecentlyPlacedChanged;

	FOnStartedPlacingEvent			StartedPlacingEvent;
	FOnStoppedPlacingEvent			StoppedPlacingEvent;

	TArray< TSharedPtr<FExtender> > ContentPaletteFiltersExtenders;
	TArray< TSharedPtr<FExtender> > PaletteExtenders;
};

IMPLEMENT_MODULE( FPlacementModeModule, PlacementMode );

