// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

enum class ETextHitPoint : uint8;

struct SLATE_API FRunInfo
{
	FRunInfo()
		: Name()
		, MetaData()
	{
	}

	FRunInfo( FString InName )
		: Name( MoveTemp(InName) )
		, MetaData()
	{

	}

	FString Name;
	TMap< FString, FString > MetaData;
};

/** Attributes that a run can have */
enum class ERunAttributes : uint8
{
	/**
	 * This run has no special attributes
	 */
	None = 0,

	/**
	 * This run supports text, and can have new text inserted into it
	 * Note that even a run which doesn't support text may contain text (likely a breaking space character), however that text should be considered immutable
	 */
	SupportsText = 1<<0,
};
ENUM_CLASS_FLAGS(ERunAttributes);

class SLATE_API IRun
{
public:

	virtual FTextRange GetTextRange() const = 0;
	virtual void SetTextRange( const FTextRange& Value ) = 0;

	virtual int16 GetBaseLine( float Scale ) const = 0;
	virtual int16 GetMaxHeight( float Scale ) const = 0;

	virtual FVector2D Measure( int32 StartIndex, int32 EndIndex, float Scale) const = 0;
	virtual int8 GetKerning(int32 CurrentIndex, float Scale) const = 0;

	virtual TSharedRef< class ILayoutBlock > CreateBlock( int32 StartIndex, int32 EndIndex, FVector2D Size, const TSharedPtr< class IRunRenderer >& Renderer ) = 0;

	virtual int32 GetTextIndexAt( const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint = nullptr ) const = 0;
	
	virtual FVector2D GetLocationAt(const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale) const = 0;

	virtual void BeginLayout() = 0;
	virtual void EndLayout() = 0;

	virtual void Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange) = 0;
	virtual TSharedRef<IRun> Clone() const = 0;

	virtual void AppendTextTo(FString& Text) const = 0;
	virtual void AppendTextTo(FString& Text, const FTextRange& Range) const = 0;

	virtual const FRunInfo& GetRunInfo() const = 0;

	virtual ERunAttributes GetRunAttributes() const = 0;

};
