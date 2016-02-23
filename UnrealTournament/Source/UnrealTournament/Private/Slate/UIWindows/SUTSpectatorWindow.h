// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTWindowBase.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTSpectatorWindow : public SUTWindowBase
{
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool GetGameMousePosition(FVector2D& MousePosition);
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	bool bLast;

private:
	// HACKS needed to keep window focus
	virtual bool SupportsKeyboardFocus() const override;


};
#endif
