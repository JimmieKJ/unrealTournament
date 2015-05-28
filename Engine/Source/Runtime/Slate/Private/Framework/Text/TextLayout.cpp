// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "BreakIterator.h"

FTextLayout::FBreakCandidate FTextLayout::CreateBreakCandidate( int32& OutRunIndex, FLineModel& Line, int32 PreviousBreak, int32 CurrentBreak )
{
	bool SuccessfullyMeasuredSlice = false;
	int16 MaxAboveBaseline = 0;
	int16 MaxBelowBaseline = 0;
	FVector2D BreakSize( ForceInitToZero );
	FVector2D BreakSizeWithoutTrailingWhitespace( ForceInitToZero );
	float FirstTrailingWhitespaceCharWidth = 0.0f;
	int32 WhitespaceStopIndex = CurrentBreak;
	uint8 Kerning = 0;

	if ( Line.Runs.IsValidIndex( OutRunIndex ) )
	{
		FRunModel& Run = Line.Runs[ OutRunIndex ];
		const FTextRange Range = Run.GetTextRange();
		int32 BeginIndex = FMath::Max( PreviousBreak, Range.BeginIndex );

		if ( BeginIndex > 0 )
		{
			Kerning = Run.GetKerning( BeginIndex, Scale );
		}
	}

	// We need to consider the Runs when detecting and measuring the text lengths of Lines because
	// the font style used makes a difference.
	for (; OutRunIndex < Line.Runs.Num(); OutRunIndex++)
	{
		FRunModel& Run = Line.Runs[ OutRunIndex ];
		const FTextRange Range = Run.GetTextRange();

		FVector2D SliceSize;
		FVector2D SliceSizeWithoutTrailingWhitespace;
		int32 StopIndex = PreviousBreak;

		WhitespaceStopIndex = StopIndex = FMath::Min( Range.EndIndex, CurrentBreak );
		int32 BeginIndex = FMath::Max( PreviousBreak, Range.BeginIndex );

		while( WhitespaceStopIndex > BeginIndex && FText::IsWhitespace( (*Line.Text)[ WhitespaceStopIndex - 1 ] ) )
		{
			--WhitespaceStopIndex;
		}

		if ( BeginIndex == StopIndex )
		{
			// This slice is empty, no need to adjust anything
			SliceSize = SliceSizeWithoutTrailingWhitespace = FVector2D::ZeroVector;
		}
		else if ( BeginIndex == WhitespaceStopIndex )
		{
			// This slice contains only whitespace, no need to adjust SliceSizeWithoutTrailingWhitespace
			SliceSize = Run.Measure( BeginIndex, StopIndex, Scale );
			SliceSizeWithoutTrailingWhitespace = FVector2D::ZeroVector;
		}
		else if ( WhitespaceStopIndex != StopIndex )
		{
			// This slice contains trailing whitespace, measure the text size, then add on the whitespace size
			SliceSize = SliceSizeWithoutTrailingWhitespace = Run.Measure( BeginIndex, WhitespaceStopIndex, Scale );
			const float WhitespaceWidth = Run.Measure( WhitespaceStopIndex, StopIndex, Scale ).X;
			SliceSize.X += WhitespaceWidth;

			// We also need to measure the width of the first piece of trailing whitespace
			if ( WhitespaceStopIndex + 1 == StopIndex )
			{
				// Only have one piece of whitespace
				FirstTrailingWhitespaceCharWidth = WhitespaceWidth;
			}
			else
			{
				// Deliberately use the run version of Measure as we don't want the run model to cache this measurement since it may be out of order and break the binary search
				FirstTrailingWhitespaceCharWidth = Run.GetRun()->Measure( WhitespaceStopIndex, WhitespaceStopIndex + 1, Scale ).X;
			}
		}
		else
		{
			// This slice contains no whitespace, both sizes are the same and can use the same measurement
			SliceSize = SliceSizeWithoutTrailingWhitespace = Run.Measure( BeginIndex, StopIndex, Scale );
		}

		BreakSize.X += SliceSize.X; // We accumulate the slice widths
		BreakSizeWithoutTrailingWhitespace.X += SliceSizeWithoutTrailingWhitespace.X; // We accumulate the slice widths

		// Get the baseline and flip it's sign; Baselines are generally negative
		const int16 Baseline = -(Run.GetBaseLine( Scale ));

		// For the height of the slice we need to take into account the largest value below and above the baseline and add those together
		MaxAboveBaseline = FMath::Max( MaxAboveBaseline, (int16)( Run.GetMaxHeight( Scale ) - Baseline ) );
		MaxBelowBaseline = FMath::Max( MaxBelowBaseline, Baseline );

		if ( StopIndex == CurrentBreak )
		{
			SuccessfullyMeasuredSlice = true;

			if ( OutRunIndex < Line.Runs.Num() && StopIndex == Line.Runs[ OutRunIndex ].GetTextRange().EndIndex )
			{
				++OutRunIndex;
			}
			break;
		}
	}
	check( SuccessfullyMeasuredSlice == true );

	BreakSize.Y = BreakSizeWithoutTrailingWhitespace.Y = MaxAboveBaseline + MaxBelowBaseline;

	FBreakCandidate BreakCandidate;
	BreakCandidate.ActualSize = BreakSize;
	BreakCandidate.TrimmedSize = BreakSizeWithoutTrailingWhitespace;
	BreakCandidate.ActualRange = FTextRange( PreviousBreak, CurrentBreak );
	BreakCandidate.TrimmedRange = FTextRange( PreviousBreak, WhitespaceStopIndex );
	BreakCandidate.FirstTrailingWhitespaceCharWidth = FirstTrailingWhitespaceCharWidth;
	BreakCandidate.MaxAboveBaseline = MaxAboveBaseline;
	BreakCandidate.MaxBelowBaseline = MaxBelowBaseline;
	BreakCandidate.Kerning = Kerning;

#if TEXT_LAYOUT_DEBUG
	BreakCandidate.DebugSlice = FString( BreakCandidate.Range.EndIndex - BreakCandidate.Range.BeginIndex, (**Line.Text) + BreakCandidate.Range.BeginIndex );
#endif

	return BreakCandidate;
}

void FTextLayout::CreateLineViewBlocks( int32 LineModelIndex, const int32 StopIndex, const float WrappedLineWidth, int32& OutRunIndex, int32& OutRendererIndex, int32& OutPreviousBlockEnd, TArray< TSharedRef< ILayoutBlock > >& OutSoftLine )
{
	const FLineModel& LineModel = LineModels[ LineModelIndex ];

	int16 MaxAboveBaseline = 0;
	int16 MaxBelowBaseline = 0;

	for (; OutRunIndex < LineModel.Runs.Num(); )
	{
		const FRunModel& Run = LineModel.Runs[ OutRunIndex ];
		const FTextRange RunRange = Run.GetTextRange();

		TSharedPtr< IRunRenderer > BlockRenderer = nullptr;

		int32 BlockStopIndex = RunRange.EndIndex;
		if ( OutRendererIndex != INDEX_NONE )
		{
			// Grab the currently active renderer
			const FTextRunRenderer& Renderer = LineModel.RunRenderers[OutRendererIndex];

			// Check to see if the last block was rendered with the same renderer
			if ( OutPreviousBlockEnd >= Renderer.Range.BeginIndex )
			{
				//If the renderer ends before our run...
				if ( Renderer.Range.EndIndex <= RunRange.EndIndex )
				{
					// Adjust the stopping point of the block to be the end of the renderer range,
					// since highlights need their own block segments
					BlockStopIndex = Renderer.Range.EndIndex;
					BlockRenderer = Renderer.Renderer;
				}
				else
				{
					// This whole run is encompassed by the renderer
					BlockRenderer = Renderer.Renderer;
				}
			}
			else
			{
				// Does the renderer range begin before our run ends?
				if ( Renderer.Range.BeginIndex <= RunRange.EndIndex )
				{
					// then adjust the current block stopping point to just before the renderer range begins,
					// since renderers need their own block segments
					BlockStopIndex = Renderer.Range.BeginIndex;
					BlockRenderer = nullptr;
				}
			}
		}

		if (StopIndex != INDEX_NONE)
		{
			BlockStopIndex = FMath::Min(StopIndex, BlockStopIndex);
		}

		int32 BlockBeginIndex = FMath::Max( OutPreviousBlockEnd, RunRange.BeginIndex );
		const bool IsLastBlock = BlockStopIndex == StopIndex;

		// if this new block will be entirely encapsulated by the run...
		if ( RunRange.BeginIndex < BlockStopIndex && RunRange.EndIndex > BlockBeginIndex )
		{
			check( BlockBeginIndex <= BlockStopIndex );

			FBlockDefinition BlockDefine;
			BlockDefine.ActualRange = FTextRange(BlockBeginIndex, BlockStopIndex);
			BlockDefine.Renderer = BlockRenderer;

			OutSoftLine.Add( Run.CreateBlock( BlockDefine, Scale ) );
			OutPreviousBlockEnd = BlockStopIndex;
		}
		else
		{
			FBlockDefinition BlockDefine;
			BlockDefine.ActualRange = RunRange;
			BlockDefine.Renderer = BlockRenderer;

			OutSoftLine.Add( Run.CreateBlock( BlockDefine, Scale ) );
			OutPreviousBlockEnd = RunRange.EndIndex;
		}

		// Get the baseline and flip it's sign; Baselines are generally negative
		const int16 Baseline = -(Run.GetBaseLine( Scale ));

		// For the height of the slice we need to take into account the largest value below and above the baseline and add those together
		MaxAboveBaseline = FMath::Max( MaxAboveBaseline, (int16)( Run.GetMaxHeight( Scale ) - Baseline ) );
		MaxBelowBaseline = FMath::Max( MaxBelowBaseline, Baseline );

		if ( BlockStopIndex == RunRange.EndIndex )
		{
			++OutRunIndex;
		}

		if ( OutRendererIndex != INDEX_NONE && BlockStopIndex == LineModel.RunRenderers[ OutRendererIndex ].Range.EndIndex )
		{
			++OutRendererIndex;

			if ( OutRendererIndex >= LineModel.RunRenderers.Num() )
			{
				OutRendererIndex = INDEX_NONE;
			}
		}

		if ( IsLastBlock )
		{
			break;
		}
	}

	FVector2D LineSize( ForceInitToZero );
	
	// Use a negative scroll offset since positive scrolling moves things negatively in screen space
	FVector2D CurrentOffset( Margin.Left - ScrollOffset.X, Margin.Top + TextLayoutSize.Height - ScrollOffset.Y );

	if ( OutSoftLine.Num() > 0 )
	{
		float CurrentHorizontalPos = 0.0f;
		for (int32 Index = 0; Index < OutSoftLine.Num(); Index++)
		{
			const TSharedRef< ILayoutBlock > Block = OutSoftLine[ Index ];
			const TSharedRef< IRun > Run = Block->GetRun();

			const int16 BlockBaseline = Run->GetBaseLine(Scale);
			const int16 VerticalOffset = MaxAboveBaseline - Block->GetSize().Y - BlockBaseline;
			const int8 BlockKerning = Run->GetKerning(Block->GetTextRange().BeginIndex, Scale);

			Block->SetLocationOffset(FVector2D(CurrentOffset.X + CurrentHorizontalPos + BlockKerning, CurrentOffset.Y + VerticalOffset));

			CurrentHorizontalPos += Block->GetSize().X;
		}

		const float UnscaleLineHeight = MaxAboveBaseline + MaxBelowBaseline;

		LineSize.X = CurrentHorizontalPos;
		LineSize.Y = UnscaleLineHeight * LineHeightPercentage;

		FTextLayout::FLineView LineView;
		LineView.Offset = CurrentOffset;
		LineView.Size = LineSize;
		LineView.TextSize = FVector2D(CurrentHorizontalPos, UnscaleLineHeight);
		LineView.Range = FTextRange(OutSoftLine[0]->GetTextRange().BeginIndex, OutSoftLine.Last()->GetTextRange().EndIndex);
		LineView.ModelIndex = LineModelIndex;
		LineView.Blocks.Append( OutSoftLine );

		LineViews.Add( LineView );
	}

	TextLayoutSize.DrawWidth = FMath::Max( TextLayoutSize.DrawWidth, LineSize.X ); // DrawWidth is the size of the longest line + the Margin
	TextLayoutSize.WrappedWidth = FMath::Max( TextLayoutSize.WrappedWidth, (StopIndex == INDEX_NONE) ? LineSize.X : WrappedLineWidth ); // WrappedWidth is the size of the longest line + the Margin + any trailing whitespace width
	TextLayoutSize.Height += LineSize.Y; // Height is the total height of all lines
}

void FTextLayout::JustifyLayout()
{
	if ( Justification == ETextJustify::Left )
	{
		return;
	}

	const float LayoutWidthNoMargin = FMath::Max(TextLayoutSize.DrawWidth, ViewSize.X * Scale) - (Margin.GetTotalSpaceAlong<Orient_Horizontal>() * Scale);

	for (FLineView& LineView : LineViews)
	{
		const float ExtraSpace = LayoutWidthNoMargin - LineView.Size.X;

		FVector2D OffsetAdjustment( ForceInitToZero );
		if ( Justification == ETextJustify::Center )
		{
			OffsetAdjustment = FVector2D( ExtraSpace * 0.5f, 0 );
		}
		else if ( Justification == ETextJustify::Right )
		{
			OffsetAdjustment = FVector2D( ExtraSpace, 0 );
		}

		LineView.Offset += OffsetAdjustment;

		for (const TSharedRef< ILayoutBlock >& Block : LineView.Blocks)
		{
			Block->SetLocationOffset( Block->GetLocationOffset() + OffsetAdjustment );
		}
	}
}

float FTextLayout::GetWrappingDrawWidth() const
{
	check( WrappingWidth >= 0 );
	return FMath::Max( 0.01f, ( WrappingWidth - Margin.GetTotalSpaceAlong<Orient_Horizontal>() ) * Scale );
}

void FTextLayout::FlowLayout()
{
	const float WrappingDrawWidth = GetWrappingDrawWidth();

	CreateWrappingCache();

	TArray< TSharedRef< ILayoutBlock > > SoftLine;
	for (int32 LineModelIndex = 0; LineModelIndex < LineModels.Num(); LineModelIndex++)
	{
		FlowLineLayout(LineModelIndex, WrappingDrawWidth, SoftLine);
	}

	// Add on the margins to the layout size
	const float MarginWidth = Margin.GetTotalSpaceAlong<Orient_Horizontal>() * Scale;
	const float MarginHeight = Margin.GetTotalSpaceAlong<Orient_Vertical>() * Scale;
	TextLayoutSize.DrawWidth += MarginWidth;
	TextLayoutSize.WrappedWidth += MarginWidth;
	TextLayoutSize.Height += MarginHeight;
}

void FTextLayout::FlowLineLayout(const int32 LineModelIndex, const float WrappingDrawWidth, TArray<TSharedRef<ILayoutBlock>>& SoftLine)
{
	const FLineModel& LineModel = LineModels[ LineModelIndex ];

	float CurrentWidth = 0.0f;
	int32 CurrentRunIndex = 0;
	int32 PreviousBlockEnd = 0;

	int32 CurrentRendererIndex = 0;
	if ( CurrentRendererIndex >= LineModel.RunRenderers.Num() )
	{
		CurrentRendererIndex = INDEX_NONE;
	}

	const bool IsWrapping = WrappingWidth > 0.0f;

	// if the Line doesn't have any BreakCandidates, or we're not wrapping text
	if (!IsWrapping || LineModel.BreakCandidates.Num() == 0 )
	{
		//Then iterate over all of it's runs
		CreateLineViewBlocks( LineModelIndex, INDEX_NONE, 0.0f, /*OUT*/CurrentRunIndex, /*OUT*/CurrentRendererIndex, /*OUT*/PreviousBlockEnd, SoftLine );
		check( CurrentRunIndex == LineModel.Runs.Num() );
		CurrentWidth = 0;
		SoftLine.Empty();
	}
	else
	{
		for (int32 BreakIndex = 0; BreakIndex < LineModel.BreakCandidates.Num(); BreakIndex++)
		{
			const FBreakCandidate& Break = LineModel.BreakCandidates[ BreakIndex ];

			const bool IsLastBreak = BreakIndex + 1 == LineModel.BreakCandidates.Num();
			const bool IsFirstBreakOnSoftLine = CurrentWidth == 0;
			const uint8 Kerning = ( IsFirstBreakOnSoftLine ) ? Break.Kerning : 0;
			const bool BreakDoesFit = !IsWrapping || CurrentWidth + Break.ActualSize.X + Kerning <= WrappingDrawWidth;

			if ( !BreakDoesFit || IsLastBreak )
			{
				const bool BreakWithoutTrailingWhitespaceDoesFit = !IsWrapping || CurrentWidth + Break.TrimmedSize.X + Kerning <= WrappingDrawWidth;
				const bool IsFirstBreak = BreakIndex == 0;

				const FBreakCandidate& FinalBreakOnSoftLine = ( !IsFirstBreak && !IsFirstBreakOnSoftLine && !BreakWithoutTrailingWhitespaceDoesFit ) ? LineModel.BreakCandidates[ --BreakIndex ] : Break;
				
				// We want the wrapped line width to contain the first piece of trailing whitespace for a line, however we only do this if we have trailing whitespace
				// otherwise very long non-breaking words can cause the wrapped line width to expand beyond the desired wrap width
				float WrappedLineWidth = CurrentWidth;
				if ( BreakWithoutTrailingWhitespaceDoesFit && !IsLastBreak )
				{
					// This break has trailing whitespace
					WrappedLineWidth += ( FinalBreakOnSoftLine.TrimmedSize.X + FinalBreakOnSoftLine.FirstTrailingWhitespaceCharWidth );
				}
				else
				{
					// This break is either longer than the wrapping point or the last break on this line, so make sure and clamp the line size to the given wrapping width
					WrappedLineWidth += FinalBreakOnSoftLine.ActualSize.X;
					WrappedLineWidth = FMath::Min(WrappedLineWidth, WrappingDrawWidth);
				}

				CreateLineViewBlocks( LineModelIndex, FinalBreakOnSoftLine.ActualRange.EndIndex, WrappedLineWidth, /*OUT*/CurrentRunIndex, /*OUT*/CurrentRendererIndex, /*OUT*/PreviousBlockEnd, SoftLine );

				if ( CurrentRunIndex < LineModel.Runs.Num() && FinalBreakOnSoftLine.ActualRange.EndIndex == LineModel.Runs[ CurrentRunIndex ].GetTextRange().EndIndex )
				{
					++CurrentRunIndex;
				}

				PreviousBlockEnd = FinalBreakOnSoftLine.ActualRange.EndIndex;

				CurrentWidth = 0;
				SoftLine.Empty();
			} 
			else
			{
				CurrentWidth += Break.ActualSize.X;
			}
		}
	}
}

void FTextLayout::FlowHighlights()
{
	// FlowLayout must have been called first
	check(!(DirtyFlags & EDirtyState::Layout));

	for (FLineView& LineView : LineViews)
	{
		LineView.UnderlayHighlights.Empty();
		LineView.OverlayHighlights.Empty();

		FLineModel& LineModel = LineModels[LineView.ModelIndex];
		
		// Insert each highlighter into every line view that's within its range, either as an underlay, or as an overlay
		for (FTextLineHighlight& LineHighlight : LineModel.LineHighlights)
		{
			if (LineHighlight.LineIndex != LineView.ModelIndex)
			{
				continue;
			}

			const bool bIsHighlightInRange = LineView.Range.InclusiveContains(LineHighlight.Range.BeginIndex) && LineView.Range.InclusiveContains(LineHighlight.Range.EndIndex);
			if(!bIsHighlightInRange)
			{
				continue;
			}

			FLineViewHighlight LineViewHighlight;
			LineViewHighlight.OffsetX = 0.0f;
			LineViewHighlight.Width = 0.0f;
			LineViewHighlight.Highlighter = LineHighlight.Highlighter;

			// Measure the blocks up to the start of this highlight to get the correct start offset
			int32 CurrentBlockIndex = 0;
			for (; CurrentBlockIndex < LineView.Blocks.Num(); ++CurrentBlockIndex)
			{
				const TSharedRef< ILayoutBlock >& Block = LineView.Blocks[CurrentBlockIndex];
				const TSharedRef<IRun> Run = Block->GetRun();

				const FTextRange& BlockTextRange = Block->GetTextRange();
				if (LineHighlight.Range.BeginIndex > BlockTextRange.EndIndex)
				{
					// Highlight starts after this block, just include its entire size
					LineViewHighlight.OffsetX += Run->Measure(BlockTextRange.BeginIndex, BlockTextRange.EndIndex, Scale).X;
				}
				else
				{
					// This block contains the start of this highlight, measure to that point and then we're done!
					LineViewHighlight.OffsetX += Run->Measure(BlockTextRange.BeginIndex, LineHighlight.Range.BeginIndex, Scale).X;
					break;
				}
			}

			// Now measure the blocks under this highlight to get the correct size
			for (; CurrentBlockIndex < LineView.Blocks.Num(); ++CurrentBlockIndex)
			{
				const TSharedRef< ILayoutBlock >& Block = LineView.Blocks[CurrentBlockIndex];
				const TSharedRef<IRun> Run = Block->GetRun();

				const FTextRange& BlockTextRange = Block->GetTextRange();
				const FTextRange IntersectedRange = BlockTextRange.Intersect(LineHighlight.Range);
				if (!IntersectedRange.IsEmpty())
				{
					// Measure the part of the run which intersects the highlight
					LineViewHighlight.Width += Run->Measure(IntersectedRange.BeginIndex, IntersectedRange.EndIndex, Scale).X;
				}
					
				if (BlockTextRange.EndIndex > LineHighlight.Range.EndIndex)
				{
					// We've measured all the runs under the highlight
					break;
				}
			}

			if (LineHighlight.ZOrder < 0)
			{
				LineView.UnderlayHighlights.Add(LineViewHighlight);
			}
			else
			{
				LineView.OverlayHighlights.Add(LineViewHighlight);
			}
		}
	}
}

void FTextLayout::EndLayout()
{
	for (FLineModel& LineModel : LineModels)
	{
		EndLineLayout(LineModel);
	}
}

void FTextLayout::EndLineLayout(FLineModel& LineModel)
{
	for (FRunModel& RunModel : LineModel.Runs)
	{
		RunModel.EndLayout();
	}
}

void FTextLayout::BeginLayout()
{
	for (FLineModel& LineModel : LineModels)
	{
		BeginLineLayout(LineModel);
	}
}

void FTextLayout::BeginLineLayout(FLineModel& LineModel)
{
	for (FRunModel& RunModel : LineModel.Runs)
	{
		RunModel.BeginLayout();
	}
}

void FTextLayout::ClearView()
{
	TextLayoutSize = FTextLayoutSize();
	LineViews.Empty();
}

void FTextLayout::CreateWrappingCache()
{
	const bool IsWrapping = WrappingWidth > 0.0f;
	if (!IsWrapping)
	{
		return;
	}

	for (FLineModel& LineModel : LineModels)
	{
		CreateLineWrappingCache(LineModel);
	}
}

void FTextLayout::CreateLineWrappingCache(FLineModel& LineModel)
{
	if (LineModel.HasWrappingInformation)
	{
		return;
	}

	// If we've not yet been provided with a custom line break iterator, then just use the default one
	if (!LineBreakIterator.IsValid())
	{
		LineBreakIterator = FBreakIterator::CreateLineBreakIterator();
	}

	LineModel.BreakCandidates.Empty();
	LineModel.HasWrappingInformation = true;

	for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
	{
		LineModel.Runs[ RunIndex ].ClearCache();
	}

	LineBreakIterator->SetString( **LineModel.Text );

	int32 PreviousBreak = 0;
	int32 CurrentBreak = 0;
	int32 CurrentRunIndex = 0;

	while( ( CurrentBreak = LineBreakIterator->MoveToNext() ) != INDEX_NONE )
	{
		LineModel.BreakCandidates.Add( CreateBreakCandidate(/*OUT*/CurrentRunIndex, LineModel, PreviousBreak, CurrentBreak) );
		PreviousBreak = CurrentBreak;
	}

	LineBreakIterator->ClearString();
}

void FTextLayout::ClearWrappingCache()
{
	for (FLineModel& LineModel : LineModels)
	{
		LineModel.HasWrappingInformation = false;
	}
}

FTextLayout::FTextLayout() 
	: LineModels()
	, LineViews()
	, DirtyFlags( EDirtyState::None )
	, Scale( 1.0f )
	, WrappingWidth( 0 )
	, Margin()
	, Justification( ETextJustify::Left )
	, LineHeightPercentage( 1.0f )
	, TextLayoutSize()
	, ViewSize( ForceInitToZero )
	, ScrollOffset( ForceInitToZero )
	, LineBreakIterator() // Initialized in FTextLayout::CreateWrappingCache if no custom iterator is provided
	, WordBreakIterator(FBreakIterator::CreateWordBreakIterator())
{

}

void FTextLayout::UpdateIfNeeded()
{
	const bool bHasChangedLayout = !!(DirtyFlags & EDirtyState::Layout);
	const bool bHasChangedHighlights = !!(DirtyFlags & EDirtyState::Highlights);

	if ( bHasChangedLayout )
	{
		// if something has changed then create a new View
		UpdateLayout();
	}

	// If the layout has changed, we always need to update the highlights
	if ( bHasChangedLayout || bHasChangedHighlights)
	{
		UpdateHighlights();
	}
}

void FTextLayout::UpdateLayout()
{
	ClearView();
	BeginLayout();

	FlowLayout();
	JustifyLayout();

	EndLayout();

	DirtyFlags &= ~EDirtyState::Layout;
}

void FTextLayout::UpdateHighlights()
{
	FlowHighlights();

	DirtyFlags &= ~EDirtyState::Highlights;
}

void FTextLayout::DirtyRunLayout(const TSharedRef<const IRun>& Run)
{
	for (int32 LineModelIndex = 0; LineModelIndex < LineModels.Num(); LineModelIndex++)
	{
		FLineModel& LineModel = LineModels[LineModelIndex];

		if (LineModel.HasWrappingInformation)
		{
			for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
			{
				if (LineModel.Runs[RunIndex].GetRun() == Run)
				{
					LineModel.Runs[RunIndex].ClearCache();
					break;
				}
			}
		}
	}

	DirtyFlags |= EDirtyState::Layout;
}

void FTextLayout::DirtyLayout()
{
	DirtyFlags |= EDirtyState::Layout;

	// Clear out the entire cache so it gets regenerated on the text call to FlowLayout
	ClearWrappingCache();
}

bool FTextLayout::IsLayoutDirty() const
{
	return !!(DirtyFlags & EDirtyState::Layout);
}

void FTextLayout::ClearRunRenderers()
{
	for (int32 Index = 0; Index < LineModels.Num(); Index++)
	{
		if (LineModels[ Index ].RunRenderers.Num() )
		{
			LineModels[ Index ].RunRenderers.Empty();
			DirtyFlags |= EDirtyState::Layout;
		}
	}
}

void FTextLayout::SetRunRenderers( const TArray< FTextRunRenderer >& Renderers )
{
	ClearRunRenderers();

	for (int32 Index = 0; Index < Renderers.Num(); Index++)
	{
		AddRunRenderer( Renderers[ Index ] );
	}
}

void FTextLayout::AddRunRenderer( const FTextRunRenderer& Renderer )
{
	checkf( LineModels.IsValidIndex( Renderer.LineIndex ), TEXT("Renderers must be for a valid Line Index") );

	FLineModel& LineModel = LineModels[ Renderer.LineIndex ];

	// Renderers needs to be in order and not overlap
	bool bWasInserted = false;
	for (int32 Index = 0; Index < LineModel.RunRenderers.Num() && !bWasInserted; Index++)
	{
		if ( LineModel.RunRenderers[ Index ].Range.BeginIndex > Renderer.Range.BeginIndex )
		{
			checkf( Index == 0 || LineModel.RunRenderers[ Index - 1 ].Range.EndIndex <= Renderer.Range.BeginIndex, TEXT("Renderers cannot overlap") );
			LineModel.RunRenderers.Insert( Renderer, Index - 1 );
			bWasInserted = true;
		}
		else if ( LineModel.RunRenderers[ Index ].Range.EndIndex > Renderer.Range.EndIndex )
		{
			checkf( LineModel.RunRenderers[ Index ].Range.BeginIndex >= Renderer.Range.EndIndex, TEXT("Renderers cannot overlap") );
			LineModel.RunRenderers.Insert( Renderer, Index - 1 );
			bWasInserted = true;
		}
	}

	if ( !bWasInserted )
	{
		LineModel.RunRenderers.Add( Renderer );
	}

	DirtyFlags |= EDirtyState::Layout;
}

void FTextLayout::ClearLineHighlights()
{
	for (int32 Index = 0; Index < LineModels.Num(); Index++)
	{
		if (LineModels[ Index ].LineHighlights.Num())
		{
			LineModels[ Index ].LineHighlights.Empty();
			DirtyFlags |= EDirtyState::Highlights;
		}
	}
}

void FTextLayout::SetLineHighlights( const TArray< FTextLineHighlight >& Highlights )
{
	ClearLineHighlights();

	for (int32 Index = 0; Index < Highlights.Num(); Index++)
	{
		AddLineHighlight( Highlights[ Index ] );
	}
}

void FTextLayout::AddLineHighlight( const FTextLineHighlight& Highlight )
{
	checkf( LineModels.IsValidIndex( Highlight.LineIndex ), TEXT("Highlights must be for a valid Line Index") );
	checkf( Highlight.ZOrder, TEXT("The highlight Z-order must be <0 to create an underlay, or >0 to create an overlay") );

	FLineModel& LineModel = LineModels[ Highlight.LineIndex ];

	// Try and maintain a stable sorted z-order - highlights with the same z-order should just render in the order they were added
	bool bWasInserted = false;
	for (int32 Index = 0; Index < LineModel.LineHighlights.Num() && !bWasInserted; Index++)
	{
		if ( LineModel.LineHighlights[ Index ].ZOrder > Highlight.ZOrder )
		{
			LineModel.LineHighlights.Insert( Highlight, Index - 1 );
			bWasInserted = true;
		}
	}

	if ( !bWasInserted )
	{
		LineModel.LineHighlights.Add( Highlight );
	}

	DirtyFlags |= EDirtyState::Highlights;
}

FTextLocation FTextLayout::GetTextLocationAt( const FLineView& LineView, const FVector2D& Relative, ETextHitPoint* const OutHitPoint )
{
	for (int32 BlockIndex = 0; BlockIndex < LineView.Blocks.Num(); BlockIndex++)
	{
		const TSharedRef< ILayoutBlock >& Block = LineView.Blocks[ BlockIndex ];
		const int32 TextIndex = Block->GetRun()->GetTextIndexAt(Block, FVector2D(Relative.X, Block->GetLocationOffset().Y), Scale, OutHitPoint);

		if ( TextIndex == INDEX_NONE )
		{
			continue;
		}

		return FTextLocation( LineView.ModelIndex, TextIndex );
	}

	const int32 LineTextLength = LineModels[ LineView.ModelIndex ].Text->Len();
	if ( LineTextLength == 0 || !LineView.Blocks.Num() )
	{
		if(OutHitPoint)
		{
			*OutHitPoint = ETextHitPoint::WithinText;
		}
		return FTextLocation( LineView.ModelIndex, 0 );
	}
	else if (Relative.X < LineView.Blocks[0]->GetLocationOffset().X)
	{
		if(OutHitPoint)
		{
			*OutHitPoint = ETextHitPoint::LeftGutter;
		}
		return FTextLocation(LineView.ModelIndex, LineView.Range.BeginIndex);
	}

	if(OutHitPoint)
	{
		*OutHitPoint = ETextHitPoint::RightGutter;
	}
	return FTextLocation( LineView.ModelIndex, LineView.Range.EndIndex );
}

int32 FTextLayout::GetLineViewIndexForTextLocation(const TArray< FTextLayout::FLineView >& InLineViews, const FTextLocation& Location, const bool bPerformInclusiveBoundsCheck)
{
	const int32 LineModelIndex = Location.GetLineIndex();
	const int32 Offset = Location.GetOffset();

	const FLineModel& LineModel = LineModels[LineModelIndex];
	for(int32 Index = 0; Index < InLineViews.Num(); Index++)
	{
		const FTextLayout::FLineView& LineView = InLineViews[Index];

		if(LineView.ModelIndex == LineModelIndex)
		{
			// Simple case where we're either the start of, or are contained within, the line view
			if(Offset == 0 || LineModel.Text->IsEmpty() || LineView.Range.Contains(Offset))
			{
				return Index;
			}

			// If we're the last line, then we also need to test for the end index being part of the range
			const bool bIsLastLineForModel = Index == (InLineViews.Num() - 1) || InLineViews[Index + 1].ModelIndex != LineModelIndex;
			if((bIsLastLineForModel || bPerformInclusiveBoundsCheck) && LineView.Range.EndIndex == Offset)
			{
				return Index;
			}
		}
	}

	return INDEX_NONE;
}

FTextLocation FTextLayout::GetTextLocationAt( const FVector2D& Relative, ETextHitPoint* const OutHitPoint )
{
	// Early out if we have no LineViews
	if (LineViews.Num() == 0)
	{
		return FTextLocation(0,0);
	}

	// Iterate until we find a LineView that is below our expected Y location
	int32 ViewIndex;
	for (ViewIndex = 0; ViewIndex < LineViews.Num(); ViewIndex++)
	{
		const FLineView& LineView = LineViews[ ViewIndex ];

		if (LineView.Offset.Y > Relative.Y)
		{
			// Set the ViewIndex back to the previous line, but not lower than the first line
			ViewIndex = FMath::Max( 0, ViewIndex - 1 );
			break;
		}
	}

	//if none of the lines are below our expected Y location then...
	if (ViewIndex >= LineViews.Num())
	{
		// just use the very last line
		ViewIndex = LineViews.Num() - 1;
		const FLineView& LineView = LineViews[ ViewIndex ];
		return GetTextLocationAt( LineView, Relative, OutHitPoint );
	}
	else 
	{
		// if the current LineView does not encapsulate our expected Y location then jump to the next LineView
		// if we aren't already at the last LineView
		const FLineView& LineView = LineViews[ ViewIndex ];
		if ((LineView.Offset.Y + LineView.Size.Y) < Relative.Y && ViewIndex < LineViews.Num() - 1)
		{
			++ViewIndex;
		}
	}

	const FLineView& LineView = LineViews[ ViewIndex ];
	return GetTextLocationAt( LineView, FVector2D(Relative.X, LineView.Offset.Y), OutHitPoint );
}

FVector2D FTextLayout::GetLocationAt( const FTextLocation& Location, const bool bPerformInclusiveBoundsCheck )
{
	const int32 LineModelIndex = Location.GetLineIndex();
	const int32 Offset = Location.GetOffset();

	// Find the LineView which encapsulates the location's offset
	const int32 LineViewIndex = GetLineViewIndexForTextLocation( LineViews, Location, bPerformInclusiveBoundsCheck );

	// if we failed to find a LineView for the location, early out
	if ( !LineViews.IsValidIndex( LineViewIndex ) )
	{
		return FVector2D( ForceInitToZero );
	}

	const FTextLayout::FLineView& LineView = LineViews[ LineViewIndex ];

	//Iterate over the LineView's blocks...
	for (int32 BlockIndex = 0; BlockIndex < LineView.Blocks.Num(); BlockIndex++)
	{
		const TSharedRef< ILayoutBlock >& Block = LineView.Blocks[ BlockIndex ];
		const FTextRange& BlockRange = Block->GetTextRange();

		//if the block's range contains the specified locations offset...
		if (BlockRange.InclusiveContains(Offset))
		{
			// Ask the block for the exact screen location
			const FVector2D ScreenLocation = Block->GetRun()->GetLocationAt( Block, Offset - BlockRange.BeginIndex, Scale );

			// if the block was unable to provide a location continue iterating
			if ( ScreenLocation.IsZero() )
			{
				continue;
			}

			return ScreenLocation;
		}
	}

	// Failed to find the screen location
	return FVector2D( ForceInitToZero );
}

bool FTextLayout::InsertAt(const FTextLocation& Location, TCHAR Character)
{
	const int32 InsertLocation = Location.GetOffset();
	const int32 LineIndex = Location.GetLineIndex();

	if (!LineModels.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineModel& LineModel = LineModels[LineIndex];

	LineModel.Text->InsertAt(InsertLocation, Character);
	LineModel.HasWrappingInformation = false;

	bool RunIsAfterInsertLocation = false;
	for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
	{
		FRunModel& RunModel = LineModel.Runs[ RunIndex ];
		const FTextRange& RunRange = RunModel.GetTextRange();

		const bool bIsLastRun = RunIndex == LineModel.Runs.Num() - 1;
		if (RunRange.Contains(InsertLocation) || (bIsLastRun && !RunIsAfterInsertLocation))
		{
			check(RunIsAfterInsertLocation == false);
			RunIsAfterInsertLocation = true;

			if ((RunModel.GetRun()->GetRunAttributes() & ERunAttributes::SupportsText) != ERunAttributes::None)
			{
				RunModel.SetTextRange( FTextRange( RunRange.BeginIndex, RunRange.EndIndex + 1 ) );
			}
			else
			{
				// Non-text runs are supposed to have a single dummy character in them
				check(RunRange.Len() == 1);

				// This run doesn't support text, so we need to insert a new text run before or after the current run depending on the insertion point
				const bool bIsInsertingToTheLeft = InsertLocation == RunRange.BeginIndex;
				if (bIsInsertingToTheLeft)
				{
					// Insert the new text run to the left of the non-text run
					TSharedRef<IRun> NewTextRun = CreateDefaultTextRun( LineModel.Text, FTextRange( RunRange.BeginIndex, RunRange.BeginIndex + 1 ) );
					RunModel.SetTextRange( FTextRange( RunRange.BeginIndex + 1, RunRange.EndIndex + 1 ) );
					LineModel.Runs.Insert( NewTextRun, RunIndex++ );
				}
				else
				{
					// Insert the new text run to the right of the non-text run
					TSharedRef<IRun> NewTextRun = CreateDefaultTextRun( LineModel.Text, FTextRange( RunRange.EndIndex, RunRange.EndIndex + 1 ) );
					LineModel.Runs.Insert( NewTextRun, ++RunIndex );
				}
			}
		}
		else if (RunIsAfterInsertLocation)
		{
			FTextRange NewRange = RunRange;
			NewRange.Offset(1);
			RunModel.SetTextRange(NewRange);
		}
	}

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

bool FTextLayout::InsertAt(const FTextLocation& Location, const FString& Text)
{
	const int32 InsertLocation = Location.GetOffset();
	const int32 LineIndex = Location.GetLineIndex();

	if (!LineModels.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineModel& LineModel = LineModels[LineIndex];

	LineModel.Text->InsertAt(InsertLocation, Text);
	LineModel.HasWrappingInformation = false;

	bool RunIsAfterInsertLocation = false;
	for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
	{
		FRunModel& RunModel = LineModel.Runs[ RunIndex ];
		const FTextRange& RunRange = RunModel.GetTextRange();

		const bool bIsLastRun = RunIndex == LineModel.Runs.Num() - 1;
		if (RunRange.Contains(InsertLocation) || (bIsLastRun && !RunIsAfterInsertLocation))
		{
			check(RunIsAfterInsertLocation == false);
			RunIsAfterInsertLocation = true;

			if ((RunModel.GetRun()->GetRunAttributes() & ERunAttributes::SupportsText) != ERunAttributes::None)
			{
				RunModel.SetTextRange( FTextRange( RunRange.BeginIndex, RunRange.EndIndex + Text.Len() ) );
			}
			else
			{
				// Non-text runs are supposed to have a single dummy character in them
				check(RunRange.Len() == 1);

				// This run doesn't support text, so we need to insert a new text run before or after the current run depending on the insertion point
				const bool bIsInsertingToTheLeft = InsertLocation == RunRange.BeginIndex;
				if (bIsInsertingToTheLeft)
				{
					// Insert the new text run to the left of the non-text run
					TSharedRef<IRun> NewTextRun = CreateDefaultTextRun( LineModel.Text, FTextRange( RunRange.BeginIndex, RunRange.BeginIndex + Text.Len() ) );
					RunModel.SetTextRange( FTextRange( RunRange.BeginIndex + 1, RunRange.EndIndex + Text.Len() ) );
					LineModel.Runs.Insert( NewTextRun, RunIndex++ );
				}
				else
				{
					// Insert the new text run to the right of the non-text run
					TSharedRef<IRun> NewTextRun = CreateDefaultTextRun( LineModel.Text, FTextRange( RunRange.EndIndex, RunRange.EndIndex + Text.Len() ) );
					LineModel.Runs.Insert( NewTextRun, ++RunIndex );
				}
			}
		}
		else if (RunIsAfterInsertLocation)
		{
			FTextRange NewRange = RunRange;
			NewRange.Offset(Text.Len());
			RunModel.SetTextRange(NewRange);
		}
	}

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

bool FTextLayout::InsertAt(const FTextLocation& Location, TSharedRef<IRun> InRun, const bool bAlwaysKeepRightRun)
{
	const int32 InsertLocation = Location.GetOffset();
	const int32 LineIndex = Location.GetLineIndex();

	if (!LineModels.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineModel& LineModel = LineModels[LineIndex];

	FString NewRunText;
	InRun->AppendTextTo(NewRunText);

	LineModel.Text->InsertAt(InsertLocation, NewRunText);
	LineModel.HasWrappingInformation = false;

	bool RunIsAfterInsertLocation = false;
	for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
	{
		const TSharedRef<IRun> Run = LineModel.Runs[RunIndex].GetRun();
		const FTextRange& RunRange = Run->GetTextRange();

		const bool bIsLastRun = RunIndex == LineModel.Runs.Num() - 1;
		if (RunRange.Contains(InsertLocation) || (bIsLastRun && !RunIsAfterInsertLocation))
		{
			check(RunIsAfterInsertLocation == false);
			RunIsAfterInsertLocation = true;

			const int32 InsertLocationEnd = InsertLocation + NewRunText.Len();

			// This run contains the insertion point, so we need to split it
			TSharedPtr<IRun> LeftRun;
			TSharedPtr<IRun> RightRun;
			if ((Run->GetRunAttributes() & ERunAttributes::SupportsText) != ERunAttributes::None)
			{
				LeftRun = Run->Clone();
				LeftRun->SetTextRange(FTextRange(RunRange.BeginIndex, InsertLocation));

				RightRun = Run;
				RightRun->SetTextRange(FTextRange(InsertLocationEnd, RunRange.EndIndex + NewRunText.Len()));
			}
			else
			{
				// Non-text runs are supposed to have a single dummy character in them
				check(RunRange.Len() == 1);

				// This run doesn't support text, so we need to insert a new text run before or after the current run depending on the insertion point
				const bool bIsInsertingToTheLeft = InsertLocation == RunRange.BeginIndex;
				if (bIsInsertingToTheLeft)
				{
					// Insert the new text run to the left of the non-text run
					LeftRun = CreateDefaultTextRun(LineModel.Text, FTextRange(RunRange.BeginIndex, InsertLocation));
				
					RightRun = Run;
					RightRun->SetTextRange(FTextRange(InsertLocationEnd, RunRange.EndIndex + NewRunText.Len()));
				}
				else
				{
					// Insert the new text run to the right of the non-text run
					LeftRun = Run;

					RightRun = CreateDefaultTextRun(LineModel.Text, FTextRange(InsertLocationEnd, RunRange.EndIndex + NewRunText.Len()));
				}
			}

			InRun->Move(LineModel.Text, FTextRange(InsertLocation, InsertLocationEnd));

			// Remove the old run (it may get re-added again as the right hand run)
			LineModel.Runs.RemoveAt(RunIndex--);

			// Insert the new runs at the correct place, and then skip over these new array entries
			const bool LeftRunHasText = !LeftRun->GetTextRange().IsEmpty();
			const bool RightRunHasText = !RightRun->GetTextRange().IsEmpty();
			if (LeftRunHasText)
			{
				LineModel.Runs.Insert(LeftRun.ToSharedRef(), ++RunIndex);
			}
			LineModel.Runs.Insert(InRun, ++RunIndex);
			if (RightRunHasText || bAlwaysKeepRightRun)
			{
				LineModel.Runs.Insert(RightRun.ToSharedRef(), ++RunIndex);
			}
		}
		else if (RunIsAfterInsertLocation)
		{
			FTextRange NewRange = RunRange;
			NewRange.Offset(NewRunText.Len());
			Run->SetTextRange(NewRange);
		}
	}

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

bool FTextLayout::JoinLineWithNextLine(int32 LineIndex)
{
	if (!LineModels.IsValidIndex(LineIndex) || !LineModels.IsValidIndex(LineIndex + 1))
	{
		return false;
	}

	//Grab both lines we are supposed to join
	FLineModel& LineModel = LineModels[LineIndex];
	FLineModel& NextLineModel = LineModels[LineIndex + 1];

	//If the next line is empty we'll just remove it
	if (NextLineModel.Text->Len() == 0)
	{
		return RemoveLine(LineIndex + 1);
	}

	const int32 LineLength = LineModel.Text->Len();

	//Append the next line to the current line
	LineModel.Text->InsertAt(LineLength, NextLineModel.Text.Get());

	//Clear the current lines wrapping information
	LineModel.HasWrappingInformation = false;

	//Iterate through all of the next lines runs and bring them over to the current line
	for (int32 RunIndex = 0; RunIndex < NextLineModel.Runs.Num(); RunIndex++)
	{
		const TSharedRef<IRun> Run = NextLineModel.Runs[RunIndex].GetRun();
		FTextRange NewRange = Run->GetTextRange();

		if (!NewRange.IsEmpty())
		{
			NewRange.Offset(LineLength);

			Run->Move(LineModel.Text, NewRange);
			LineModel.Runs.Add(FRunModel(Run));
		}
	}

	//Remove the next line from the list of line models
	LineModels.RemoveAt(LineIndex + 1);

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

bool FTextLayout::SplitLineAt(const FTextLocation& Location)
{
	int32 BreakLocation = Location.GetOffset();
	int32 LineIndex = Location.GetLineIndex();

	if (!LineModels.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineModel& LineModel = LineModels[LineIndex];

	FLineModel LeftLineModel(MakeShareable(new FString(BreakLocation, **LineModel.Text)));
	FLineModel RightLineModel(MakeShareable(new FString(LineModel.Text->Len() - BreakLocation, **LineModel.Text + BreakLocation)));

	check(LeftLineModel.Text->Len() == BreakLocation);

	bool RunIsToTheLeftOfTheBreakLocation = true;
	for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
	{
		const TSharedRef<IRun> Run = LineModel.Runs[RunIndex].GetRun();
		const FTextRange& RunRange = Run->GetTextRange();
		
		const bool bIsLastRun = RunIndex == LineModel.Runs.Num() - 1;
		if (RunRange.Contains(BreakLocation) || (bIsLastRun && RunIsToTheLeftOfTheBreakLocation))
		{
			check(RunIsToTheLeftOfTheBreakLocation == true);
			RunIsToTheLeftOfTheBreakLocation = false;

			TSharedPtr<IRun> LeftRun;
			TSharedPtr<IRun> RightRun;
			if ((Run->GetRunAttributes() & ERunAttributes::SupportsText) != ERunAttributes::None)
			{
				LeftRun = Run->Clone();
				LeftRun->Move(LeftLineModel.Text, FTextRange(RunRange.BeginIndex, LeftLineModel.Text->Len()));
				
				RightRun = Run;
				RightRun->Move(RightLineModel.Text, FTextRange(0, RunRange.EndIndex - LeftLineModel.Text->Len()));
			}
			else
			{
				// Non-text runs are supposed to have a single dummy character in them
				check(RunRange.Len() == 1);

				// This run doesn't support text, so we need to insert a new text run before or after the current run depending on the insertion point
				const bool bIsInsertingToTheLeft = BreakLocation == RunRange.BeginIndex;
				if (bIsInsertingToTheLeft)
				{
					// Insert the new text run to the left of the non-text run
					LeftRun = CreateDefaultTextRun(LeftLineModel.Text, FTextRange(RunRange.BeginIndex, LeftLineModel.Text->Len()));
				
					RightRun = Run;
					RightRun->Move(RightLineModel.Text, FTextRange(0, RunRange.EndIndex - LeftLineModel.Text->Len()));
				}
				else
				{
					// Insert the new text run to the right of the non-text run
					LeftRun = Run;
					LeftRun->Move(LeftLineModel.Text, FTextRange(RunRange.BeginIndex, LeftLineModel.Text->Len()));

					RightRun = CreateDefaultTextRun(RightLineModel.Text, FTextRange(0, RunRange.EndIndex - LeftLineModel.Text->Len()));
				}
			}

			LeftLineModel.Runs.Add(FRunModel(LeftRun.ToSharedRef()));
			RightLineModel.Runs.Add(FRunModel(RightRun.ToSharedRef()));
		}
		else if (RunIsToTheLeftOfTheBreakLocation)
		{
			Run->Move(LeftLineModel.Text, RunRange);
			LeftLineModel.Runs.Add(FRunModel(Run));
		}
		else
		{
			// This run is after the break, so adjust the range to match that of RHS of the split (RightLineModel)
			// We can do this by subtracting the LeftLineModel text size, since that's the LHS of the split
			FTextRange NewRange = RunRange;
			NewRange.Offset(-LeftLineModel.Text->Len());

			Run->Move(RightLineModel.Text, NewRange);
			RightLineModel.Runs.Add(FRunModel(Run));
		}
	}

	LineModels.RemoveAt(LineIndex);
	LineModels.Insert(LeftLineModel, LineIndex);
	LineModels.Insert(RightLineModel, LineIndex + 1);

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

bool FTextLayout::RemoveAt( const FTextLocation& Location, int32 Count )
{
	int32 RemoveLocation = Location.GetOffset();
	int32 LineIndex = Location.GetLineIndex();

	if (!LineModels.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineModel& LineModel = LineModels[LineIndex];

	//Make sure we aren't trying to remove more characters then we have
	Count = RemoveLocation + Count > LineModel.Text->Len() ? Count - ((RemoveLocation + Count) - LineModel.Text->Len()) : Count;

	if (Count == 0)
	{
		return false;
	}

	LineModel.Text->RemoveAt(RemoveLocation, Count);
	LineModel.HasWrappingInformation = false;

	const FTextRange RemoveTextRange(RemoveLocation, RemoveLocation + Count);
	for (int32 RunIndex = LineModel.Runs.Num() - 1; RunIndex >= 0; --RunIndex)
	{
		FRunModel& RunModel = LineModel.Runs[RunIndex];
		const FTextRange RunRange = RunModel.GetTextRange();

		const FTextRange IntersectedRangeToRemove = RunRange.Intersect(RemoveTextRange);
		if (IntersectedRangeToRemove.IsEmpty() && RunRange.BeginIndex >= RemoveTextRange.EndIndex)
		{
			// The whole run is contained to the right of the removal range, just adjust its range by the amount of text that was removed
			FTextRange NewRange = RunRange;
			NewRange.Offset(-Count);
			RunModel.SetTextRange(NewRange);
		}
		else if (!IntersectedRangeToRemove.IsEmpty())
		{
			const int32 RunLength = RunRange.Len();
			const int32 IntersectedLength = IntersectedRangeToRemove.Len();
			if (RunLength == IntersectedLength)
			{
				// The text for this entire run has been removed, so remove this run
				LineModel.Runs.RemoveAt(RunIndex);

				// Every line needs at least one run - if we just removed the last run for this line, add a new default text run with a zero range
				if (LineModel.Runs.Num() == 0)
				{
					TSharedRef<IRun> NewTextRun = CreateDefaultTextRun(LineModel.Text, FTextRange(0, 0));
					LineModel.Runs.Add(NewTextRun);
				}
			}
			else if (RunRange.BeginIndex > RemoveTextRange.BeginIndex)
			{
				// Some of this run has been removed, and this run is the right hand part of the removal
				// So we need to adjust the range so that we start at the removal point since the text has been removed from the beginning of this run
				const FTextRange NewRange(RemoveTextRange.BeginIndex, RunRange.EndIndex - Count);
				check(!NewRange.IsEmpty());
				RunModel.SetTextRange(NewRange);
			}
			else
			{
				// Some of this run has been removed, and this run is the left hand part of the removal
				// So we need to adjust the range by the amount of text that has been removed from the end of this run
				const FTextRange NewRange(RunRange.BeginIndex, RunRange.EndIndex - IntersectedLength);
				check(!NewRange.IsEmpty());
				RunModel.SetTextRange(NewRange);
			}

			if (RunRange.BeginIndex <= RemoveTextRange.BeginIndex)
			{
				// break since we don't need to process the runs to the left of the removal point
				break;
			}
		}
		else if(IntersectedRangeToRemove.IsEmpty() && RunRange.IsEmpty() && RemoveTextRange.Contains(RunRange.BeginIndex) && RemoveTextRange.Contains(RunRange.EndIndex))
		{
			// empty run that was inside our removed range, safe to remove
			LineModel.Runs.RemoveAt(RunIndex);
		}
	}

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

bool FTextLayout::RemoveLine(int32 LineIndex)
{
	if (!LineModels.IsValidIndex(LineIndex))
	{
		return false;
	}

	LineModels.RemoveAt(LineIndex);

	DirtyFlags |= EDirtyState::Layout;
	return true;
}

void FTextLayout::AddLine( const TSharedRef< FString >& Text, const TArray< TSharedRef< IRun > >& Runs )
{
	{
		FLineModel LineModel( Text );

		for (int32 Index = 0; Index < Runs.Num(); Index++)
		{
			LineModel.Runs.Add( FRunModel( Runs[ Index ] ) );
		}

		LineModels.Add( LineModel );
	}

	// If our layout is clean, then we can add this new line immediately (and efficiently)
	// If our layout is dirty, then we might as well wait as the next UpdateLayout call will add it
	if (!(DirtyFlags & EDirtyState::Layout))
	{
		const int32 LineModelIndex = LineModels.Num() - 1;
		FLineModel& LineModel = LineModels[LineModelIndex];

		BeginLineLayout(LineModel);

		TArray<TSharedRef<ILayoutBlock>> SoftLine;
		FlowLineLayout(LineModelIndex, GetWrappingDrawWidth(), SoftLine);

		// We need to re-justify all lines, as the new line view(s) added by this line model may have affected everything
		JustifyLayout();

		EndLineLayout(LineModel);
	}
}

void FTextLayout::ClearLines()
{
	LineModels.Empty();
	DirtyFlags |= EDirtyState::Layout;
}

bool FTextLayout::IsEmpty() const
{
	return (LineModels.Num() == 0 || (LineModels.Num() == 1 && LineModels[0].Text->Len() == 0));
}

void FTextLayout::GetAsText(FString& DisplayText, FTextOffsetLocations* const OutTextOffsetLocations) const
{
	GetAsTextAndOffsets(&DisplayText, OutTextOffsetLocations);
}

void FTextLayout::GetAsText(FText& DisplayText, FTextOffsetLocations* const OutTextOffsetLocations) const
{
	FString DisplayString;
	GetAsText(DisplayString, OutTextOffsetLocations);
	DisplayText = FText::FromString(DisplayString);
}

void FTextLayout::GetTextOffsetLocations(FTextOffsetLocations& OutTextOffsetLocations) const
{
	GetAsTextAndOffsets(nullptr, &OutTextOffsetLocations);
}

void FTextLayout::GetAsTextAndOffsets(FString* const OutDisplayText, FTextOffsetLocations* const OutTextOffsetLocations) const
{
	int32 DisplayTextLength = 0;

	if (OutTextOffsetLocations)
	{
		OutTextOffsetLocations->OffsetData.Reserve(LineModels.Num());
	}

	const int32 LineTerminatorLength = FCString::Strlen(LINE_TERMINATOR);

	for (int32 LineModelIndex = 0; LineModelIndex < LineModels.Num(); LineModelIndex++)
	{
		const FLineModel& LineModel = LineModels[LineModelIndex];

		// Append line terminator to the end of the previous line
		if (LineModelIndex > 0)
		{
			if (OutDisplayText)
			{
				*OutDisplayText += LINE_TERMINATOR;
			}
			DisplayTextLength += LineTerminatorLength;
		}

		int32 LineLength = 0;
		for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
		{
			const FRunModel& Run = LineModel.Runs[RunIndex];
			if (OutDisplayText)
			{
				Run.AppendTextTo(*OutDisplayText);
			}
			LineLength += Run.GetTextRange().Len();
		}

		if (OutTextOffsetLocations)
		{
			OutTextOffsetLocations->OffsetData.Add(FTextOffsetLocations::FOffsetEntry(DisplayTextLength, LineLength));
		}

		DisplayTextLength += LineLength;
	}
}

void GetRangeAsTextFromLine(FString& DisplayText, const FTextLayout::FLineModel& LineModel, const FTextRange& Range)
{
	for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
	{
		const FTextLayout::FRunModel& Run = LineModel.Runs[RunIndex];
		const FTextRange& RunRange = Run.GetTextRange();

		const FTextRange IntersectRange = RunRange.Intersect(Range);

		if (!IntersectRange.IsEmpty())
		{
			Run.AppendTextTo(DisplayText, IntersectRange);
		}
		else if (RunRange.BeginIndex > Range.EndIndex)
		{
			//We're past the selection range so we can stop
			break;
		}
	}
}

void FTextLayout::GetSelectionAsText(FString& DisplayText, const FTextSelection& Selection) const
{
	int32 SelectionBeginningLineIndex = Selection.GetBeginning().GetLineIndex();
	int32 SelectionBeginningLineOffset = Selection.GetBeginning().GetOffset();

	int32 SelectionEndLineIndex = Selection.GetEnd().GetLineIndex();
	int32 SelectionEndLineOffset = Selection.GetEnd().GetOffset();

	if (LineModels.IsValidIndex(SelectionBeginningLineIndex) && LineModels.IsValidIndex(SelectionEndLineIndex))
	{
		if (SelectionBeginningLineIndex == SelectionEndLineIndex)
		{
			const FTextRange SelectionRange(SelectionBeginningLineOffset, SelectionEndLineOffset);
			const FLineModel& LineModel = LineModels[SelectionBeginningLineIndex];

			GetRangeAsTextFromLine(DisplayText, LineModel, SelectionRange);
		}
		else
		{
			for (int32 LineIndex = SelectionBeginningLineIndex; LineIndex <= SelectionEndLineIndex; LineIndex++)
			{
				if (LineIndex == SelectionBeginningLineIndex)
				{
					const FLineModel& LineModel = LineModels[SelectionBeginningLineIndex];
					const FTextRange SelectionRange(SelectionBeginningLineOffset, LineModel.Text->Len());

					GetRangeAsTextFromLine(DisplayText, LineModel, SelectionRange);
				}
				else if (LineIndex == SelectionEndLineIndex)
				{
					const FLineModel& LineModel = LineModels[SelectionEndLineIndex];
					const FTextRange SelectionRange(0, SelectionEndLineOffset);

					GetRangeAsTextFromLine(DisplayText, LineModel, SelectionRange);
				}
				else
				{
					const FLineModel& LineModel = LineModels[LineIndex];
					const FTextRange SelectionRange(0, LineModel.Text->Len());

					GetRangeAsTextFromLine(DisplayText, LineModel, SelectionRange);
				}

				if (LineIndex != SelectionEndLineIndex)
				{
					DisplayText += LINE_TERMINATOR;
				}
			}
		}
	}
}

FTextSelection FTextLayout::GetWordAt(const FTextLocation& Location) const
{
	const int32 LineIndex = Location.GetLineIndex();
	const int32 Offset = Location.GetOffset();

	if (!LineModels.IsValidIndex(LineIndex))
	{
		return FTextSelection();
	}

	const FLineModel& LineModel = LineModels[LineIndex];

	WordBreakIterator->SetString(**LineModel.Text);

	int32 PreviousBreak = WordBreakIterator->MoveToCandidateAfter(Offset);
	int32 CurrentBreak = 0;

	while ((CurrentBreak = WordBreakIterator->MoveToPrevious()) != INDEX_NONE)
	{
		bool HasLetter = false;
		for (int32 Index = CurrentBreak; Index < PreviousBreak; Index++)
		{
			if (!FText::IsWhitespace((*LineModel.Text)[Index]))
			{
				HasLetter = true;
				break;
			}
		}

		if (HasLetter)
		{
			break;
		}

		PreviousBreak = CurrentBreak;
	}

	WordBreakIterator->ClearString();

	if (PreviousBreak == CurrentBreak || CurrentBreak == INDEX_NONE)
	{
		return FTextSelection();
	}

	return FTextSelection(FTextLocation(LineIndex, CurrentBreak), FTextLocation(LineIndex, PreviousBreak));
}

void FTextLayout::SetVisibleRegion( const FVector2D& InViewSize, const FVector2D& InScrollOffset )
{
	if (ViewSize != InViewSize)
	{
		ViewSize = InViewSize;

		if (Justification != ETextJustify::Left)
		{
			// If the view size has changed, we may need to update our positions based on our justification
			DirtyFlags |= EDirtyState::Layout;
		}
	}
	
	if (ScrollOffset != InScrollOffset)
	{
		const FVector2D PreviousScrollOffset = ScrollOffset;
		ScrollOffset = InScrollOffset;

		// Use a negative scroll offset since positive scrolling moves things negatively in screen space
		const FVector2D OffsetAdjustment = -(ScrollOffset - PreviousScrollOffset);

		for (FLineView& LineView : LineViews)
		{
			LineView.Offset += OffsetAdjustment;

			for (const TSharedRef< ILayoutBlock >& Block : LineView.Blocks)
			{
				Block->SetLocationOffset( Block->GetLocationOffset() + OffsetAdjustment );
			}
		}
	}
}

void FTextLayout::SetLineBreakIterator( TSharedPtr<IBreakIterator> InLineBreakIterator )
{
	LineBreakIterator = InLineBreakIterator;
	DirtyFlags |= EDirtyState::Layout;

	// Changing the line break iterator will affect the wrapping information for *all lines*
	// Clear out the entire cache so it gets regenerated on the text call to FlowLayout
	ClearWrappingCache();
}

void FTextLayout::SetMargin( const FMargin& InMargin )
{
	if ( Margin == InMargin )
	{
		return;
	}

	const FMargin PreviousMargin = Margin;
	Margin = InMargin;

	if ( WrappingWidth > 0 )
	{
		// Since we are wrapping our text we'll need to rebuild the view as the actual wrapping width includes the margin.
		DirtyFlags |= EDirtyState::Layout;
	}
	else
	{
		FVector2D OffsetAdjustment( Margin.Left - PreviousMargin.Left, Margin.Top - PreviousMargin.Top );
		OffsetAdjustment *= Scale;
		for (int32 LineViewIndex = 0; LineViewIndex < LineViews.Num(); LineViewIndex++)
		{
			FLineView& LineView = LineViews[ LineViewIndex ];

			for (int32 BlockIndex = 0; BlockIndex < LineView.Blocks.Num(); BlockIndex++)
			{
				const TSharedRef< ILayoutBlock >& Block = LineView.Blocks[ BlockIndex ];
				Block->SetLocationOffset( Block->GetLocationOffset() + OffsetAdjustment );
			}
		}

		const float MarginDeltaWidth = ( Margin.GetTotalSpaceAlong<Orient_Horizontal>() - PreviousMargin.GetTotalSpaceAlong<Orient_Horizontal>() ) * Scale;
		const float MarginDeltaHeight = ( Margin.GetTotalSpaceAlong<Orient_Vertical>() - PreviousMargin.GetTotalSpaceAlong<Orient_Vertical>() ) * Scale;
		TextLayoutSize.DrawWidth += MarginDeltaWidth;
		TextLayoutSize.WrappedWidth += MarginDeltaWidth;
		TextLayoutSize.Height += MarginDeltaHeight;
	}
}

FMargin FTextLayout::GetMargin() const
{
	return Margin;
}

void FTextLayout::SetScale( float Value )
{
	if ( Scale == Value )
	{
		return;
	}

	Scale = Value;
	DirtyFlags |= EDirtyState::Layout;

	// Changing the scale will affect the wrapping information for *all lines*
	// Clear out the entire cache so it gets regenerated on the text call to FlowLayout
	ClearWrappingCache();
}

float FTextLayout::GetScale() const
{
	return Scale;
}

void FTextLayout::SetJustification( ETextJustify::Type Value )
{
	if ( Justification == Value )
	{
		return;
	}

	Justification = Value;
	DirtyFlags |= EDirtyState::Layout;
}

ETextJustify::Type FTextLayout::GetJustification() const
{
	return Justification;
}

void FTextLayout::SetLineHeightPercentage( float Value )
{
	if ( LineHeightPercentage != Value )
	{
		LineHeightPercentage = Value; 
		DirtyFlags |= EDirtyState::Layout;
	}
}

float FTextLayout::GetLineHeightPercentage() const
{
	return LineHeightPercentage;
}

void FTextLayout::SetWrappingWidth( float Value )
{
	if ( WrappingWidth != Value )
	{
		WrappingWidth = Value; 
		DirtyFlags |= EDirtyState::Layout;
	}
}

float FTextLayout::GetWrappingWidth() const
{
	return WrappingWidth;
}

FVector2D FTextLayout::GetDrawSize() const
{
	return TextLayoutSize.GetDrawSize();
}

FVector2D FTextLayout::GetWrappedSize() const
{
	return TextLayoutSize.GetWrappedSize() * ( 1 / Scale );
}

FVector2D FTextLayout::GetSize() const
{
	return TextLayoutSize.GetDrawSize() * ( 1 / Scale );
}

const TArray< FTextLayout::FLineModel >& FTextLayout::GetLineModels() const
{
	return LineModels;
}

const TArray< FTextLayout::FLineView >& FTextLayout::GetLineViews() const
{
	return LineViews;
}

FTextLayout::~FTextLayout()
{

}

FTextLayout::FLineModel::FLineModel( const TSharedRef< FString >& InText ) 
	: Text( InText )
	, Runs()
	, BreakCandidates()
	, RunRenderers()
	, HasWrappingInformation( false )
{

}

void FTextLayout::FRunModel::ClearCache()
{
	MeasuredRanges.Empty();
	MeasuredRangeSizes.Empty();
}

void FTextLayout::FRunModel::AppendTextTo(FString& Text) const
{
	Run->AppendTextTo(Text);
}

void FTextLayout::FRunModel::AppendTextTo(FString& Text, const FTextRange& Range) const
{
	Run->AppendTextTo(Text, Range);
}

TSharedRef< ILayoutBlock > FTextLayout::FRunModel::CreateBlock( const FBlockDefinition& BlockDefine, float InScale ) const
{
	const FTextRange& SizeRange = BlockDefine.ActualRange;

	if ( MeasuredRanges.Num() == 0 )
	{
		FTextRange RunRange = Run->GetTextRange();
		return Run->CreateBlock( BlockDefine.ActualRange.BeginIndex, BlockDefine.ActualRange.EndIndex, Run->Measure( SizeRange.BeginIndex, SizeRange.EndIndex, InScale ), BlockDefine.Renderer );
	}

	int32 StartRangeIndex = 0;
	int32 EndRangeIndex = 0;

	if ( MeasuredRanges.Num() > 16 )
	{
		if ( SizeRange.BeginIndex != 0 )
		{
			StartRangeIndex = BinarySearchForBeginIndex( MeasuredRanges, SizeRange.BeginIndex );
			check( StartRangeIndex != INDEX_NONE);
		}

		EndRangeIndex = StartRangeIndex;
		if ( StartRangeIndex != MeasuredRanges.Num() - 1 )
		{
			EndRangeIndex = BinarySearchForEndIndex( MeasuredRanges, StartRangeIndex, SizeRange.EndIndex );
			check( EndRangeIndex != INDEX_NONE);
		}
	}
	else
	{
		const int32 MaxValidIndex = MeasuredRanges.Num() - 1;

		if ( SizeRange.BeginIndex != 0 )
		{
			bool FoundBeginIndexMatch = false;
			for (; StartRangeIndex < MaxValidIndex; StartRangeIndex++)
			{
				FoundBeginIndexMatch = MeasuredRanges[ StartRangeIndex ].BeginIndex >= SizeRange.BeginIndex;
				if ( FoundBeginIndexMatch ) 
				{
					break;
				}
			}
			check( FoundBeginIndexMatch == true || StartRangeIndex == MaxValidIndex );
		}

		EndRangeIndex = StartRangeIndex;
		if ( StartRangeIndex != MaxValidIndex )
		{
			bool FoundEndIndexMatch = false;
			for (; EndRangeIndex < MeasuredRanges.Num(); EndRangeIndex++)
			{
				FoundEndIndexMatch = MeasuredRanges[ EndRangeIndex ].EndIndex >= SizeRange.EndIndex;
				if ( FoundEndIndexMatch ) 
				{
					break;
				}
			}
			check( FoundEndIndexMatch == true || EndRangeIndex == MaxValidIndex );
		}
	}

	FVector2D BlockSize( ForceInitToZero );
	if ( StartRangeIndex == EndRangeIndex )
	{
		if ( MeasuredRanges[ StartRangeIndex ].BeginIndex == SizeRange.BeginIndex && 
			MeasuredRanges[ StartRangeIndex ].EndIndex == SizeRange.EndIndex )
		{
			BlockSize += MeasuredRangeSizes[ StartRangeIndex ];
		}
		else
		{
			BlockSize += Run->Measure( SizeRange.BeginIndex, SizeRange.EndIndex, InScale );
		}
	}
	else
	{
		if ( MeasuredRanges[ StartRangeIndex ].BeginIndex == SizeRange.BeginIndex )
		{
			BlockSize += MeasuredRangeSizes[ StartRangeIndex ];
		}
		else
		{
			BlockSize += Run->Measure( SizeRange.BeginIndex, MeasuredRanges[ StartRangeIndex ].EndIndex, InScale );
		}

		for (int32 Index = StartRangeIndex + 1; Index < EndRangeIndex; Index++)
		{
			BlockSize.X += MeasuredRangeSizes[ Index ].X;
			BlockSize.Y = FMath::Max( MeasuredRangeSizes[ Index ].Y, BlockSize.Y );
		}

		if ( MeasuredRanges[ EndRangeIndex ].EndIndex == SizeRange.EndIndex )
		{
			BlockSize.X += MeasuredRangeSizes[ EndRangeIndex ].X;
			BlockSize.Y = FMath::Max( MeasuredRangeSizes[ EndRangeIndex ].Y, BlockSize.Y );
		}
		else
		{
			FVector2D Size = Run->Measure( MeasuredRanges[ EndRangeIndex ].BeginIndex, SizeRange.EndIndex, InScale );
			BlockSize.X += Size.X;
			BlockSize.Y = FMath::Max( Size.Y, BlockSize.Y );
		}
	}

	return Run->CreateBlock( BlockDefine.ActualRange.BeginIndex, BlockDefine.ActualRange.EndIndex, BlockSize, BlockDefine.Renderer );
}

int32 FTextLayout::FRunModel::BinarySearchForEndIndex( const TArray< FTextRange >& Ranges, int32 RangeBeginIndex, int32 EndIndex )
{
	int32 Min = RangeBeginIndex;
	int32 Mid = 0;
	int32 Max = Ranges.Num() - 1;
	while (Max >= Min)
	{
		Mid = Min + ((Max - Min) / 2);
		if ( Ranges[Mid].EndIndex == EndIndex )
		{
			return Mid; 
		}
		else if ( Ranges[Mid].EndIndex < EndIndex )
		{
			Min = Mid + 1;
		}
		else
		{
			Max = Mid - 1;
		}
	}

	return Mid;
}

int32 FTextLayout::FRunModel::BinarySearchForBeginIndex( const TArray< FTextRange >& Ranges, int32 BeginIndex )
{
	int32 Min = 0;
	int32 Mid = 0;
	int32 Max = Ranges.Num() - 1;
	while (Max >= Min)
	{
		Mid = Min + ((Max - Min) / 2);
		if ( Ranges[Mid].BeginIndex == BeginIndex )
		{
			return Mid; 
		}
		else if ( Ranges[Mid].BeginIndex < BeginIndex )
		{
			Min = Mid + 1;
		}
		else
		{
			Max = Mid - 1;
		}
	}

	return Mid;
}

uint8 FTextLayout::FRunModel::GetKerning(int32 CurrentIndex, float InScale)
{
	return Run->GetKerning(CurrentIndex, InScale);
}

FVector2D FTextLayout::FRunModel::Measure(int32 BeginIndex, int32 EndIndex, float InScale)
{
	FVector2D Size = Run->Measure(BeginIndex, EndIndex, InScale);

	MeasuredRanges.Add( FTextRange( BeginIndex, EndIndex ) );
	MeasuredRangeSizes.Add( Size );

	return Size;
}

int16 FTextLayout::FRunModel::GetMaxHeight(float InScale) const
{
	return Run->GetMaxHeight(InScale);
}

int16 FTextLayout::FRunModel::GetBaseLine( float InScale ) const
{
	return Run->GetBaseLine(InScale);
}

FTextRange FTextLayout::FRunModel::GetTextRange() const
{
	return Run->GetTextRange();
}

void FTextLayout::FRunModel::SetTextRange( const FTextRange& Value )
{
	Run->SetTextRange( Value );
}

void FTextLayout::FRunModel::EndLayout()
{
	Run->EndLayout();
}

void FTextLayout::FRunModel::BeginLayout()
{
	Run->BeginLayout();
}

TSharedRef< IRun > FTextLayout::FRunModel::GetRun() const
{
	return Run;
}

FTextLayout::FRunModel::FRunModel( const TSharedRef< IRun >& InRun ) : Run( InRun )
{

}

int32 FTextLayout::FTextOffsetLocations::TextLocationToOffset(const FTextLocation& InLocation) const
{
	const int32 LineIndex = InLocation.GetLineIndex();
	if(LineIndex >= 0 && LineIndex < OffsetData.Num())
	{
		const FOffsetEntry& OffsetEntry = OffsetData[LineIndex];
		return OffsetEntry.FlatStringIndex + InLocation.GetOffset();
	}
	return INDEX_NONE;
}

FTextLocation FTextLayout::FTextOffsetLocations::OffsetToTextLocation(const int32 InOffset) const
{
	for(int32 OffsetEntryIndex = 0; OffsetEntryIndex < OffsetData.Num(); ++OffsetEntryIndex)
	{
		const FOffsetEntry& OffsetEntry = OffsetData[OffsetEntryIndex];
		const FTextRange FlatLineRange(OffsetEntry.FlatStringIndex, OffsetEntry.FlatStringIndex + OffsetEntry.DocumentLineLength);
		if(FlatLineRange.InclusiveContains(InOffset))
		{
			const int32 OffsetWithinLine = InOffset - OffsetEntry.FlatStringIndex;
			return FTextLocation(OffsetEntryIndex, OffsetWithinLine);
		}
	}
	return FTextLocation();
}

int32 FTextLayout::FTextOffsetLocations::GetTextLength() const
{
	if(OffsetData.Num())
	{
		const FOffsetEntry& OffsetEntry = OffsetData.Last();
		return OffsetEntry.FlatStringIndex + OffsetEntry.DocumentLineLength;
	}
	return 0;
}