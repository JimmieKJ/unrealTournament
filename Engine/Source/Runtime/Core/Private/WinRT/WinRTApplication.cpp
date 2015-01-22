// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "WinRTApplication.h"
#include "WinRTInputInterface.h"
#include "GenericApplication.h"

FWinRTCursor::FWinRTCursor()
{
	// Set the default cursor
	SetType(EMouseCursor::Default);
}

FVector2D FWinRTCursor::GetPosition() const
{
	return CursorPosition;
}

void FWinRTCursor::SetPosition( const int32 X, const int32 Y )
{
	CursorPosition.X = X;
	CursorPosition.Y = Y;
}

void FWinRTCursor::UpdatePosition( const FVector2D& NewPosition )
{
	CursorPosition = NewPosition;
}

void FWinRTCursor::SetType( const EMouseCursor::Type InNewCursor )
{
	CurrentType = InNewCursor;
}

void FWinRTCursor::GetSize( int32& Width, int32& Height ) const
{

}

void FWinRTCursor::Show( bool bShow )
{

}

void FWinRTCursor::Lock( const RECT* const Bounds )
{

}

static FWinRTApplication* WinRTApplication = NULL;

FWinRTApplication* FWinRTApplication::CreateWinRTApplication()
{
	check( WinRTApplication == NULL );
	WinRTApplication = new FWinRTApplication();
	return WinRTApplication;
}

FWinRTApplication* FWinRTApplication::GetWinRTApplication()
{
	return WinRTApplication;
}

FWinRTApplication::FWinRTApplication()
	: GenericApplication( MakeShareable( new FWinRTCursor() ) )
	, InputInterface( FWinRTInputInterface::Create( MessageHandler ) )
{
}

void FWinRTApplication::PollGameDeviceState( const float TimeDelta )
{
	// Poll game device state and send new events
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();
}

FPlatformRect FWinRTApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	//@todo WINRT: Use the actual device settings here.
	FPlatformRect WorkArea;
	WorkArea.Left = 0;
	WorkArea.Top = 0;
	WorkArea.Right = 1920;
	WorkArea.Bottom = 1080;

	return WorkArea;
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	//@todo WINRT: Use the actual device settings here.
	OutDisplayMetrics.PrimaryDisplayWidth = 1920;
	OutDisplayMetrics.PrimaryDisplayHeight = 1080;

	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = 1920;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = 1080;

	OutDisplayMetrics.VirtualDisplayRect.Left = 0;
	OutDisplayMetrics.VirtualDisplayRect.Top = 0;
	OutDisplayMetrics.VirtualDisplayRect.Right = 1920;
	OutDisplayMetrics.VirtualDisplayRect.Bottom = 1080;
}

TSharedRef< class FGenericApplicationMessageHandler > FWinRTApplication::GetMessageHandler() const
{
	return MessageHandler;
}

void FWinRTApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}


TSharedRef< class FWinRTCursor > FWinRTApplication::GetCursor() const
{
	return StaticCastSharedPtr<FWinRTCursor>( Cursor ).ToSharedRef();
}
