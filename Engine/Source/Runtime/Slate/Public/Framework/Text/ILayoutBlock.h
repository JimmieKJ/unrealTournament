// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SLATE_API ILayoutBlock
{
public:

	virtual TSharedRef< class IRun > GetRun() const = 0;
	virtual FTextRange GetTextRange() const = 0;
	virtual FVector2D GetSize() const = 0;
	virtual TSharedPtr< class IRunRenderer > GetRenderer() const = 0;

	virtual void SetLocationOffset( const FVector2D& InLocationOffset ) = 0;
	virtual FVector2D GetLocationOffset() const = 0;
};