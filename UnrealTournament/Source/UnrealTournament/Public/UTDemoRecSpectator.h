// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerController.h"

#include "UTDemoRecSpectator.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTDemoRecSpectator : public AUTPlayerController
{
	GENERATED_UCLASS_BODY()

	virtual void ReceivedPlayer() override;

	virtual void OnFire() override
	{
		if (GetWorld()->GetGameState() != NULL)
		{
			ViewAPlayer(+1);
		}
	}
	virtual void OnStopAltFire() override
	{
		ResetCameraMode();
		SetViewTarget(this);
	}

	virtual void ViewAPlayer(int32 dir) override;
	virtual APlayerState* GetNextViewablePlayer(int32 dir) override;

	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	
	virtual void InitPlayerState() override
	{
		Super::InitPlayerState();
		PlayerState->bOnlySpectator = true;
	}
	virtual void CleanupPlayerState() override
	{
		// don't do AddInactivePlayer() stuff for demo spectator
		AController::CleanupPlayerState();
	}
};