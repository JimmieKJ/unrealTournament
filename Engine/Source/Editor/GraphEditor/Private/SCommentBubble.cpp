// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SCommentBubble.h"
#include "SInlineEditableTextBlock.h"
#include "ScopedTransaction.h"

namespace SCommentBubbleDefs
{
	/** Bubble fade up/down delay */
	static const float FadeDelay = -3.5f;

	/** Bubble Toggle Icon Fade Speed */
	static const float FadeDownSpeed = 5.f;

	/** Height of the arrow connecting the bubble to the node */
	static const float BubbleArrowHeight = 8.f;

	/** Luminance CoEficients */
	static const FLinearColor LuminanceCoEff( 0.2126f, 0.7152f, 0.0722f, 0.f );

	/** Light foreground color */
	static const FLinearColor LightForegroundClr( 0.f, 0.f, 0.f, 0.65f );

	/** Dark foreground color */
	static const FLinearColor DarkForegroundClr( 1.f, 1.f, 1.f, 0.65f );

	/** Clear text box background color */
	static const FLinearColor TextClearBackground( 0.f, 0.f, 0.f, 0.f );

};


void SCommentBubble::Construct( const FArguments& InArgs )
{
	check( InArgs._Text.IsBound() );
	check( InArgs._GraphNode );

	GraphNode				= InArgs._GraphNode;
	CommentAttribute		= InArgs._Text;
	ColorAndOpacity			= InArgs._ColorAndOpacity;
	bAllowPinning			= InArgs._AllowPinning;
	bEnableTitleBarBubble	= InArgs._EnableTitleBarBubble;
	bEnableBubbleCtrls		= InArgs._EnableBubbleCtrls;
	GraphLOD				= InArgs._GraphLOD;
	IsGraphNodeHovered		= InArgs._IsGraphNodeHovered;
	HintText				= InArgs._HintText.IsSet() ? InArgs._HintText : NSLOCTEXT( "CommentBubble", "EditCommentHint", "Click to edit" );
	OpacityValue			= SCommentBubbleDefs::FadeDelay;

	// Create Widget
	UpdateBubble();
}

FCursorReply SCommentBubble::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	const FVector2D Size( GetDesiredSize().X, GetDesiredSize().Y - SCommentBubbleDefs::BubbleArrowHeight );
	const FSlateRect TestRect( MyGeometry.AbsolutePosition, MyGeometry.AbsolutePosition + Size );

	if( TestRect.ContainsPoint( CursorEvent.GetScreenSpacePosition() ))
	{
		return FCursorReply::Cursor( EMouseCursor::TextEditBeam );
	}
	return FCursorReply::Cursor( EMouseCursor::Default );
}

void SCommentBubble::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

	bool bIsHovered = IsHovered();

	const FLinearColor BubbleColor = ColorAndOpacity.Get().GetSpecifiedColor() * SCommentBubbleDefs::LuminanceCoEff;
	const float BubbleLuminance = BubbleColor.R + BubbleColor.G + BubbleColor.B;
	ForegroundColor = BubbleLuminance < 0.5f ? SCommentBubbleDefs::DarkForegroundClr : SCommentBubbleDefs::LightForegroundClr;

	if( bEnableTitleBarBubble && IsGraphNodeHovered.IsBound() )
	{
		bIsHovered |= IsGraphNodeHovered.Execute();

		if( !GraphNode->bCommentBubbleVisible )
		{
			if( bIsHovered )
			{
				if( OpacityValue < 1.f )
				{
					const float FadeUpAmt = InDeltaTime * SCommentBubbleDefs::FadeDownSpeed;
					OpacityValue = FMath::Min( OpacityValue + FadeUpAmt, 1.f );
				}
			}
			else
			{
				if( OpacityValue > SCommentBubbleDefs::FadeDelay )
				{
					const float FadeDownAmt = InDeltaTime * SCommentBubbleDefs::FadeDownSpeed;
					OpacityValue = FMath::Max( OpacityValue - FadeDownAmt, SCommentBubbleDefs::FadeDelay );
				}
			}
		}
	}
}

void SCommentBubble::UpdateBubble()
{
	if( GraphNode->bCommentBubbleVisible )
	{
		const FSlateBrush* CommentCalloutArrowBrush = FEditorStyle::GetBrush(TEXT("Graph.Node.CommentArrow"));
		const FMargin BubblePadding = FEditorStyle::GetMargin( TEXT("Graph.Node.Comment.BubblePadding"));
		const FMargin PinIconPadding = FEditorStyle::GetMargin( TEXT("Graph.Node.Comment.PinIconPadding"));
		const FMargin BubbleOffset = FEditorStyle::GetMargin( TEXT("Graph.Node.Comment.BubbleOffset"));
		// Conditionally create bubble controls
		TSharedPtr<SWidget> BubbleControls = SNullWidget::NullWidget;

		if( bEnableBubbleCtrls )
		{
			if( bAllowPinning )
			{
				SAssignNew( BubbleControls, SVerticalBox )
				+SVerticalBox::Slot()
				.Padding( 1.f )
				.AutoHeight()
				.VAlign( VAlign_Top )
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( PinIconPadding )
					[
						SNew( SCheckBox )
						.Style( &FEditorStyle::Get().GetWidgetStyle<FCheckBoxStyle>( "CommentBubblePin" ))
						.IsChecked( GraphNode->bCommentBubblePinned ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
						.OnCheckStateChanged( this, &SCommentBubble::OnPinStateToggle )
						.ToolTipText( this, &SCommentBubble::GetScaleButtonTooltip )
						.Cursor( EMouseCursor::Default )
						.ForegroundColor( this, &SCommentBubble::GetForegroundColor )
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 1.f )
				.VAlign( VAlign_Top )
				[
					SNew( SCheckBox )
					.Style( &FEditorStyle::Get().GetWidgetStyle< FCheckBoxStyle >( "CommentBubbleButton" ))
					.IsChecked( GraphNode->bCommentBubbleVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
					.OnCheckStateChanged( this, &SCommentBubble::OnCommentBubbleToggle )
					.ToolTipText( NSLOCTEXT( "CommentBubble", "ToggleCommentTooltip", "Toggle Comment Bubble" ))
					.Cursor( EMouseCursor::Default )
					.ForegroundColor( FLinearColor::White )
				];
			}
			else
			{
				SAssignNew( BubbleControls, SVerticalBox )
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 1.f )
				.VAlign( VAlign_Top )
				[
					SNew( SCheckBox )
					.Style( &FEditorStyle::Get().GetWidgetStyle< FCheckBoxStyle >( "CommentBubbleButton" ))
					.IsChecked( GraphNode->bCommentBubbleVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
					.OnCheckStateChanged( this, &SCommentBubble::OnCommentBubbleToggle )
					.ToolTipText( NSLOCTEXT( "CommentBubble", "ToggleCommentTooltip", "Toggle Comment Bubble" ))
					.Cursor( EMouseCursor::Default )
					.ForegroundColor( FLinearColor::White )
				];
			}
		}
		// Create the comment bubble widget
		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image( FEditorStyle::GetBrush( TEXT("Graph.Node.CommentBubble")) )
						.ColorAndOpacity( ColorAndOpacity )
					]
					+SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.Padding( BubblePadding )
						.AutoWidth()
						[
							SAssignNew(TextBlock, SMultiLineEditableTextBox)
							.Text( this, &SCommentBubble::GetCommentText )
							.HintText( NSLOCTEXT( "CommentBubble", "EditCommentHint", "Click to edit" ))
							.Font( FEditorStyle::GetFontStyle( TEXT("Graph.Node.CommentFont")))
							.SelectAllTextWhenFocused( true )
							.ForegroundColor( this, &SCommentBubble::GetTextForegroundColor )
							.BackgroundColor( this, &SCommentBubble::GetTextBackgroundColor )
							.OnTextCommitted( this, &SCommentBubble::OnCommentTextCommitted )
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign( HAlign_Right )
						.Padding( 0.f )
						[
							BubbleControls.ToSharedRef()
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding( BubbleOffset )
				.MaxWidth( CommentCalloutArrowBrush->ImageSize.X )
				[
					SNew(SImage)
					.Image( CommentCalloutArrowBrush )
					.ColorAndOpacity( ColorAndOpacity )
				]
			]
		];
	}
	else 
	{
		TSharedPtr<SWidget> TitleBarBubble = SNullWidget::NullWidget;

		if( bEnableTitleBarBubble )
		{
			const FMargin BubbleOffset = FEditorStyle::GetMargin( TEXT("Graph.Node.Comment.BubbleOffset"));
			// Create Title bar bubble toggle widget
			SAssignNew( TitleBarBubble, SHorizontalBox )
			.Visibility( this, &SCommentBubble::GetToggleButtonVisibility )
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Top )
			.Padding( BubbleOffset )
			[
				SNew( SCheckBox )
				.Style( &FEditorStyle::Get().GetWidgetStyle< FCheckBoxStyle >( "CommentTitleButton" ))
				.IsChecked( ECheckBoxState::Unchecked )
				.OnCheckStateChanged( this, &SCommentBubble::OnCommentBubbleToggle )
				.ToolTipText( NSLOCTEXT( "CommentBubble", "ToggleCommentTooltip", "Toggle Comment Bubble" ))
				.Cursor( EMouseCursor::Default )
				.ForegroundColor( this, &SCommentBubble::GetToggleButtonColor )
			];
		}
		ChildSlot
		[
			TitleBarBubble.ToSharedRef()
		];
	}
}


FVector2D SCommentBubble::GetOffset() const
{
	return FVector2D( 0.f, -GetDesiredSize().Y );
}

FVector2D SCommentBubble::GetSize() const
{
	return GetDesiredSize();
}

bool SCommentBubble::IsBubbleVisible() const
{
	EGraphRenderingLOD::Type CurrLOD = GraphLOD.Get();
	const bool bShowScaled = CurrLOD > EGraphRenderingLOD::LowestDetail;
	const bool bShowPinned = CurrLOD <= EGraphRenderingLOD::MediumDetail;

	if( bAllowPinning )
	{
		return GraphNode->bCommentBubblePinned ? true : bShowScaled;
	}
	return bShowPinned;
}

bool SCommentBubble::IsScalingAllowed() const
{
	return !GraphNode->bCommentBubblePinned || !GraphNode->bCommentBubbleVisible;
}

FText SCommentBubble::GetCommentText() const
{
	if( CommentAttribute.Get().IsEmpty() )
	{
		return HintText.Get();
	}
	else if( CachedComment != CommentAttribute.Get() )
	{
		CachedComment = CommentAttribute.Get();
		CachedCommentText = FText::FromString( CachedComment );
	}
	return CachedCommentText;
}

FText SCommentBubble::GetScaleButtonTooltip() const
{
	if( GraphNode->bCommentBubblePinned )
	{
		return NSLOCTEXT( "CommentBubble", "AllowScaleButtonTooltip", "Allow this bubble to scale with zoom" );
	}
	else
	{
		return NSLOCTEXT( "CommentBubble", "PreventScaleButtonTooltip", "Prevent this bubble scaling with zoom" );
	}
}

FSlateColor SCommentBubble::GetToggleButtonColor() const
{
	const FLinearColor BubbleColor = ColorAndOpacity.Get().GetSpecifiedColor();
	return FLinearColor( 1.f, 1.f, 1.f, OpacityValue * OpacityValue );
}

FSlateColor SCommentBubble::GetTextBackgroundColor() const
{
	return TextBlock->HasKeyboardFocus() ? FLinearColor::White : SCommentBubbleDefs::TextClearBackground;
}

FSlateColor SCommentBubble::GetTextForegroundColor() const
{
	return TextBlock->HasKeyboardFocus() ? FLinearColor::Black : ForegroundColor;
}

void SCommentBubble::OnCommentTextCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	const bool bValidText = !NewText.EqualTo( HintText.Get() );
	if( GraphNode && bValidText )
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "CommentBubble", "CommentCommitted", "Comment Changed" ) );
		GraphNode->Modify();
		GraphNode->NodeComment = NewText.ToString();
	}
}

EVisibility SCommentBubble::GetToggleButtonVisibility() const
{
	EVisibility Visibility = EVisibility::Hidden;

	if( OpacityValue > 0.f && !GraphNode->bCommentBubbleVisible )
	{
		Visibility = EVisibility::Visible;
	}
	return Visibility;
}

EVisibility SCommentBubble::GetBubbleVisibility() const
{
	const bool bIsVisible = !CachedCommentText.IsEmpty() && IsBubbleVisible();
	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}

void SCommentBubble::OnCommentBubbleToggle( ECheckBoxState State )
{
	if( GraphNode )
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "CommentBubble", "BubbleVisibility", "Comment Bubble Visibilty" ) );
		GraphNode->Modify();
		GraphNode->bCommentBubbleVisible = !GraphNode->bCommentBubbleVisible;
		OpacityValue = 0.f;
		UpdateBubble();
	}
}

void SCommentBubble::OnPinStateToggle( ECheckBoxState State ) const
{
	if( GraphNode )
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "CommentBubble", "BubblePinned", "Comment Bubble Pin" ) );
		GraphNode->Modify();
		GraphNode->bCommentBubblePinned = State == ECheckBoxState::Checked;
	}
}