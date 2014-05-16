// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUD.generated.h"

UCLASS()
class AUTHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

public:

	/** Primary draw call for the HUD */
	virtual void DrawHUD() OVERRIDE;

	/** Receive a localized message from somewhere to be displayed */
	virtual void ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject = NULL);


private:
	/** Crosshair asset pointer */
	UTexture2D* CrosshairTex;

};

