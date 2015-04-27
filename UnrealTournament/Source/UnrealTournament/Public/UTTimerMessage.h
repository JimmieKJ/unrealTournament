// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTimerMessage.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTTimerMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Message)
	TArray<FText> CountDownText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Message)
	TArray<FName> CountDownAnnouncements;

	UUTTimerMessage(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("GameMessages"));
		StyleTag = FName(TEXT("Countdown"));
		bIsUnique = true;
		Lifetime = 2.0f;
		bIsStatusAnnouncement = true;
		bOptionalSpoken = true; // @TODO FIXMESTEVE - depends on the message - some should just delay!

		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text1","1..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text2","2..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text3","3..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text4","4..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text5","5..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text6","6..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text7","7..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text8","8..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text9","9..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text10","10..."));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text30Secs","30 seconds left!"));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text1Min","One minute remains!"));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text3Min","Three minutes remain!"));
		CountDownText.Add( NSLOCTEXT("UTTimerMessage","Text5Min","Five minutes remain!"));

		CountDownAnnouncements.Add(TEXT("CD1"));
		CountDownAnnouncements.Add(TEXT("CD2"));
		CountDownAnnouncements.Add(TEXT("CD3"));
		CountDownAnnouncements.Add(TEXT("CD4"));
		CountDownAnnouncements.Add(TEXT("CD5"));
		CountDownAnnouncements.Add(TEXT("CD6"));
		CountDownAnnouncements.Add(TEXT("CD7"));
		CountDownAnnouncements.Add(TEXT("CD8"));
		CountDownAnnouncements.Add(TEXT("CD9"));
		CountDownAnnouncements.Add(TEXT("CD10"));
		CountDownAnnouncements.Add(TEXT("ThirtySecondsRemain"));
		CountDownAnnouncements.Add(TEXT("1MinRemains"));
		CountDownAnnouncements.Add(TEXT("3MinRemains"));
		CountDownAnnouncements.Add(TEXT("FiveMinuteWarning"));
	}

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		return (Switch >= 0 && Switch < CountDownAnnouncements.Num() ? CountDownAnnouncements[Switch] : NAME_None);
	}

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		return (Switch >= 0 && Switch < CountDownText.Num() ? CountDownText[Switch] : FText::GetEmpty());
	}		
};