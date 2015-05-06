// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUChatPanel.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUMidGameInfoPanel : public SUChatPanel
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);

protected:

	TSharedPtr<class STextBlock> ServerName;
	TSharedPtr<class STextBlock> ServerRules;
	TSharedPtr<class STextBlock> ServerMOTD;

	virtual FString GetServerName() const;
	virtual FString GetServerMOTD() const;
	virtual FText GetServerRules() const;

	virtual void BuildNonChatPanel();

	virtual void SortUserList() override;

};

#endif