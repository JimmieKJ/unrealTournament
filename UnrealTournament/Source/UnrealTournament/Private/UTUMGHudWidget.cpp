// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTUMGHudWidget.h"

UUTUMGHudWidget::UUTUMGHudWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayZOrder = 1.0f;
}

void UUTUMGHudWidget::AssociateHUD(AUTHUD* NewHUDAnchor)
{
	HUDAnchor = NewHUDAnchor;
	VisibilityDelegate.BindUFunction(this, FName(TEXT("GetHUDVisibility")));
}

ESlateVisibility UUTUMGHudWidget::ApplyHUDVisibility_Implementation() const
{
	if (HUDAnchor.IsValid())
	{
		return HUDAnchor->bShowScores ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	}

	return ESlateVisibility::Collapsed;
}

ESlateVisibility UUTUMGHudWidget::GetHUDVisibility() const
{
	return ApplyHUDVisibility();
}

void UUTUMGHudWidget::NotifyHUDWidgetIsDone()
{
	if (HUDAnchor.IsValid())
	{
		HUDAnchor->DeactivateActualUMGHudWidget(this);
		HUDAnchor.Reset();
	}
}

void UUTUMGHudWidget::SetLifeTime(float Lifespan)
{
	if (HUDAnchor.IsValid())
	{
		HUDAnchor->GetWorldTimerManager().SetTimer(LifeTimer, this, &UUTUMGHudWidget::NotifyHUDWidgetIsDone, Lifespan, false);
	}
}
