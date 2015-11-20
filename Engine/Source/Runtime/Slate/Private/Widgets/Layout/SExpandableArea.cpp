// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SExpandableArea.h"


#define LOCTEXT_NAMESPACE "SExpandableArea"


void SExpandableArea::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	bAreaCollapsed = InArgs._InitiallyCollapsed;
	MaxHeight = InArgs._MaxHeight;
	OnAreaExpansionChanged = InArgs._OnAreaExpansionChanged;
	CollapsedImage = &InArgs._Style->CollapsedImage;
	ExpandedImage = &InArgs._Style->ExpandedImage;

	// If it should be initially visible, show it now
	RolloutCurve = FCurveSequence(0.0f, 0.1f, ECurveEaseFunction::QuadOut);

	if (!bAreaCollapsed)
	{
		RolloutCurve.JumpToEnd();
	}

	TSharedRef<SWidget> HeaderContent = InArgs._HeaderContent.Widget;
	if( HeaderContent == SNullWidget::NullWidget )
	{
		HeaderContent = 
			SNew(STextBlock)
			.Text(InArgs._AreaTitle)
			.Font(InArgs._AreaTitleFont)
			.ShadowOffset(FVector2D(1.0f, 1.0f));
	}

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( InArgs._BorderImage )
		.BorderBackgroundColor( InArgs._BorderBackgroundColor )
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SButton )
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(InArgs._HeaderPadding)
				.ForegroundColor(FSlateColor::UseForeground())
				.OnClicked( this, &SExpandableArea::OnHeaderClicked )
				[
					ConstructHeaderWidget( InArgs, HeaderContent )
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.Visibility(this, &SExpandableArea::OnGetContentVisibility)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(InArgs._Padding)
				.DesiredSizeScale(this, &SExpandableArea::GetSectionScale)
				[
					InArgs._BodyContent.Widget
				]
			]
		]
	];
}

void SExpandableArea::SetExpanded( bool bExpanded )
{
	// If setting to expanded and we are collapsed, change the state
	if ( bAreaCollapsed == bExpanded )
	{
		bAreaCollapsed = !bExpanded;

		if ( bExpanded )
		{
			RolloutCurve.JumpToEnd();
		}
		else
		{
			RolloutCurve.JumpToStart();
		}

		// Allow some section-specific code to be executed when the section becomes visible or collapsed
		OnAreaExpansionChanged.ExecuteIfBound( bExpanded );
	}
}

TSharedRef<SWidget> SExpandableArea::ConstructHeaderWidget( const FArguments& InArgs, TSharedRef<SWidget> HeaderContent )
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 3, 0)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(this, &SExpandableArea::OnGetCollapseImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			HeaderContent
		];
}

EVisibility SExpandableArea::OnGetContentVisibility() const
{
	float Scale = GetSectionScale().Y;
	// The section is visible if its scale in Y is greater than 0
	return Scale > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

/*
FReply SExpandableArea::OnMouseDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		//we need to capture the mouse for MouseUp events
		return FReply::Handled().CaptureMouse( HeaderBorder.ToSharedRef() ).SetUserFocus( AsShared(), EFocusCause::Mouse );
	}

	return FReply::Unhandled();
}*/


FReply SExpandableArea::OnHeaderClicked()
{
	// Only expand/collapse if the mouse was pressed down and up inside the area
	OnToggleContentVisibility();

	return FReply::Handled();
}

void SExpandableArea::OnToggleContentVisibility()
{
	bAreaCollapsed = !bAreaCollapsed;

	if( !bAreaCollapsed )
	{
		RolloutCurve = FCurveSequence( 0.0f, 0.1f, ECurveEaseFunction::CubicOut );
		RolloutCurve.Play( this->AsShared() );
	}
	else
	{
		RolloutCurve = FCurveSequence( 0.0f, 0.1f, ECurveEaseFunction::CubicIn );
		RolloutCurve.PlayReverse( this->AsShared() );
	}

	// Allow some section-specific code to be executed when the section becomes visible or collapsed
	OnAreaExpansionChanged.ExecuteIfBound( !bAreaCollapsed );
}

const FSlateBrush* SExpandableArea::OnGetCollapseImage() const
{
	return bAreaCollapsed ? CollapsedImage : ExpandedImage;
}

FVector2D SExpandableArea::GetSectionScale() const
{
	float Scale = RolloutCurve.GetLerp();
	return FVector2D( 1.0f, Scale );
}

FVector2D SExpandableArea::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ChildVisibility != EVisibility::Collapsed )
	{
		FVector2D SlotWidgetDesiredSize = ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.SlotPadding.Get().GetDesiredSize();
		
		// Only clamp if the user specified a max height
		if( MaxHeight > 0.0f )
		{
			SlotWidgetDesiredSize.Y = FMath::Min( MaxHeight, SlotWidgetDesiredSize.Y );
		}
		return SlotWidgetDesiredSize;
	}

	return FVector2D::ZeroVector;
}

#undef LOCTEXT_NAMESPACE