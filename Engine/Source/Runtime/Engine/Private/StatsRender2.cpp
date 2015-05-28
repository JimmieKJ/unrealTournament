// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This file contains the rendering functions used in the stats code
 */

#include "EnginePrivate.h"

#if STATS

#include "StatsData.h"

/** Stats rendering constants. */
enum class EStatRenderConsts
{
	NUM_COLUMNS = 5,
};

/** Enumerates stat font types and maximum length of the stat names. */
enum class EStatFontTypes : int32
{
	/** Tiny font, used when ViewRectX < 1280. */
	Tiny = 0,
	/** Small font. */
	Small = 1,
	/** Number of fonts. */
	NumFonts,
};

/** Contains misc stat font properties. */
struct FStatFont
{
	/** Default constructor. */
	FStatFont( int32 InMaxDisplayedChars, int32 InFontHeight, int32 InFontHeightOffsets ) :
		MaxDisplayedChars( InMaxDisplayedChars ),
		FontHeight( InFontHeight ),
		FontHeightOffset( InFontHeightOffsets )
	{}

	/** Maximum length of the displayed stat names. */
	const int32 MaxDisplayedChars;

	/** Font height, manually selected to fix more stats on screen. */
	const int32 FontHeight;

	/** Y offset of the background tile, manually selected to fix more stats on screen. */
	const int32 FontHeightOffset;
};

static FStatFont GStatFonts[(int32)EStatFontTypes::NumFonts] =
{
	/** Tiny. */
	FStatFont( 36, 10, 1 ),
	/** Small. */
	FStatFont( 72, 12, 2 ),
};

void FromString( EStatFontTypes& OutValue, const TCHAR* Buffer )
{
	OutValue = EStatFontTypes::Small;

	if( FCString::Stricmp( Buffer, TEXT( "Tiny" ) ) == 0 )
	{
		OutValue = EStatFontTypes::Tiny;
	}
}

/** Holds various parameters used for rendering stats. */
struct FStatRenderGlobals
{
	/** Rendering offset for first column from stat label. */
	int32 AfterNameColumnOffset;

	/** Rendering offsets for additional columns. */
	int32 InterColumnOffset;

	/** Color for rendering stats. */
	FLinearColor StatColor;

	/** Color to use when rendering headings. */
	FLinearColor HeadingColor;

	/** Color to use when rendering group names. */
	FLinearColor GroupColor;

	/** Color used as the background for every other stat item to make it easier to read across lines. */
	FLinearColor BackgroundColor;

	/** The font used for rendering stats. */
	UFont* StatFont;

	/** Current size of the viewport, used to detect resolution changes. */
	FIntPoint SizeXY;

	/** Current stat font type. */
	EStatFontTypes StatFontType;

	/** Whether we need update internals. */
	bool bNeedRefresh;

	FStatRenderGlobals() :
		AfterNameColumnOffset(0),
		InterColumnOffset(0),
		StatColor(0.f,1.f,0.f),
		HeadingColor(1.f,0.2f,0.f),
		GroupColor(FLinearColor::White),
		BackgroundColor(0.05f, 0.05f, 0.05f, 0.90f),	// dark gray mostly occluding the background
		StatFont(nullptr),
		StatFontType(EStatFontTypes::NumFonts),
		bNeedRefresh(true)
	{
		SetNewFont( EStatFontTypes::Small );
	}

	/**
	 * Initializes stat render globals.
	 */
	void Initialize( const FIntPoint NewSizeXY )
	{
		if( SizeXY != NewSizeXY )
		{
			SizeXY = NewSizeXY;
			bNeedRefresh = true;
		}

		if( SizeXY.X < 1280 )
		{
			SetNewFont( EStatFontTypes::Tiny );
		}

		if( bNeedRefresh )
		{
			// This is the number of W characters to leave spacing for in the stat name column.
			const FString STATNAME_COLUMN_WIDTH = FString::ChrN( GetNumCharsForStatName(), 'S' );

			// This is the number of W characters to leave spacing for in the other stat columns.
			const FString STATDATA_COLUMN_WIDTH = FString::ChrN( StatFontType == EStatFontTypes::Small ? 8 : 7, 'W' );

			// Get the size of the spaces, since we can't render the width calculation strings.
			int32 StatColumnSpaceSizeY = 0;
			int32 TimeColumnSpaceSizeY = 0;

			// @TODO yrx 2015-04-17 Compute on the stats thread to get the exact measurement
			// Determine where the first column goes.
			StringSize(StatFont, AfterNameColumnOffset, StatColumnSpaceSizeY, *STATNAME_COLUMN_WIDTH);

			// Determine the width of subsequent columns.
			StringSize(StatFont, InterColumnOffset, TimeColumnSpaceSizeY, *STATDATA_COLUMN_WIDTH);

			bNeedRefresh = false;
		}
	}
	
	/**
	 * @return number of characters use to render the stat name
	 */
	int32 GetNumCharsForStatName() const
	{
		return GStatFonts[(int32)StatFontType].MaxDisplayedChars;
	}
	
	/**
	 * @return number of characters use to render the stat name
	 */
	int32 GetFontHeight() const
	{
		return GStatFonts[( int32 )StatFontType].FontHeight;
	}

	/**
	 * @return Y offset of the background tile, so this will align with the text
	 */
	int32 GetYOffset() const
	{
		return GStatFonts[(int32)StatFontType].FontHeightOffset;
	}

	/** Sets a new font. */
	void SetNewFont( EStatFontTypes NewFontType )
	{
		if( StatFontType != NewFontType )
		{
			StatFontType = NewFontType;
			if( StatFontType == EStatFontTypes::Tiny )
			{
				StatFont = GEngine->GetTinyFont();
			}
			else if( StatFontType == EStatFontTypes::Small )
			{
				StatFont = GEngine->GetSmallFont();
			}
			bNeedRefresh = true;
		}
	}
};

FStatRenderGlobals& GetStatRenderGlobals()
{
	// Sanity checks.
	check( IsInGameThread() );
	check( GEngine );
	static FStatRenderGlobals Singleton;
	return Singleton;
}

/** Shorten a name for stats display. */
static FString ShortenName( TCHAR const* LongName )
{
	FString Result( LongName );
	const int32 Limit = GetStatRenderGlobals().GetNumCharsForStatName();
	if( Result.Len() > Limit )
	{
		Result = FString( TEXT( "..." ) ) + Result.Right( Limit );
	}
	return Result;
}

/** Exec used to execute engine stats command on the game thread. */
static class FStatCmdEngine : private FSelfRegisteringExec
{
public:
	/** Console commands. */
	virtual bool Exec( UWorld*, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if( FParse::Command( &Cmd, TEXT( "stat" ) ) )
		{
			if( FParse::Command( &Cmd, TEXT( "display" ) ) )
			{
				TParsedValueWithDefault<EStatFontTypes> Font( Cmd, TEXT( "font=" ), EStatFontTypes::Small );
				GetStatRenderGlobals().SetNewFont( Font.Get() );
				return true;
			}
		}
		return false;
	}
}
StatCmdEngineExec;

static void RightJustify(FCanvas* Canvas, int32 X, int32 Y, TCHAR const* Text, FLinearColor const& Color)
{
	int32 StatColumnSpaceSizeX, StatColumnSpaceSizeY;
	StringSize(GetStatRenderGlobals().StatFont, StatColumnSpaceSizeX, StatColumnSpaceSizeY, Text);

	Canvas->DrawShadowedString(X + GetStatRenderGlobals().InterColumnOffset - StatColumnSpaceSizeX,Y,Text,GetStatRenderGlobals().StatFont,Color);
}


/**
 *
 * @param Item the stat to render
 * @param Canvas the render interface to draw with
 * @param X the X location to start drawing at
 * @param Y the Y location to start drawing at
 * @param Indent Indentation of this cycles, used when rendering hierarchy
 * @param bStackStat If false, this is a non-stack cycle counter, don't render the call count column
 */
static int32 RenderCycle( const FComplexStatMessage& Item, class FCanvas* Canvas, int32 X, int32 Y, const int32 Indent, const bool bStackStat )
{
	FColor Color = GetStatRenderGlobals().StatColor;	

	check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));

	const bool bIsInitialized = Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64;

	const int32 IndentWidth = Indent*8;

	if( bIsInitialized )
	{
		const float InMs = FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve));
		// Color will be determined by the average value of history
		// If show inclusive and and show exclusive is on, then it will choose color based on inclusive average
		// @TODO yrx 2014-08-21 This is slow, fix this.
		FString CounterName = Item.GetShortName().ToString();
		CounterName.RemoveFromStart(TEXT("STAT_"));
		GEngine->GetStatValueColoration(CounterName, InMs, Color);

		const float MaxMeter = 33.3f; // the time of a "full bar" in ms
		const int32 MeterWidth = GetStatRenderGlobals().AfterNameColumnOffset;

		int32 BarWidth = int32((InMs / MaxMeter) * MeterWidth);
		if (BarWidth > 2)
		{
			if (BarWidth > MeterWidth ) 
			{
				BarWidth = MeterWidth;
			}

			FCanvasBoxItem BoxItem( FVector2D(X + MeterWidth - BarWidth, Y + .4f * GetStatRenderGlobals().GetFontHeight()), FVector2D(BarWidth, 0.2f * GetStatRenderGlobals().GetFontHeight()) );
			BoxItem.SetColor( FLinearColor::Red );
			BoxItem.Draw( Canvas );		
		}
	}

	// @TODO yrx 2015-04-17 Move to the stats thread to avoid expensive computation on the game thread.
	const FString StatDesc = Item.GetDescription();
	const FString StatDisplay = StatDesc.Len() == 0 ? Item.GetShortName().GetPlainNameString() : StatDesc;

	Canvas->DrawShadowedString(X + IndentWidth, Y, *ShortenName(*StatDisplay), GetStatRenderGlobals().StatFont, Color);

	int32 CurrX = X + GetStatRenderGlobals().AfterNameColumnOffset;
	// Now append the call count
	if( bStackStat )
	{
		if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration) && bIsInitialized)
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%u"), Item.GetValue_CallCount(EComplexStatField::IncAve)),Color);
		}
		CurrX += GetStatRenderGlobals().InterColumnOffset;
	}

	// Add the two inclusive columns if asked
	if( bIsInitialized )
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve))),Color); 
	}
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	if( bIsInitialized )
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncMax))),Color);
		
	}
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	if( bStackStat )
	{
		// And the exclusive if asked
		if( bIsInitialized )
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcAve))),Color);
		}
		CurrX += GetStatRenderGlobals().InterColumnOffset;

		if( bIsInitialized )
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcMax))),Color);
		}
		CurrX += GetStatRenderGlobals().InterColumnOffset;
	}
	return GetStatRenderGlobals().GetFontHeight();
}

/**
 * Renders the headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
static int32 RenderGroupedHeadings(class FCanvas* Canvas,int X,int32 Y,const bool bIsHierarchy)
{
	// The heading looks like:
	// Stat [32chars]    CallCount [8chars]    IncAvg [8chars]    IncMax [8chars]    ExcAvg [8chars]    ExcMax [8chars]

	static const TCHAR* CaptionFlat = TEXT("Cycle counters (flat)");
	static const TCHAR* CaptionHier = TEXT("Cycle counters (hierarchy)");

	Canvas->DrawShadowedString(X,Y,bIsHierarchy?CaptionHier:CaptionFlat,GetStatRenderGlobals().StatFont,GetStatRenderGlobals().HeadingColor);

	int32 CurrX = X + GetStatRenderGlobals().AfterNameColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("CallCount"),GetStatRenderGlobals().HeadingColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	RightJustify(Canvas,CurrX,Y,TEXT("InclusiveAvg"),GetStatRenderGlobals().HeadingColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("InclusiveMax"),GetStatRenderGlobals().HeadingColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	RightJustify(Canvas,CurrX,Y,TEXT("ExclusiveAvg"),GetStatRenderGlobals().HeadingColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("ExclusiveMax"),GetStatRenderGlobals().HeadingColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	return GetStatRenderGlobals().GetFontHeight() + (GetStatRenderGlobals().GetFontHeight() / 3);
}

/**
 * Renders the counter headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
static int32 RenderCounterHeadings(class FCanvas* Canvas,int32 X,int32 Y)
{
	// The heading looks like:
	// Stat [32chars]    Value [8chars]    Average [8chars]

	Canvas->DrawShadowedString(X,Y,TEXT("Counters"),GetStatRenderGlobals().StatFont,GetStatRenderGlobals().HeadingColor);

	// Determine where the first column goes
	int32 CurrX = X + GetStatRenderGlobals().AfterNameColumnOffset;

	// Draw the average column label.
	RightJustify(Canvas,CurrX,Y,TEXT("Average"),GetStatRenderGlobals().HeadingColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	// Draw the max column label.
	RightJustify(Canvas,CurrX,Y,TEXT("Max"),GetStatRenderGlobals().HeadingColor);
	return GetStatRenderGlobals().GetFontHeight() + (GetStatRenderGlobals().GetFontHeight() / 3);
}

/**
 * Renders the memory headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
static int32 RenderMemoryHeadings(class FCanvas* Canvas,int32 X,int32 Y)
{
	// The heading looks like:
	// Stat [32chars]    MemUsed [8chars]    PhysMem [8chars]

	Canvas->DrawShadowedString(X,Y,TEXT("Memory Counters"),GetStatRenderGlobals().StatFont,GetStatRenderGlobals().HeadingColor);

	// Determine where the first column goes
	int32 CurrX = X + GetStatRenderGlobals().AfterNameColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("MemUsedMax"),GetStatRenderGlobals().HeadingColor);

	CurrX += GetStatRenderGlobals().InterColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("MemUsedMax%"),GetStatRenderGlobals().HeadingColor);

	CurrX += GetStatRenderGlobals().InterColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("MemPool"),GetStatRenderGlobals().HeadingColor);

	CurrX += GetStatRenderGlobals().InterColumnOffset;
	RightJustify(Canvas,CurrX,Y,TEXT("Pool Capacity"),GetStatRenderGlobals().HeadingColor);

	return GetStatRenderGlobals().GetFontHeight() + (GetStatRenderGlobals().GetFontHeight() / 3);
}

// @param bAutoType true: automatically choose GB/MB/KB/... false: always use MB for easier comparisons
static FString GetMemoryString(double Value, bool bAutoType = true)
{
	if(bAutoType)
	{
		if (Value > 1024.0 * 1024.0 * 1024.0)
		{
			return FString::Printf(TEXT("%.2f GB"), float(Value / (1024.0 * 1024.0 * 1024.0)));
		}
		if (Value > 1024.0 * 1024.0)
		{
			return FString::Printf(TEXT("%.2f MB"), float(Value / (1024.0 * 1024.0)));
		}
		if (Value > 1024.0)
		{
			return FString::Printf(TEXT("%.2f KB"), float(Value / (1024.0)));
		}
		return FString::Printf(TEXT("%.2f B"), float(Value));
	}

	return FString::Printf(TEXT("%.2f MB"), float(Value / (1024.0 * 1024.0)));
}

static int32 RenderMemoryCounter(const FGameThreadHudData& ViewData, const FComplexStatMessage& All,class FCanvas* Canvas,int32 X,int32 Y)
{
	FPlatformMemory::EMemoryCounterRegion Region = FPlatformMemory::EMemoryCounterRegion(All.NameAndInfo.GetField<EMemoryRegion>());
	// At this moment we only have memory stats that are marked as non frame stats, so can't be cleared every frame.
	//const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);
	
	const float MaxMemUsed = All.GetValue_double(EComplexStatField::IncMax);

	// Draw the label
	Canvas->DrawShadowedString(X,Y,*ShortenName(*All.GetDescription()),GetStatRenderGlobals().StatFont,GetStatRenderGlobals().StatColor);
	int32 CurrX = X + GetStatRenderGlobals().AfterNameColumnOffset;

	// always use MB for easier comparisons
	const bool bAutoType = false;

	// Now append the max value of the stat
	RightJustify(Canvas,CurrX,Y,*GetMemoryString(MaxMemUsed, bAutoType),GetStatRenderGlobals().StatColor);
	CurrX += GetStatRenderGlobals().InterColumnOffset;
	if (ViewData.PoolCapacity.Contains(Region))
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%.0f%%"), float(100.0 * MaxMemUsed / double(ViewData.PoolCapacity[Region]))),GetStatRenderGlobals().StatColor);
	}
	CurrX += GetStatRenderGlobals().InterColumnOffset;
	if (ViewData.PoolAbbreviation.Contains(Region))
	{
		RightJustify(Canvas,CurrX,Y,*ViewData.PoolAbbreviation[Region],GetStatRenderGlobals().StatColor);
	}
	CurrX += GetStatRenderGlobals().InterColumnOffset;
	if (ViewData.PoolCapacity.Contains(Region))
	{
		RightJustify(Canvas,CurrX,Y,*GetMemoryString(double(ViewData.PoolCapacity[Region]), bAutoType),GetStatRenderGlobals().StatColor);
	}
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	return GetStatRenderGlobals().GetFontHeight();
}

static int32 RenderCounter(const FGameThreadHudData& ViewData, const FComplexStatMessage& All,class FCanvas* Canvas,int32 X,int32 Y)
{
	// If this is a cycle, render it as a cycle. This is a special case for manually set cycle counters.
	const bool bIsCycle = All.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle);
	if( bIsCycle )
	{
		return RenderCycle( All, Canvas, X, Y, 0, false );
	}

	const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);

	// Draw the label
	Canvas->DrawShadowedString(X,Y,*ShortenName(*All.GetDescription()),GetStatRenderGlobals().StatFont,GetStatRenderGlobals().StatColor);
	int32 CurrX = X + GetStatRenderGlobals().AfterNameColumnOffset;

	if( bDisplayAll )
	{
		// Append the average.
		if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f"), All.GetValue_double(EComplexStatField::IncAve)),GetStatRenderGlobals().StatColor);
		}
		else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%lld"), All.GetValue_int64(EComplexStatField::IncAve)),GetStatRenderGlobals().StatColor);
		}
	}
	CurrX += GetStatRenderGlobals().InterColumnOffset;

	// Append the maximum.
	if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f"), All.GetValue_double(EComplexStatField::IncMax)),GetStatRenderGlobals().StatColor);
	}
	else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%lld"), All.GetValue_int64(EComplexStatField::IncMax)),GetStatRenderGlobals().StatColor);
	}
	return GetStatRenderGlobals().GetFontHeight();
}

void RenderHierCycles( FCanvas* Canvas, int32 X, int32& Y, const FHudGroup& HudGroup )
{
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->GradientTexture0;

	// Render all cycle counters.
	for( int32 RowIndex = 0; RowIndex < HudGroup.HierAggregate.Num(); ++RowIndex )
	{
		const FComplexStatMessage& ComplexStat = HudGroup.HierAggregate[RowIndex];
		const int32 Indent = HudGroup.Indentation[RowIndex];

		if(BackgroundTexture != nullptr)
		{
			Canvas->DrawTile( X, Y + GetStatRenderGlobals().GetYOffset(), GetStatRenderGlobals().AfterNameColumnOffset + GetStatRenderGlobals().InterColumnOffset * (int32)EStatRenderConsts::NUM_COLUMNS, GetStatRenderGlobals().GetFontHeight(),
				0, 0, 1, 1,
				GetStatRenderGlobals().BackgroundColor, BackgroundTexture->Resource, true );
		}

		Y += RenderCycle( ComplexStat, Canvas, X, Y, Indent, true );
	}
}

template< typename T >
void RenderArrayOfStats( FCanvas* Canvas, int32 X, int32& Y, const TArray<FComplexStatMessage>& Aggregates, const FGameThreadHudData& ViewData, const T& FunctionToCall )
{
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->GradientTexture0;

	// Render all counters.
	for( int32 RowIndex = 0; RowIndex < Aggregates.Num(); ++RowIndex )
	{
		const FComplexStatMessage& ComplexStat = Aggregates[RowIndex];

		if(BackgroundTexture != nullptr)
		{
			Canvas->DrawTile( X, Y + GetStatRenderGlobals().GetYOffset(), GetStatRenderGlobals().AfterNameColumnOffset + GetStatRenderGlobals().InterColumnOffset * (int32)EStatRenderConsts::NUM_COLUMNS, GetStatRenderGlobals().GetFontHeight(),
				0, 0, 1, 1,
				GetStatRenderGlobals().BackgroundColor, BackgroundTexture->Resource, true );
		}

		Y += FunctionToCall( ViewData, ComplexStat, Canvas, X, Y );
	}
}

static int32 RenderFlatCycle( const FGameThreadHudData& ViewData, const FComplexStatMessage& Item, class FCanvas* Canvas, int32 X, int32 Y )
{
	return RenderCycle( Item, Canvas, X, Y, 0, true );
}

/**
 * Renders stats using groups
 *
 * @param ViewData data from the stats thread
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 */
static void RenderGroupedWithHierarchy(const FGameThreadHudData& ViewData, FViewport* Viewport, class FCanvas* Canvas, int32 X, int32& Y)
{
	// Grab texture for rendering text background.
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;

	// Render all groups.
	for( int32 GroupIndex = 0; GroupIndex < ViewData.HudGroups.Num(); ++GroupIndex )
	{
		// If the stat isn't enabled for this particular viewport, skip
		FString StatGroupName = ViewData.GroupNames[GroupIndex].ToString();
		StatGroupName.RemoveFromStart(TEXT("STATGROUP_"));
		if (!Viewport->GetClient() || !Viewport->GetClient()->IsStatEnabled(StatGroupName))
		{
			continue;
		}

		// Render header.
		const FName& GroupName = ViewData.GroupNames[GroupIndex];
		const FString& GroupDesc = ViewData.GroupDescriptions[GroupIndex];
		const FString GroupLongName = FString::Printf( TEXT("%s [%s]"), *GroupDesc, *GroupName.GetPlainNameString() );
		Canvas->DrawShadowedString( X, Y, *GroupLongName, GetStatRenderGlobals().StatFont, GetStatRenderGlobals().GroupColor );
		Y += GetStatRenderGlobals().GetFontHeight();

		const FHudGroup& HudGroup = ViewData.HudGroups[GroupIndex];
		const bool bHasHierarchy = !!HudGroup.HierAggregate.Num();
		const bool bHasFlat = !!HudGroup.FlatAggregate.Num();

		if (bHasHierarchy || bHasFlat)
		{
			// Render grouped headings.
			Y += RenderGroupedHeadings( Canvas, X, Y, bHasHierarchy );
		}

		// Render hierarchy.
		if( bHasHierarchy )
		{
			RenderHierCycles( Canvas, X, Y, HudGroup );
			Y += GetStatRenderGlobals().GetFontHeight();
		}

		// Render flat.
		if( bHasFlat )
		{
			RenderArrayOfStats(Canvas,X,Y,HudGroup.FlatAggregate, ViewData, RenderFlatCycle);
			Y += GetStatRenderGlobals().GetFontHeight();
		}

		// Render memory counters.
		if( HudGroup.MemoryAggregate.Num() )
		{
			Y += RenderMemoryHeadings(Canvas,X,Y);
			RenderArrayOfStats(Canvas,X,Y,HudGroup.MemoryAggregate, ViewData, RenderMemoryCounter);
			Y += GetStatRenderGlobals().GetFontHeight();
		}

		// Render remaining counters.
		if( HudGroup.CountersAggregate.Num() )
		{
			Y += RenderCounterHeadings(Canvas,X,Y);
			RenderArrayOfStats(Canvas,X,Y,HudGroup.CountersAggregate, ViewData, RenderCounter);
			Y += GetStatRenderGlobals().GetFontHeight();
		}
	}
}

/**
 * Renders the stats data
 *
 * @param Viewport	The viewport to render to
 * @param Canvas	Canvas object to use for rendering
 * @param X			the X location to start rendering at
 * @param Y			the Y location to start rendering at
 */
void RenderStats(FViewport* Viewport, class FCanvas* Canvas, int32 X, int32 Y)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "RenderStats" ), STAT_RenderStats, STATGROUP_StatSystem );

	FGameThreadHudData* ViewData = FHUDGroupGameThreadRenderer::Get().Latest;
	if (!ViewData)
	{
		return;
	}
	GetStatRenderGlobals().Initialize( Viewport->GetSizeXY() );
	if( !ViewData->bDrawOnlyRawStats )
	{
		RenderGroupedWithHierarchy(*ViewData, Viewport, Canvas, X, Y);
	}
	else
	{
		// Render all counters.
		for( int32 RowIndex = 0; RowIndex < ViewData->GroupDescriptions.Num(); ++RowIndex )
		{
			Canvas->DrawShadowedString(X, Y, *ViewData->GroupDescriptions[RowIndex], GetStatRenderGlobals().StatFont, GetStatRenderGlobals().StatColor);
			Y += GetStatRenderGlobals().GetFontHeight();
		}
	}
}

#endif
