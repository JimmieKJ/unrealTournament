// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTChatMessage.generated.h"

UCLASS(config = game)
class UNREALTOURNAMENT_API UUTChatMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()
	
	virtual void ClientReceiveChat(const FClientReceiveData& ClientData, FName Destination = NAME_None) const;
	virtual FLinearColor GetMessageColor(int32 MessageIndex) const override;
	virtual bool UseLargeFont(int32 MessageIndex) const override;
};

