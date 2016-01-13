// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../Base/SUTPanelBase.h"
#include "../SUWindowsStyle.h"
#include "SUTChatPanel.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTMidGameInfoPanel : public SUTChatPanel
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);

protected:

	TSharedPtr<class STextBlock> ServerName;
	TSharedPtr<class STextBlock> ServerRules;
	TSharedPtr<class STextBlock> ServerMOTD;

	virtual FText GetServerName() const;
	virtual FText GetServerMOTD() const;
	virtual FText GetServerRules() const;

	virtual void BuildNonChatPanel();

	virtual void SortUserList() override;

};

#endif