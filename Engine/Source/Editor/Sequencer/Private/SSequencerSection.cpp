// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SSequencerSection.h"
#include "Rendering/DrawElements.h"
#include "EditorStyleSet.h"
#include "SequencerSelectionPreview.h"
#include "GroupedKeyArea.h"
#include "SequencerSettings.h"
#include "Editor.h"
#include "Sequencer.h"
#include "SequencerSectionPainter.h"
#include "CommonMovieSceneTools.h"
#include "ISequencerEditTool.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"

double SSequencerSection::SelectionThrobEndTime = 0;

struct FSequencerSectionPainterImpl : FSequencerSectionPainter
{
	FSequencerSectionPainterImpl(FSequencer& InSequencer, UMovieSceneSection& InSection, FSlateWindowElementList& _OutDrawElements, const FGeometry& InSectionGeometry)
		: FSequencerSectionPainter(_OutDrawElements, InSectionGeometry, InSection)
		, Sequencer(InSequencer)
		, TimeToPixelConverter(InSection.IsInfinite() ?
			FTimeToPixel(SectionGeometry, Sequencer.GetViewRange()) :
			FTimeToPixel(SectionGeometry, TRange<float>(InSection.GetStartTime(), InSection.GetEndTime()))
			)
	{
		CalculateSelectionColor();
	}

	virtual int32 PaintSectionBackground(const FLinearColor& Tint) override
	{
		const ESlateDrawEffect::Type DrawEffects = bParentEnabled
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;

		static const FSlateBrush* SectionBackgroundBrush = FEditorStyle::GetBrush("Sequencer.Section.Background");
		static const FSlateBrush* SectionBackgroundTintBrush = FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint");
		static const FSlateBrush* SelectedSectionOverlay = FEditorStyle::GetBrush("Sequencer.Section.SelectedSectionOverlay");

		FGeometry InfiniteGeometry = SectionGeometry.MakeChild(FVector2D(-100.f, 0.f), FVector2D(SectionGeometry.GetLocalSize().X + 200.f, SectionGeometry.GetLocalSize().Y));

		const FLinearColor FinalTint = FSequencerSectionPainter::BlendColor(Tint);

		FPaintGeometry PaintGeometry = Section.IsInfinite() ?
				InfiniteGeometry.ToPaintGeometry() :
				SectionGeometry.ToPaintGeometry();
				
		FSlateDrawElement::MakeBox(
			DrawElements,
			LayerId++,
			PaintGeometry,
			SectionBackgroundBrush,
			SectionClippingRect,
			DrawEffects
		);

		FMargin Inset = Section.IsInfinite() ? FMargin(0.f, 1.f) : FMargin(1.f);
		FSlateDrawElement::MakeBox(
			DrawElements,
			LayerId++,
			PaintGeometry,
			SectionBackgroundTintBrush,
			SectionClippingRect.InsetBy(Inset),
			DrawEffects,
			FinalTint
		);

		// Draw the selection hash
		if (SelectionColor.IsSet())
		{
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				SectionGeometry.ToPaintGeometry(FVector2D(1, 1), SectionGeometry.Size - FVector2D(2,2)),
				SelectedSectionOverlay,
				SectionClippingRect,
				DrawEffects,
				SelectionColor.GetValue().CopyWithNewOpacity(0.8f)
			);
		}

		return LayerId;
	}

	virtual const FTimeToPixel& GetTimeConverter() const
	{
		return TimeToPixelConverter;
	}

	void CalculateSelectionColor()
	{
		// Don't draw selected if infinite
		if (Section.IsInfinite())
		{
			return;
		}

		FSequencerSelection& Selection = Sequencer.GetSelection();
		FSequencerSelectionPreview& SelectionPreview = Sequencer.GetSelectionPreview();

		ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState(&Section);

		if (SelectionPreviewState == ESelectionPreviewState::NotSelected)
		{
			// Explicitly not selected in the preview selection
			return;
		}
		
		if (SelectionPreviewState == ESelectionPreviewState::Undefined && !Selection.IsSelected(&Section))
		{
			// No preview selection for this section, and it's not selected
			return;
		}

		SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());

		// Use a muted selection color for selection previews
		if (SelectionPreviewState == ESelectionPreviewState::Selected)
		{
			SelectionColor.GetValue() = SelectionColor.GetValue().LinearRGBToHSV();
			SelectionColor.GetValue().R += 0.1f; // +10% hue
			SelectionColor.GetValue().G = 0.6f; // 60% saturation

			SelectionColor = SelectionColor.GetValue().HSVToLinearRGB();
		}
	}

	TOptional<FLinearColor> SelectionColor;
	FSequencer& Sequencer;
	FTimeToPixel TimeToPixelConverter;
};

void SSequencerSection::Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 InSectionIndex )
{
	SectionIndex = InSectionIndex;
	ParentSectionArea = SectionNode;
	SectionInterface = SectionNode->GetSections()[InSectionIndex];
	Layout = FSectionLayout(*SectionNode, InSectionIndex);
	HandleOffsetPx = 0.f;

	ChildSlot
	[
		SectionInterface->GenerateSectionWidget()
	];
}


FVector2D SSequencerSection::ComputeDesiredSize(float) const
{
	return FVector2D(100, Layout->GetTotalHeight());
}


FGeometry SSequencerSection::GetKeyAreaGeometry( const FSectionLayoutElement& KeyArea, const FGeometry& SectionGeometry ) const
{
	// Compute the geometry for the key area
	return SectionGeometry.MakeChild( FVector2D( 0, KeyArea.GetOffset() ), FVector2D( SectionGeometry.Size.X, KeyArea.GetHeight()  ) );
}


FSequencerSelectedKey SSequencerSection::GetKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry ) const
{
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();

	// Search every key area until we find the one under the mouse
	for (const FSectionLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
		if (!KeyArea.IsValid())
		{
			continue;
		}

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
	for (const FSectionLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
		if (!KeyArea.IsValid())
		{
			continue;
		}

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

			if (Section.TryModify())
			{
				// If the pressed key exists, offset the new key and look for it in the newly laid out key areas
				if (InPressedKey.IsValid())
				{
					// Offset by 1 pixel worth of time
					const float TimeFuzz = (GetSequencer().GetViewRange().GetUpperBoundValue() - GetSequencer().GetViewRange().GetLowerBoundValue()) / ParentGeometry.GetLocalSize().X;

					TArray<FKeyHandle> KeyHandles = KeyArea->AddKeyUnique(KeyTime+TimeFuzz, GetSequencer().GetKeyInterpolation(), KeyTime);

					Layout = FSectionLayout(*ParentSectionArea, SectionIndex);

					// Look specifically for the key with the offset key time
					for (const FSectionLayoutElement& NewElement : Layout->GetElements())
					{
						TSharedPtr<IKeyArea> NewKeyArea = NewElement.GetKeyArea();
						if (!NewKeyArea.IsValid())
						{
							continue;
						}

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
					KeyArea->AddKeyUnique(KeyTime, GetSequencer().GetKeyInterpolation(), KeyTime);
					Layout = FSectionLayout(*ParentSectionArea, SectionIndex);
						
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
		GetSequencer().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Left, FSectionHandle(ParentSectionArea, SectionIndex))) );
	}
	else if( SectionRectRight.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
	{
		GetSequencer().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Right, FSectionHandle(ParentSectionArea, SectionIndex))) );
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

	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	FSequencerSectionPainterImpl Painter(ParentSectionArea->GetSequencer(), SectionObject, OutDrawElements, SectionGeometry);

	FGeometry PaintSpaceParentGeometry = ParentGeometry;
	PaintSpaceParentGeometry.AppendTransform(FSlateLayoutTransform(Inverse(Args.GetWindowToDesktopTransform())));

	FSlateRect ParentClippingRect = PaintSpaceParentGeometry.GetClippingRect();

	// Clip vertically
	ParentClippingRect.Top = FMath::Max(ParentClippingRect.Top, MyClippingRect.Top);
	ParentClippingRect.Bottom = FMath::Min(ParentClippingRect.Bottom, MyClippingRect.Bottom);

	Painter.SectionClippingRect = Painter.SectionGeometry.GetClippingRect().IntersectionWith(ParentClippingRect);

	Painter.LayerId = LayerId;
	Painter.bParentEnabled = bEnabled;

	// Ask the interface to draw the section
	LayerId = SectionInterface->OnPaintSection(Painter);
	
	LayerId = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );

	const ISequencerEditTool* EditTool = GetSequencer().GetEditTool();
	const ISequencerHotspot* Hotspot = EditTool ? EditTool->GetDragHotspot() : nullptr;
	if (!Hotspot)
	{
		Hotspot = GetSequencer().GetHotspot().Get();
	}

	const bool bShowHandles =
		IsHovered() ||			// If it's hovered
		HandleOffsetPx ||		// Or the handles are being drawn outside
		(						// Or we're dragging one of the handles
			Hotspot &&
			(Hotspot->GetType() == ESequencerHotspot::SectionResize_L || Hotspot->GetType() == ESequencerHotspot::SectionResize_R) &&
			static_cast<const FSectionResizeHotspot*>(Hotspot)->Section == FSectionHandle(ParentSectionArea, SectionIndex)
		);

	if (bShowHandles)
	{
		FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());
		DrawSectionHandles(AllottedGeometry, MyClippingRect, OutDrawElements, ++LayerId, DrawEffects, SelectionColor, Hotspot);
	}

	FSlateRect KeyClippingRect = Painter.SectionGeometry.GetClippingRect().ExtendBy(FMargin(10.f, 0.f)).IntersectionWith(ParentClippingRect);
	
	Painter.LayerId = ++LayerId;
	PaintKeys( Painter, InWidgetStyle, KeyClippingRect );

	if (bLocked)
	{
		static const FName SelectionBorder("Sequencer.Section.LockedBorder");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush(SelectionBorder),
			MyClippingRect,
			DrawEffects,
			FLinearColor::Red
		);
	}

	// Section name with drop shadow
	FText SectionTitle = SectionInterface->GetSectionTitle();
	FMargin ContentPadding = SectionInterface->GetContentPadding();

	if (!SectionTitle.IsEmpty())
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			Painter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(ContentPadding.Left + 1, ContentPadding.Top + 1)),
			SectionTitle,
			FEditorStyle::GetFontStyle("NormalFont"),
			MyClippingRect,
			DrawEffects,
			FLinearColor(0,0,0,.5f)
		);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			Painter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(ContentPadding.Left, ContentPadding.Top)),
			SectionTitle,
			FEditorStyle::GetFontStyle("NormalFont"),
			MyClippingRect,
			DrawEffects,
			FColor(200, 200, 200)
		);
	}

	return LayerId;
}


void SSequencerSection::PaintKeys( FSequencerSectionPainter& InPainter, const FWidgetStyle& InWidgetStyle, const FSlateRect& KeyClippingRect ) const
{
	static const FName HighlightBrushName("Sequencer.AnimationOutliner.DefaultBorder");

	static const FName BackgroundBrushName("Sequencer.Section.BackgroundTint");
	static const FName CircleKeyBrushName("Sequencer.KeyCircle");
	static const FName DiamondKeyBrushName("Sequencer.KeyDiamond");
	static const FName SquareKeyBrushName("Sequencer.KeySquare");
	static const FName TriangleKeyBrushName("Sequencer.KeyTriangle");
	static const FName StripeOverlayBrushName("Sequencer.Section.StripeOverlay");

	static const FName SelectionColorName("SelectionColor");
	static const FName SelectionInactiveColorName("SelectionColorInactive");
	static const FName SelectionColorPressedName("SelectionColor_Pressed");

	static const float BrushBorderWidth = 2.0f;

	const FLinearColor PressedKeyColor = FEditorStyle::GetSlateColor(SelectionColorPressedName).GetColor( InWidgetStyle );
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor( InWidgetStyle );
	FLinearColor SelectedKeyColor = SelectionColor;
	FSequencer& Sequencer = ParentSectionArea->GetSequencer();
	TSharedPtr<ISequencerHotspot> Hotspot = Sequencer.GetHotspot();

	// get hovered key
	FSequencerSelectedKey HoveredKey;

	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKey = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
	}

	auto& Selection = Sequencer.GetSelection();
	auto& SelectionPreview = Sequencer.GetSelectionPreview();

	const float ThrobScaleValue = GetSelectionThrobValue();

	// draw all keys in each key area
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	const FSlateBrush* HighlightBrush = FEditorStyle::GetBrush(HighlightBrushName);
	const FSlateBrush* BackgroundBrush = FEditorStyle::GetBrush(BackgroundBrushName);
	const FSlateBrush* StripeOverlayBrush = FEditorStyle::GetBrush(StripeOverlayBrushName);
	const FSlateBrush* CircleKeyBrush = FEditorStyle::GetBrush(CircleKeyBrushName);
	const FSlateBrush* DiamondKeyBrush = FEditorStyle::GetBrush(DiamondKeyBrushName);
	const FSlateBrush* SquareKeyBrush = FEditorStyle::GetBrush(SquareKeyBrushName);
	const FSlateBrush* TriangleKeyBrush = FEditorStyle::GetBrush(TriangleKeyBrushName);

	const ESlateDrawEffect::Type DrawEffects = InPainter.bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	int32 LayerId = InPainter.LayerId;

	for (const FSectionLayoutElement& LayoutElement : Layout->GetElements())
	{
		// get key handles
		TSharedPtr<IKeyArea> KeyArea = LayoutElement.GetKeyArea();

		FGeometry KeyAreaGeometry = GetKeyAreaGeometry(LayoutElement, InPainter.SectionGeometry);

		TOptional<FLinearColor> KeyAreaColor = KeyArea.IsValid() ? KeyArea->GetColor() : TOptional<FLinearColor>();

		// draw a box for the key area 
		if (KeyAreaColor.IsSet() && Sequencer.GetSettings()->GetShowChannelColors())
		{
			static float BoxThickness = 5.f;
			FVector2D KeyAreaSize = KeyAreaGeometry.GetLocalSize();
			FSlateDrawElement::MakeBox( 
				InPainter.DrawElements,
				LayerId + 1,
				KeyAreaGeometry.ToPaintGeometry(FVector2D(KeyAreaSize.X, BoxThickness), FSlateLayoutTransform(FVector2D(0.f, KeyAreaSize.Y*.5f - BoxThickness*.5f))),
				StripeOverlayBrush,
				InPainter.SectionClippingRect.InsetBy(1.f),
				DrawEffects,
				KeyAreaColor.GetValue()
			); 
		}

		if (LayoutElement.GetDisplayNode().IsValid())
		{
			FLinearColor HighlightColor;
			bool bDrawHighlight = false;
			if (Sequencer.GetSelection().NodeHasSelectedKeysOrSections(LayoutElement.GetDisplayNode().ToSharedRef()))
			{
				bDrawHighlight = true;
				HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);
			}
			else if (LayoutElement.GetDisplayNode()->IsHovered())
			{
				bDrawHighlight = true;
				HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.05f);
			}

			if (bDrawHighlight)
			{
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					LayerId + 1,
					KeyAreaGeometry.ToPaintGeometry(),
					HighlightBrush,
					InPainter.SectionClippingRect,
					DrawEffects,
					HighlightColor
				); 
			}
		}

		if (Selection.IsSelected(LayoutElement.GetDisplayNode().ToSharedRef()))
		{
			static const FName SelectedTrackTint("Sequencer.Section.SelectedTrackTint");

			FLinearColor KeyAreaOutlineColor = SelectionColor;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId + 2,
				KeyAreaGeometry.ToPaintGeometry(),
				FEditorStyle::GetBrush(SelectedTrackTint),
				InPainter.SectionClippingRect,
				DrawEffects,
				KeyAreaOutlineColor
			);
		}

		// Can't do any of the rest if there are no keys
		if (!KeyArea.IsValid())
		{
			continue;
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
			FVector2D FillOffset(0.0f, 0.0f);

			switch (KeyArea->GetKeyInterpMode(KeyHandle))
			{
			case RCIM_Linear:
				KeyBrush = TriangleKeyBrush;
				KeyColor = FLinearColor(0.0f, 0.617f, 0.449f, 1.0f); // blueish green
				FillOffset = FVector2D(0.0f, 1.0f);
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

			if (LayoutElement.GetType() == FSectionLayoutElement::Group)
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
				FillOffset = SectionInterface->GetKeyBrushOrigin(KeyHandle);
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
			if (LayoutElement.GetType() == FSectionLayoutElement::Group)
			{
				auto Group = StaticCastSharedPtr<FGroupedKeyArea>(KeyArea);
				KeyColor *= Group->GetKeyTint(KeyHandle);
			}

			// draw border
			float KeyPosition = TimeToPixelConverter.TimeToPixel(KeyTime);

			static FVector2D ThrobAmount(12.f, 12.f);
			const FVector2D KeySize = bSelected ? SequencerSectionConstants::KeySize + ThrobAmount * ThrobScaleValue : SequencerSectionConstants::KeySize;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
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
				KeyClippingRect,
				DrawEffects,
				BorderColor
			);

			// draw fill
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 2 : KeyLayer + 1,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					FillOffset + 
					FVector2D(
						(KeyPosition - FMath::CeilToFloat((KeySize.X / 2.0f) - BrushBorderWidth)),
						((KeyAreaGeometry.Size.Y / 2.0f) - ((KeySize.Y / 2.0f) - BrushBorderWidth))
					),
					KeySize - 2.0f * BrushBorderWidth
				),
				KeyBrush,
				KeyClippingRect,
				DrawEffects,
				FillColor
			);
		}
	}
}


void SSequencerSection::DrawSectionHandles( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor, const ISequencerHotspot* Hotspot ) const
{
	UMovieSceneSection* Section = SectionInterface->GetSectionObject();
	if (!Section || Section->IsInfinite())
	{
		return;
	}

	const FSlateBrush* LeftGripBrush = FEditorStyle::GetBrush("Sequencer.Section.GripLeft");
	const FSlateBrush* RightGripBrush = FEditorStyle::GetBrush("Sequencer.Section.GripRight");

	bool bLeftHandleActive = false;
	bool bRightHandleActive = false;

	// Get the hovered/selected state for the section handles from the hotspot
	if (Hotspot && (
		Hotspot->GetType() == ESequencerHotspot::SectionResize_L ||
		Hotspot->GetType() == ESequencerHotspot::SectionResize_R))
	{
		const FSectionResizeHotspot* ResizeHotspot = static_cast<const FSectionResizeHotspot*>(Hotspot);
		if (ResizeHotspot->Section == FSectionHandle(ParentSectionArea, SectionIndex))
		{
			bLeftHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_L;
			bRightHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_R;
		}
	}

	float Opacity = FMath::Clamp(.5f + HandleOffsetPx / SectionInterface->GetSectionGripSize() * .5f, .5f, 1.f);

	// Left Grip
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(0.0f, 0.0f),
			FVector2D(SectionInterface->GetSectionGripSize(), AllottedGeometry.GetDrawSize().Y)
		),
		LeftGripBrush,
		MyClippingRect.InsetBy(FMargin(1.f)),
		DrawEffects,
		(bLeftHandleActive ? SelectionColor : LeftGripBrush->GetTint(FWidgetStyle())).CopyWithNewOpacity(Opacity)
	);
	
	// Right Grip
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(AllottedGeometry.Size.X-SectionInterface->GetSectionGripSize(), 0.0f),
			FVector2D(SectionInterface->GetSectionGripSize(), AllottedGeometry.GetDrawSize().Y)
		),
		RightGripBrush,
		MyClippingRect.InsetBy(FMargin(1.f)),
		DrawEffects,
		(bRightHandleActive ? SelectionColor : RightGripBrush->GetTint(FWidgetStyle())).CopyWithNewOpacity(Opacity)
	);
}

void SSequencerSection::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{	
	if( GetVisibility() == EVisibility::Visible )
	{
		Layout = FSectionLayout(*ParentSectionArea, SectionIndex);

		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Section && !Section->IsInfinite())
		{
			FTimeToPixel TimeToPixelConverter(ParentGeometry, GetSequencer().GetViewRange());

			const int32 SectionLengthPx = FMath::Max(0,
				FMath::RoundToInt(
					TimeToPixelConverter.TimeToPixel(Section->GetEndTime())) - FMath::RoundToInt(TimeToPixelConverter.TimeToPixel(Section->GetStartTime())
				)
			);

			const float SectionGripSize = SectionInterface->GetSectionGripSize();
			HandleOffsetPx = FMath::Max(FMath::RoundToFloat((2*SectionGripSize - SectionLengthPx) * .5f), 0.f);
		}

		FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );
		SectionInterface->Tick(SectionGeometry, ParentGeometry, InCurrentTime, InDeltaTime);
	}
}

FReply SSequencerSection::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencer& Sequencer = GetSequencer();

	FSequencerSelectedKey HoveredKey;
	
	// The hovered key is defined from the sequencer hotspot
	TSharedPtr<ISequencerHotspot> Hotspot = GetSequencer().GetHotspot();
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKey = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		GEditor->BeginTransaction(NSLOCTEXT("Sequencer", "CreateKey_Transaction", "Create Key"));

		// Generate a key and set it as the PressedKey
		FSequencerSelectedKey NewKey = CreateKeyUnderMouse(MouseEvent.GetScreenSpacePosition(), MyGeometry, HoveredKey);

		if (NewKey.IsValid())
		{
			HoveredKey = NewKey;

			Sequencer.GetSelection().EmptySelectedKeys();
			Sequencer.GetSelection().AddToSelection(NewKey);

			// Pass the event to the tool to copy the hovered key and move it
			GetSequencer().SetHotspot( MakeShareable( new FKeyHotspot(NewKey) ) );

			// Return unhandled so that the EditTool can handle the mouse down based on the newly created keyframe and prepare to move it
			return FReply::Unhandled();
		}
	}

	return FReply::Unhandled();
}


FGeometry SSequencerSection::MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const
{
	return AllottedGeometry.MakeChild(
		AllottedGeometry.GetDrawSize() - FVector2D( HandleOffsetPx*2, 0.0f ),
		FSlateLayoutTransform(FVector2D(HandleOffsetPx, 0 ))
	);
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
	// Checked for hovered key
	FSequencerSelectedKey KeyUnderMouse = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );
	if ( KeyUnderMouse.IsValid() )
	{
		GetSequencer().SetHotspot( MakeShareable( new FKeyHotspot(KeyUnderMouse) ) );
	}
	else
	{
		GetSequencer().SetHotspot( MakeShareable( new FSectionHotspot(FSectionHandle(ParentSectionArea, SectionIndex))) );

		// Only check for edge interaction if not hovering over a key
		CheckForEdgeInteraction( MouseEvent, MyGeometry );
	}

	return FReply::Unhandled();
}
	
FReply SSequencerSection::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		GEditor->EndTransaction();

		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SSequencerSection::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave( MouseEvent );
	GetSequencer().SetHotspot(nullptr);
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
