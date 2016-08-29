// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "Components/SafeZone.h"
#include "Components/SafeZoneSlot.h"
#include "Layout/SSafeZone.h"

#define LOCTEXT_NAMESPACE "UMG"

USafeZone::USafeZone()
{
	bCanHaveMultipleChildren = false;
	Visibility = ESlateVisibility::SelfHitTestInvisible;
}

#if WITH_EDITOR

const FText USafeZone::GetPaletteCategory()
{
	return LOCTEXT( "Panel", "Panel" );
}
#endif

void USafeZone::OnSlotAdded( UPanelSlot* InSlot )
{
	Super::OnSlotAdded( InSlot );

	UpdateWidgetProperties();
}

void USafeZone::OnSlotRemoved( UPanelSlot* InSlot )
{
	Super::OnSlotRemoved( InSlot );

	if ( MySafeZone.IsValid() )
	{
		MySafeZone->SetContent( SNullWidget::NullWidget );
	}
}

UClass* USafeZone::GetSlotClass() const
{
	return USafeZoneSlot::StaticClass();
}

void USafeZone::UpdateWidgetProperties()
{
	if ( MySafeZone.IsValid() && GetChildrenCount() > 0 )
	{
		USafeZoneSlot* SafeSlot = CastChecked< USafeZoneSlot >( Slots[ 0 ] );

		MySafeZone->SetSafeAreaScale( SafeSlot->SafeAreaScale );
		MySafeZone->SetTitleSafe( SafeSlot->bIsTitleSafe );
		MySafeZone->SetHAlign( SafeSlot->HAlign.GetValue() );
		MySafeZone->SetVAlign( SafeSlot->VAlign.GetValue() );
		MySafeZone->SetPadding( SafeSlot->Padding );
	}
}

TSharedRef<SWidget> USafeZone::RebuildWidget()
{
	USafeZoneSlot* SafeSlot = Slots.Num() > 0 ? Cast< USafeZoneSlot >( Slots[ 0 ] ) : nullptr;

	MySafeZone = SNew( SSafeZone )
		.IsTitleSafe( SafeSlot ? SafeSlot->bIsTitleSafe : false )
		.SafeAreaScale( SafeSlot ? SafeSlot->SafeAreaScale : FMargin(1,1,1,1))
		.HAlign( SafeSlot ? SafeSlot->HAlign.GetValue() : HAlign_Fill )
		.VAlign( SafeSlot ? SafeSlot->VAlign.GetValue() : VAlign_Fill )
		.Padding( SafeSlot ? SafeSlot->Padding : FMargin() )
		[
			GetChildAt( 0 ) ? GetChildAt( 0 )->TakeWidget() : SNullWidget::NullWidget
		];

	return BuildDesignTimeWidget( MySafeZone.ToSharedRef() );
}

void USafeZone::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySafeZone.Reset();
}

#undef LOCTEXT_NAMESPACE