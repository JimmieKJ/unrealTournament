// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplication.h"

class FWinRTCursor : public ICursor
{
public:

	FWinRTCursor();

	virtual FVector2D GetPosition() const override;

	virtual void SetPosition( const int32 X, const int32 Y ) override;

	void UpdatePosition( const FVector2D& NewPosition );

	virtual void SetType( const EMouseCursor::Type InNewCursor ) override;

	virtual EMouseCursor::Type GetType() const override
	{
		return CurrentType;
	}

	virtual void GetSize( int32& Width, int32& Height ) const override;

	virtual void Show( bool bShow ) override;

	virtual void Lock( const RECT* const Bounds ) override;


private:

	EMouseCursor::Type CurrentType;
	FVector2D CursorPosition;	
};

class FWinRTApplication : public GenericApplication
{
public:

	static FWinRTApplication* CreateWinRTApplication();

	static FWinRTApplication* GetWinRTApplication();


public:	

	virtual ~FWinRTApplication() {}

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	TSharedRef< class FGenericApplicationMessageHandler > GetMessageHandler() const;

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;

	TSharedRef< class FWinRTCursor > GetCursor() const;


private:

	FWinRTApplication();


private:

	TSharedPtr< class FWinRTInputInterface > InputInterface;

	FVector2D CursorPosition;
};