// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTWindowBase.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTSpectatorWindow : public SUTWindowBase
{
protected:
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool GetGameMousePosition(FVector2D& MousePosition);

};
#endif
