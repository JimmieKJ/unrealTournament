// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTSpreeMessage.generated.h"

UCLASS(CustomConstructor)
class UUTSpreeMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	/** text displayed for player who's on the spree */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FText> OwnerAnnouncementText;
	/** text displayed for others */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FText> OtherAnnouncementText;
	/** text displayed when the spree ends */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FText> EndedAnnouncementText;
	/** text displayed when the spree ends via suicide */
	TArray<FText> EndedSuicideMaleAnnouncementText;
	TArray<FText> EndedSuicideFemaleAnnouncementText;
	/** announcement heard by the player who's on the spree */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FName> AnnouncementNames;

	UUTSpreeMessage(const FPostConstructInitializeProperties& PCIP)
		: Super(PCIP)
	{
		MessageArea = FName(TEXT("DeathMessage"));
		StyleTag = FName(TEXT("Spree"));

		OwnerAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OwnerAnnouncementText[0]", "Killing Spree!"));
		OwnerAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OwnerAnnouncementText[1]", "Rampage!"));
		OwnerAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OwnerAnnouncementText[2]", "Dominating!"));
		OwnerAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OwnerAnnouncementText[3]", "Unstoppable!"));
		OwnerAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OwnerAnnouncementText[4]", "Godlike!"));
		AnnouncementNames.Add(TEXT("KillingSpree"));
		AnnouncementNames.Add(TEXT("Rampage"));
		AnnouncementNames.Add(TEXT("Dominating"));
		AnnouncementNames.Add(TEXT("Unstoppable"));
		AnnouncementNames.Add(TEXT("Godlike"));
		
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[0]", "{Player1Name} is on a killing spree!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[1]", "{Player1Name} is on a rampage!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[2]", "{Player1Name} is dominating!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[3]", "{Player1Name} is unstoppable!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[4]", "{Player1Name} is Godlike!"));

		EndedAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "EndedAnnouncementText[0]", "{Player1Name}'s killing spree was ended by {Player2Name}."));

		EndedSuicideMaleAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "EndedSuicideMaleAnnouncementText[0]", "{Player1Name} was looking pretty good until he killed himself!"));
		EndedSuicideFemaleAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "EndedSuicideFemaleAnnouncementText[0]", "{Player1Name} was looking pretty good until she killed herself!"));

		Importance = 0.8f;
		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 4.0f;
	}

	virtual void ClientReceive(const FClientReceiveData& ClientData) const OVERRIDE
	{
		Super::ClientReceive(ClientData);
		if (ClientData.RelatedPlayerState_1 != NULL && ClientData.LocalPC == ClientData.RelatedPlayerState_1->GetOwner())
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
			if (PC != NULL && PC->RewardAnnouncer != NULL)
			{
				PC->RewardAnnouncer->PlayAnnouncement(GetClass(), ClientData.MessageIndex, ClientData.OptionalObject);
			}
		}
	}
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const OVERRIDE
	{
		if (Switch > 0)
		{
			// Switch is spree level + 1
			return AnnouncementNames.Num() > 0 ? AnnouncementNames[FMath::Clamp<int32>(Switch - 1, 0, AnnouncementNames.Num() - 1)] : NAME_None;
		}
		else
		{
			return NAME_None;
		}
	}
	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const OVERRIDE
	{
		// positive switch is spree level + 1
		// negative switch is ended spree level - 1
		// zero is not used
		const TArray<FText>* TextArray = NULL;
		if (Switch > 0)
		{
			TextArray = bTargetsPlayerState1 ? &OwnerAnnouncementText : &OtherAnnouncementText;
		}
		else if (Switch < 0)
		{
			Switch = -Switch;
			if (RelatedPlayerState_2 != NULL && RelatedPlayerState_2 != RelatedPlayerState_1)
			{
				TextArray = &EndedAnnouncementText;
			}
			else
			{
				TextArray = (Cast<AUTPlayerState>(RelatedPlayerState_1) != NULL && ((AUTPlayerState*)RelatedPlayerState_1)->IsFemale()) ? &EndedSuicideFemaleAnnouncementText : &EndedSuicideMaleAnnouncementText;
			}
		}
		
		return (TextArray != NULL && TextArray->IsValidIndex(Switch - 1)) ? (*TextArray)[Switch - 1] : FText();
	}
};