// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTDeathMessage.generated.h"

UCLASS(config=game)
class UUTDeathMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText GenericKillMessage;

	virtual void ClientReceive(const FClientReceiveData& ClientData) const OVERRIDE;
	virtual void GetArgs(FFormatNamedArguments& Args, int32 Switch, bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const OVERRIDE;	
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const OVERRIDE;

};



