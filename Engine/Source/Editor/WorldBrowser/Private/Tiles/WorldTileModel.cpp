// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WorldBrowserPrivatePCH.h"

#include "FileHelpers.h"
#include "MessageLog.h"
#include "WorldTileDetails.h"
#include "WorldTileCollectionModel.h"
#include "WorldTileModel.h"
#include "Engine/LevelBounds.h"
#include "Engine/WorldComposition.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingKismet.h"
#include "Landscape.h"
#include "AssetData.h"


#define LOCTEXT_NAMESPACE "WorldBrowser"
DEFINE_LOG_CATEGORY_STATIC(WorldBrowser, Log, All);

FWorldTileModel::FWorldTileModel(const TWeakObjectPtr<UEditorEngine>& InEditor, 
								 FWorldTileCollectionModel& InWorldModel, 
								 int32 InTileIdx)
	: FLevelModel(InWorldModel, InEditor) 
	, TileIdx(InTileIdx)
	, TileDetails(NULL)
	, bWasShelved(false)
{
	UWorldComposition* WorldComposition = LevelCollectionModel.GetWorld()->WorldComposition;

	// Tile display details object
	TileDetails = NewObject<UWorldTileDetails>(GetTransientPackage(), NAME_None, RF_RootSet | RF_Transient);

	// Subscribe to tile properties changes
	TileDetails->PositionChangedEvent.AddRaw(this, &FWorldTileModel::OnPositionPropertyChanged);
	TileDetails->ParentPackageNameChangedEvent.AddRaw(this, &FWorldTileModel::OnParentPackageNamePropertyChanged);
	TileDetails->LODSettingsChangedEvent.AddRaw(this, &FWorldTileModel::OnLODSettingsPropertyChanged);
	TileDetails->ZOrderChangedEvent.AddRaw(this, &FWorldTileModel::OnZOrderPropertyChanged);
	TileDetails->HideInTileViewChangedEvent.AddRaw(this, &FWorldTileModel::OnHideInTileViewChanged);
			
	// Initialize tile details
	if (WorldComposition->GetTilesList().IsValidIndex(TileIdx))
	{
		FWorldCompositionTile& Tile = WorldComposition->GetTilesList()[TileIdx];
		
		TileDetails->PackageName = Tile.PackageName;
		TileDetails->bPersistentLevel = false;
				
		// Asset name for storing tile thumbnail inside package
		SetAssetName(Tile.PackageName);
		//FString AssetNameString = FString::Printf(TEXT("%s %s.TheWorld:PersistentLevel"), *ULevel::StaticClass()->GetName(), *Tile.PackageName.ToString());
		FString AssetNameString = FString::Printf(TEXT("%s %s"), *UPackage::StaticClass()->GetName(), *Tile.PackageName.ToString());
		AssetName =	FName(*AssetNameString);
	
		// Assign level object in case this level already loaded
		UPackage* LevelPackage = Cast<UPackage>(StaticFindObjectFast( UPackage::StaticClass(), NULL, Tile.PackageName) );
		if (LevelPackage)
		{
			// Find the world object
			UWorld* World = UWorld::FindWorldInPackage(LevelPackage);
			if (World)
			{
				LoadedLevel = World->PersistentLevel;
				// Enable tile properties
				TileDetails->bTileEditable = true;
				if (World->PersistentLevel->bIsVisible)
				{
					LoadedLevel.Get()->LevelBoundsActorUpdated().AddRaw(this, &FWorldTileModel::OnLevelBoundsActorUpdated);
				}
			}
		}

		TileDetails->SetInfo(Tile.Info, LoadedLevel.Get());
	}
	else
	{
		TileDetails->PackageName = LevelCollectionModel.GetWorld()->GetOutermost()->GetFName();
		TileDetails->bPersistentLevel = true;
		LoadedLevel = LevelCollectionModel.GetWorld()->PersistentLevel;
	}
}

FWorldTileModel::~FWorldTileModel()
{
	if (TileDetails)
	{
		TileDetails->PositionChangedEvent.RemoveAll(this);
		TileDetails->ParentPackageNameChangedEvent.RemoveAll(this);
		
		TileDetails->RemoveFromRoot();
		TileDetails->MarkPendingKill();
	}

	if (LoadedLevel.IsValid())
	{
		LoadedLevel.Get()->LevelBoundsActorUpdated().RemoveAll(this);
	}
}

UObject* FWorldTileModel::GetNodeObject()
{
	// this pointer is used as unique key in SNodePanel
	return TileDetails;
}

ULevel* FWorldTileModel::GetLevelObject() const
{
	return LoadedLevel.Get();
}

bool FWorldTileModel::IsRootTile() const
{
	return TileDetails->bPersistentLevel;
}

void FWorldTileModel::SetAssetName(const FName& PackageName)
{
	FString AssetNameString = FString::Printf(TEXT("%s %s"), *UPackage::StaticClass()->GetName(), *PackageName.ToString());
	AssetName = FName(*AssetNameString);
}

FName FWorldTileModel::GetAssetName() const
{
	return AssetName;
}

FName FWorldTileModel::GetLongPackageName() const
{
	return TileDetails->PackageName;
}

void FWorldTileModel::UpdateAsset(const FAssetData& AssetData)
{
	check(TileDetails != nullptr);
	const FName OldPackageName = TileDetails->PackageName;

	// Patch up any parent references which have been renamed
	for (const TSharedPtr<FLevelModel>& LevelModel : LevelCollectionModel.GetAllLevels())
	{
		TSharedPtr<FWorldTileModel> WorldTileModel = StaticCastSharedPtr<FWorldTileModel>(LevelModel);

		check(WorldTileModel->TileDetails != nullptr);
		if (WorldTileModel->TileDetails->ParentPackageName == OldPackageName)
		{
			WorldTileModel->TileDetails->ParentPackageName = AssetData.PackageName;
		}
	}

	const FName PackageName = AssetData.PackageName;
	SetAssetName(PackageName);
	TileDetails->PackageName = PackageName;
}

FVector2D FWorldTileModel::GetLevelPosition2D() const
{
	if (TileDetails->Bounds.IsValid)
	{
		FVector2D LevelPosition = GetLevelCurrentPosition();
		return LevelPosition - FVector2D(TileDetails->Bounds.GetExtent()) + GetLevelTranslationDelta();
	}

	return FVector2D(0, 0);
}

FVector2D FWorldTileModel::GetLevelSize2D() const
{
	if (TileDetails->Bounds.IsValid)
	{
		FVector LevelSize = TileDetails->Bounds.GetSize();
		return FVector2D(LevelSize.X, LevelSize.Y);
	}
	
	return FVector2D(-1, -1);
}

void FWorldTileModel::OnDrop(const TSharedPtr<FLevelDragDropOp>& Op)
{
	FLevelModelList LevelModelList;

	for (auto It = Op->LevelsToDrop.CreateConstIterator(); It; ++It)
	{
		ULevel* Level = (*It).Get();
		TSharedPtr<FLevelModel> LevelModel = LevelCollectionModel.FindLevelModel(Level);
		if (LevelModel.IsValid())
		{
			LevelModelList.Add(LevelModel);
		}
	}	
	
	if (LevelModelList.Num())
	{
		LevelCollectionModel.AssignParent(LevelModelList, this->AsShared());
	}
}

bool FWorldTileModel::IsGoodToDrop(const TSharedPtr<FLevelDragDropOp>& Op) const
{
	return true;
}

bool FWorldTileModel::ShouldBeVisible(FBox EditableArea) const
{
	if (IsRootTile())
	{
		return true;
	}

	// Visibility does not depend on level positions when world origin rebasing is disabled
	if (!LevelCollectionModel.GetWorld()->GetWorldSettings()->bEnableWorldOriginRebasing)
	{
		return true;
	}

	// When this hack is activated level should be visible regardless of current world origin
	if (LevelCollectionModel.GetWorld()->WorldComposition->bTemporallyDisableOriginTracking)
	{
		return true;
	}
	
	FBox LevelBBox = GetLevelBounds();

	// Visible if level has no valid bounds
	if (!LevelBBox.IsValid)
	{
		return true;
	}

	// Visible if level bounds inside editable area
	if (EditableArea.IsInsideXY(LevelBBox))
	{
		return true;
	}

	// Visible if level bounds intersects editable area
	if (LevelBBox.IntersectXY(EditableArea))
	{
		return true;
	}

	return false;
}

void FWorldTileModel::SetVisible(bool bVisible)
{
	if (LevelCollectionModel.IsReadOnly())
	{
		return;
	}
		
	ULevel* Level = GetLevelObject();
	
	if (Level == NULL)
	{
		return;
	}
	
	// Don't create unnecessary transactions
	if (IsVisible() == bVisible)
	{
		return;
	}

	// Can not show level outside of editable area
	if (bVisible && !ShouldBeVisible(LevelCollectionModel.EditableWorldArea()))
	{
		return;
	}
	
	// The level is no longer shelved
	bWasShelved = false;
	
	{
		FScopedTransaction Transaction( LOCTEXT("ToggleVisibility", "Toggle Level Visibility") );
		
		// This call hides/shows all owned actors, etc
		EditorLevelUtils::SetLevelVisibility( Level, bVisible, true );	// we need to enable layers too here so the LODs export correctly
			
		// Ensure operation is completed succesfully
		check(GetLevelObject()->bIsVisible == bVisible);

		// Now there is no way to correctly undo level visibility
		// remove ability to undo this operation
		Transaction.Cancel();
	}
}

bool FWorldTileModel::IsShelved() const
{
	return (GetLevelObject() == NULL || bWasShelved);
}

void FWorldTileModel::Shelve()
{
	if (LevelCollectionModel.IsReadOnly() || IsShelved() || IsRootTile() || !LevelCollectionModel.GetWorld()->GetWorldSettings()->bEnableWorldOriginRebasing)
	{
		return;
	}
	
	//
	SetVisible(false);
	bWasShelved = true;
}

void FWorldTileModel::Unshelve()
{
	if (LevelCollectionModel.IsReadOnly() || !IsShelved())
	{
		return;
	}

	//
	SetVisible(true);
	bWasShelved = false;
}

bool FWorldTileModel::IsLandscapeBased() const
{
	return Landscape.IsValid();
}

bool FWorldTileModel::IsTiledLandscapeBased() const
{
	if (IsLandscapeBased() && !GetLandscape()->ReimportHeightmapFilePath.IsEmpty())
	{
		// Check if single landscape actor resolution matches heightmap file size
		IFileManager& FileManager = IFileManager::Get();
		const int64 ImportFileSize = FileManager.FileSize(*GetLandscape()->ReimportHeightmapFilePath);
		
		FIntRect ComponentsRect = GetLandscape()->GetBoundingRect();
		int64 LandscapeSamples	= (int64)(ComponentsRect.Width()+1)*(ComponentsRect.Height()+1);
		// Height samples are 2 bytes wide
		if (LandscapeSamples*2 == ImportFileSize)
		{
			return true;
		}
	}

	return false;
}

bool FWorldTileModel::IsLandscapeProxy() const
{
	return (Landscape.IsValid() && !Landscape.Get()->IsA(ALandscape::StaticClass()));
}

bool FWorldTileModel::IsInLayersList(const TArray<FWorldTileLayer>& InLayerList) const
{
	if (InLayerList.Num() > 0)
	{
		return InLayerList.Contains(TileDetails->Layer);
	}
	
	return true;
}

ALandscapeProxy* FWorldTileModel::GetLandscape() const
{
	return Landscape.Get();
}

void FWorldTileModel::AssignToLayer(const FWorldTileLayer& InLayer)
{
	if (LevelCollectionModel.IsReadOnly())
	{
		return;
	}
	
	if (!IsRootTile() && IsLoaded())
	{
		TileDetails->Layer = InLayer;
		OnLevelInfoUpdated();
	}
}

FBox FWorldTileModel::GetLevelBounds() const
{
	// Level local bounding box
	FBox Bounds = TileDetails->Bounds;

	if (Bounds.IsValid)
	{
		// Current level position in the world
		FVector LevelPosition(GetLevelCurrentPosition(), 0.f);
		FVector LevelExtent = Bounds.GetExtent();
		// Calculate bounding box in world space
		Bounds.Min = LevelPosition - LevelExtent;
		Bounds.Max = LevelPosition + LevelExtent;
	}
	
	return Bounds;
}

FIntPoint FWorldTileModel::CalcAbsoluteLevelPosition() const
{
	TSharedPtr<FWorldTileModel> ParentModel = StaticCastSharedPtr<FWorldTileModel>(GetParent());
	if (ParentModel.IsValid())
	{
		return TileDetails->Position + ParentModel->CalcAbsoluteLevelPosition();
	}

	return IsRootTile() ? FIntPoint::ZeroValue : TileDetails->Position;
}

FIntPoint FWorldTileModel::GetAbsoluteLevelPosition() const
{
	return IsRootTile() ? FIntPoint::ZeroValue : TileDetails->AbsolutePosition;
}
	
FIntPoint FWorldTileModel::GetRelativeLevelPosition() const
{
	return IsRootTile() ? FIntPoint::ZeroValue : TileDetails->Position;
}

FVector2D FWorldTileModel::GetLevelCurrentPosition() const
{
	if (TileDetails->Bounds.IsValid)
	{
		UWorld* CurrentWorld = (LevelCollectionModel.IsSimulating() ? LevelCollectionModel.GetSimulationWorld() : LevelCollectionModel.GetWorld());

		FVector2D LevelLocalPosition(TileDetails->Bounds.GetCenter());
		FIntPoint LevelOffset = GetAbsoluteLevelPosition();
			
		return LevelLocalPosition + FVector2D(LevelOffset - GetWorldOriginLocationXY(CurrentWorld)); 
	}

	return FVector2D(0, 0);
}

FVector2D FWorldTileModel::GetLandscapeComponentSize() const
{
	return LandscapeComponentSize;
}

void FWorldTileModel::SetLevelPosition(const FIntPoint& InPosition)
{
	// Parent absolute position
	TSharedPtr<FWorldTileModel> ParentModel = StaticCastSharedPtr<FWorldTileModel>(GetParent());
	FIntPoint ParentAbsolutePostion = ParentModel.IsValid() ? ParentModel->GetAbsoluteLevelPosition() : FIntPoint::ZeroValue;
	
	// Actual offset
	FIntPoint Offset = InPosition - TileDetails->AbsolutePosition;
	
	// Update absolute position
	TileDetails->AbsolutePosition = InPosition;

	// Assign new position as relative to parent
	TileDetails->Position = TileDetails->AbsolutePosition - ParentAbsolutePostion;
	
	// Flush changes to level package
	OnLevelInfoUpdated();
	
	// Move actors if necessary
	ULevel* Level = GetLevelObject();
	if (Level != nullptr && Level->bIsVisible)
	{
		// Shelve level, if during this translation level will end up out of Editable area
		if (!ShouldBeVisible(LevelCollectionModel.EditableWorldArea()))
		{
			Shelve();
		}
		
		// Move actors
		if (Offset != FIntPoint::ZeroValue)
		{
			Level->ApplyWorldOffset(FVector(Offset), false);

			for (auto Actor : Level->Actors)
			{
				if (Actor != nullptr)
				{
					Editor->BroadcastOnActorMoved(Actor);
				}
			}
		}
	}

	if (IsLandscapeBased())
	{
		UpdateLandscapeSectionsOffset(Offset);
		bool bShowWarnings = true;
		ULandscapeInfo::RecreateLandscapeInfo(LevelCollectionModel.GetWorld(), bShowWarnings);
	}
	
	// Transform child levels
	for (auto It = AllChildren.CreateIterator(); It; ++It)
	{
		TSharedPtr<FWorldTileModel> ChildModel = StaticCastSharedPtr<FWorldTileModel>(*It);
		FIntPoint ChildPosition = TileDetails->AbsolutePosition + ChildModel->GetRelativeLevelPosition();
		ChildModel->SetLevelPosition(ChildPosition);
	}
}

void FWorldTileModel::UpdateLandscapeSectionsOffset(FIntPoint LevelOffset)
{
	ALandscapeProxy* LandscapeProxy = GetLandscape();
	if (LandscapeProxy)
	{
		// Calculate new section coordinates for landscape
		FVector	DrawScale = LandscapeProxy->GetRootComponent()->RelativeScale3D;
		FIntPoint QuadsSpaceOffset;
		QuadsSpaceOffset.X = FMath::RoundToInt(LevelOffset.X / DrawScale.X);
		QuadsSpaceOffset.Y = FMath::RoundToInt(LevelOffset.Y / DrawScale.Y);
		LandscapeProxy->SetAbsoluteSectionBase(QuadsSpaceOffset + LandscapeProxy->LandscapeSectionOffset);
	}
}

void FWorldTileModel::Update()
{
	if (!IsRootTile())
	{
		Landscape = NULL;
		LandscapeComponentsXY.Empty();
		LandscapeComponentSize = FVector2D(0.f, 0.f);
		LandscapeComponentsRectXY = FIntRect(FIntPoint(MAX_int32, MAX_int32), FIntPoint(MIN_int32, MIN_int32));
		
		ULevel* Level = GetLevelObject();
		// Receive tile info from world composition
		FWorldTileInfo Info = LevelCollectionModel.GetWorld()->WorldComposition->GetTileInfo(TileDetails->PackageName);
		TileDetails->SetInfo(Info, Level);
						
		if (Level != nullptr && Level->bIsVisible)
		{
			if (Level->LevelBoundsActor.IsValid())
			{
				TileDetails->Bounds = Level->LevelBoundsActor.Get()->GetComponentsBoundingBox();
			}
								
			// True level bounds without offsets applied
			if (TileDetails->Bounds.IsValid)
			{
				FBox LevelWorldBounds = TileDetails->Bounds;
				FIntPoint LevelAbolutePosition = GetAbsoluteLevelPosition();
				FIntPoint LevelOffset = LevelAbolutePosition - GetWorldOriginLocationXY(LevelCollectionModel.GetWorld());

				TileDetails->Bounds = LevelWorldBounds.ShiftBy(-FVector(LevelOffset));
			}

			OnLevelInfoUpdated();

			// Cache landscape information
			for (int32 ActorIndex = 0; ActorIndex < Level->Actors.Num() ; ++ActorIndex)
			{
				AActor* Actor = Level->Actors[ActorIndex];
				if (Actor)
				{
					ALandscapeProxy* LandscapeActor = Cast<ALandscapeProxy>(Actor);
					if (LandscapeActor)
					{
						Landscape = LandscapeActor;
						break;
					}
				}
			}
		}
	}

	FLevelModel::Update();
}

void FWorldTileModel::LoadLevel()
{
	// Level is currently loading or has been loaded already
	if (bLoadingLevel || LoadedLevel.IsValid())
	{
		return;
	}

	// Create transient level streaming object and add to persistent level
	ULevelStreaming* LevelStreaming = GetAssosiatedStreamingLevel();

	// Whether this tile should be made visible at current world bounds
	const bool bShouldBeVisible = ShouldBeVisible(LevelCollectionModel.EditableWorldArea());
	
	// Load level
	bLoadingLevel = true;
	LevelStreaming->bShouldBeLoaded = true;
	LevelStreaming->bShouldBeVisible = false; // Should be always false in the Editor
	LevelStreaming->bShouldBeVisibleInEditor = bShouldBeVisible;
	LevelCollectionModel.GetWorld()->FlushLevelStreaming();
	bLoadingLevel = false;
	// Mark tile as shelved in case it is hidden(does not fit to world bounds)
	bWasShelved = !bShouldBeVisible;
	//
	LoadedLevel = LevelStreaming->GetLoadedLevel();
	// Enable tile properties
	TileDetails->bTileEditable = (LoadedLevel != nullptr);
}

ULevelStreaming* FWorldTileModel::GetAssosiatedStreamingLevel()
{
	FName PackageName = TileDetails->PackageName;
	UWorld* PersistentWorld = LevelCollectionModel.GetWorld();
				
	// Try to find existing object first
	auto Predicate = [&](ULevelStreaming* StreamingLevel) 
	{
		return (StreamingLevel->GetWorldAssetPackageFName() == PackageName && StreamingLevel->HasAnyFlags(RF_Transient));
	};
	
	int32 Index = PersistentWorld->StreamingLevels.IndexOfByPredicate(Predicate);
	
	if (Index == INDEX_NONE)
	{
		// Create new streaming level
		auto AssociatedStreamingLevel = NewObject<ULevelStreamingKismet>(PersistentWorld, NAME_None, RF_Transient);

		//
		AssociatedStreamingLevel->SetWorldAssetByPackageName(PackageName);
		AssociatedStreamingLevel->LevelColor		= FLinearColor::MakeRandomColor();
		AssociatedStreamingLevel->LevelTransform	= FTransform::Identity;
		AssociatedStreamingLevel->PackageNameToLoad	= PackageName;
		//
		Index =PersistentWorld->StreamingLevels.Add(AssociatedStreamingLevel);
	}

	return PersistentWorld->StreamingLevels[Index];
}

void FWorldTileModel::OnLevelAddedToWorld(ULevel* InLevel)
{
	if (!LoadedLevel.IsValid())
	{
		LoadedLevel = InLevel;
	}
		
	FLevelModel::OnLevelAddedToWorld(InLevel);

	EnsureLevelHasBoundsActor();
	LoadedLevel.Get()->LevelBoundsActorUpdated().AddRaw(this, &FWorldTileModel::OnLevelBoundsActorUpdated);
}

void FWorldTileModel::OnLevelRemovedFromWorld()
{
	FLevelModel::OnLevelRemovedFromWorld();

	LoadedLevel.Get()->LevelBoundsActorUpdated().RemoveAll(this);
}

void FWorldTileModel::OnParentChanged()
{
	// Transform level offset to absolute
	TileDetails->Position = GetAbsoluteLevelPosition();
	// Remove link to parent	
	TileDetails->ParentPackageName = NAME_None;
	
	// Attach to new parent
	TSharedPtr<FWorldTileModel> NewParentTileModel = StaticCastSharedPtr<FWorldTileModel>(GetParent());
	if (!NewParentTileModel->IsRootTile())
	{
		// Transform level offset to relative
		TileDetails->Position-= NewParentTileModel->GetAbsoluteLevelPosition();
		// Setup link to parent 
		TileDetails->ParentPackageName = NewParentTileModel->TileDetails->PackageName;
	}
		
	OnLevelInfoUpdated();
}

bool FWorldTileModel::IsVisibleInCompositionView() const
{
	return !TileDetails->bHideInTileView && LevelCollectionModel.PassesAllFilters(*this);
}

void FWorldTileModel::OnLevelBoundsActorUpdated()
{
	Update();
}

void FWorldTileModel::EnsureLevelHasBoundsActor()
{
	ULevel* Level = GetLevelObject();
	if (Level && !Level->LevelBoundsActor.IsValid())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.OverrideLevel = Level;

		LevelCollectionModel.GetWorld()->SpawnActor<ALevelBounds>(SpawnParameters);
	}
}

void FWorldTileModel::SortRecursive()
{
	AllChildren.Sort(FCompareByLongPackageName());
	FilteredChildren.Sort(FCompareByLongPackageName());
	
	for (auto It = AllChildren.CreateIterator(); It; ++It)
	{
		StaticCastSharedPtr<FWorldTileModel>(*It)->SortRecursive();
	}
}

void FWorldTileModel::OnLevelInfoUpdated()
{
	if (!IsRootTile())
	{
		LevelCollectionModel.GetWorld()->WorldComposition->OnTileInfoUpdated(TileDetails->PackageName, TileDetails->GetInfo());
		ULevel* Level = GetLevelObject();
		if (Level)
		{
			bool bMarkDirty = false;
			bMarkDirty|= !(Level->LevelSimplification[0] == TileDetails->LOD1.SimplificationDetails);
			bMarkDirty|= !(Level->LevelSimplification[1] == TileDetails->LOD2.SimplificationDetails);
			bMarkDirty|= !(Level->LevelSimplification[2] == TileDetails->LOD3.SimplificationDetails);
			bMarkDirty|= !(Level->LevelSimplification[3] == TileDetails->LOD4.SimplificationDetails);
			
			if (bMarkDirty)
			{
				Level->LevelSimplification[0] = TileDetails->LOD1.SimplificationDetails;
				Level->LevelSimplification[1] = TileDetails->LOD2.SimplificationDetails;
				Level->LevelSimplification[2] = TileDetails->LOD3.SimplificationDetails;
				Level->LevelSimplification[3] = TileDetails->LOD4.SimplificationDetails;
				Level->MarkPackageDirty();
			}
		}
	}
}

void FWorldTileModel::OnPositionPropertyChanged()
{
	FWorldTileInfo Info = LevelCollectionModel.GetWorld()->WorldComposition->GetTileInfo(TileDetails->PackageName);

	if (GetLevelObject())
	{
		// Get the delta
		FIntPoint Delta = TileDetails->Position - Info.Position;

		// Snap the delta
		FLevelModelList LevelsList; LevelsList.Add(this->AsShared());
		FVector2D SnappedDelta = LevelCollectionModel.SnapTranslationDelta(LevelsList, FVector2D(Delta), false, 0.f);

		// Set new level position
		SetLevelPosition(Info.AbsolutePosition + FIntPoint(SnappedDelta.X, SnappedDelta.Y));
		return;
	}
	
	// Restore original value
	TileDetails->Position = Info.Position;
}

void FWorldTileModel::OnParentPackageNamePropertyChanged()
{	
	if (GetLevelObject())
	{
		TSharedPtr<FLevelModel> NewParent = LevelCollectionModel.FindLevelModel(TileDetails->ParentPackageName);
		// Assign to a root level if new parent is not found or we assigning to ourself 
		if (!NewParent.IsValid() || NewParent.Get() == this) 
		{
			NewParent = static_cast<FWorldTileCollectionModel&>(LevelCollectionModel).GetWorldRootModel();
		}

		FLevelModelList LevelList; LevelList.Add(this->AsShared());
		LevelCollectionModel.AssignParent(LevelList, NewParent);
		return;
	}
	
	// Restore original parent
	FWorldTileInfo Info = LevelCollectionModel.GetWorld()->WorldComposition->GetTileInfo(TileDetails->PackageName);
	TileDetails->ParentPackageName = FName(*Info.ParentTilePackageName);
}

void FWorldTileModel::OnLODSettingsPropertyChanged()
{
	OnLevelInfoUpdated();
}

void FWorldTileModel::OnZOrderPropertyChanged()
{
	OnLevelInfoUpdated();
}

void FWorldTileModel::OnHideInTileViewChanged()
{
	OnLevelInfoUpdated();
}

bool FWorldTileModel::CreateAdjacentLandscapeProxy(ALandscapeProxy* SourceLandscape, FIntPoint SourceTileOffset, FWorldTileModel::EWorldDirections InWhere)
{
	if (!IsLoaded())	
	{
		return false;
	}

	// Determine import parameters from source landscape
	FBox SourceLandscapeBounds = SourceLandscape->GetComponentsBoundingBox(true);
	FVector SourceLandscapeScale = SourceLandscape->GetRootComponent()->GetComponentToWorld().GetScale3D();
	FIntRect SourceLandscapeRect = SourceLandscape->GetBoundingRect();
	FIntPoint SourceLandscapeSize = SourceLandscapeRect.Size();

	FLandscapeImportSettings ImportSettings = {};
	ImportSettings.LandscapeGuid		= SourceLandscape->GetLandscapeGuid();
	ImportSettings.LandscapeMaterial	= SourceLandscape->GetLandscapeMaterial();
	ImportSettings.ComponentSizeQuads	= SourceLandscape->ComponentSizeQuads;
	ImportSettings.QuadsPerSection		= SourceLandscape->SubsectionSizeQuads;
	ImportSettings.SectionsPerComponent = SourceLandscape->NumSubsections;
	ImportSettings.SizeX				= SourceLandscapeRect.Width() + 1;
	ImportSettings.SizeY				= SourceLandscapeRect.Height() + 1;

	// Initialize with blank heightmap data
	ImportSettings.HeightData.AddUninitialized(ImportSettings.SizeX * ImportSettings.SizeY);
	for (auto& HeightSample : ImportSettings.HeightData)
	{
		HeightSample = 32768;
	}

	// Set proxy location at landscape bounds Min point
	ImportSettings.LandscapeTransform.SetLocation(FVector(0.f, 0.f, SourceLandscape->GetActorLocation().Z));
	ImportSettings.LandscapeTransform.SetScale3D(SourceLandscapeScale);
	
	// Create new landscape object
	ALandscapeProxy* AdjacentLandscape = ImportLandscapeTile(ImportSettings);
	if (AdjacentLandscape)
	{
		// Copy source landscape properties 
		AdjacentLandscape->GetSharedProperties(SourceLandscape);
		
		// Refresh level model bounding box
		FBox AdjacentLandscapeBounds = AdjacentLandscape->GetComponentsBoundingBox(true);
		TileDetails->Bounds = AdjacentLandscapeBounds;

		// Calculate proxy offset from source landscape actor
		FVector ProxyOffset(SourceLandscapeBounds.GetCenter() - AdjacentLandscapeBounds.GetCenter());

		// Add offset by chosen direction
		switch (InWhere)
		{
		case FWorldTileModel::XNegative:
			ProxyOffset += FVector(-SourceLandscapeScale.X*SourceLandscapeSize.X, 0.f, 0.f);
			break;
		case FWorldTileModel::XPositive:
			ProxyOffset += FVector(+SourceLandscapeScale.X*SourceLandscapeSize.X, 0.f, 0.f);
			break;
		case FWorldTileModel::YNegative:
			ProxyOffset += FVector(0.f, -SourceLandscapeScale.Y*SourceLandscapeSize.Y, 0.f);
			break;
		case FWorldTileModel::YPositive:
			ProxyOffset += FVector(0.f, +SourceLandscapeScale.Y*SourceLandscapeSize.Y, 0.f);
			break;
		}

		// Add source level position
		FIntPoint IntOffset = FIntPoint(ProxyOffset.X, ProxyOffset.Y) + GetWorldOriginLocationXY(LevelCollectionModel.GetWorld());

		// Move level with landscape proxy to desired position
		SetLevelPosition(IntOffset);
		return true;
	}

	return false;
}

ALandscapeProxy* FWorldTileModel::ImportLandscapeTile(const FLandscapeImportSettings& Settings)
{
	if (!IsLoaded())
	{
		return nullptr;
	}
	
	check(Settings.LandscapeGuid.IsValid())
	
	ALandscapeProxy* LandscapeProxy = Cast<UWorld>(LoadedLevel->GetOuter())->SpawnActor<ALandscapeProxy>();
	LandscapeProxy->SetActorTransform(Settings.LandscapeTransform);
		
	if (Settings.LandscapeMaterial)
	{
		LandscapeProxy->LandscapeMaterial = Settings.LandscapeMaterial;
	}
	
	// Cache pointer to landscape in the level model
	Landscape = LandscapeProxy;

	// Create landscape components	
	LandscapeProxy->Import(
		Settings.LandscapeGuid, 
		Settings.SizeX, 
		Settings.SizeY, 
		Settings.ComponentSizeQuads, 
		Settings.SectionsPerComponent, 
		Settings.QuadsPerSection, 
		Settings.HeightData.GetData(), 
		*Settings.HeightmapFilename, 
		Settings.ImportLayers);

	return LandscapeProxy;
}

#undef LOCTEXT_NAMESPACE
