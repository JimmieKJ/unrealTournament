// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

/////////////////////////////////////////////////////
// UPanelSlot

UPanelSlot::UPanelSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPanelSlot::SetDesiredPosition(FVector2D InPosition)
{

}

void UPanelSlot::SetDesiredSize(FVector2D InSize)
{

}

void UPanelSlot::Resize(const FVector2D& Direction, const FVector2D& Amount)
{

}

bool UPanelSlot::CanResize(const FVector2D& Direction) const
{
	return false;
}

void UPanelSlot::MoveTo(const FVector2D& Location)
{

}

bool UPanelSlot::CanMove() const
{
	return false;
}

bool UPanelSlot::IsDesignTime() const
{
	return Parent->IsDesignTime();
}

void UPanelSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	if ( Content )
	{
		Content->ReleaseSlateResources(bReleaseChildren);
	}
}