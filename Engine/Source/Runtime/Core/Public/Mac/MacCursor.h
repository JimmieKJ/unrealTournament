// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICursor.h"

#ifndef __OBJC__
class NSCursor;
#endif

class FMacCursor : public ICursor
{
public:

	FMacCursor();

	virtual ~FMacCursor();

	virtual FVector2D GetPosition() const override;

	virtual void SetPosition( const int32 X, const int32 Y ) override;

	virtual void SetType( const EMouseCursor::Type InNewCursor ) override;

	virtual EMouseCursor::Type GetType() const override
	{
		return CurrentType;
	}

	virtual void GetSize( int32& Width, int32& Height ) const override;

	virtual void Show( bool bShow ) override;

	virtual void Lock( const RECT* const Bounds ) override;


public:

	bool UpdateCursorClipping( FVector2D& CursorPosition );

	void WarpCursor( const int32 X, const int32 Y );

	FVector2D GetMouseWarpDelta();

	void SetHighPrecisionMouseMode( bool const bEnable );

	void SetMouseScaling( FVector2D Scale );

	FVector2D GetMouseScaling( void );

	void UpdateCurrentPosition( const FVector2D& Position );

	void UpdateVisibility();

private:
	EMouseCursor::Type CurrentType;

	/** Cursors */
	NSCursor* CursorHandles[EMouseCursor::TotalCursorCount];

	FIntRect CusorClipRect;

	bool bIsVisible;
	bool bUseHighPrecisionMode;
	NSCursor* CurrentCursor;

	FVector2D CurrentPosition;
	FVector2D MouseWarpDelta;
	FVector2D MouseScale;
	bool bIsPositionInitialised;

	io_object_t HIDInterface;
	double SavedAcceleration;
};
