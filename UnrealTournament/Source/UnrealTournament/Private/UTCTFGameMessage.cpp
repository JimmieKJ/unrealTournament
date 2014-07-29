// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObjectMessage.h"
#include "UTCTFGameMessage.h"

UUTCTFGameMessage::UUTCTFGameMessage(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	MessageArea = FName(TEXT("GameMessages"));
	ReturnMessage = NSLOCTEXT("CTFGameMessage","ReturnMessage","{Player1Name} returned the {OptionalTeam} Flag!");
	ReturnedMessage = NSLOCTEXT("CTFGameMessage","ReturnedMessage","The {OptionalTeam} Flag was returned!");
	CaptureMessage = NSLOCTEXT("CTFGameMessage","CaptureMessage","{Player1Name} captured the {OptionalTeam} Flag!");
	DroppedMessage = NSLOCTEXT("CTFGameMessage","DroppedMessage","{Player1Name} dropped the {OptionalTeam} Flag!");
	HasMessage = NSLOCTEXT("CTFGameMessage","HasMessage","{Player1Name} took the {OptionalTeam} Flag!");
	KilledMessage = NSLOCTEXT("CTFGameMessage","KilledMessage","{Player1Name} killed the {OptionalTeam} flag carrier!");

	bIsStatusAnnouncement = true;


}

FText UUTCTFGameMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch(Switch)
	{
		case 0 : return ReturnMessage; break;
		case 1 : return ReturnedMessage; break;
		case 2 : return CaptureMessage; break;
		case 3 : return DroppedMessage; break;
		case 4 : return HasMessage; break;
		case 5 : return KilledMessage; break;
	}

	return FText::GetEmpty();
}
