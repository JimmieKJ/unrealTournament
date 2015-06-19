// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "SGameLayerManager.h"

#include "Engine/UserInterfaceSettings.h"
#include "Slate/SceneViewport.h"

#include "STooltipPresenter.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SPopup.h"

/* SGameLayerManager interface
 *****************************************************************************/

SGameLayerManager::SGameLayerManager()
{
}

void SGameLayerManager::Construct(const SGameLayerManager::FArguments& InArgs)
{
	SceneViewport = InArgs._SceneViewport;

	TSharedRef<SDPIScaler> DPIScaler =
		SNew(SDPIScaler)
		.DPIScale(this, &SGameLayerManager::GetGameViewportDPIScale)
		[
			SNew(SOverlay)
				
			+ SOverlay::Slot()
			[
				SAssignNew(PlayerCanvas, SCanvas)
			]

			+ SOverlay::Slot()
			[
				InArgs._Content.Widget
			]

			+ SOverlay::Slot()
			[
				SNew(SPopup)
				[
					SAssignNew(TooltipPresenter, STooltipPresenter)
				]
			]
		];

	if ( InArgs._UseScissor )
	{
		ChildSlot
		[
			SNew(SScissorRectBox)
			[
				DPIScaler
			]
		];
	}
	else
	{
		ChildSlot
		[
			DPIScaler
		];
	}
}

void SGameLayerManager::NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer)
{
	UpdateLayout();
}

void SGameLayerManager::NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer)
{
	UpdateLayout();
}

void SGameLayerManager::AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
	
	// NOTE: Returns FSimpleSlot but we're ignoring here.  Could be used for alignment though.
	PlayerLayer->Widget->AddSlot(ZOrder)
	[
		ViewportContent
	];
}

void SGameLayerManager::RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(Player);
	if ( PlayerLayerPtr )
	{
		TSharedPtr<FPlayerLayer> PlayerLayer = *PlayerLayerPtr;
		PlayerLayer->Widget->RemoveSlot(ViewportContent);
	}
}

void SGameLayerManager::ClearWidgetsForPlayer(ULocalPlayer* Player)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(Player);
	if ( PlayerLayerPtr )
	{
		TSharedPtr<FPlayerLayer> PlayerLayer = *PlayerLayerPtr;
		PlayerLayer->Widget->ClearChildren();
	}
}

void SGameLayerManager::ClearWidgets()
{
	PlayerCanvas->ClearChildren();

	for(const auto& LayerIt : PlayerLayers)
	{
		const TSharedPtr<FPlayerLayer>& Layer = LayerIt.Value;
		if (Layer.IsValid())
		{
			Layer->Slot = nullptr;
		}
	}
	PlayerLayers.Reset();
}

void SGameLayerManager::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CachedGeometry = AllottedGeometry;

	UpdateLayout();
}

bool SGameLayerManager::OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent)
{
	TooltipPresenter->SetContent(TooltipContent.IsValid() ? TooltipContent.ToSharedRef() : SNullWidget::NullWidget);

	return true;
}

float SGameLayerManager::GetGameViewportDPIScale() const
{
	if ( const FSceneViewport* Viewport = SceneViewport.Get() )
	{
		FIntPoint ViewportSize = Viewport->GetSize();
		return GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(ViewportSize);
	}

	return 1;
}

void SGameLayerManager::UpdateLayout()
{
	if ( const FSceneViewport* Viewport = SceneViewport.Get() )
	{
		if ( UWorld* World = Viewport->GetClient()->GetWorld() )
		{
			if ( World->IsGameWorld() == false )
			{
				PlayerLayers.Reset();
				return;
			}

			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				const TArray<ULocalPlayer*>& GamePlayers = GEngine->GetGamePlayers(World);

				RemoveMissingPlayerLayers(GamePlayers);
				AddOrUpdatePlayerLayers(CachedGeometry, ViewportClient, GamePlayers);
			}
		}
	}
}

TSharedPtr<SGameLayerManager::FPlayerLayer> SGameLayerManager::FindOrCreatePlayerLayer(ULocalPlayer* LocalPlayer)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(LocalPlayer);
	if ( PlayerLayerPtr == nullptr )
	{
		// Prevent any navigation outside of a player's layer once focus has been placed there.
		TSharedRef<FNavigationMetaData> StopNavigation = MakeShareable(new FNavigationMetaData());
		StopNavigation->SetNavigationStop(EUINavigation::Up);
		StopNavigation->SetNavigationStop(EUINavigation::Down);
		StopNavigation->SetNavigationStop(EUINavigation::Left);
		StopNavigation->SetNavigationStop(EUINavigation::Right);
		StopNavigation->SetNavigationStop(EUINavigation::Previous);
		StopNavigation->SetNavigationStop(EUINavigation::Next);

		// Create a new entry for the player
		TSharedPtr<FPlayerLayer> NewLayer = MakeShareable(new FPlayerLayer());

		// Create a new overlay widget to house any widgets we want to display for the player.
		NewLayer->Widget = SNew(SOverlay)
			.AddMetaData(StopNavigation);
		
		// Add the overlay to the player canvas, which we'll update every frame to match
		// the dimensions of the player's split screen rect.
		PlayerCanvas->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Expose(NewLayer->Slot)
			[
				NewLayer->Widget.ToSharedRef()
			];

		PlayerLayerPtr = &PlayerLayers.Add(LocalPlayer, NewLayer);
	}

	return *PlayerLayerPtr;
}

void SGameLayerManager::RemoveMissingPlayerLayers(const TArray<ULocalPlayer*>& GamePlayers)
{
	TArray<ULocalPlayer*> ToRemove;

	// Find the player layers for players that no longer exist
	for ( TMap< ULocalPlayer*, TSharedPtr<FPlayerLayer> >::TIterator It(PlayerLayers); It; ++It )
	{
		ULocalPlayer* Key = ( *It ).Key;
		if ( !GamePlayers.Contains(Key) )
		{
			ToRemove.Add(Key);
		}
	}

	// Remove the missing players
	for ( ULocalPlayer* Player : ToRemove )
	{
		RemovePlayerWidgets(Player);
	}
}

void SGameLayerManager::RemovePlayerWidgets(ULocalPlayer* LocalPlayer)
{
	TSharedPtr<FPlayerLayer> Layer = PlayerLayers.FindRef(LocalPlayer);
	PlayerCanvas->RemoveSlot(Layer->Widget.ToSharedRef());

	PlayerLayers.Remove(LocalPlayer);
}

void SGameLayerManager::AddOrUpdatePlayerLayers(const FGeometry& AllottedGeometry, UGameViewportClient* ViewportClient, const TArray<ULocalPlayer*>& GamePlayers)
{
	ESplitScreenType::Type SplitType = ViewportClient->GetCurrentSplitscreenConfiguration();
	TArray<struct FSplitscreenData>& SplitInfo = ViewportClient->SplitscreenInfo;

	float InverseDPIScale = 1.0f / GetGameViewportDPIScale();

	// Add and Update Player Layers
	for ( int32 PlayerIndex = 0; PlayerIndex < GamePlayers.Num(); PlayerIndex++ )
	{
		ULocalPlayer* Player = GamePlayers[PlayerIndex];

		if ( SplitType < SplitInfo.Num() && PlayerIndex < SplitInfo[SplitType].PlayerData.Num() )
		{
			TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
			FPerPlayerSplitscreenData& SplitData = SplitInfo[SplitType].PlayerData[PlayerIndex];

			// Viewport Sizes
			FVector2D Size, Position;
			Size.X = SplitData.SizeX;
			Size.Y = SplitData.SizeY;
			Position.X = SplitData.OriginX;
			Position.Y = SplitData.OriginY;

			Size = Size * AllottedGeometry.GetLocalSize() * InverseDPIScale;
			Position = Position * AllottedGeometry.GetLocalSize() * InverseDPIScale;

			PlayerLayer->Slot->Size(Size);
			PlayerLayer->Slot->Position(Position);
		}
	}
}