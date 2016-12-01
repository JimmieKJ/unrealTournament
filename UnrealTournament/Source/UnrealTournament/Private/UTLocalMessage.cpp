// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	bOptionalSpoken = false;
	AnnouncementDelay = 0.f;
	bWantsBotReaction = false;
	bDrawAsDeathMessage = false;
	bDrawAtIntermission = true;
	bDrawOnlyIfAlive = false;
	ScaleInSize = 1.f;
	ScaleInTime = 0.2f;
	FontSizeIndex = 2;
	bPlayDuringIntermission = true;
	bCombineEmphasisText = false;
	bPlayDuringInstantReplay = true;
}

bool UUTLocalMessage::IsOptionalSpoken(int32 MessageIndex) const
{
	return bOptionalSpoken;
}

int32 UUTLocalMessage::GetFontSizeIndex(int32 MessageIndex) const
{
	return FontSizeIndex;
}

bool UUTLocalMessage::ShouldDrawMessage(int32 MessageIndex, AUTPlayerController* PC, bool bIsAtIntermission, bool bNoLivePawnTarget) const
{
	if ((bIsAtIntermission && !bDrawAtIntermission) || (bNoLivePawnTarget && bDrawOnlyIfAlive))
	{
		return false;
	}
	return true;
}

FString UUTLocalMessage::GetConsoleString(const FClientReceiveData& ClientData, FText LocalMessageText) const
{
	return LocalMessageText.ToString();
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
			Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient->ViewportConsole->OutputText( GetConsoleString(ClientData, LocalMessageText) );
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

bool UUTLocalMessage::IsLocalForAnnouncement(const FClientReceiveData& ClientData, bool bCheckFirstPS, bool bCheckSecondPS) const
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
	if ((PC != NULL) && (PC->Announcer != NULL))
	{
		APlayerState* ViewedPS = (PC && PC->PlayerState && PC->PlayerState->bOnlySpectator && PC->LastSpectatedPlayerState) ? PC->LastSpectatedPlayerState : PC->PlayerState;
		if (bCheckFirstPS && ViewedPS && (ClientData.RelatedPlayerState_1 == ViewedPS))
		{
			return true;
		}
		return bCheckSecondPS && ViewedPS && (ClientData.RelatedPlayerState_2 == ViewedPS);
	}
	return false;
}

float UUTLocalMessage::GetAnnouncementDelay(int32 Switch)
{
	return AnnouncementDelay;
}

float UUTLocalMessage::GetAnnouncementPriority(const FAnnouncementInfo AnnouncementInfo) const
{
	return bIsStatusAnnouncement ? 0.5f : 0.8f;
}

FLinearColor UUTLocalMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

float UUTLocalMessage::GetScaleInTime_Implementation(int32 MessageIndex) const
{
	return ScaleInTime;
}

float UUTLocalMessage::GetScaleInSize_Implementation(int32 MessageIndex) const
{
	return ScaleInSize;
}

void UUTLocalMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	// naive default implementation just keeps querying until we get a fail
	for (int32 i = 0; i < 50; i++)
	{
		FName SoundName = GetAnnouncementName(i, NULL, NULL, NULL);
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

void UUTLocalMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	PrefixText = FText::GetEmpty();
	EmphasisText = FText::GetEmpty();
	PostfixText = FText::GetEmpty();
}

FText UUTLocalMessage::BuildEmphasisText(int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	FText PrefixText, EmphasisText, PostfixText;
	FLinearColor EmphasisColor;
	GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	FFormatNamedArguments Args;
	Args.Add(TEXT("PrefixText"), PrefixText);
	Args.Add(TEXT("EmphasisText"), EmphasisText);
	Args.Add(TEXT("PostfixText"), PostfixText);
	return FText::Format(NSLOCTEXT("UTLocalMessage", "CombinedMessage", "{PrefixText}{EmphasisText}{PostfixText}"), Args);
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

bool UUTLocalMessage::PartiallyDuplicates(int32 Switch1, int32 Switch2, UObject* OptionalObject1, UObject* OptionalObject2 ) const
{
	return (Switch1 == Switch2);
}

FName UUTLocalMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	return NAME_None;
}

float UUTLocalMessage::GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return 0.2f;
}

USoundBase* UUTLocalMessage::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return NULL;
}

bool UUTLocalMessage::InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	FAnnouncementInfo AnnouncementInfo(GetClass(), Switch, nullptr, nullptr, OptionalObject, 0.f);
	FAnnouncementInfo OtherAnnouncementInfo(OtherMessageClass, OtherSwitch, nullptr, nullptr, OtherOptionalObject, 0.f);
	return InterruptAnnouncement(AnnouncementInfo, OtherAnnouncementInfo);
}

bool UUTLocalMessage::InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const
{
	// by default interrupt messages of same type, and countdown messages
	return (AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass) || Cast<UUTLocalMessage>(OtherAnnouncementInfo.MessageClass->GetDefaultObject())->IsOptionalSpoken(OtherAnnouncementInfo.Switch);
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

bool UUTLocalMessage::ShouldCountInstances_Implementation(int32 MessageIndex, UObject* OptionalObject) const
{
	return false;
}

FString UUTLocalMessage::GetAnnouncementUMGClassname(int32 Switch, const UObject* OptionalObject) const
{
	return TEXT("");
}