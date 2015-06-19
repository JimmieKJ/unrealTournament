// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "HittestGrid.h"

FPaintArgs::FPaintArgs( const TSharedRef<SWidget>& Parent, FHittestGrid& InHittestGrid, FVector2D InWindowOffset, double InCurrentTime, float InDeltaTime )
: ParentPtr(Parent)
, Grid(InHittestGrid)
, LastHittestIndex(INDEX_NONE)
, LastRecordedVisibility( EVisibility::Visible )
, WindowOffset(InWindowOffset)
, CurrentTime(InCurrentTime)
, DeltaTime(InDeltaTime)
{
}


FPaintArgs FPaintArgs::WithNewParent( const SWidget* Parent ) const
{
	FPaintArgs Args = FPaintArgs( const_cast<SWidget*>(Parent)->AsShared(), this->Grid, this->WindowOffset, this->CurrentTime, this->DeltaTime );
	Args.LastHittestIndex = this->LastHittestIndex;
	Args.LastRecordedVisibility = this->LastRecordedVisibility;
	return Args;
}


FPaintArgs FPaintArgs::RecordHittestGeometry(const SWidget* Widget, const FGeometry& WidgetGeometry, const FSlateRect& InClippingRect) const
{
	const EVisibility RecordedVisibility = (LastRecordedVisibility.AreChildrenHitTestVisible())
		? Widget->GetVisibility()
		: LastRecordedVisibility;

	const int32 RecordedHittestIndex = Grid.InsertWidget( LastHittestIndex, RecordedVisibility, FArrangedWidget(const_cast<SWidget*>(Widget)->AsShared(), WidgetGeometry), WindowOffset, InClippingRect );

	FPaintArgs UpdatedArgs(*this);
	UpdatedArgs.LastHittestIndex = RecordedHittestIndex;
	UpdatedArgs.LastRecordedVisibility = RecordedVisibility;
	return UpdatedArgs;
}

FPaintArgs FPaintArgs::InsertCustomHitTestPath( TSharedRef<ICustomHitTestPath> CustomHitTestPath, int32 InLastHittestIndex ) const
{
	const_cast<FHittestGrid&>(Grid).InsertCustomHitTestPath( CustomHitTestPath, InLastHittestIndex );
	return *this;
}