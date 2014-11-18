// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAnnouncer.h"

UUTAnnouncer::UUTAnnouncer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AnnouncementComp = ObjectInitializer.CreateDefaultSubobject<UAudioComponent>(this, TEXT("AnnouncementComp"));
	AnnouncementComp->OnAudioFinished.AddDynamic(this, &UUTAnnouncer::AnnouncementFinished);

	Spacing = 0.5f;
}

void UUTAnnouncer::PlayAnnouncement(TSubclassOf<UUTLocalMessage> MessageClass, int32 Switch, const UObject* OptionalObject)
{
	if (MessageClass != NULL)
	{
		FName SoundName = MessageClass.GetDefaultObject()->GetAnnouncementName(Switch, OptionalObject);
		if (SoundName != NAME_None)
		{
			FAnnouncementInfo NewAnnouncement(MessageClass, Switch, OptionalObject);
			// if we should cancel the current announcement, then play the new one over top of it
			if (CurrentAnnouncement.MessageClass != NULL && MessageClass.GetDefaultObject()->InterruptAnnouncement(Switch, OptionalObject, CurrentAnnouncement.MessageClass, CurrentAnnouncement.Switch, CurrentAnnouncement.OptionalObject))
			{
				QueuedAnnouncements.Insert(NewAnnouncement, 0);
				PlayNextAnnouncement();
			}
			else
			{
				// see if we should cancel any existing announcements
				for (int32 i = QueuedAnnouncements.Num() - 1; i >= 0; i--)
				{
					if (MessageClass.GetDefaultObject()->InterruptAnnouncement(Switch, OptionalObject, QueuedAnnouncements[i].MessageClass, QueuedAnnouncements[i].Switch, QueuedAnnouncements[i].OptionalObject))
					{
						QueuedAnnouncements.RemoveAt(i);
					}
				}
				// add to the end
				QueuedAnnouncements.Add(NewAnnouncement);

				// play now if nothing in progress
				if (!GetWorld()->GetTimerManager().IsTimerActive(this, &UUTAnnouncer::PlayNextAnnouncement))
				{
					if (CurrentAnnouncement.MessageClass == NULL)
					{
						float Delay = MessageClass->GetDefaultObject<UUTLocalMessage>()->AnnouncementDelay;
						if (Delay > 0.f)
						{
							GetWorld()->GetTimerManager().SetTimer(this, &UUTAnnouncer::PlayNextAnnouncement, Delay, false);
						}
						else
						{
							PlayNextAnnouncement();
						}
					}
				}
			}
		}
	}
}

void UUTAnnouncer::AnnouncementFinished()
{
	CurrentAnnouncement = FAnnouncementInfo();
	GetWorld()->GetTimerManager().SetTimer(this, &UUTAnnouncer::PlayNextAnnouncement, Spacing, false);
}

void UUTAnnouncer::PlayNextAnnouncement()
{
	GetWorld()->GetTimerManager().ClearTimer(this, &UUTAnnouncer::PlayNextAnnouncement);
	if (QueuedAnnouncements.Num() > 0)
	{
		if (AnnouncementComp->IsPlaying())
		{
			// disable the delegate while interrupting to avoid recursion
			AnnouncementComp->OnAudioFinished.RemoveDynamic(this, &UUTAnnouncer::AnnouncementFinished);
			AnnouncementComp->Stop();
			AnnouncementComp->OnAudioFinished.AddDynamic(this, &UUTAnnouncer::AnnouncementFinished);
		}

		FAnnouncementInfo Next = QueuedAnnouncements[0];
		QueuedAnnouncements.RemoveAt(0);

		FName SoundName = Next.MessageClass.GetDefaultObject()->GetAnnouncementName(Next.Switch, Next.OptionalObject);
		if (SoundName != NAME_None)
		{
			USoundBase* Audio = NULL;
			USoundBase** CachePtr = CachedAudio.Find(SoundName);
			// note that we store a NULL in the map for sounds known to not exist
			if (CachePtr == NULL || (*CachePtr) != NULL)
			{
				if (CachePtr != NULL)
				{
					Audio = *CachePtr;
				}
				if (Audio == NULL)
				{
					for (int32 i = 0; i < AudioList.Num(); i++)
					{
						if (AudioList[i].SoundName == SoundName)
						{
							Audio = AudioList[i].Sound;
							break;
						}
					}
					if (Audio == NULL)
					{
						// make sure path ends with trailing slash
						if (!AudioPath.EndsWith(TEXT("/")))
						{
							AudioPath += TEXT("/");
						}
						Audio = LoadObject<USoundBase>(NULL, *(AudioPath + AudioNamePrefix + SoundName.ToString() + TEXT(".") + AudioNamePrefix + SoundName.ToString()), NULL, LOAD_NoWarn | LOAD_Quiet);
					}
				}
				if (CachePtr == NULL)
				{
					CachedAudio.Add(SoundName, Audio);
				}
				if (Audio != NULL)
				{
					AnnouncementComp->Sound = Audio;
					AnnouncementComp->Play();
					CurrentAnnouncement = Next;
				}
				else
				{
					CurrentAnnouncement = FAnnouncementInfo();
					PlayNextAnnouncement();
				}
			}
		}
	}
}