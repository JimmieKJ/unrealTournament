// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTutorialAnnouncement.h"
#include "UTAnnouncer.h"

UUTTutorialAnnouncement::UUTTutorialAnnouncement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsUnique = false;
	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MultiKill"));
	Lifetime = 1.f;
}

void UUTTutorialAnnouncement::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i <= 18; i++)
	{
		Announcer->PrecacheAnnouncement(GetAnnouncementName(i, NULL, NULL, NULL));
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		// make sure we don't use abstract, deprecated, or blueprint skeleton classes
		if (It->IsChildOf(AUTPickup::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			UClass* PickupClass = *It;
			if (!PickupClass->IsPendingKill())
			{
				AUTPickup* DefaultPickupObj = Cast<AUTPickup>(PickupClass->GetDefaultObject());
				if (DefaultPickupObj)
				{
					DefaultPickupObj->PrecacheTutorialAnnouncements(Announcer);
				}
			}
		}
		else if (It->IsChildOf(AUTInventory::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) && !It->GetName().StartsWith(TEXT("SKEL_")))
		{
			UClass* InvClass = *It;
			if (!InvClass->IsPendingKill())
			{
				AUTInventory* DefaultInventoryObj = Cast<AUTInventory>(InvClass->GetDefaultObject());
				if (DefaultInventoryObj)
				{
					DefaultInventoryObj->PrecacheTutorialAnnouncements(Announcer);
				}
			}
		}
	}
}

FName UUTTutorialAnnouncement::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	const AUTInventory* Inv = Cast<AUTInventory>(OptionalObject);
	if (Inv)
	{
		return Inv->GetTutorialAnnouncement(Switch);
	}

	const AUTPickup* Pickup = Cast<AUTPickup>(OptionalObject);
	if (Pickup)
	{
		return Pickup->GetTutorialAnnouncement(Switch);
	}

	switch (Switch)
	{
	case 0: return TEXT("WelcomeToMovementTraining"); break;
	case 1: return TEXT("JumpToClearObstacle"); break;
	case 2: return TEXT("DodgeBetweenPlatforms"); break;
	case 3: return TEXT("CrouchSlideUnderObstacle"); break;
	case 4: return TEXT("UseJumpPad"); break;
	case 5: return TEXT("UseLift"); break;
	case 6: return TEXT("UseLiftDifficult"); break;
	case 7: return TEXT("UseSlopeDodgeObstacle"); break;
	case 8: return TEXT("UseWRunningAcross"); break;
	case 9: return TEXT("MovementTrainingComplete"); break;
	case 10: return TEXT("Excellent"); break;
	case 11: return TEXT("WellDone"); break;
	case 12: return TEXT("UseWDodgingAcross"); break;
	case 13: return TEXT("WelcomeWeaponTraining"); break;
	case 14: return TEXT("WeaponTrainingComplete"); break;
	case 15: return TEXT("WelcomePickupTraining"); break;
	case 16: return TEXT("PickupTrainingComplete"); break;
	case 17: return TEXT("MakeYourOwn"); break;
	}
	return NAME_None;
}

bool UUTTutorialAnnouncement::InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const
{
	if ((AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass) && (AnnouncementInfo.OptionalObject == nullptr) && (OtherAnnouncementInfo.OptionalObject == nullptr) && (OtherAnnouncementInfo.Switch != 0) && (OtherAnnouncementInfo.Switch != 13))
	{
		if (((AnnouncementInfo.Switch == 7) || (AnnouncementInfo.Switch == 8)) && ((OtherAnnouncementInfo.Switch == 7) || (OtherAnnouncementInfo.Switch == 8)))
		{
			return false;
		}
		return true;
	}
	if ((AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass) && (AnnouncementInfo.OptionalObject != OtherAnnouncementInfo.OptionalObject) && (OtherAnnouncementInfo.OptionalObject != nullptr))
	{
		//UE_LOG(UT, Warning, TEXT("Interrupt %s %d because of %s %d"), *OtherAnnouncementInfo.OptionalObject->GetName(), OtherAnnouncementInfo.Switch, *AnnouncementInfo.OptionalObject->GetName(), AnnouncementInfo.Switch);
		return true;
	}
		
	return false;
}

bool UUTTutorialAnnouncement::CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	if ((OtherMessageClass == GetClass()) && (Switch == OtherSwitch) && (OptionalObject == OtherOptionalObject))
	{
		//UE_LOG(UT, Warning, TEXT("Cancel %s %d because of %s %d"), *OptionalObject->GetName(), Switch, *OtherOptionalObject->GetName(), OtherSwitch);
	}
	return (OtherMessageClass == GetClass()) && (Switch == OtherSwitch) && (OptionalObject == OtherOptionalObject);
}


