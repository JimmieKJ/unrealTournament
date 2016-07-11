// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UserWidget.h"
#include "AssetData.h"
#include "UTUMGHudWidget.h"
#include "UTUMGHudWidget_LocalMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTUMGHudWidget_LocalMessage : public UUTUMGHudWidget
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Message)
	TSubclassOf<class UUTLocalMessage> MessageClass;

	UPROPERTY(BlueprintReadOnly, Category = Message)
	int32 Switch;

	UPROPERTY(BlueprintReadOnly, Category = Message)
	TWeakObjectPtr<APlayerState> RelatedPlayerState_1;

	UPROPERTY(BlueprintReadOnly, Category = Message)
	TWeakObjectPtr<APlayerState> RelatedPlayerState_2;

	UPROPERTY(BlueprintReadOnly, Category = Message)
	TWeakObjectPtr<UObject> OptionalObject;

	UFUNCTION(BlueprintNativeEvent, Category = Message)
	void InitializeMessage(TSubclassOf<class UUTLocalMessage> inMessageClass, int32 inSwitch, APlayerState* inRelatedPlayerState_1, APlayerState* inRelatedPlayerState_2, UObject* inOptionalObject);

protected:

};
