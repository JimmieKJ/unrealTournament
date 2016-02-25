// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <GameKit/GameKit.h>

@interface FTurnBasedMatchmakerDelegateIOS : NSObject < GKTurnBasedMatchmakerViewControllerDelegate >

- (id)initWithDelegate:(FTurnBasedMatchmakerDelegate*)delegate;

- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController*)viewController didFailWithError:(NSError*)error;
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController*)viewController didFindMatch:(GKTurnBasedMatch*)match;
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController*)viewController playerQuitForMatch:(GKTurnBasedMatch*)match;
- (void)turnBasedMatchmakerViewControllerWasCancelled:(GKTurnBasedMatchmakerViewController*)viewController;

@end
