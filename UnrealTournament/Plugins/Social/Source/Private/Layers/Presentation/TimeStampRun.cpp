// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "TimeStampRun.h"

#include "SlateWidgetRun.h"

TSharedRef< FTimeStampRun > FTimeStampRun::Create(const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTimeStampRunInfo& InWidgetInfo,  const TSharedRef<FChatItemTransparency>& InChatTransparency)
{
	TSharedRef<FTimeStampRun> TimeStampRun = MakeShareable(new FTimeStampRun(TextLayout, InRunInfo, InText, InWidgetInfo, InChatTransparency));
	TimeStampRun->Initialize();
	return TimeStampRun;
}

FTimeStampRun::~FTimeStampRun()
{
	if (TickerHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

FTextRange FTimeStampRun::GetTextRange() const 
{
	return Range;
}

void FTimeStampRun::SetTextRange( const FTextRange& Value )
{
	Range = Value;
}

int16 FTimeStampRun::GetBaseLine( float Scale ) const 
{
	return Info.Baseline * Scale;
}

int16 FTimeStampRun::GetMaxHeight( float Scale ) const 
{
	return Info.Size.Get( TimeStampWidget->GetDesiredSize() ).Y * Scale;
}

FVector2D FTimeStampRun::Measure( int32 StartIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext ) const 
{
	return Info.Size.Get( TimeStampWidget->GetDesiredSize() ) * Scale;
}

int8 FTimeStampRun::GetKerning( int32 CurrentIndex, float Scale, const FRunTextContext& TextContext ) const 
{
	return 0;
}

TSharedRef< ILayoutBlock > FTimeStampRun::CreateBlock( int32 StartIndex, int32 EndIndex, FVector2D Size, const FLayoutBlockTextContext& TextContext, const TSharedPtr< IRunRenderer >& Renderer )
{
	return FDefaultLayoutBlock::Create( SharedThis( this ), FTextRange( StartIndex, EndIndex ), Size, TextContext, Renderer );
}

int32 FTimeStampRun::OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const 
{
	FWidgetStyle BlendedWidgetStyle = InWidgetStyle;
	BlendedWidgetStyle.BlendColorAndOpacityTint(FLinearColor(1,1,1,ChatTransparency->GetTransparency()));

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	const FVector2D DesiredWidgetSize = TimeStampWidget->GetDesiredSize();
	if ( (DesiredWidgetSize != WidgetSize ) || ForceRedraw)
	{
		WidgetSize = DesiredWidgetSize;

		const TSharedPtr<FTextLayout> TextLayoutPtr = TextLayout.Pin();
		if (TextLayoutPtr.IsValid())
		{
			TextLayoutPtr->DirtyRunLayout(SharedThis(this));
		}
		ForceRedraw = false;
	}

	const FGeometry WidgetGeometry = AllottedGeometry.MakeChild(TransformVector(InverseScale, Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, Block->GetLocationOffset())));
	return TimeStampWidget->Paint( Args, WidgetGeometry, MyClippingRect, OutDrawElements, LayerId, BlendedWidgetStyle, bParentEnabled );
}

const TArray< TSharedRef<SWidget> >& FTimeStampRun::GetChildren()
{
	return Children;
}

void FTimeStampRun::ArrangeChildren( const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	ArrangedChildren.AddWidget(
		AllottedGeometry.MakeChild(TimeStampWidget.ToSharedRef(), TransformVector(InverseScale, Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, Block->GetLocationOffset())))
		);
}

int32 FTimeStampRun::GetTextIndexAt( const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint ) const
{
	// A widget should always contain a single character (a breaking space)
	check(Range.Len() == 1);

	const FVector2D& BlockOffset = Block->GetLocationOffset();
	const FVector2D& BlockSize = Block->GetSize();

	const float Left = BlockOffset.X;
	const float Top = BlockOffset.Y;
	const float Right = BlockOffset.X + BlockSize.X;
	const float Bottom = BlockOffset.Y + BlockSize.Y;

	const bool ContainsPoint = Location.X >= Left && Location.X < Right && Location.Y >= Top && Location.Y < Bottom;

	if ( !ContainsPoint )
	{
		return INDEX_NONE;
	}

	const FVector2D ScaledWidgetSize = TimeStampWidget->GetDesiredSize() * Scale;
	const int32 Index = (Location.X <= (Left + (ScaledWidgetSize.X * 0.5f))) ? Range.BeginIndex : Range.EndIndex;
	
	if (OutHitPoint)
	{
		const FTextRange BlockRange = Block->GetTextRange();
		*OutHitPoint = (Index == BlockRange.EndIndex) ? ETextHitPoint::RightGutter : ETextHitPoint::WithinText;
	}
	return Index;
}

FVector2D FTimeStampRun::GetLocationAt( const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale ) const
{
	return Block->GetLocationOffset();
}

void FTimeStampRun::Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange)
{
	Text = NewText;
	Range = NewRange;
}

TSharedRef<IRun> FTimeStampRun::Clone() const
{
	return FTimeStampRun::Create(TextLayout.Pin().ToSharedRef(), RunInfo, Text, Info, ChatTransparency);
}

void FTimeStampRun::AppendTextTo(FString& AppendToText) const
{
	AppendToText.Append(**Text + Range.BeginIndex, Range.Len());
}

void FTimeStampRun::AppendTextTo(FString& AppendToText, const FTextRange& PartialRange) const
{
	check(Range.BeginIndex <= PartialRange.BeginIndex);
	check(Range.EndIndex >= PartialRange.EndIndex);

	AppendToText.Append(**Text + PartialRange.BeginIndex, PartialRange.Len());
}

const FRunInfo& FTimeStampRun::GetRunInfo() const
{
	return RunInfo;
}

ERunAttributes FTimeStampRun::GetRunAttributes() const
{
	return ERunAttributes::None;
}

bool FTimeStampRun::UpdateTimeStamp(float Delay)
{
	bool ContinueTicking = true;

	FDateTime Now = FDateTime::Now();

	if ((Now - TimeStamp).GetDuration() <= FTimespan::FromMinutes(1.0))
	{
		static FText NowTimeStamp = NSLOCTEXT("FTimeStampRun", "now_TimeStamp", "now");
		MessageTimeAsText = NowTimeStamp;
	}
	else if ((Now - TimeStamp).GetDuration() <= FTimespan::FromMinutes(2.0))
	{
		static FText OneMinuteTimeStamp = NSLOCTEXT("FTimeStampRun", "1_Minute_TimeStamp", "1 min");
		MessageTimeAsText = OneMinuteTimeStamp;
	}
	else if ((Now - TimeStamp).GetDuration() <= FTimespan::FromMinutes(3.0))
	{
		static FText TwoMinuteTimeStamp = NSLOCTEXT("FTimeStampRun", "2_Minute_TimeStamp", "2 min");
		MessageTimeAsText = TwoMinuteTimeStamp;
	}
	else if ((Now - TimeStamp).GetDuration() <= FTimespan::FromMinutes(4.0))
	{
		static FText ThreeMinuteTimeStamp = NSLOCTEXT("FTimeStampRun", "3_Minute_TimeStamp", "3 min");
		MessageTimeAsText = ThreeMinuteTimeStamp;
	}
	else if ((Now - TimeStamp).GetDuration() <= FTimespan::FromMinutes(5.0))
	{
		static FText FourMinuteTimeStamp = NSLOCTEXT("FTimeStampRun", "4_Minute_TimeStamp", "4 min");
		MessageTimeAsText = FourMinuteTimeStamp;
	}
	else if ((Now - TimeStamp).GetDuration() <= FTimespan::FromMinutes(6.0))
	{
		static FText FiveMinuteTimeStamp = NSLOCTEXT("FTimeStampRun", "5_Minute_TimeStamp", "5 min");
		MessageTimeAsText = FiveMinuteTimeStamp;
	}
	else
	{
		MessageTimeAsText = FText::AsTime(TimeStamp, EDateTimeStyle::Short, "GMT");
		ContinueTicking = false;
	}

	ForceRedraw = true;

	return ContinueTicking;
}

void FTimeStampRun::Initialize()
{
	TimeStampWidget = SNew(STextBlock)
		.TextStyle(&Info.TextBoxStyle)
		.Text(this, &FTimeStampRun::GetMessageTimeText);

	if (UpdateTimeStamp(60)) //this will broadcast a changed event
	{
		TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FTimeStampRun::UpdateTimeStamp), 60);
	}

	TimeStampWidget->SlatePrepass();
	WidgetSize = TimeStampWidget->GetDesiredSize();
	Children.Add(TimeStampWidget.ToSharedRef());
}


FTimeStampRun::FTimeStampRun(const TSharedRef<class FTextLayout>& InTextLayout,
			const FRunInfo& InRunInfo,
			const TSharedRef< const FString >& InText, 
			const FTimeStampRunInfo& InWidgetInfo,
			const TSharedRef<FChatItemTransparency>& InChatTransparency)
	: TextLayout(InTextLayout)
	, RunInfo( InRunInfo )
	, Text( InText )
	, Range( 0, 1 )
	, Info( InWidgetInfo )
	, Children()
	, TimeStamp(InWidgetInfo.TimeStamp)
	, ChatTransparency(InChatTransparency)
	, ForceRedraw(false)
	, WidgetSize()
{
}
