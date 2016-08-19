#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTAssistMessage.h"
#include "GameFramework/LocalMessage.h"

UUTAssistMessage::UUTAssistMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPartiallyUnique = true;
	Lifetime = 3.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("Multikill"));
	bIsConsoleMessage = false;
	AssistMessage = NSLOCTEXT("CTFRewardMessage", "Assist", "Assist!");
	HatTrickMessage = NSLOCTEXT("CTFRewardMessage", "HatTrick", "Hat Trick!");
	OtherHatTrickMessage = NSLOCTEXT("CTFRewardMessage", "OtherHatTrick", "{Player1Name} got a Hat Trick!");
	bIsStatusAnnouncement = false;
	bWantsBotReaction = true;
	ScaleInSize = 3.f;
	AnnouncementDelay = 2.5f;
}

void UUTAssistMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i < 8; i++)
	{
		Announcer->PrecacheAnnouncement(GetAnnouncementName(i, NULL, NULL, NULL));
	}
}

FName UUTAssistMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch (Switch)
	{
	case 2: return TEXT("Assist"); break;
	case 5: return TEXT("HatTrick"); break;
	}
	return NAME_None;
}

bool UUTAssistMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return IsLocalForAnnouncement(ClientData, true, true);
}

FText UUTAssistMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 2: return AssistMessage; break;
	case 5: return (bTargetsPlayerState1 ? HatTrickMessage : OtherHatTrickMessage); break;
	}

	return FText::GetEmpty();
}
