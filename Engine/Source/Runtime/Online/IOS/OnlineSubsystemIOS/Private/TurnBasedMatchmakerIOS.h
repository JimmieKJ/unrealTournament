// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineTurnBasedInterface.h"

@class FTurnBasedMatchmakerDelegateIOS;
@class GKMatchRequest;
@class GKTurnBasedMatchmakerViewController;

class FTurnBasedMatchmakerIOS : public FTurnBasedMatchmakerDelegate
{
public:
    FTurnBasedMatchmakerIOS(FTurnBasedMatchmakerDelegate& _Delegate);
    ~FTurnBasedMatchmakerIOS();

    void ShowWithMatchRequest(const FTurnBasedMatchRequest& Request);

    virtual void OnMatchmakerCancelled() override;
    virtual void OnMatchmakerFailed() override;
    virtual void OnMatchFound(FTurnBasedMatchRef Match) override;

private:
    GKMatchRequest* GetGKMatchRequestFromMPMatchRequest(const FTurnBasedMatchRequest& Request) const;
    void DismissMatchmaker();

    FTurnBasedMatchRequest MatchRequest;

    GKTurnBasedMatchmakerViewController* MatchmakerViewController;
    FTurnBasedMatchmakerDelegateIOS* IOSDelegate;
    FTurnBasedMatchmakerDelegate* Delegate;
};
