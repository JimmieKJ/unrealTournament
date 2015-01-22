// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SLATE_API FDefaultLayoutBlock : public ILayoutBlock
{
public:

	static TSharedRef< FDefaultLayoutBlock > Create( const TSharedRef< IRun >& InRun, const FTextRange& InRange, FVector2D InSize, const TSharedPtr< IRunRenderer >& InRenderer )
	{
		return MakeShareable( new FDefaultLayoutBlock( InRun, InRange, InSize, InRenderer ) );
	}

	virtual ~FDefaultLayoutBlock() {}

	virtual TSharedRef< IRun > GetRun() const override { return Run; }
	virtual FTextRange GetTextRange() const override { return Range; }
	virtual FVector2D GetSize() const override { return Size; }
	virtual TSharedPtr< IRunRenderer > GetRenderer() const override { return Renderer; }

	virtual void SetLocationOffset( const FVector2D& InLocationOffset ) override { LocationOffset = InLocationOffset; }
	virtual FVector2D GetLocationOffset() const override { return LocationOffset; }

private:

	static TSharedRef< FDefaultLayoutBlock > Create( const FDefaultLayoutBlock& Block )
	{
		return MakeShareable( new FDefaultLayoutBlock( Block ) );
	}

	FDefaultLayoutBlock( const TSharedRef< IRun >& InRun, const FTextRange& InRange, FVector2D InSize, const TSharedPtr< IRunRenderer >& InRenderer )
		: Run( InRun )
		, Range( InRange )
		, Size( InSize )
		, LocationOffset( ForceInitToZero )
		, Renderer( InRenderer )
	{

	}

	FDefaultLayoutBlock( const FDefaultLayoutBlock& Block )
		: Run( Block.Run )
		, Range( Block.Range )
		, Size( Block.Size )
		, LocationOffset( ForceInitToZero )
		, Renderer( Block.Renderer )
	{

	}

private:

	TSharedRef< IRun > Run;

	FTextRange Range;
	FVector2D Size;
	FVector2D LocationOffset;
	TSharedPtr< IRunRenderer > Renderer;
};