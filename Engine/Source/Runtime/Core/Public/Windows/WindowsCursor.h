// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICursor.h"

#include "AllowWindowsPlatformTypes.h"
	#include "Ole2.h"
	#include "OleIdl.h"
#include "HideWindowsPlatformTypes.h"

class FWindowsCursor : public ICursor
{
public:

	FWindowsCursor();

	virtual ~FWindowsCursor();

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
	virtual void SetCustomShape( HCURSOR CursorHandle );

private:

	EMouseCursor::Type CurrentType;

	/** Cursors */
	HCURSOR CursorHandles[ EMouseCursor::TotalCursorCount ];
};