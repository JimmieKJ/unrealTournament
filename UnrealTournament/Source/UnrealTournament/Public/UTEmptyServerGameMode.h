// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTEmptyServerGameMode.generated.h"

/**
 * Dedicated server "empty shell" for advertising that this server is available to host a game.
 */
UCLASS()
class UNREALTOURNAMENT_API AUTEmptyServerGameMode : public AUTBaseGameMode
{
	GENERATED_BODY()

public:

	AUTEmptyServerGameMode();

	TSubclassOf<AGameSession> GetGameSessionClass() const override;
	void InitGameState() override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
