// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.generated.h"

USTRUCT(BlueprintType)
struct FAnnouncementInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Announcement)
	TSubclassOf<class UUTLocalMessage> MessageClass;
	UPROPERTY(BlueprintReadWrite, Category = Announcement)
	int32 Switch;
	UPROPERTY(BlueprintReadWrite, Category = Announcement)
	const UObject* OptionalObject;

	FAnnouncementInfo()
		: MessageClass(NULL), Switch(0), OptionalObject(NULL)
	{}
	FAnnouncementInfo(TSubclassOf<UUTLocalMessage> InMessageClass, int32 InSwitch, const UObject* InOptionalObject)
		: MessageClass(InMessageClass), Switch(InSwitch), OptionalObject(InOptionalObject)
	{}
};

USTRUCT(BlueprintType)
struct FAnnouncerSound
{
	GENERATED_USTRUCT_BODY()

	/** the generic name of the sound that will be used to look up the audio */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
	FName SoundName;
	/** reference to the actual sound object */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundBase* Sound;
};

UENUM()
enum EAnnouncerType
{
	AT_Reward, // only supports reward announcements (multikill, etc)
	AT_Status, // only supports status announcements (red flag taken, etc)
	AT_All, // supports all announcements
};

UCLASS(Blueprintable, Abstract, Within=UTPlayerController)
class UNREALTOURNAMENT_API UUTAnnouncer : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual class UWorld* GetWorld() const override
	{
		return GetOuter()->GetWorld();
	}

	/** type of announcements supported */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcer)
	TEnumAsByte<EAnnouncerType> Type;
	/** audio path containing the announcer audio; all audio in this path must match the SoundName used by the various message types in order to be found */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcer)
	FString AudioPath;
	/** additional prefix for all sound names (since it needs to be applied twice - to file name and to asset name) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcer)
	FString AudioNamePrefix;
	/** array allowing manually matching SoundName to sound in case the naming convention wasn't followed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcer)
	TArray<FAnnouncerSound> AudioList;
	/** amount of time between the end of one announcement and the start of the next when there is a queue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Announcer)
	float Spacing;

	FTimerHandle PlayNextAnnouncementHandle;

	/** fast lookup to audio we've used previously */
	TMap<FName, USoundBase*> CachedAudio;

	/** currently playing announcement */
	FAnnouncementInfo CurrentAnnouncement;
	/** list of queued announcements */
	UPROPERTY(Transient)
	TArray<FAnnouncementInfo> QueuedAnnouncements;

	/** component used to control the audio */
	UPROPERTY()
	UAudioComponent* AnnouncementComp;

	virtual void PostInitProperties() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
	{
		Super::AddReferencedObjects(InThis, Collector);

		UUTAnnouncer* AnnThis = Cast<UUTAnnouncer>(InThis);
		for (TMap<FName, USoundBase*>::TIterator It(AnnThis->CachedAudio); It; ++It)
		{
			Collector.AddReferencedObject(It.Value(), AnnThis);
		}
	}

	inline bool IsRewardAnnouncer() const
	{
		return Type == AT_Reward || Type == AT_All;
	}
	inline bool IsStatusAnnouncer() const
	{
		return Type == AT_Status || Type == AT_All;
	}

	UFUNCTION(BlueprintCallable, Category = Announcement)
	virtual void PlayAnnouncement(TSubclassOf<UUTLocalMessage> MessageClass, int32 Switch, const UObject* OptionalObject);

	/** play next announcement in queue (if any)
	 * cancels any currently playing announcement
	 */
	UFUNCTION(BlueprintCallable, Category = Announcement)
	virtual void PlayNextAnnouncement();

	/** load and cache reference to announcement sound (if it exists) without playing it */
	UFUNCTION(BlueprintCallable, Category = Announcement)
	virtual void PrecacheAnnouncement(FName SoundName);

protected:
	/** called when announcement audio ends (set to AudioComponent delegate) */
	UFUNCTION()
	virtual void AnnouncementFinished();
};