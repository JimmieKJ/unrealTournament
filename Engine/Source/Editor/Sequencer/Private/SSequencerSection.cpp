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


/** When 0, regeneration of dynamic key layouts is enabled, when non-zero, such behaviour is disabled */
FThreadSafeCounter LayoutRegenerationLock;


void SSequencerSection::DisableLayoutRegeneration()
{
	LayoutRegenerationLock.Increment();
}


void SSequencerSection::EnableLayoutRegeneration()
{
	LayoutRegenerationLock.Decrement();
}


void SSequencerSection::Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 InSectionIndex )
{
	SectionIndex = InSectionIndex;
	ParentSectionArea = SectionNode;
	SectionInterface = SectionNode->GetSections()[InSectionIndex];
	Layout = FKeyAreaLayout(*SectionNode, InSectionIndex);

	ResetState();

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
			Section.Modify();

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

	return FSequencerSelectedKey();
}


void SSequencerSection::CheckForEdgeInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	bLeftEdgeHovered = false;
	bRightEdgeHovered = false;
	bLeftEdgePressed = false;
	bRightEdgePressed = false;

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
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			bLeftEdgePressed = true;
		}
		else
		{
			bLeftEdgeHovered = true;
		}

		GetSequencer().GetEditTool().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Left, FSectionHandle(ParentSectionArea, SectionIndex))) );
	}
	else if( SectionRectRight.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			bRightEdgePressed = true;
		}
		else
		{
			bRightEdgeHovered = true;
		}

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
	bool bEnabled = bParentEnabled && SectionObject.IsActive();
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
	const ESlateDrawEffect::Type DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();
	FSequencer& Sequencer = ParentSectionArea->GetSequencer();

	static const FName BackgroundBrushName("Sequencer.SectionArea.Background");
	static const FName KeyBrushName("Sequencer.Key");

	const FSlateBrush* BackgroundBrush = FEditorStyle::GetBrush(BackgroundBrushName);
	const FSlateBrush* DefaultKeyBrush = FEditorStyle::GetBrush(KeyBrushName);

	static const FName SelectionColorName("SelectionColor");
	static const FName SelectionInactiveColorName("SelectionColorInactive");
	static const FName SelectionColorPressedName("SelectionColor_Pressed");

	const FLinearColor PressedKeyColor = FEditorStyle::GetSlateColor(SelectionColorPressedName).GetColor( InWidgetStyle );
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor( InWidgetStyle );
	FLinearColor SelectedKeyColor = SelectionColor;
	// @todo Sequencer temp color, make hovered brighter than selected.
	FLinearColor HoveredKeyColor = SelectedKeyColor * FLinearColor(1.5,1.5,1.5,1.0f);

	auto& Selection = Sequencer.GetSelection();
	auto& SelectionPreview = Sequencer.GetSelectionPreview();

	if (Selection.GetActiveSelection() != FSequencerSelection::EActiveSelection::KeyAndSection)
	{
		// Selected key color is different when the active selection is not keys
		SelectedKeyColor = FEditorStyle::GetSlateColor(SelectionInactiveColorName).GetColor( InWidgetStyle )
			* FLinearColor(.25, .25, .25, 1);  // Make the color a little darker since it's not very visible next to white keyframes.
	}

	// Draw all keys in each key area
	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		// Get the key area at the same index of the section.  Each section in this widget has the same layout and the same number of key areas
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
		TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();

		if (!KeyHandles.Num())
		{
			continue;
		}

		FGeometry KeyAreaGeometry = GetKeyAreaGeometry( Element, AllottedGeometry );

		FTimeToPixel TimeToPixelConverter = SectionObject.IsInfinite()
			? FTimeToPixel( ParentGeometry, GetSequencer().GetViewRange())
			: FTimeToPixel( KeyAreaGeometry, TRange<float>( SectionObject.GetStartTime(), SectionObject.GetEndTime() ) );

		// Draw a box for the key area 
		// @todo Sequencer - Allow the IKeyArea to do this
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId,
			KeyAreaGeometry.ToPaintGeometry(),
			BackgroundBrush,
			MyClippingRect,
			DrawEffects,
			FLinearColor( .1f, .1f, .1f, 0.7f )
		); 

		const int32 KeyLayer = LayerId + 1;

		for (const FKeyHandle& KeyHandle : KeyHandles)
		{
			float KeyTime = KeyArea->GetKeyTime(KeyHandle);

			// Omit keys which would not be visible
			if( !SectionObject.IsTimeWithinSection( KeyTime ) )
			{
				continue;
			}

			const FSlateBrush* BrushToUse = SectionInterface->GetKeyBrush(KeyHandle);
			if(BrushToUse == nullptr)
			{
				BrushToUse = DefaultKeyBrush;
			}
			FLinearColor KeyColor( 1.0f, 1.0f, 1.0f, 1.0f );
			FLinearColor KeyTint(1.f, 1.f, 1.f, 1.f);

			if (Element.GetType() == FKeyAreaLayoutElement::Group)
			{
				auto Group = StaticCastSharedPtr<FGroupedKeyArea>(KeyArea);
				KeyTint = Group->GetKeyTint(KeyHandle);
				BrushToUse = Group->GetBrush(KeyHandle);
			}

			// Where to start drawing the key (relative to the section)
			float KeyPosition =  TimeToPixelConverter.TimeToPixel( KeyTime );

			FSequencerSelectedKey TestKey( SectionObject, KeyArea, KeyHandle );

			bool bSelected = Selection.IsSelected( TestKey );
			ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState( TestKey );

			if( TestKey == PressedKey )
			{
				KeyColor = PressedKeyColor;
			}
			else if( TestKey == HoveredKey )
			{
				KeyColor = HoveredKeyColor;
			}
			else if( SelectionPreviewState == ESelectionPreviewState::Selected )
			{
				FLinearColor PreviewSelectionColor = SelectionColor.LinearRGBToHSV();
				PreviewSelectionColor.R += 0.1f; // +10% hue
				PreviewSelectionColor.G = 0.6f; // 60% saturation
				KeyColor = PreviewSelectionColor.HSVToLinearRGB();
			}
			else if( SelectionPreviewState == ESelectionPreviewState::NotSelected )
			{
				// Default white selection color
			}
			else if( bSelected )
			{
				KeyColor = SelectedKeyColor;
			}

			KeyColor *= KeyTint;

			// Draw the key
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer+1 : KeyLayer,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry( FVector2D( KeyPosition - FMath::CeilToFloat(SequencerSectionConstants::KeySize.X/2.0f), ((KeyAreaGeometry.Size.Y*.5f)-(SequencerSectionConstants::KeySize.Y*.5f)) ), SequencerSectionConstants::KeySize ),
				BrushToUse,
				MyClippingRect,
				DrawEffects,
				KeyColor
			);
		}
	}
}


void SSequencerSection::DrawSectionHandles( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor ) const
{
	const FSlateBrush* LeftGripBrush = FEditorStyle::GetBrush(SectionInterface->GetSectionGripLeftBrushName());
	const FSlateBrush* RightGripBrush = FEditorStyle::GetBrush(SectionInterface->GetSectionGripRightBrushName());

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
		(bLeftEdgePressed || bLeftEdgeHovered) ? SelectionColor : LeftGripBrush->GetTint(FWidgetStyle())
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
		(bRightEdgePressed || bRightEdgeHovered) ? SelectionColor : RightGripBrush->GetTint(FWidgetStyle())
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


TSharedPtr<SWidget> SSequencerSection::OnSummonContextMenu( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencerSelectedKey Key = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );
	FSequencer& Sequencer = GetSequencer();
	UMovieSceneSection* SceneSection = SectionInterface->GetSectionObject();

	// @todo sequencer replace with UI Commands instead of faking it
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	bool bHasMenuItems = false;
	if (Key.IsValid() || Sequencer.GetSelection().GetSelectedKeys().Num())
	{
		bHasMenuItems = true;
		MenuBuilder.BeginSection("SequencerInterpolation", NSLOCTEXT("Sequencer", "KeyInterpolationMenu", "Key Interpolation"));
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SetKeyInterpolationAuto", "Auto"),
				NSLOCTEXT("Sequencer", "SetKeyInterpolationAutoTooltip", "Set key interpolation to auto"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SetInterpTangentMode, RCIM_Cubic, RCTM_Auto),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(&Sequencer, &FSequencer::IsInterpTangentModeSelected, RCIM_Cubic, RCTM_Auto) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SetKeyInterpolationUser", "User"),
				NSLOCTEXT("Sequencer", "SetKeyInterpolationUserTooltip", "Set key interpolation to user"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SetInterpTangentMode, RCIM_Cubic, RCTM_User),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(&Sequencer, &FSequencer::IsInterpTangentModeSelected, RCIM_Cubic, RCTM_User) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SetKeyInterpolationBreak", "Break"),
				NSLOCTEXT("Sequencer", "SetKeyInterpolationBreakTooltip", "Set key interpolation to break"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SetInterpTangentMode, RCIM_Cubic, RCTM_Break),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(&Sequencer, &FSequencer::IsInterpTangentModeSelected, RCIM_Cubic, RCTM_Break) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SetKeyInterpolationLinear", "Linear"),
				NSLOCTEXT("Sequencer", "SetKeyInterpolationLinearTooltip", "Set key interpolation to linear"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SetInterpTangentMode, RCIM_Linear, RCTM_Auto),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(&Sequencer, &FSequencer::IsInterpTangentModeSelected, RCIM_Linear, RCTM_Auto) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SetKeyInterpolationConstant", "Constant"),
				NSLOCTEXT("Sequencer", "SetKeyInterpolationConstantTooltip", "Set key interpolation to constant"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SetInterpTangentMode, RCIM_Constant, RCTM_Auto),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(&Sequencer, &FSequencer::IsInterpTangentModeSelected, RCIM_Constant, RCTM_Auto) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
		MenuBuilder.EndSection(); // SequencerInterpolation

		MenuBuilder.BeginSection("SequencerKeys", NSLOCTEXT("Sequencer", "KeysMenu", "Keys"));
		{
			const bool bUseFrames = SequencerSnapValues::IsTimeSnapIntervalFrameRate(Sequencer.GetSettings()->GetTimeSnapInterval());

			MenuBuilder.AddMenuEntry(
				bUseFrames ? NSLOCTEXT("Sequencer", "SetKeyFrame", "Set Key Frame") : NSLOCTEXT("Sequencer", "SetKeyTime", "Set Key Time"),
				bUseFrames ? NSLOCTEXT("Sequencer", "SetKeyFrameTooltip", "Set key frame") : NSLOCTEXT("Sequencer", "SetKeyTimeTooltip", "Set key time"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SetKeyTime, bUseFrames),
					FCanExecuteAction::CreateSP(&Sequencer, &FSequencer::CanSetKeyTime))
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SnapToFrame", "Snap to Frame"),
				NSLOCTEXT("Sequencer", "SnapToFrameToolTip", "Snap selected keys to frame"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(&Sequencer, &FSequencer::SnapToFrame),
					FCanExecuteAction::CreateSP(&Sequencer, &FSequencer::CanSnapToFrame))
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "DeleteKey", "Delete"),
				NSLOCTEXT("Sequencer", "DeleteKeyToolTip", "Deletes the selected keys"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(&Sequencer, &FSequencer::DeleteSelectedKeys))
			);
		}
		MenuBuilder.EndSection(); // SequencerKeys
	}
	
	if (SceneSection != nullptr && Sequencer.GetSelection().IsSelected(SceneSection))
	{
		bHasMenuItems = true;
		SectionInterface->BuildSectionContextMenu(MenuBuilder);

		MenuBuilder.BeginSection("SequencerSections", NSLOCTEXT("Sequencer", "SectionsMenu", "Sections"));
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "SelectAllKeys", "Select All Keys"),
				NSLOCTEXT("Sequencer", "SelectAllKeysTooltip", "Select all keys in section"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerSection::SelectAllKeys),
					FCanExecuteAction::CreateSP(this, &SSequencerSection::CanSelectAllKeys))
			);

			MenuBuilder.AddSubMenu(
				NSLOCTEXT("Sequencer", "EditSection", "Edit"),
				NSLOCTEXT("Sequencer", "EditSectionTooltip", "Edit section"),
				FNewMenuDelegate::CreateRaw(this, &SSequencerSection::AddEditMenu));

			MenuBuilder.AddSubMenu(
				NSLOCTEXT("Sequencer", "SetPreInfinityExtrap", "Pre-Infinity"), 
				NSLOCTEXT("Sequencer", "SetPreInfinityExtrapTooltip", "Set pre-infinity extrapolation"),
				FNewMenuDelegate::CreateRaw(this, &SSequencerSection::AddExtrapolationMenu, true));

			MenuBuilder.AddSubMenu(
				NSLOCTEXT("Sequencer", "SetPostInfinityExtrap", "Post-Infinity"), 
				NSLOCTEXT("Sequencer", "SetPostInfinityExtrapTooltip", "Set post-infinity extrapolation"),
				FNewMenuDelegate::CreateRaw(this, &SSequencerSection::AddExtrapolationMenu, false));

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "ToggleSectionActive", "Active"),
				NSLOCTEXT("Sequencer", "ToggleSectionActiveTooltip", "Toggle section active/inactive"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerSection::ToggleSectionActive),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SSequencerSection::IsToggleSectionActive)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			// @todo Sequencer this should delete all selected sections
			// delete/selection needs to be rethought in general
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "DeleteSection", "Delete"),
				NSLOCTEXT("Sequencer", "DeleteSectionToolTip", "Deletes this section"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SSequencerSection::DeleteSection))
			);
		}
		MenuBuilder.EndSection(); // SequencerSections
	}
	
	ResetState();

	if (bHasMenuItems)
	{
		return MenuBuilder.MakeWidget();
	}

	return TSharedPtr<SWidget>();
}


void SSequencerSection::AddEditMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "TrimSectionLeft", "Trim Left"),
		NSLOCTEXT("Sequencer", "TrimSectionLeftTooltip", "Trim section at current time to the left"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::TrimSection, true),
			FCanExecuteAction::CreateSP(this, &SSequencerSection::IsTrimmable))
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "TrimSectionRight", "Trim Right"),
		NSLOCTEXT("Sequencer", "TrimSectionRightTooltip", "Trim section at current time to the right"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::TrimSection, false),
			FCanExecuteAction::CreateSP(this, &SSequencerSection::IsTrimmable))
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "SplitSection", "Split"),
		NSLOCTEXT("Sequencer", "SplitSectionTooltip", "Split section at current time"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::SplitSection),
			FCanExecuteAction::CreateSP(this, &SSequencerSection::IsTrimmable))
	);
}


void SSequencerSection::AddExtrapolationMenu(FMenuBuilder& MenuBuilder, bool bPreInfinity)
{
	FSequencer& Sequencer = GetSequencer();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "SetExtrapCycle", "Cycle"),
		NSLOCTEXT("Sequencer", "SetExtrapCycleTooltip", "Set extrapolation cycle"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::SetExtrapolationMode, RCCE_Cycle, bPreInfinity),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SSequencerSection::IsExtrapolationModeSelected, RCCE_Cycle, bPreInfinity) ),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "SetExtrapCycleWithOffset", "Cycle with Offset"),
		NSLOCTEXT("Sequencer", "SetExtrapCycleWithOffsetTooltip", "Set extrapolation cycle with offset"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::SetExtrapolationMode, RCCE_CycleWithOffset, bPreInfinity),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SSequencerSection::IsExtrapolationModeSelected, RCCE_CycleWithOffset, bPreInfinity) ),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "SetExtrapOscillate", "Oscillate"),
		NSLOCTEXT("Sequencer", "SetExtrapOscillateTooltip", "Set extrapolation oscillate"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::SetExtrapolationMode, RCCE_Oscillate, bPreInfinity),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SSequencerSection::IsExtrapolationModeSelected, RCCE_Oscillate, bPreInfinity) ),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "SetExtrapLinear", "Linear"),
		NSLOCTEXT("Sequencer", "SetExtrapLinearTooltip", "Set extrapolation linear"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::SetExtrapolationMode, RCCE_Linear, bPreInfinity),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SSequencerSection::IsExtrapolationModeSelected, RCCE_Linear, bPreInfinity) ),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "SetExtrapConstant", "Constant"),
		NSLOCTEXT("Sequencer", "SetExtrapConstantTooltip", "Set extrapolation constant"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerSection::SetExtrapolationMode, RCCE_Constant, bPreInfinity),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SSequencerSection::IsExtrapolationModeSelected, RCCE_Constant, bPreInfinity) ),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}

void SSequencerSection::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{	
	if( GetVisibility() == EVisibility::Visible )
	{
		if (LayoutRegenerationLock.GetValue() == 0)
		{
			Layout = FKeyAreaLayout(*ParentSectionArea, SectionIndex);
		}
		FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

		SectionInterface->Tick(SectionGeometry, ParentGeometry, InCurrentTime, InDeltaTime);
	}
}


FReply SSequencerSection::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencer& Sequencer = GetSequencer();

	// Check for clicking on a key and mark it as the pressed key for drag detection (if necessary) later
	PressedKey = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if( !PressedKey.IsValid() )
		{
			CheckForEdgeInteraction( MouseEvent, MyGeometry );
		}
	}
	
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		// Generate a key and set it as the PressedKey
		PressedKey = CreateKeyUnderMouse(MouseEvent.GetScreenSpacePosition(), MyGeometry, PressedKey);
		HoveredKey = PressedKey;

		Sequencer.GetSelection().EmptySelectedKeys();
		Sequencer.GetSelection().AddToSelection(PressedKey);

		// Pass the event to the tool to copy the hovered key and move it
		auto& EditTool = GetSequencer().GetEditTool();

		EditTool.SetHotspot( MakeShareable( new FSectionHotspot(FSectionHandle(ParentSectionArea, SectionIndex))) );

		if ( HoveredKey.IsValid() )
		{
			EditTool.SetHotspot( MakeShareable( new FKeyHotspot(HoveredKey, ParentSectionArea) ) );
			FReply Reply = EditTool.OnMouseButtonDown(*this, MyGeometry, MouseEvent);
			if (Reply.IsEventHandled())
			{
				return Reply;
			}
		}

		return FReply::Unhandled();
	}

	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) || (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton))
	{
		HandleSelection(MyGeometry, MouseEvent);
	}
	
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && (PressedKey.IsValid() || Sequencer.GetSelection().GetSelectedKeys().Num() || Sequencer.GetSelection().GetSelectedSections().Num()))
	{
		// Don't return handled if we didn't click on a key so that right-click panning gets a look in
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


void SSequencerSection::ResetState()
{
	ResetHoveredState();
	PressedKey = FSequencerSelectedKey();
}


void SSequencerSection::ResetHoveredState()
{
	bLeftEdgeHovered = false;
	bRightEdgeHovered = false;
	bLeftEdgePressed = false;
	bRightEdgePressed = false;
	HoveredKey = FSequencerSelectedKey();

	GetSequencer().GetEditTool().SetHotspot(nullptr);
}


FGeometry SSequencerSection::MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const
{
	const bool bSectionsAreConnected = InSectionInterface->AreSectionsConnected();
	const float SectionGripSize = !bSectionsAreConnected ? InSectionInterface->GetSectionGripSize() : 0.0f;

	return AllottedGeometry.MakeChild( FVector2D( SectionGripSize, 0 ), AllottedGeometry.GetDrawSize() - FVector2D( SectionGripSize*2, 0.0f ) );
}


FReply SSequencerSection::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton ) && MyGeometry.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
	{
		// Snap time to the key under the mouse if shift is down
		if (MouseEvent.IsShiftDown() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FSequencerSelectedKey Key = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );
			if (Key.IsValid())
			{
				float KeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());
				GetSequencer().SetGlobalTime(KeyTime);
			}
		}
	}

	if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		TSharedPtr<SWidget> MenuContent = OnSummonContextMenu( MyGeometry, MouseEvent );
		if (MenuContent.IsValid())
		{
			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

			FSlateApplication::Get().PushMenu(
				AsShared(),
				WidgetPath,
				MenuContent.ToSharedRef(),
				MouseEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
				);

			return FReply::Handled().SetUserFocus(MenuContent.ToSharedRef(), EFocusCause::SetDirectly);
		}
	}

	ResetState();

	return FReply::Handled();
}


FReply SSequencerSection::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	ResetState();

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
	ResetHoveredState();

	// Checked for hovered key
	// @todo Sequencer - Needs visual cue
	HoveredKey = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );

	auto& EditTool = GetSequencer().GetEditTool();

	EditTool.SetHotspot( MakeShareable( new FSectionHotspot(FSectionHandle(ParentSectionArea, SectionIndex))) );

	if ( HoveredKey.IsValid() )
	{
		EditTool.SetHotspot( MakeShareable( new FKeyHotspot(HoveredKey, ParentSectionArea) ) );
	}
	else
	{
		// Only check for edge interaction if not hovering over a key
		CheckForEdgeInteraction( MouseEvent, MyGeometry );
	}

	return FReply::Unhandled();
}


void SSequencerSection::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave( MouseEvent );
	ResetHoveredState();
}


FCursorReply SSequencerSection::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if( bLeftEdgeHovered || bRightEdgeHovered )
	{
		return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
	}

	return FCursorReply::Unhandled();
}


void SSequencerSection::HandleSelection( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencerSelectedKey Key = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );

	// If we have a currently pressed down and the key under the mouse is the pressed key, select that key now
	if( PressedKey.IsValid() && Key.IsValid() && PressedKey == Key )
	{
		bool bSelectDueToDrag = false;
		HandleKeySelection( Key, MouseEvent, bSelectDueToDrag );
	}
	else if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) || (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton))
	{
		bool bSelectDueToDrag = false;
		HandleSectionSelection( MouseEvent, bSelectDueToDrag );
	}
}


void SSequencerSection::HandleKeySelection( const FSequencerSelectedKey& Key, const FPointerEvent& MouseEvent, bool bSelectDueToDrag )
{
	if( Key.IsValid() )
	{
		bool bKeyIsSelected = GetSequencer().GetSelection().IsSelected( Key );

		// Clear previous key selection if:
		// we are selecting due to drag and the key being dragged is not selected
		bool bShouldClearSelectionDueToDrag =  bSelectDueToDrag ? !bKeyIsSelected : true;

		// Toggle selection if control is down and key is selected
		if (MouseEvent.IsControlDown() && bKeyIsSelected)
		{
			GetSequencer().GetSelection().RemoveFromSelection( Key );
		}
		else
		{
			if( (!MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown() && bShouldClearSelectionDueToDrag) && !bKeyIsSelected )
			{
				GetSequencer().GetSelection().EmptySelectedKeys();
			}

			GetSequencer().GetSelection().AddToSelection( Key );
		}
	}
}


void SSequencerSection::HandleSectionSelection( const FPointerEvent& MouseEvent, bool bSelectDueToDrag )
{
	// handle selecting sections 
	UMovieSceneSection* Section = SectionInterface->GetSectionObject();

	bool bSectionIsSelected = GetSequencer().GetSelection().IsSelected(Section);

	// Clear previous section selection if:
	// we are selecting due to drag and the section being dragged is not selected
	bool bShouldClearSelectionDueToDrag =  bSelectDueToDrag ? !bSectionIsSelected : true;

	// Keep key selection if right clicking to bring up a menu and the current key is selected
	bool bKeepSectionSelection = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bSectionIsSelected;
	
	// Don't clear selection edge is being dragged
	bool bEdgePressed = bLeftEdgePressed || bRightEdgePressed;

	if (MouseEvent.IsControlDown() && bSectionIsSelected && !bEdgePressed)
	{
		GetSequencer().GetSelection().RemoveFromSelection(Section);
	}
	else
	{
		if ( (!MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown() && bShouldClearSelectionDueToDrag) && !bKeepSectionSelection)
		{
			GetSequencer().GetSelection().EmptySelectedSections();
		}
	}
	
	if (!bSectionIsSelected)
	{
		GetSequencer().GetSelection().AddToSelection(Section);
	}
	else if (!bKeepSectionSelection)
	{
		GetSequencer().GetSelection().RemoveFromSelection(Section);
	}
}


void SSequencerSection::SelectAllKeys()
{
	// @todo Sequencer should operate on selected sections
	UMovieSceneSection* Section = SectionInterface->GetSectionObject();

	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();

		TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();
		for( int32 KeyIndex = 0; KeyIndex < KeyHandles.Num(); ++KeyIndex )
		{
			FKeyHandle KeyHandle = KeyHandles[KeyIndex];
			FSequencerSelectedKey SelectKey(*Section, KeyArea, KeyHandle);
			GetSequencer().GetSelection().AddToSelection(SelectKey);
		}
	}
}


bool SSequencerSection::CanSelectAllKeys() const
{
	// @todo Sequencer should operate on selected sections
	UMovieSceneSection* Section = SectionInterface->GetSectionObject();

	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();

		TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();
		if (KeyHandles.Num() > 0)
		{
			return true;
		}
	}

	return false;
}


void SSequencerSection::TrimSection(bool bTrimLeft)
{
	FScopedTransaction TrimSectionTransaction(NSLOCTEXT("Sequencer", "TrimSection_Transaction", "Trim Section"));

	MovieSceneToolHelpers::TrimSection(GetSequencer().GetSelection().GetSelectedSections(), GetSequencer().GetGlobalTime(), bTrimLeft);
	GetSequencer().NotifyMovieSceneDataChanged();
}


void SSequencerSection::SplitSection()
{
	FScopedTransaction SplitSectionTransaction(NSLOCTEXT("Sequencer", "SplitSection_Transaction", "Split Section"));

	MovieSceneToolHelpers::SplitSection(GetSequencer().GetSelection().GetSelectedSections(), GetSequencer().GetGlobalTime());
	GetSequencer().NotifyMovieSceneDataChanged();
}

bool SSequencerSection::IsTrimmable() const
{
	for (auto Section : GetSequencer().GetSelection().GetSelectedSections())
	{
		if (Section.IsValid() && Section->IsTimeWithinSection(GetSequencer().GetGlobalTime()))
		{
			return true;
		}
	}
	return false;
}


void SSequencerSection::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	// @todo Sequencer should operate on selected sections
	UMovieSceneSection* Section = SectionInterface->GetSectionObject();
	FScopedTransaction SetExtrapolationModeTransaction(NSLOCTEXT("Sequencer", "SetExtrapolationMode_Transaction", "Set Extrapolation Mode"));
	bool bAnythingChanged = false;

	Section->Modify();
	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
		bAnythingChanged = true;
		KeyArea->SetExtrapolationMode(ExtrapMode, bPreInfinity);
	}

	if (bAnythingChanged)
	{
		GetSequencer().UpdateRuntimeInstances();
	}
}


bool SSequencerSection::IsExtrapolationModeSelected(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) const
{
	// @todo Sequencer should operate on selected sections
	bool bAllSelected = false;

	for (const FKeyAreaLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();

		bAllSelected = true;
		if (KeyArea->GetExtrapolationMode(bPreInfinity) != ExtrapMode)
		{
			bAllSelected = false;
			break;
		}
	}

	return bAllSelected;
}


void SSequencerSection::ToggleSectionActive()
{
	FScopedTransaction ToggleSectionActiveTransaction( NSLOCTEXT("Sequencer", "ToggleSectionActive_Transaction", "Toggle Section Active") );

	bool bIsActive = !IsToggleSectionActive();
	bool bAnythingChanged = false;

	if (GetSequencer().GetSelection().GetSelectedSections().Num() != 0)
	{
		for (auto Section : GetSequencer().GetSelection().GetSelectedSections())
		{
			bAnythingChanged = true;
			Section->Modify();
			Section->SetIsActive(bIsActive);
		}
	}
	else
	{
		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Section)
		{
			bAnythingChanged = true;
			Section->Modify();
			Section->SetIsActive(bIsActive);
		}
	}

	if (bAnythingChanged)
	{
		GetSequencer().UpdateRuntimeInstances();
	}
}


bool SSequencerSection::IsToggleSectionActive() const
{
	// Active only if all are active
	if (GetSequencer().GetSelection().GetSelectedSections().Num() != 0)
	{
		for (auto Section : GetSequencer().GetSelection().GetSelectedSections())
		{
			if (!Section->IsActive())
			{
				return false;
			}
		}
	}
	else
	{
		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Section)
		{
			if (!Section->IsActive())
			{
				return false;
			}
		}
	}

	return true;
}


void SSequencerSection::DeleteSection()
{
	if (GetSequencer().GetSelection().GetSelectedSections().Num() != 0)
	{
		GetSequencer().DeleteSections(GetSequencer().GetSelection().GetSelectedSections());
	}
	else
	{
		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Section)
		{
			TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
			Sections.Add(Section);

			GetSequencer().DeleteSections(Sections);
		}
	}
}
