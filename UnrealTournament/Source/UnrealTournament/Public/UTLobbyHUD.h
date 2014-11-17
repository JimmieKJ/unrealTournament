// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTLobbyHUD.generated.h"

UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTLobbyHUD : public AUTHUD
{
	GENERATED_UCLASS_BODY()

public:
	void PostRender();
};

