// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICursor.h"
#include "LinuxWindow.h"

#include <SDL.h>

typedef SDL_Cursor*		SDL_HCursor;

class FLinuxCursor : public ICursor
{
public:
	FLinuxCursor();

	virtual ~FLinuxCursor();

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

	/**
	 * Defines a custom cursor shape for the EMouseCursor::Custom type.
	 * 
	 * @param CursorHandle	A native cursor handle to show when EMouseCursor::Custom is selected.
	 */
	virtual void SetCustomShape( SDL_HCursor CursorHandle );

	bool UpdateCursorClipping( FVector2D& CursorPosition );

	bool IsHidden();

	uint32 GetCursorEvent();

	/** Updates accumulated offset which is used for maintaining a "shadow" cursor position */
	void AddOffset(const int32 DX, const int32 DY);

	/** Resets accumulated offsets to (0,0), */
	void ResetOffset();

	/**
	 * Invalidates whatever cached data cursor may have
	 */
	void InvalidateCaches();

	/** Set the internal cached position, setting the cache to valid */
	void SetCachedPosition( const int32 X, const int32 Y );

private:

	EMouseCursor::Type CurrentType;
	bool bHidden;

	/** Cursors */
	SDL_HCursor CursorHandles[ EMouseCursor::TotalCursorCount ];

	FIntRect CursorClipRect;

	uint32 CursorEvent;

	/** Cached global X position */
	mutable int32 CachedGlobalXPosition;

	/** Cached global Y position */
	mutable int32 CachedGlobalYPosition;

	/** Whether mouse position cache is valid */
	mutable bool bPositionCacheIsValid;
};
