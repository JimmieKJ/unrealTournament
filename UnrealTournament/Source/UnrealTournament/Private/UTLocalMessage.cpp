// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "UTLocalMessage.h"
#include "UTHUD.h"
#include "UTAnnouncer.h"
#include "Engine/DemoNetDriver.h"

UUTLocalMessage::UUTLocalMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Lifetime = 2.0;
	MessageArea = FName(TEXT("ConsoleMessage"));
	StyleTag = FName(TEXT("Default"));
	bOptionalSpoken = false;
	AnnouncementDelay = 0.f;
	bWantsBotReaction = false;
}

void UUTLocalMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	// Skip playing local messages if we're fast forwarding from a demo seek
	UDemoNetDriver* DemoDriver = ClientData.LocalPC->GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		if (DemoDriver->IsFastForwarding())
		{
			return;
		}
	}

	FText LocalMessageText = ResolveMessage(ClientData.MessageIndex, (ClientData.RelatedPlayerState_1 == ClientData.LocalPC->PlayerState), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject);
	if ( !LocalMessageText.IsEmpty() )
	{
		if ( Cast<AUTHUD>(ClientData.LocalPC->MyHUD) != NULL )
		{
			Cast<AUTHUD>(ClientData.LocalPC->MyHUD)->ReceiveLocalMessage(GetClass(), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.MessageIndex, LocalMessageText, ClientData.OptionalObject );
		}

		if(IsConsoleMessage(ClientData.MessageIndex) && Cast<ULocalPlayer>(ClientData.LocalPC->Player) != NULL && Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient != NULL)
		{
			Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient->ViewportConsole->OutputText( LocalMessageText.ToString() );
		}
	}

	if (ShouldPlayAnnouncement(ClientData))
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != NULL && PC->Announcer != NULL)
		{
			PC->Announcer->PlayAnnouncement(GetClass(), ClientData.MessageIndex, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject);
		}
	}
	OnClientReceive(ClientData.LocalPC, ClientData.MessageIndex, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject);
}

bool UUTLocalMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return bIsStatusAnnouncement;
}

float UUTLocalMessage::GetAnnouncementDelay(int32 Switch)
{
	return AnnouncementDelay;
}

float UUTLocalMessage::GetAnnouncementPriority(int32 Switch) const
{
	return bIsStatusAnnouncement ? 0.5f : 0.8f;
}

bool UUTLocalMessage::UseLargeFont(int32 MessageIndex) const
{
	return true;
}

FLinearColor UUTLocalMessage::GetMessageColor(int32 MessageIndex) const
{
	return FLinearColor::White;
}

float UUTLocalMessage::GetScaleInTime(int32 MessageIndex) const
{
	return 0.2f;
}

float UUTLocalMessage::GetScaleInSize(int32 MessageIndex) const
{
	return 1.f;
}

void UUTLocalMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	// naive default implementation just keeps querying until we get a fail
	for (int32 i = 0; i < 50; i++)
	{
		FName SoundName = GetAnnouncementName(i, NULL);
		if (SoundName != NAME_None)
		{
			Announcer->PrecacheAnnouncement(SoundName);
		}
		else
		{
			break;
		}
	}
}

FText UUTLocalMessage::ResolveMessage_Implementation(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	FFormatNamedArguments Args;
	GetArgs(Args, Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	return FText::Format(GetText(Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject), Args);
}

void UUTLocalMessage::GetArgs(FFormatNamedArguments& Args, int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject ) const
{
	Args.Add(TEXT("Player1Name"), RelatedPlayerState_1 != NULL ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty());
	Args.Add(TEXT("Player1Score"), RelatedPlayerState_1 != NULL ? FText::AsNumber(int32(RelatedPlayerState_1->Score)) : FText::GetEmpty());
	Args.Add(TEXT("Player2Name"), RelatedPlayerState_2 != NULL ? FText::FromString(RelatedPlayerState_2->PlayerName) : FText::GetEmpty());
	Args.Add(TEXT("Player2Score"), RelatedPlayerState_2 != NULL ? FText::AsNumber(int32(RelatedPlayerState_2->Score)) : FText::GetEmpty());
	Args.Add(TEXT("Player1OldName"), RelatedPlayerState_1 != NULL ? FText::FromString(RelatedPlayerState_1->OldName) : FText::GetEmpty());

	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(RelatedPlayerState_1);
	Args.Add(TEXT("Player1Team"), (UTPlayerState && UTPlayerState->Team) ? UTPlayerState->Team->TeamName : FText::GetEmpty());

	UTPlayerState = Cast<AUTPlayerState>(RelatedPlayerState_2);
	Args.Add(TEXT("Player2Team"), (UTPlayerState && UTPlayerState->Team) ? UTPlayerState->Team->TeamName : FText::GetEmpty());

	UClass* DamageTypeClass = Cast<UClass>(OptionalObject);
	if (DamageTypeClass != NULL && DamageTypeClass->IsChildOf(UUTDamageType::StaticClass()))
	{
		UUTDamageType* DamageType = DamageTypeClass->GetDefaultObject<UUTDamageType>();			
		if (DamageType != NULL)
		{
			Args.Add(TEXT("WeaponName"), DamageType->AssociatedWeaponName);
		}
	}

	AUTTeamInfo* TeamInfo = Cast<AUTTeamInfo>(OptionalObject);
	Args.Add(TEXT("OptionalTeam"), TeamInfo ? TeamInfo->TeamName : FText::GetEmpty());
}

FText UUTLocalMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	return Blueprint_GetText(Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

float UUTLocalMessage::GetLifeTime(int32 Switch) const
{
    return Blueprint_GetLifeTime(Switch);
}

bool UUTLocalMessage::IsConsoleMessage(int32 Switch) const
{
    return GetDefault<UUTLocalMessage>(GetClass())->bIsConsoleMessage;
}

bool UUTLocalMessage::PartiallyDuplicates(int32 Switch1, int32 Switch2, UObject* OptionalObject1, UObject* OptionalObject2 )
{
	return (Switch1 == Switch2);
}

FName UUTLocalMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return NAME_None;
}

USoundBase* UUTLocalMessage::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return NULL;
}

bool UUTLocalMessage::InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	// by default interrupt messages of same type, and countdown messages
	return (GetClass() == OtherMessageClass) || Cast<UUTLocalMessage>(OtherMessageClass->GetDefaultObject())->bOptionalSpoken;
}

bool UUTLocalMessage::CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	return false;
}

void UUTLocalMessage::OnAnnouncementPlayed_Implementation(int32 Switch, const UObject* OptionalObject) const
{
}

FText UUTLocalMessage::Blueprint_GetText_Implementation(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	return FText::GetEmpty();
}

float UUTLocalMessage::Blueprint_GetLifeTime_Implementation(int32 Switch) const
{
	return GetDefault<UUTLocalMessage>(GetClass())->Lifetime;
}