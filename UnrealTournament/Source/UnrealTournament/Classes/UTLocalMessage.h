// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTLocalMessage.generated.h"

UCLASS(abstract)
class UUTLocalMessage : public ULocalMessage
{
	GENERATED_UCLASS_BODY()

	/** Message area on HUD (index into UTHUD.MessageOffset[]) */
	UPROPERTY()
	FName MessageArea;

	// How much weight should be given to this message.  The MessageWidget will
	// use this number to determine how important the message is. Range is 0-1.

	UPROPERTY()
	float Importance;			

	UPROPERTY()
	uint32 bIsSpecial:1;    // If true, don't add to normal queue.

	UPROPERTY()
	uint32 bIsUnique:1;    // If true and special, only one can be in the HUD queue at a time.

	UPROPERTY()
	uint32 bIsPartiallyUnique:1;    // If true and special, only one can be in the HUD queue with the same switch value

	UPROPERTY()
	uint32 bIsConsoleMessage:1;    // If true, put a GetString on the console.

	UPROPERTY()
	uint32 bCountInstances:1;    // if true, if sent to HUD multiple times, count up instances (only if bIsUnique)

	UPROPERTY()
	float Lifetime;    // # of seconds to stay in HUD message queue.

	virtual void ClientReceive(const FClientReceiveData& ClientData) const OVERRIDE;
	virtual FText ResolveMessage(int32 Switch = 0,bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const;

	virtual void GetArgs(FFormatNamedArguments& Args, int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const;
	virtual FText GetText(int32 Switch = 0,bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const;
	virtual float GetLifeTime(int32 Switch) const;
	virtual bool IsConsoleMessage(int32 Switch) const;
	bool PartiallyDuplicates(int32 Switch1, int32 Switch2, class UObject* OptionalObject1, class UObject* OptionalObject2 );
};



