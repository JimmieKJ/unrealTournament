// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMenuBase.h"

#if !UE_SERVER
class SUTInGameMenu : public SUTMenuBase
{
protected:

	virtual void BuildLeftMenuBar();

	virtual FText GetDisconnectButtonText() const;

	virtual FReply OnDisconnect();
	virtual FReply OnTeamChangeClick();
	virtual FReply OnSpectateClick();
	virtual FReply OnCloseClicked();

	virtual void SetInitialPanel();


};
#endif
