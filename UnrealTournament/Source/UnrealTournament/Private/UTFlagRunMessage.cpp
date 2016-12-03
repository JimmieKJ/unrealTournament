// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunMessage.h"
#include "UTAnnouncer.h"
#include "UTFlagRunScoreboard.h"

/*
1: Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "DefendersMustStop", " must hold on defense to have \n a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
2: BRONZE	Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "DefendersMustHold", " must hold {AttackerName} to {BonusType} \n to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
3: SILVER
4: BRONZE 	Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScore", " must score {BonusType} to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
5: SILVER
6: GOLD
7: Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "UnhandledCondition", "UNHANDLED WIN CONDITION"), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
8: BRONZE  Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " must score {BonusType} to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
9: SILVER
10: GOLD
Time*100 + 4,5,6  Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreTime", " must score {BonusType} with at least\n {TimeNeeded} remaining to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
Time * 100 + 8,9,10  Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWin", " must score {BonusType} with\n at least {TimeNeeded} remaining to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
*/

UUTFlagRunMessage::UUTFlagRunMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Lifetime = 6.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));

	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	AnnouncementDelay = 2.f;
	bPlayDuringInstantReplay = false;
}

FName UUTFlagRunMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	int32 TimeNeeded = 0;
	if (Switch >= 100)
	{
		TimeNeeded = Switch / 100;
		Switch = Switch - 100 * TimeNeeded;
	}
	switch (Switch)
	{
	case 1: return TEXT("DefendersMustDefendForChance"); break;
	case 2: /* DefendersMustHold */; break;
	case 3: /* DefendersMustHold; BonusType = SilverBonusText */;  break;
	case 4: return TEXT("AttackersMustScore1ForChance"); break;
	case 5: return TEXT("AttackersMustScore2ForChance"); break;
	case 6: return TEXT("AttackersMustScore3ForChance"); break;
	case 7: /*UnhandledCondition */; break;
	case 8: return TEXT("AttackersMustScore1ToWin"); break;
	case 9: return TEXT("AttackersMustScore2ToWin"); break;
	case 10: return TEXT("AttackersMustScore3ToWin"); break;
	}
	return NAME_None;
}

FText UUTFlagRunMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	return FText::GetEmpty();
}

