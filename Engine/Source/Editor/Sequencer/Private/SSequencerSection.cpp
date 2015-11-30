// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "Sequencer.h"
#include "SSequencerSection.h"
#include "IKeyArea.h"
#include "ISequencerSection.h"
#include "MovieSceneSection.h"
#include "MovieSceneShotSection.h"
#include "MovieSceneToolHelpers.h"
#include "CommonMovieSceneTools.h"
#include "SequencerHotspots.h"
#include "ScopedTransaction.h"

double SSequencerSection::SelectionThrobEndTime = 0;


void SSequencerSection::Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 InSectionIndex )
{
	SectionIndex = InSectionIndex;
	ParentSectionArea = SectionNode;
	SectionInterface = SectionNode->GetSections()[InSectionIndex];
	Layout = FKeyAreaLayout(*SectionNode, InSectionIndex);

	ChildSlot
	[
		SectionInterface->GenerateSectionWidget()
	];
}


FVector2D SSequencerSection::ComputeDesiredSize(float) const
{
	return FVector2D(100, Layout->GetTotalHeight());
}


FGeometry SSequencerSection::GetKeyAreaGeometry( const FKeyAreaLayoutElement& KeyArea, const FGeometry& SectionGeometry ) const
{
	// Compute the geometry for the key area
	return SectionGeometry.MakeChild( FVector2D( 0, KeyArea.GetOffset() ), FVector2D( SectionGeometry.Size.X, KeyArea.GetHeight(SectionGeometry) ) );
}


FSequencerSelectedKey SSequencerSection::GetKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry ) const
{
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();

	// Search every key area until we find the one under the mouse
	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();

		// Compute the current key area geometry
		FGeometry KeyAreaGeometryPadded = GetKeyAreaGeometry( Element, AllottedGeometry );

		// Is the key area under the mouse
		if( KeyAreaGeometryPadded.IsUnderLocation( MousePosition ) )
		{
			FGeometry KeyAreaGeometry = GetKeyAreaGeometry( Element, SectionGeometry );
			FVector2D LocalSpaceMousePosition = KeyAreaGeometry.AbsoluteToLocal( MousePosition );

			FTimeToPixel TimeToPixelConverter = Section.IsInfinite()
				? FTimeToPixel( ParentGeometry, GetSequencer().GetViewRange())
				: FTimeToPixel( KeyAreaGeometry, TRange<float>( Section.GetStartTime(), Section.GetEndTime() ) );

			// Check each key until we find one under the mouse (if any)
			TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();
			for( int32 KeyIndex = 0; KeyIndex < KeyHandles.Num(); ++KeyIndex )
			{
				FKeyHandle KeyHandle = KeyHandles[KeyIndex];
				float KeyPosition = TimeToPixelConverter.TimeToPixel( KeyArea->GetKeyTime(KeyHandle) );
				FGeometry KeyGeometry = KeyAreaGeometry.MakeChild( 
					FVector2D( KeyPosition - FMath::CeilToFloat(SequencerSectionConstants::KeySize.X/2.0f), ((KeyAreaGeometry.Size.Y*.5f)-(SequencerSectionConstants::KeySize.Y*.5f)) ),
					SequencerSectionConstants::KeySize
				);

				if( KeyGeometry.IsUnderLocation( MousePosition ) )
				{
					// The current key is under the mouse
					return FSequencerSelectedKey( Section, KeyArea, KeyHandle );
				}
			}

			// no key was selected in the current key area but the mouse is in the key area so it cannot possibly be in any other key area
			return FSequencerSelectedKey();
		}
	}

	// No key was selected in any key area
	return FSequencerSelectedKey();
}


FSequencerSelectedKey SSequencerSection::CreateKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry, FSequencerSelectedKey InPressedKey )
{
	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	// Search every key area until we find the one under the mouse
	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();

		// Compute the current key area geometry
		FGeometry KeyAreaGeometryPadded = GetKeyAreaGeometry( Element, AllottedGeometry );

		// Is the key area under the mouse
		if( KeyAreaGeometryPadded.IsUnderLocation( MousePosition ) )
		{
			FTimeToPixel TimeToPixelConverter = Section.IsInfinite() ? 			
				FTimeToPixel( ParentGeometry, GetSequencer().GetViewRange()) : 
				FTimeToPixel( SectionGeometry, TRange<float>( Section.GetStartTime(), Section.GetEndTime() ) );

			// If a key was pressed on, get the pressed on key's time to duplicate that key
			float KeyTime;

			if (InPressedKey.IsValid())
			{
				KeyTime = KeyArea->GetKeyTime(InPressedKey.KeyHandle.GetValue());
			}
			// Otherwise, use the time where the mouse is pressed
			else
			{
				FVector2D LocalSpaceMousePosition = SectionGeometry.AbsoluteToLocal( MousePosition );
				KeyTime = TimeToPixelConverter.PixelToTime(LocalSpaceMousePosition.X);
			}

			FScopedTransaction CreateKeyTransaction(NSLOCTEXT("Sequencer", "CreateKey_Transaction", "Create Key"));
			if (Section.TryModify())
			{
				// If the pressed key exists, offset the new key and look for it in the newly laid out key areas
				if (InPressedKey.IsValid())
				{
					// Offset by 1 pixel worth of time
					const float TimeFuzz = (GetSequencer().GetViewRange().GetUpperBoundValue() - GetSequencer().GetViewRange().GetLowerBoundValue()) / ParentGeometry.GetLocalSize().X;

					TArray<FKeyHandle> KeyHandles = KeyArea->AddKeyUnique(KeyTime+TimeFuzz, GetSequencer().GetKeyInterpolation(), KeyTime);

					Layout = FKeyAreaLayout(*ParentSectionArea, SectionIndex);

					// Look specifically for the key with the offset key time
					for (const FKeyAreaLayoutElement& NewElement : Layout->GetElements())
					{
						TSharedPtr<IKeyArea> NewKeyArea = NewElement.GetKeyArea();

						for (auto KeyHandle : KeyHandles)
						{
							for (auto UnsortedKeyHandle : NewKeyArea->GetUnsortedKeyHandles())
							{
								if (FMath::IsNearlyEqual(KeyTime+TimeFuzz, NewKeyArea->GetKeyTime(UnsortedKeyHandle), KINDA_SMALL_NUMBER))
								{
									return FSequencerSelectedKey(Section, NewKeyArea, UnsortedKeyHandle);
								}
							}
						}
					}
				}
				else
				{
					KeyArea->AddKeyUnique(KeyTime, GetSequencer().GetKeyInterpolation());
					Layout = FKeyAreaLayout(*ParentSectionArea, SectionIndex);
						
					return GetKeyUnderMouse(MousePosition, AllottedGeometry);
				}
			}
		}
	}

	return FSequencerSelectedKey();
}


void SSequencerSection::CheckForEdgeInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	if (!SectionInterface->SectionIsResizable())
	{
		return;
	}

	// Make areas to the left and right of the geometry.  We will use these areas to determine if someone dragged the left or right edge of a section
	FGeometry SectionRectLeft = SectionGeometry.MakeChild(
		FVector2D::ZeroVector,
		FVector2D( SectionInterface->GetSectionGripSize(), SectionGeometry.Size.Y )
	);

	FGeometry SectionRectRight = SectionGeometry.MakeChild(
		FVector2D( SectionGeometry.Size.X - SectionInterface->GetSectionGripSize(), 0 ), 
		SectionGeometry.Size 
	);

	if( SectionRectLeft.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
	{
		GetSequencer().GetEditTool().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Left, FSectionHandle(ParentSectionArea, SectionIndex))) );
	}
	else if( SectionRectRight.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
	{
		GetSequencer().GetEditTool().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Right, FSectionHandle(ParentSectionArea, SectionIndex))) );
	}
}


FSequencer& SSequencerSection::GetSequencer() const
{
	return ParentSectionArea->GetSequencer();
}


int32 SSequencerSection::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	LayerId += SectionObject.GetOverlapPriority();
	
	const bool bEnabled = bParentEnabled && SectionObject.IsActive();
	const bool bLocked = SectionObject.IsLocked();
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	int32 StartLayer = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );
	FSlateRect SectionClipRect = SectionGeometry.GetClippingRect().IntersectionWith( MyClippingRect );

	// Ask the interface to draw the section
	int32 PostSectionLayer = SectionInterface->OnPaintSection( SectionGeometry, SectionClipRect, OutDrawElements, LayerId, bEnabled );
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());

	DrawSectionHandles(AllottedGeometry, MyClippingRect, OutDrawElements, PostSectionLayer, DrawEffects, SelectionColor);
	DrawSelectionBorder(AllottedGeometry, MyClippingRect, OutDrawElements, PostSectionLayer, DrawEffects, SelectionColor);
	PaintKeys( SectionGeometry, MyClippingRect, OutDrawElements, PostSectionLayer, InWidgetStyle, bEnabled );

	if (bLocked)
	{
		static const FName SelectionBorder("Sequencer.Section.LockedBorder");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			PostSectionLayer+1,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush(SelectionBorder),
			MyClippingRect,
			DrawEffects,
			FLinearColor::Red
		);
	}

	// Section name with drop shadow
	FText SectionTitle = SectionInterface->GetSectionTitle();

	if (!SectionTitle.IsEmpty())
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			PostSectionLayer+1,
			SectionGeometry.ToOffsetPaintGeometry(FVector2D(6, 6)),
			SectionTitle,
			FEditorStyle::GetFontStyle("NormalFont"),
			MyClippingRect,
			DrawEffects,
			FLinearColor::Black
		);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			PostSectionLayer+2,
			SectionGeometry.ToOffsetPaintGeometry(FVector2D(5, 5)),
			SectionTitle,
			FEditorStyle::GetFontStyle("NormalFont"),
			MyClippingRect,
			DrawEffects,
			FLinearColor::White
		);
	}

	return LayerId;
}


void SSequencerSection::PaintKeys( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	static const FName BackgroundBrushName("Sequencer.SectionArea.Background");
	static const FName CircleKeyBrushName("Sequencer.KeyCircle");
	static const FName DiamondKeyBrushName("Sequencer.KeyDiamond");
	static const FName SquareKeyBrushName("Sequencer.KeySquare");
	static const FName TriangleKeyBrushName("Sequencer.KeyTriangle");

	static const FName SelectionColorName("SelectionColor");
	static const FName SelectionInactiveColorName("SelectionColorInactive");
	static const FName SelectionColorPressedName("SelectionColor_Pressed");

	static const float BrushBorderWidth = 2.0f;

	const FLinearColor PressedKeyColor = FEditorStyle::GetSlateColor(SelectionColorPressedName).GetColor( InWidgetStyle );
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor( InWidgetStyle );
	FLinearColor SelectedKeyColor = SelectionColor;
	FSequencer& Sequencer = ParentSectionArea->GetSequencer();
	TSharedPtr<ISequencerHotspot> Hotspot = Sequencer.GetEditTool().GetHotspot();

	// get hovered key
	FSequencerSelectedKey HoveredKey;

	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKey = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
	}

	auto& Selection = Sequencer.GetSelection();
	auto& SelectionPreview = Sequencer.GetSelectionPreview();

	if (Selection.GetActiveSelection() != FSequencerSelection::EActiveSelection::KeyAndSection)
	{
		// Selected key color is different when the active selection is not keys
		SelectedKeyColor = FEditorStyle::GetSlateColor(SelectionInactiveColorName).GetColor(InWidgetStyle)
			* FLinearColor(.25, .25, .25, 1);  // Make the color a little darker since it's not very visible next to white key frames.
	}

	const float ThrobScaleValue = GetSelectionThrobValue();

	// draw all keys in each key area
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	const FSlateBrush* BackgroundBrush = FEditorStyle::GetBrush(BackgroundBrushName);
	const FSlateBrush* CircleKeyBrush = FEditorStyle::GetBrush(CircleKeyBrushName);
	const FSlateBrush* DiamondKeyBrush = FEditorStyle::GetBrush(DiamondKeyBrushName);
	const FSlateBrush* SquareKeyBrush = FEditorStyle::GetBrush(SquareKeyBrushName);
	const FSlateBrush* TriangleKeyBrush = FEditorStyle::GetBrush(TriangleKeyBrushName);

	const ESlateDrawEffect::Type DrawEffects = bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	for (const FKeyAreaLayoutElement& LayoutElement : Layout->GetElements())
	{
		// get key handles
		TSharedPtr<IKeyArea> KeyArea = LayoutElement.GetKeyArea();


		FGeometry KeyAreaGeometry = GetKeyAreaGeometry(LayoutElement, AllottedGeometry);

		FTimeToPixel TimeToPixelConverter = SectionObject.IsInfinite()
			? FTimeToPixel(ParentGeometry, Sequencer.GetViewRange())
			: FTimeToPixel(KeyAreaGeometry, TRange<float>(SectionObject.GetStartTime(), SectionObject.GetEndTime()));

		// draw a box for the key area 
		if (SectionInterface->ShouldDrawKeyAreaBackground())
		{
			FSlateDrawElement::MakeBox( 
				OutDrawElements,
				LayerId + 1,
				KeyAreaGeometry.ToPaintGeometry(),
				BackgroundBrush,
				MyClippingRect,
				DrawEffects,
				KeyArea->GetColor()
			); 
		}

		TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();

		if (!KeyHandles.Num())
		{
			continue;
		}

		const int32 KeyLayer = LayerId + 1;

		for (const FKeyHandle& KeyHandle : KeyHandles)
		{
			float KeyTime = KeyArea->GetKeyTime(KeyHandle);

			// omit keys which would not be visible
			if( !SectionObject.IsTimeWithinSection(KeyTime))
			{
				continue;
			}

			// determine the key's brush & color
			const FSlateBrush* KeyBrush;
			FLinearColor KeyColor;
			
			switch (KeyArea->GetKeyInterpMode(KeyHandle))
			{
			case RCIM_Linear:
				KeyBrush = TriangleKeyBrush;
				KeyColor = FLinearColor(0.0f, 0.617f, 0.449f, 1.0f); // blueish green
				break;

			case RCIM_Constant:
				KeyBrush = SquareKeyBrush;
				KeyColor = FLinearColor(0.0f, 0.445f, 0.695f, 1.0f); // blue
				break;

			case RCIM_Cubic:
				KeyBrush = CircleKeyBrush;

				switch (KeyArea->GetKeyTangentMode(KeyHandle))
				{
				case RCTM_Auto:
					KeyColor = FLinearColor(0.972f, 0.2f, 0.2f, 1.0f); // vermillion
					break;

				case RCTM_Break:
					KeyColor = FLinearColor(0.336f, 0.703f, 0.5f, 0.91f); // sky blue
					break;

				case RCTM_User:
					KeyColor = FLinearColor(0.797f, 0.473f, 0.5f, 0.652f); // reddish purple
					break;

				default:
					KeyColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f); // light gray
				}
				break;

			default:
				KeyBrush = DiamondKeyBrush;
				KeyColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // white
				break;
			}

			// allow group & section overrides
			const FSlateBrush* OverrideBrush = nullptr;

			if (LayoutElement.GetType() == FKeyAreaLayoutElement::Group)
			{
				auto Group = StaticCastSharedPtr<FGroupedKeyArea>(KeyArea);
				OverrideBrush = Group->GetBrush(KeyHandle);
			}
			else
			{
				OverrideBrush = SectionInterface->GetKeyBrush(KeyHandle);
			}

			if (OverrideBrush != nullptr)
			{
				KeyBrush = OverrideBrush;
			}

			// determine draw colors based on hover, selection, etc.
			FLinearColor BorderColor;
			FLinearColor FillColor;

			FSequencerSelectedKey TestKey(SectionObject, KeyArea, KeyHandle);
			ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState(TestKey);
			bool bSelected = Selection.IsSelected(TestKey);

			if (SelectionPreviewState == ESelectionPreviewState::Selected)
			{
				FLinearColor PreviewSelectionColor = SelectionColor.LinearRGBToHSV();
				PreviewSelectionColor.R += 0.1f; // +10% hue
				PreviewSelectionColor.G = 0.6f; // 60% saturation
				BorderColor = PreviewSelectionColor.HSVToLinearRGB();
				FillColor = BorderColor;
			}
			else if (SelectionPreviewState == ESelectionPreviewState::NotSelected)
			{
				BorderColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
				FillColor = KeyColor;
			}
			else if (bSelected)
			{
				BorderColor = SelectedKeyColor;
				FillColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
			}
			else if (TestKey == HoveredKey)
			{
				BorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
				FillColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				BorderColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
				FillColor = KeyColor;
			}

			// allow group to tint the color
			if (LayoutElement.GetType() == FKeyAreaLayoutElement::Group)
			{
				auto Group = StaticCastSharedPtr<FGroupedKeyArea>(KeyArea);
				KeyColor *= Group->GetKeyTint(KeyHandle);
			}

			// draw border
			float KeyPosition = TimeToPixelConverter.TimeToPixel(KeyTime);

			static FVector2D ThrobAmount(12.f, 12.f);
			const FVector2D KeySize = bSelected ? SequencerSectionConstants::KeySize + ThrobAmount * ThrobScaleValue : SequencerSectionConstants::KeySize;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 1 : KeyLayer,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					FVector2D(
						KeyPosition - FMath::CeilToFloat(KeySize.X / 2.0f),
						((KeyAreaGeometry.Size.Y / 2.0f) - (KeySize.Y / 2.0f))
					),
					KeySize
				),
				KeyBrush,
				MyClippingRect,
				DrawEffects,
				BorderColor
			);

			// draw fill
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 2 : KeyLayer + 1,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					FVector2D(
						KeyPosition - FMath::CeilToFloat(KeySize.X / 2.0f - BrushBorderWidth),
						((KeyAreaGeometry.Size.Y / 2.0f) - (KeySize.Y / 2.0f - BrushBorderWidth))
					),
					KeySize - 2.0f * BrushBorderWidth
				),
				KeyBrush,
				MyClippingRect,
				DrawEffects,
				FillColor
			);
		}
	}
}


void SSequencerSection::DrawSectionHandles( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor ) const
{
	const FSlateBrush* LeftGripBrush = FEditorStyle::GetBrush(SectionInterface->GetSectionGripLeftBrushName());
	const FSlateBrush* RightGripBrush = FEditorStyle::GetBrush(SectionInterface->GetSectionGripRightBrushName());

	bool bLeftHandleActive = false;
	bool bRightHandleActive = false;

	// Get the hovered/selected state for the section handles from the hotspot
	TSharedPtr<ISequencerHotspot> Hotspot = GetSequencer().GetEditTool().GetHotspot();

	if (Hotspot.IsValid() && (
		Hotspot->GetType() == ESequencerHotspot::SectionResize_L ||
		Hotspot->GetType() == ESequencerHotspot::SectionResize_R))
	{
		FSectionResizeHotspot& ResizeHotspot = static_cast<FSectionResizeHotspot&>(*Hotspot.Get());
		if (ResizeHotspot.Section == FSectionHandle(ParentSectionArea, SectionIndex))
		{
			bLeftHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_L;
			bRightHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_R;
		}
	}

	// Left Grip
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		// Center the key along Y.  Ensure the middle of the key is at the actual key time
		AllottedGeometry.ToPaintGeometry(FVector2D(0.0f, 0.0f), FVector2D(SectionInterface->GetSectionGripSize(), AllottedGeometry.GetDrawSize().Y)),
		LeftGripBrush,
		MyClippingRect,
		DrawEffects,
		bLeftHandleActive ? SelectionColor : LeftGripBrush->GetTint(FWidgetStyle())
	);
	
	// Right Grip
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		// Center the key along Y.  Ensure the middle of the key is at the actual key time
		AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.Size.X-SectionInterface->GetSectionGripSize(), 0.0f), FVector2D(SectionInterface->GetSectionGripSize(), AllottedGeometry.GetDrawSize().Y)),
		RightGripBrush,
		MyClippingRect,
		DrawEffects,
		bRightHandleActive ? SelectionColor : RightGripBrush->GetTint(FWidgetStyle())
	);
}


void SSequencerSection::DrawSelectionBorder( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor ) const
{
	FSequencerSelection& Selection = ParentSectionArea->GetSequencer().GetSelection();
	FSequencerSelectionPreview& SelectionPreview = ParentSectionArea->GetSequencer().GetSelectionPreview();
	UMovieSceneSection* SectionObject = SectionInterface->GetSectionObject();
	ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState( SectionObject );

	if (SelectionPreviewState == ESelectionPreviewState::NotSelected)
	{
		// Explicitly not selected in the preview selection
		return;
	}
	
	if (SelectionPreviewState == ESelectionPreviewState::Undefined && !Selection.IsSelected(SectionObject))
	{
		// No preview selection for this section, and it's not selected
		return;
	}
	
	// Use a muted selection color for selection previews
	if( SelectionPreviewState == ESelectionPreviewState::Selected )
	{
		SelectionColor = SelectionColor.LinearRGBToHSV();
		SelectionColor.R += 0.1f; // +10% hue
		SelectionColor.G = 0.6f; // 60% saturation
		SelectionColor = SelectionColor.HSVToLinearRGB();
	}
	else if (Selection.GetActiveSelection() != FSequencerSelection::EActiveSelection::KeyAndSection)
	{
		// Use an inactive selection color for existing selections that are not active
		SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionInactiveColorName).GetColor(FWidgetStyle());
	}

	// draw selection box
	static const FName SelectionBorder("Sequencer.Section.SelectionBorder");

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(),
		FEditorStyle::GetBrush(SelectionBorder),
		MyClippingRect,
		DrawEffects,
		SelectionColor
	);
}

void SSequencerSection::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{	
	if( GetVisibility() == EVisibility::Visible )
	{
		Layout = FKeyAreaLayout(*ParentSectionArea, SectionIndex);
		FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

		SectionInterface->Tick(SectionGeometry, ParentGeometry, InCurrentTime, InDeltaTime);
	}
}

FReply SSequencerSection::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencer& Sequencer = GetSequencer();

	FSequencerSelectedKey HoveredKey;
	
	// The hovered key is defined from the sequencer hotspot
	TSharedPtr<ISequencerHotspot> Hotspot = GetSequencer().GetEditTool().GetHotspot();
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKey = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		// Generate a key and set it as the PressedKey
		FSequencerSelectedKey NewKey = CreateKeyUnderMouse(MouseEvent.GetScreenSpacePosition(), MyGeometry, HoveredKey);

		if (NewKey.IsValid())
		{
			HoveredKey = NewKey;

			Sequencer.GetSelection().EmptySelectedKeys();
			Sequencer.GetSelection().AddToSelection(NewKey);

			// Pass the event to the tool to copy the hovered key and move it
			auto& EditTool = GetSequencer().GetEditTool();
			EditTool.SetHotspot( MakeShareable( new FKeyHotspot(NewKey, ParentSectionArea) ) );

			// Return unhandled so that the EditTool can handle the mouse down based on the newly created keyframe and prepare to move it
			return FReply::Unhandled();
		}
	}

	return FReply::Unhandled();
}


FGeometry SSequencerSection::MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const
{
	const bool bSectionsAreConnected = InSectionInterface->AreSectionsConnected();
	const float SectionGripSize = !bSectionsAreConnected ? InSectionInterface->GetSectionGripSize() : 0.0f;

	return AllottedGeometry.MakeChild( FVector2D( SectionGripSize, 0 ), AllottedGeometry.GetDrawSize() - FVector2D( SectionGripSize*2, 0.0f ) );
}


FReply SSequencerSection::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		FReply Reply = SectionInterface->OnSectionDoubleClicked( MyGeometry, MouseEvent );

		if (Reply.IsEventHandled())
		{
			return Reply;
		}

		GetSequencer().ZoomToSelectedSections();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FReply SSequencerSection::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	auto& EditTool = GetSequencer().GetEditTool();

	// Checked for hovered key
	FSequencerSelectedKey KeyUnderMouse = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );
	if ( KeyUnderMouse.IsValid() )
	{
		EditTool.SetHotspot( MakeShareable( new FKeyHotspot(KeyUnderMouse, ParentSectionArea) ) );
	}
	else
	{
		EditTool.SetHotspot( MakeShareable( new FSectionHotspot(FSectionHandle(ParentSectionArea, SectionIndex))) );

		// Only check for edge interaction if not hovering over a key
		CheckForEdgeInteraction( MouseEvent, MyGeometry );
	}

	return FReply::Unhandled();
}


void SSequencerSection::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave( MouseEvent );
	GetSequencer().GetEditTool().SetHotspot(nullptr);
}

static float ThrobDurationSeconds = .5f;
void SSequencerSection::ThrobSelection(int32 ThrobCount)
{
	SelectionThrobEndTime = FPlatformTime::Seconds() + ThrobCount*ThrobDurationSeconds;
}

float EvaluateThrob(float Alpha)
{
	return .5f - FMath::Cos(FMath::Pow(Alpha, 0.5f) * 2 * PI) * .5f;
}

float SSequencerSection::GetSelectionThrobValue()
{
	double CurrentTime = FPlatformTime::Seconds();

	if (SelectionThrobEndTime > CurrentTime)
	{
		float Difference = SelectionThrobEndTime - CurrentTime;
		return EvaluateThrob(1.f - FMath::Fmod(Difference, ThrobDurationSeconds));
	}

	return 0.f;
}