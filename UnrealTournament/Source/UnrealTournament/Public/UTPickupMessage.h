// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPickupMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTPickupMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FText PendingWeaponPickupText;

	virtual FText ResolveMessage_Implementation(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const override;
	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
	virtual bool ShouldCountInstances_Implementation(int32 MessageIndex, UObject* OptionalObject) const override;
};