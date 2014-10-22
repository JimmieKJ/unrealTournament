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

	// A quick function so I don't have to keep adding one when I want to test something.  @REMOVEME: Before the final version
	UFUNCTION(exec)
	virtual void DebugTest(FString TestCommand);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerDebugTest(const FString& TestCommand);


};