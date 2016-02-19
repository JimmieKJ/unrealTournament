// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "../Widgets/SUTScaleBox.h"
#include "../Base/SUTWindowBase.h"
#include "../SUTStyle.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUTWindowBase::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	PlayerOwner = InPlayerOwner;
	TabStop = 0;
	bSkipWorldRender = false;
	SetWindowState(EUIWindowState::Initializing);

	// Calculate the size.  If the size is relative, then scale it against the designed by resolution as the 
	// DPI Scale panel will take care of the rest.
	FVector2D DesignedRez(1920, 1080);
	ActualSize = InArgs._Size;
	if (InArgs._bSizeIsRelative)
	{
		ActualSize *= DesignedRez;
	}

	// Now we have to center it.  The tick here is we have to scale the current viewportSize UP by scale used in the DPI panel other
	// we can't position properly.

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	if (InArgs._bPositionIsRelative)
	{
		ActualPosition = (DesignedRez * InArgs._Position) - (ActualSize * InArgs._AnchorPoint);
	}
	else
	{
		ActualPosition = InArgs._Position - (ActualSize * InArgs._AnchorPoint);
	}

	// Build the content holder for this window.

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SImage)
			.Image(InArgs._bShadow ? SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded") : new FSlateNoResource)
			.ColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,InArgs._ShadowAlpha))
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Canvas, SCanvas)

			// We use a Canvas Slot to position and size the dialog.  
			+ SCanvas::Slot()
			.Position(ActualPosition)
			.Size(ActualSize)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				// This is our primary overlay that holds the content of this menu.
				SAssignNew(Content,SOverlay)
			]
		]
	];

	BuildWindow();
}

void SUTWindowBase::BuildWindow()
{
	// All children should override this function to build their widgets.
	Content->AddSlot()
	//.Padding(FMargin(InArgs._ContentPadding.X, InArgs._ContentPadding.Y, InArgs._ContentPadding.X, InArgs._ContentPadding.Y))
	[
		SNew(SCanvas)
	];
}

void SUTWindowBase::SetWindowState(EUIWindowState::Type NewWindowState)
{
	WindowState = NewWindowState;
}

EUIWindowState::Type SUTWindowBase::GetWindowState()
{
	return WindowState;
}

void SUTWindowBase::Open()
{
	SetWindowState(EUIWindowState::Opening);
	OnOpened();
}
void SUTWindowBase::OnOpened()
{
	SetWindowState(EUIWindowState::Active);
}
bool SUTWindowBase::Close()
{
	SetWindowState(EUIWindowState::Closing);
	OnClosed();

	return true;
}
void SUTWindowBase::OnClosed()
{
	SetWindowState(EUIWindowState::Closed);
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->WindowClosed(SharedThis(this));
	}
}


#endif