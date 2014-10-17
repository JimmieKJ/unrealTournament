// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTBasePlayerController.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTBasePlayerController : public APlayerController
{
	GENERATED_UCLASS_BODY()

	virtual void SetupInputComponent() override;


	/**	Will popup the in-game menu	 **/
	UFUNCTION(exec)
	virtual void ShowMenu();

	UFUNCTION(exec)
	virtual void HideMenu();

#if !UE_SERVER
	virtual void ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate());
#endif


};