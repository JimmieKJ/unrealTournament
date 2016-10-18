
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTGameEngine.h"
#include "SUTUMGPanel.h"
#include "../Widgets/SUTBorder.h"


#if !UE_SERVER

SUTUMGPanel::~SUTUMGPanel()
{
	UMGWidget.Reset();
}

void SUTUMGPanel::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	UMGClass = InArgs._UMGClass;
	bShowBackButton = InArgs._bShowBackButton;

	// The SlateContainer will be created in ConstructPanel
	SUTPanelBase::Construct( SUTPanelBase::FArguments(), InPlayerOwner);

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


void SUTUMGPanel::ConstructPanel(FVector2D ViewportSize)
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
			.OnAnimEnd(this, &SUTUMGPanel::AnimEnd)
			[
				SAssignNew(SlateContainer, SOverlay)
			]
		]
	];
}

void SUTUMGPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);
	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(100.0f, 0.0f), FVector2D(0.0f, 0.0f), 0.0f, 1.0f, 0.3f);
	}
}

void SUTUMGPanel::OnHidePanel()
{
	bClosing = true;
	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(0.0f, 0.0f), FVector2D(-100.0f, 0.0f), 1.0f, 0.0f, 0.3f);
	}
	else
	{
		SUTPanelBase::OnHidePanel();
	}
}


void SUTUMGPanel::AnimEnd()
{
	if (bClosing)
	{
		bClosing = false;
		TSharedPtr<SWidget> Panel = this->AsShared();
		ParentWindow->PanelHidden(Panel);
		ParentWindow.Reset();
	}
}

void SUTUMGPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
}


#endif