// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "TileSetEditor.h"
#include "SSingleObjectDetailsPanel.h"
#include "SceneViewport.h"
#include "PaperEditorViewportClient.h"
#include "TileMapEditing/EdModeTileMap.h"
#include "GraphEditor.h"
#include "SPaperEditorViewport.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"

#define LOCTEXT_NAMESPACE "TileSetEditor"

//////////////////////////////////////////////////////////////////////////

const FName TileSetEditorAppName = FName(TEXT("TileSetEditorApp"));

//////////////////////////////////////////////////////////////////////////

struct FTileSetEditorModes
{
	// Mode identifiers
	static const FName StandardMode;
};

struct FTileSetEditorTabs
{
	// Tab identifiers
	static const FName DetailsID;
	static const FName TextureViewID;
};

//////////////////////////////////////////////////////////////////////////

const FName FTileSetEditorModes::StandardMode(TEXT("StandardMode"));
const FName FTileSetEditorTabs::DetailsID(TEXT("Details"));
const FName FTileSetEditorTabs::TextureViewID(TEXT("TextureCanvas"));

//////////////////////////////////////////////////////////////////////////
// FTileSetEditorViewportClient 

class FTileSetEditorViewportClient : public FPaperEditorViewportClient
{
public:
	FTileSetEditorViewportClient(UPaperTileSet* InTileSet)
		: TileSetBeingEdited(InTileSet)
		, bHasValidPaintRectangle(false)
		, TileIndex(INDEX_NONE)
	{
	}

	// FViewportClient interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	// End of FViewportClient interface

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	// End of FEditorViewportClient interface

public:
	// Tile set
	TWeakObjectPtr<UPaperTileSet> TileSetBeingEdited;

	bool bHasValidPaintRectangle;
	FViewportSelectionRectangle ValidPaintRectangle;
	int32 TileIndex;
};

void FTileSetEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	// Super will clear the viewport
	FPaperEditorViewportClient::Draw(Viewport, Canvas);

	// Can only proceed if we have a valid tile set
	UPaperTileSet* TileSet = TileSetBeingEdited.Get();
	if (TileSet == nullptr)
	{
		return;
	}

	UTexture2D* Texture = TileSet->TileSheet;

	if (Texture != nullptr)
	{
		const bool bUseTranslucentBlend = Texture->HasAlphaChannel();

		// Fully stream in the texture before drawing it.
		Texture->SetForceMipLevelsToBeResident(30.0f);
		Texture->WaitForStreaming();

		// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
		// the texture has transparency
		//if (TextureEditorPtr.Pin()->GetIsCheckeredBackground())
			// Handle case of using the checkerboard as a background
// 			if (TextureEditorPtr.Pin()->GetCheckeredBackground_Fill())
// 			{
// 				DrawTile(Canvas, 0.0f, 0.0f, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0.0f, 0.0f, (Viewport->GetSizeXY().X / CheckerboardTexture->GetSizeX()), (Viewport->GetSizeXY().Y / CheckerboardTexture->GetSizeY()), FLinearColor::White, CheckerboardTexture->Resource);
// 			}
// 			else
// 			{
// 				DrawTile(Canvas, XPos, YPos, Width, Height, 0.0f, 0.0f, (Width / CheckerboardTexture->GetSizeX()), (Height / CheckerboardTexture->GetSizeY()), FLinearColor::White, CheckerboardTexture->Resource);
// 			}

		FLinearColor TextureDrawColor = FLinearColor::White;

		const float XPos = -ZoomPos.X * ZoomAmount;
		const float YPos = -ZoomPos.Y * ZoomAmount;
		const float Width = Texture->GetSurfaceWidth() * ZoomAmount;
		const float Height = Texture->GetSurfaceHeight() * ZoomAmount;

		Canvas->DrawTile(XPos, YPos, Width, Height, 0.0f, 0.0f, 1.0f, 1.0f, TextureDrawColor, Texture->Resource, bUseTranslucentBlend);
	}

	// Overlay the selection rectangles
	DrawSelectionRectangles(Viewport, Canvas);

	if (bHasValidPaintRectangle)
	{
		const FViewportSelectionRectangle& Rect = ValidPaintRectangle;

		const float X = (Rect.TopLeft.X - ZoomPos.X) * ZoomAmount;
		const float Y = (Rect.TopLeft.Y - ZoomPos.Y) * ZoomAmount;
		const float W = Rect.Dimensions.X * ZoomAmount;
		const float H = Rect.Dimensions.Y * ZoomAmount;

		FCanvasBoxItem BoxItem(FVector2D(X, Y), FVector2D(W, H));
		BoxItem.SetColor(Rect.Color);
		Canvas->DrawItem(BoxItem);

	}

	if (TileIndex != INDEX_NONE)
	{
		const FString TileIndexString = FString::Printf(TEXT("Tile# %d"), TileIndex);

		int32 XL;
		int32 YL;
		StringSize(GEngine->GetLargeFont(), XL, YL, *TileIndexString);
		Canvas->DrawShadowedString(4, Viewport->GetSizeXY().Y - YL - 4, *TileIndexString, GEngine->GetLargeFont(), FLinearColor::White);
	}
}

FLinearColor FTileSetEditorViewportClient::GetBackgroundColor() const
{
	if (UPaperTileSet* TileSet = TileSetBeingEdited.Get())
	{
		return TileSet->BackgroundColor;
	}
	else
	{
		return FEditorViewportClient::GetBackgroundColor();
	}
}

//////////////////////////////////////////////////////////////////////////
// STileSetSelectorViewport

STileSetSelectorViewport::~STileSetSelectorViewport()
{
	TypedViewportClient = nullptr;
}

void STileSetSelectorViewport::Construct(const FArguments& InArgs, UPaperTileSet* InTileSet, FEdModeTileMap* InTileMapEditor)
{
	SelectionTopLeft = FIntPoint::ZeroValue;
	SelectionDimensions = FIntPoint::ZeroValue;

	TileSetPtr = InTileSet;
	TileMapEditor = InTileMapEditor;

	TypedViewportClient = MakeShareable(new FTileSetEditorViewportClient(InTileSet));

	SPaperEditorViewport::Construct(
		SPaperEditorViewport::FArguments().OnSelectionChanged(this, &STileSetSelectorViewport::OnSelectionChanged),
		TypedViewportClient.ToSharedRef());

	// Make sure we get input instead of the viewport stealing it
	ViewportWidget->SetVisibility(EVisibility::HitTestInvisible);
}

void STileSetSelectorViewport::ChangeTileSet(UPaperTileSet* InTileSet)
{
	if (InTileSet != TileSetPtr.Get())
	{
		TileSetPtr = InTileSet;
		TypedViewportClient->TileSetBeingEdited = InTileSet;

		// Update the selection rectangle
		RefreshSelectionRectangle();
		TypedViewportClient->Invalidate();
	}
}

FText STileSetSelectorViewport::GetTitleText() const
{
	if (UPaperTileSet* TileSet = TileSetPtr.Get())
	{
		return FText::FromString(TileSet->GetName());
	}

	return LOCTEXT("TileSetSelectorTitle", "Tile Set Selector");
}


void STileSetSelectorViewport::OnSelectionChanged(FMarqueeOperation Marquee, bool bIsPreview)
{
	UPaperTileSet* TileSetBeingEdited = TileSetPtr.Get();
	if (TileSetBeingEdited == nullptr)
	{
		return;
	}

	const FVector2D TopLeftUnrounded = Marquee.Rect.GetUpperLeft();
	const FVector2D BottomRightUnrounded = Marquee.Rect.GetLowerRight();
	if ((TopLeftUnrounded != FVector2D::ZeroVector) || Marquee.IsValid())
	{
		// Round down the top left corner
		SelectionTopLeft.X = FMath::Clamp<int>((int)(TopLeftUnrounded.X / TileSetBeingEdited->TileWidth), 0, TileSetBeingEdited->GetTileCountX());
		SelectionTopLeft.Y = FMath::Clamp<int>((int)(TopLeftUnrounded.Y / TileSetBeingEdited->TileHeight), 0, TileSetBeingEdited->GetTileCountY());

		// Round up the bottom right corner
		FIntPoint SelectionBottomRight;
		SelectionBottomRight.X = FMath::Clamp<int>(FMath::DivideAndRoundUp((int)BottomRightUnrounded.X, TileSetBeingEdited->TileWidth), 0, TileSetBeingEdited->GetTileCountX());
		SelectionBottomRight.Y = FMath::Clamp<int>(FMath::DivideAndRoundUp((int)BottomRightUnrounded.Y, TileSetBeingEdited->TileHeight), 0, TileSetBeingEdited->GetTileCountY());

		// Compute the new selection dimensions
		SelectionDimensions = SelectionBottomRight - SelectionTopLeft;
	}
	else
	{
		SelectionTopLeft.X = 0;
		SelectionTopLeft.Y = 0;
		SelectionDimensions.X = 0;
		SelectionDimensions.Y = 0;
	}

	const bool bHasSelection = (SelectionDimensions.X * SelectionDimensions.Y) > 0;
	if (bIsPreview && bHasSelection)
	{
		if (TileMapEditor != nullptr)
		{
			TileMapEditor->SetActivePaint(TileSetBeingEdited, SelectionTopLeft, SelectionDimensions);

			// Switch to paint brush mode if we were in the eraser mode since the user is trying to select some ink to paint with
			if (TileMapEditor->GetActiveTool() == ETileMapEditorTool::Eraser)
			{
				TileMapEditor->SetActiveTool(ETileMapEditorTool::Paintbrush);
			}
		}

		RefreshSelectionRectangle();
	}
}

void STileSetSelectorViewport::RefreshSelectionRectangle()
{
	if (FTileSetEditorViewportClient* Client = TypedViewportClient.Get())
	{
		UPaperTileSet* TileSetBeingEdited = TileSetPtr.Get();

		const bool bHasSelection = (SelectionDimensions.X * SelectionDimensions.Y) > 0;
		Client->bHasValidPaintRectangle = bHasSelection && (TileSetBeingEdited != nullptr);

		const int32 TileIndex = (bHasSelection && (TileSetBeingEdited != nullptr)) ? (SelectionTopLeft.X + SelectionTopLeft.Y * TileSetBeingEdited->GetTileCountX()) : INDEX_NONE;
		Client->TileIndex = TileIndex;

		if (bHasSelection)
		{
			Client->ValidPaintRectangle.Color = FLinearColor::White;
			Client->ValidPaintRectangle.Dimensions = FVector2D(SelectionDimensions.X * TileSetBeingEdited->TileWidth, SelectionDimensions.Y * TileSetBeingEdited->TileHeight);
			Client->ValidPaintRectangle.TopLeft = FVector2D(SelectionTopLeft.X * TileSetBeingEdited->TileWidth, SelectionTopLeft.Y * TileSetBeingEdited->TileHeight);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FTileSetViewportSummoner

struct FTileSetViewportSummoner : public FWorkflowTabFactory
{
public:
	FTileSetViewportSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
		: FWorkflowTabFactory(FTileSetEditorTabs::TextureViewID, InHostingApp)
	{
		TabLabel = LOCTEXT("TextureViewTabLabel", "Viewport");
		//TabIcon = FEditorStyle::GetBrush("Kismet.Tabs.Variables");

		bIsSingleton = true;

		ViewMenuDescription = LOCTEXT("TextureViewTabMenu_Description", "Viewport");
		ViewMenuTooltip = LOCTEXT("TextureViewTabMenu_ToolTip", "Shows the viewport");
	}

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		TSharedPtr<FTileSetEditor> TileSetEditorPtr = StaticCastSharedPtr<FTileSetEditor>(HostingApp.Pin());

		return SNew(STileSetSelectorViewport, TileSetEditorPtr->GetTileSetBeingEdited(), /*EdMode=*/ nullptr);
	}
};

/////////////////////////////////////////////////////
// STileSetPropertiesTabBody

class STileSetPropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(STileSetPropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning TileSet editor instance (the keeper of state)
	TWeakPtr<class FTileSetEditor> TileSetEditorPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FTileSetEditor> InTileSetEditor)
	{
		TileSetEditorPtr = InTileSetEditor;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments());
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		return TileSetEditorPtr.Pin()->GetTileSetBeingEdited();
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

//////////////////////////////////////////////////////////////////////////
// FTileSetDetailsSummoner

struct FTileSetDetailsSummoner : public FWorkflowTabFactory
{
public:
	FTileSetDetailsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
		: FWorkflowTabFactory(FTileSetEditorTabs::DetailsID, InHostingApp)
	{
		TabLabel = LOCTEXT("DetailsTabLabel", "Details");
		//TabIcon = FEditorStyle::GetBrush("Kismet.Tabs.Variables");

		bIsSingleton = true;

		ViewMenuDescription = LOCTEXT("DetailsTabMenu_Description", "Details");
		ViewMenuTooltip = LOCTEXT("DetailsTabMenu_ToolTip", "Shows the details panel");
	}

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		TSharedPtr<FTileSetEditor> TileSetEditorPtr = StaticCastSharedPtr<FTileSetEditor>(HostingApp.Pin());
		return SNew(STileSetPropertiesTabBody, TileSetEditorPtr);
	}
};

//////////////////////////////////////////////////////////////////////////
// FBasicTileSetEditorMode

class FBasicTileSetEditorMode : public FApplicationMode
{
public:
	FBasicTileSetEditorMode(TSharedPtr<class FTileSetEditor> InTileSetEditor, FName InModeName);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	// End of FApplicationMode interface

protected:
	TWeakPtr<FTileSetEditor> MyTileSetEditor;
	FWorkflowAllowedTabSet TabFactories;
};

FBasicTileSetEditorMode::FBasicTileSetEditorMode(TSharedPtr<class FTileSetEditor> InTileSetEditor, FName InModeName)
	: FApplicationMode(InModeName)
{
	MyTileSetEditor = InTileSetEditor;

	TabFactories.RegisterFactory(MakeShareable(new FTileSetViewportSummoner(InTileSetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FTileSetDetailsSummoner(InTileSetEditor)));

	TabLayout = FTabManager::NewLayout("Standalone_TileSetEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.8f)
				->SetHideTabWell(true)
				->AddTab(FTileSetEditorTabs::TextureViewID, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->SetHideTabWell(true)
				->AddTab(FTileSetEditorTabs::DetailsID, ETabState::OpenedTab)
			)
		);
}

void FBasicTileSetEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FTileSetEditor> Editor = MyTileSetEditor.Pin();

	Editor->PushTabFactories(TabFactories);
}

//////////////////////////////////////////////////////////////////////////
// FTileSetEditor

void FTileSetEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FWorkflowCentricApplication::RegisterTabSpawners(TabManager);
}

void FTileSetEditor::InitTileSetEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UPaperTileSet* InitTileSet)
{
	FAssetEditorManager::Get().CloseOtherEditors(InitTileSet, this);
	TileSetBeingEdited = InitTileSet;

	// Initialize the asset editor and spawn nothing (dummy layout)
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	InitAssetEditor(Mode, InitToolkitHost, TileSetEditorAppName, DummyLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ false, InitTileSet);

	// Create the modes and activate one (which will populate with a real layout)
	AddApplicationMode(
		FTileSetEditorModes::StandardMode, 
		MakeShareable(new FBasicTileSetEditorMode(SharedThis(this), FTileSetEditorModes::StandardMode)));
	SetCurrentMode(FTileSetEditorModes::StandardMode);
}

FName FTileSetEditor::GetToolkitFName() const
{
	return FName("TileSetEditor");
}

FText FTileSetEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "TileSet Editor" );
}

FText FTileSetEditor::GetToolkitName() const
{
	const bool bDirtyState = TileSetBeingEdited->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("TileSetName"), FText::FromString(TileSetBeingEdited->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty());
	return FText::Format(LOCTEXT("TileSetAppLabel", "{TileSetName}{DirtyState}"), Args);
}

FString FTileSetEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("TileSetEditor");
}

FLinearColor FTileSetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FTileSetEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TileSetBeingEdited);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE