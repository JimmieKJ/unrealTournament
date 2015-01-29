// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTLobbyHUD.generated.h"

UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTLobbyHUD : public AUTHUD
{
	GENERATED_UCLASS_BODY()

public:
	int32 LobbyDebugLevel;
	void PostRender();
};

