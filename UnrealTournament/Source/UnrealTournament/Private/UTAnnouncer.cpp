// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAnnouncer.h"
#include "UTPlayerState.h"

UUTAnnouncer::UUTAnnouncer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AnnouncementComp = ObjectInitializer.CreateDefaultSubobject<UAudioComponent>(this, TEXT("AnnouncementComp"));
	AnnouncementComp->OnAudioFinished.AddDynamic(this, &UUTAnnouncer::AnnouncementFinished);
	AnnouncementComp->Priority = 10.f;
	AnnouncementComp->bAlwaysPlay = true;
	AnnouncementComp->bOverridePriority = true;
	static ConstructorHelpers::FObjectFinder<USoundClass> SoundClassFinder(TEXT("SoundClass'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/Announcer.Announcer'"));
	AnnouncementComp->SoundClassOverride = SoundClassFinder.Object;
}

void UUTAnnouncer::PostInitProperties()
{
	Super::PostInitProperties();

	if (!IsTemplate())
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			TSubclassOf<AUTGameMode> UTGameClass(*GS->GameModeClass);
			if (UTGameClass != NULL)
			{
				UTGameClass.GetDefaultObject()->PrecacheAnnouncements(this);
			}
		}
	}
}

void UUTAnnouncer::PlayAnnouncement(TSubclassOf<UUTLocalMessage> MessageClass, int32 Switch, const APlayerState* PlayerState1, const APlayerState* PlayerState2, const UObject* OptionalObject)
{
	if (MessageClass != NULL)
	{
		if (MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
		{
			UE_LOG(UT, Warning, TEXT("Play announcement %s %d"), *MessageClass->GetName(), Switch);
		}
		if (!MessageClass.GetDefaultObject()->ShouldPlayDuringIntermission(Switch))
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS && (!GS->IsMatchInProgress() || GS->IsMatchIntermission()))
			{
				if (MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
				{
					UE_LOG(UT, Warning, TEXT("SKIP FOR INTERMISSION"));
				}
				return;
			}
		}
		FName SoundName = MessageClass.GetDefaultObject()->GetAnnouncementName(Switch, OptionalObject, PlayerState1, PlayerState2);
		if (SoundName != NAME_None)
		{
			FAnnouncementInfo NewAnnouncement(MessageClass, Switch, PlayerState1, PlayerState2, OptionalObject, GetWorld()->GetTimeSeconds());
			// if we should cancel the current announcement, then play the new one over top of it
			if (CurrentAnnouncement.MessageClass != NULL && MessageClass.GetDefaultObject()->InterruptAnnouncement(NewAnnouncement, CurrentAnnouncement))
			{
				if (CurrentAnnouncement.MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
				{
					UE_LOG(UT, Warning, TEXT("%s %d immediate interrupting %s %d"), *MessageClass->GetName(), Switch, *CurrentAnnouncement.MessageClass->GetName(), CurrentAnnouncement.Switch);
				}
				for (int32 i = QueuedAnnouncements.Num() - 1; i >= 0; i--)
				{
					if (MessageClass.GetDefaultObject()->InterruptAnnouncement(NewAnnouncement, QueuedAnnouncements[i]))
					{
						if (QueuedAnnouncements[i].MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
						{
							UE_LOG(UT, Warning, TEXT("%s %d also interrupting %s %d"), *MessageClass->GetName(), Switch, *QueuedAnnouncements[i].MessageClass->GetName(), QueuedAnnouncements[i].Switch);
						}
						QueuedAnnouncements.RemoveAt(i);
					}
				}
				QueuedAnnouncements.Insert(NewAnnouncement, 0);
				StartNextAnnouncement(false);
			}
			else
			{
				bool bCancelThisAnnouncement = false;
				int32 InsertIndex = -1;
				float AnnouncementPriority = MessageClass.GetDefaultObject()->GetAnnouncementPriority(NewAnnouncement);
				// see if we should cancel any existing announcements
				if ((CurrentAnnouncement.MessageClass != NULL) && MessageClass.GetDefaultObject()->CancelByAnnouncement(Switch, OptionalObject, CurrentAnnouncement.MessageClass, CurrentAnnouncement.Switch, CurrentAnnouncement.OptionalObject))
				{
					if (MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
					{
						UE_LOG(UT, Warning, TEXT("%s %d cancelled by %s %d"), *MessageClass->GetName(), Switch, *CurrentAnnouncement.MessageClass->GetName(), CurrentAnnouncement.Switch);
					}
					bCancelThisAnnouncement = true;
				}
				else
				{
					for (int32 i = QueuedAnnouncements.Num() - 1; i >= 0; i--)
					{
						if (AnnouncementPriority > QueuedAnnouncements[i].MessageClass.GetDefaultObject()->GetAnnouncementPriority(QueuedAnnouncements[i]))
						{
							InsertIndex = i;
						}
						if (MessageClass.GetDefaultObject()->InterruptAnnouncement(NewAnnouncement, QueuedAnnouncements[i]))
						{
							if (QueuedAnnouncements[i].MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
							{
								UE_LOG(UT, Warning, TEXT("%s %d interrupting %s %d"), *MessageClass->GetName(), Switch, *QueuedAnnouncements[i].MessageClass->GetName(), QueuedAnnouncements[i].Switch);
							}
							QueuedAnnouncements.RemoveAt(i);
							InsertIndex = FMath::Min(InsertIndex, QueuedAnnouncements.Num());
						}
						else if (MessageClass.GetDefaultObject()->CancelByAnnouncement(Switch, OptionalObject, QueuedAnnouncements[i].MessageClass, QueuedAnnouncements[i].Switch, QueuedAnnouncements[i].OptionalObject))
						{
							if (MessageClass.GetDefaultObject()->EnableAnnouncerLogging())
							{
								UE_LOG(UT, Warning, TEXT("%s %d cancelled by %s %d"), *MessageClass->GetName(), Switch, *QueuedAnnouncements[i].MessageClass->GetName(), QueuedAnnouncements[i].Switch);
							}
							bCancelThisAnnouncement = true;
						}
					}
				}

				if (!bCancelThisAnnouncement)
				{
					// add to the end
					if (InsertIndex > 0)
					{
						QueuedAnnouncements.Insert(NewAnnouncement, InsertIndex);
					}
					else
					{
						QueuedAnnouncements.Add(NewAnnouncement);
					}

					// play now if nothing in progress
					if ((CurrentAnnouncement.MessageClass == NULL) && !GetWorld()->GetTimerManager().IsTimerActive(PlayNextAnnouncementHandle) && !AnnouncementComp->IsPlaying())
					{
						StartNextAnnouncement(false);
					}
				}
			}
		}
	}
}

void UUTAnnouncer::AnnouncementFinished()
{
	ReactionAnnouncement = (CurrentAnnouncement.MessageClass && CurrentAnnouncement.MessageClass.GetDefaultObject()->bWantsBotReaction) ? CurrentAnnouncement : FAnnouncementInfo();
	CurrentAnnouncement = FAnnouncementInfo();
	StartNextAnnouncement(true);
}

void UUTAnnouncer::ClearAnnouncements()
{
	if (AnnouncementComp->IsPlaying())
	{
		//UE_LOG(UT, Warning, TEXT("%s Interrupt announcement %s"), *GetName(), *AnnouncementComp->Sound->GetName());
		// disable the delegate while interrupting to avoid recursion
		AnnouncementComp->OnAudioFinished.RemoveDynamic(this, &UUTAnnouncer::AnnouncementFinished);
		AnnouncementComp->Stop();
		AnnouncementComp->OnAudioFinished.AddDynamic(this, &UUTAnnouncer::AnnouncementFinished);
	}
	GetWorld()->GetTimerManager().ClearTimer(PlayNextAnnouncementHandle);
	QueuedAnnouncements.Empty();
}

void UUTAnnouncer::StartNextAnnouncement(bool bUseSpacing)
{
	float AnnouncementDelay = 0.f;
	if (bUseSpacing && (QueuedAnnouncements.Num() > 0))
	{
		FAnnouncementInfo Next = QueuedAnnouncements[0];
		AnnouncementDelay = Next.MessageClass.GetDefaultObject()->GetAnnouncementSpacing(Next.Switch, Next.OptionalObject) - (GetWorld()->GetTimeSeconds() - Next.QueueTime);
	}
	if (AnnouncementDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(PlayNextAnnouncementHandle, this, &UUTAnnouncer::PlayNextAnnouncement, AnnouncementDelay, false);
	}
	else
	{
		PlayNextAnnouncement();
	}
}

void UUTAnnouncer::PlayNextAnnouncement()
{
	//UE_LOG(UT, Warning, TEXT("%s PlayNextAnnouncement"), *GetName());
	GetWorld()->GetTimerManager().ClearTimer(PlayNextAnnouncementHandle);
	if (QueuedAnnouncements.Num() > 0)
	{
		if (AnnouncementComp->IsPlaying())
		{
			//UE_LOG(UT, Warning, TEXT("%s Interrupt announcement %s"), *GetName(), *AnnouncementComp->Sound->GetName());
			// disable the delegate while interrupting to avoid recursion
			AnnouncementComp->OnAudioFinished.RemoveDynamic(this, &UUTAnnouncer::AnnouncementFinished);
			AnnouncementComp->Stop();
			AnnouncementComp->OnAudioFinished.AddDynamic(this, &UUTAnnouncer::AnnouncementFinished);
		}

		FAnnouncementInfo Next = QueuedAnnouncements[0];
		if (!Next.MessageClass.GetDefaultObject()->ShouldPlayDuringIntermission(Next.Switch))
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS && (!GS->IsMatchInProgress() || GS->IsMatchIntermission()))
			{
				QueuedAnnouncements.RemoveAt(0);
				if (QueuedAnnouncements.Num() > 0)
				{
					Next = QueuedAnnouncements[0];
				}
				else
				{
					return;
				}
			}
		}
		QueuedAnnouncements.RemoveAt(0);

		FName SoundName = Next.MessageClass.GetDefaultObject()->GetAnnouncementName(Next.Switch, Next.OptionalObject, Next.RelatedPlayerState_1, Next.RelatedPlayerState_2);
		USoundBase* Audio = NULL;
		if (SoundName == NAME_Custom)
		{
			Audio = Next.MessageClass.GetDefaultObject()->GetAnnouncementSound(Next.Switch, Next.OptionalObject);
		}
		else if (SoundName != NAME_None)
		{
/*			if ((StatusCachedAudio.Num() == 0) && (RewardCachedAudio.Num() == 0))
			{
				UE_LOG(UT, Warning, TEXT("Play announcement %s with no cached audio"), *SoundName.ToString());
			}*/
			USoundBase** CachePtr = Next.MessageClass.GetDefaultObject()->bIsStatusAnnouncement ? StatusCachedAudio.Find(SoundName) : RewardCachedAudio.Find(SoundName);
			// note that we store a NULL in the map for sounds known to not exist
			if (CachePtr == NULL || (*CachePtr) != NULL)
			{
				if (CachePtr != NULL)
				{
					Audio = *CachePtr;
				}
				if (Audio == NULL)
				{
					//UE_LOG(UT, Warning, TEXT("Audio not precached for %s is status %d"), *SoundName.ToString(), Next.MessageClass.GetDefaultObject()->bIsStatusAnnouncement);
					if (Next.MessageClass.GetDefaultObject()->bIsStatusAnnouncement)
					{
						for (int32 i = 0; i < StatusAudioList.Num(); i++)
						{
							if (StatusAudioList[i].SoundName == SoundName)
							{
								Audio = StatusAudioList[i].Sound;
								break;
							}
						}
					}
					else
					{
						for (int32 i = 0; i < RewardAudioList.Num(); i++)
						{
							if (RewardAudioList[i].SoundName == SoundName)
							{
								Audio = RewardAudioList[i].Sound;
								break;
							}
						}
					}
					if (Audio == NULL)
					{
						FString NewAudioPath = Next.MessageClass.GetDefaultObject()->bIsStatusAnnouncement ? StatusAudioPath : RewardAudioPath;
						FString NewAudioNamePrefix = Next.MessageClass.GetDefaultObject()->bIsStatusAnnouncement ? StatusAudioNamePrefix : RewardAudioNamePrefix;
						Audio = LoadAudio(NewAudioPath, NewAudioNamePrefix, SoundName);
					}
				}
				if (CachePtr == NULL)
				{
					if (Next.MessageClass.GetDefaultObject()->bIsStatusAnnouncement)
					{
						StatusCachedAudio.Add(SoundName, Audio);
					}
					else
					{
						RewardCachedAudio.Add(SoundName, Audio);
					}
				}
			}
		}
		if (Audio != NULL)
		{
			AnnouncementComp->Sound = Audio;
			AnnouncementComp->Play();
			CurrentAnnouncement = Next;
			//UE_LOG(UT, Warning, TEXT("%s Play announcement %s"), *GetName(), *AnnouncementComp->Sound->GetName());
		}
		else
		{
			//UE_LOG(UT, Warning, TEXT("NO AUDIO FOR SWITCH %d"), Next.Switch);
			CurrentAnnouncement = FAnnouncementInfo();
			StartNextAnnouncement(true);
		}
	}
	else if (ReactionAnnouncement.MessageClass != NULL)
	{
		RequestReaction();
		ReactionAnnouncement = FAnnouncementInfo();
	}
}

void UUTAnnouncer::RequestReaction()
{
	const AUTPlayerState* BestReacter = Cast<AUTPlayerState>(ReactionAnnouncement.RelatedPlayerState_2);
	const AUTPlayerState* ReactionPS = Cast<AUTPlayerState>(ReactionAnnouncement.RelatedPlayerState_1);
	if (BestReacter && ReactionPS)
	{
		BestReacter->AnnounceReactionTo(ReactionPS);
	}
	// target, live teammate or dead enemy
/*	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != NULL && PS->bIsABot)
		{

			if (!BestReacter || (!BestReacter->))
		}
	}*/
}

void UUTAnnouncer::PrecacheAnnouncement(FName SoundName)
{
	if (SoundName != NAME_None && StatusCachedAudio.Find(SoundName) == NULL)
	{
		USoundBase* Audio = NULL;
		for (int32 i = 0; i < StatusAudioList.Num(); i++)
		{
			if (StatusAudioList[i].SoundName == SoundName)
			{
				Audio = StatusAudioList[i].Sound;
				break;
			}
		}
		if (Audio == NULL)
		{
			Audio = LoadAudio(StatusAudioPath, StatusAudioNamePrefix, SoundName);
		}
		if (Audio != NULL)
		{
			StatusCachedAudio.Add(SoundName, Audio);
		}
	}
	if (SoundName != NAME_None && RewardCachedAudio.Find(SoundName) == NULL)
	{
		USoundBase* Audio = NULL;
		for (int32 i = 0; i < RewardAudioList.Num(); i++)
		{
			if (RewardAudioList[i].SoundName == SoundName)
			{
				Audio = RewardAudioList[i].Sound;
				break;
			}
		}
		if (Audio == NULL)
		{
			Audio = LoadAudio(RewardAudioPath, RewardAudioNamePrefix, SoundName);
		}
		if (Audio != NULL)
		{
			RewardCachedAudio.Add(SoundName, Audio);
		}
	}
}

USoundBase* UUTAnnouncer::LoadAudio(FString NewAudioPath, FString NewAudioNamePrefix, FName SoundName)
{
	// make sure path ends with trailing slash
	if (!NewAudioPath.EndsWith(TEXT("/")))
	{
		NewAudioPath += TEXT("/");
	}
	// manually check that the file exists to avoid spurious log warnings (the loading code seems to be ignoring LOAD_NoWarn | LOAD_Quiet)
	FString PackageName = NewAudioPath + NewAudioNamePrefix + SoundName.ToString();
	if (FPackageName::DoesPackageExist(PackageName))
	{
		return LoadObject<USoundBase>(NULL, *(PackageName + TEXT(".") + NewAudioNamePrefix + SoundName.ToString()), NULL, LOAD_NoWarn | LOAD_Quiet);
	}
	return NULL;
}
