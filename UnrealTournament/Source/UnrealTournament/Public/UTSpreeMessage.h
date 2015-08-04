// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"
#include "UTDamageType.h"

#include "UTSpreeMessage.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTSpreeMessage : public UUTLocalMessage
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

	/** weapon spree announcement heard by the player who's on the spree */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FName> WeaponAnnouncementNames;

	/** sound played when someone else is on a spree */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	USoundBase* OtherSpreeSound;
	/** sound played when someone else's spree is ended */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	USoundBase* OtherSpreeEndedSound;

	UUTSpreeMessage(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
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
		WeaponAnnouncementNames.Add(TEXT("RocketScience"));
		WeaponAnnouncementNames.Add(TEXT("ShockTherapy"));
		WeaponAnnouncementNames.Add(TEXT("SharpShooter"));
		WeaponAnnouncementNames.Add(TEXT("JackHammer"));
		WeaponAnnouncementNames.Add(TEXT("HeadHunter"));
		WeaponAnnouncementNames.Add(TEXT("ChainLightning"));
		
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[0]", "{Player1Name} is on a killing spree!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[1]", "{Player1Name} is on a rampage!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[2]", "{Player1Name} is dominating!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[3]", "{Player1Name} is unstoppable!"));
		OtherAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "OtherAnnouncementText[4]", "{Player1Name} is Godlike!"));

		EndedAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "EndedAnnouncementText[0]", "{Player1Name}'s killing spree was ended by {Player2Name}."));

		EndedSuicideMaleAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "EndedSuicideMaleAnnouncementText[0]", "{Player1Name} was looking pretty good until he killed himself!"));
		EndedSuicideFemaleAnnouncementText.Add(NSLOCTEXT("UTSpreeMessage", "EndedSuicideFemaleAnnouncementText[0]", "{Player1Name} was looking pretty good until she killed herself!"));

		static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_EnemySpree01.A_UI_EnemySpree01'"));
		OtherSpreeSound = OtherSpreeSoundFinder.Object;
		static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeEndedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_EnemySpreeBroken01.A_UI_EnemySpreeBroken01'"));
		OtherSpreeEndedSound = OtherSpreeEndedSoundFinder.Object;

		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 3.f;
		bWantsBotReaction = true;
	}

	virtual void ClientReceive(const FClientReceiveData& ClientData) const override
	{
		Super::ClientReceive(ClientData);
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (ClientData.RelatedPlayerState_1 != NULL && ClientData.LocalPC == ClientData.RelatedPlayerState_1->GetOwner())
		{
			if (PC != NULL && PC->Announcer != NULL)
			{
				PC->Announcer->PlayAnnouncement(GetClass(), ClientData.MessageIndex, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject);
			}
		}
		else if (PC != NULL)
		{
			USoundBase* SoundToPlay = (ClientData.MessageIndex > 0) ? OtherSpreeSound : OtherSpreeEndedSound;
			if (SoundToPlay != NULL)
			{
				PC->ClientPlaySound(SoundToPlay);
			}
		}
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return (MessageIndex < 0) ? FLinearColor::White : FLinearColor(1.f, 0.5f, 0.f, 1.f);
	}

	virtual float GetScaleInSize(int32 MessageIndex) const
	{
		return UseLargeFont(MessageIndex) ? 3.f : 1.f;
	}

	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return (MessageIndex >= 0);
	}

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		if (Switch == 99)
		{
			const AUTPlayerState* UTPS = Cast<AUTPlayerState>(OptionalObject);
			if (UTPS && UTPS->WeaponSpreeDamage)
			{
				return Cast<UUTDamageType>(UTPS->WeaponSpreeDamage->GetDefaultObject())->SpreeSoundName;
			}
		}
		else if (Switch > 0)
		{
			// Switch is spree level + 1
			return AnnouncementNames.Num() > 0 ? AnnouncementNames[FMath::Clamp<int32>(Switch - 1, 0, AnnouncementNames.Num() - 1)] : NAME_None;
		}
		else
		{
			return NAME_None;
		}
		return NAME_None;
	}

	virtual void PrecacheAnnouncements_Implementation(class UUTAnnouncer* Announcer) const
	{
		// switch 0 has no announcement, skip it
		for (int32 i = 1; i < 50; i++)
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

		// precache weapon spree announcements
		for (int32 i = 0; i < WeaponAnnouncementNames.Num(); i++)
		{
			Announcer->PrecacheAnnouncement(WeaponAnnouncementNames[i]);
		}
	}

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		// positive switch is spree level + 1
		// negative switch is ended spree level - 1
		// zero is not used
		const TArray<FText>* TextArray = NULL;
		if (Switch == 99)
		{
			const AUTPlayerState* UTPS = Cast<AUTPlayerState>(OptionalObject);
			if (UTPS && UTPS->WeaponSpreeDamage)
			{
				return FText::FromString(Cast<UUTDamageType>(UTPS->WeaponSpreeDamage->GetDefaultObject())->SpreeString);
			}
		}
		else  if (Switch > 0)
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
		if (TextArray != NULL && TextArray->Num() == 0)
		{
			TextArray = NULL;
		}
		
		return (TextArray != NULL && TextArray->IsValidIndex(Switch - 1)) ? (*TextArray)[Switch - 1] : TextArray->Last();
	}
};