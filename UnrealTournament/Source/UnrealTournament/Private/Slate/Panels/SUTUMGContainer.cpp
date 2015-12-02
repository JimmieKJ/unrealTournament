
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTGameEngine.h"
#include "SUTUMGContainer.h"
#include "../Widgets/SUTBorder.h"


#if !UE_SERVER

SUTUMGContainer::~SUTUMGContainer()
{
	UMGWidget.Reset();
}

void SUTUMGContainer::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> PlayerOwner)
{
	UMGClass = InArgs._UMGClass;
	bShowBackButton = InArgs._bShowBackButton;

	// The SlateContainer will be created in ConstructPanel
	SUWPanel::Construct( SUWPanel::FArguments(), PlayerOwner );

	if (SlateContainer.IsValid() && !UMGClass.IsEmpty())
	{

		UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
		if (UTEngine)
		{
			UClass* UMGWidgetClass = LoadClass<UUserWidget>(NULL, *UMGClass, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
			if (UMGWidgetClass)
			{
				UMGWidget = CreateWidget<UUserWidget>(PlayerOwner->GetWorld(), UMGWidgetClass);
				if (UMGWidget.IsValid())
				{
					SlateContainer->AddSlot()
					[
						UMGWidget->TakeWidget()
					];
				}
			}
		}
	}
}


void SUTUMGContainer::ConstructPanel(FVector2D ViewportSize)
{
	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(AnimWidget, SUTBorder)
			.OnAnimEnd(this, &SUTUMGContainer::AnimEnd)
			[
				SAssignNew(SlateContainer, SOverlay)
			]
		]
	];
}

void SUTUMGContainer::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(100.0f, 0.0f), FVector2D(0.0f, 0.0f), 0.0f, 1.0f, 0.3f);
	}
}

void SUTUMGContainer::OnHidePanel()
{
	bClosing = true;
	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(0.0f, 0.0f), FVector2D(-100.0f, 0.0f), 1.0f, 0.0f, 0.3f);
	}
	else
	{
		SUWPanel::OnHidePanel();
	}
}


void SUTUMGContainer::AnimEnd()
{
	if (bClosing)
	{
		bClosing = false;
		TSharedPtr<SWidget> Panel = this->AsShared();
		ParentWindow->PanelHidden(Panel);
		ParentWindow.Reset();
	}
}

void SUTUMGContainer::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
}


#endif