// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequenceActor.h"


ALevelSequenceActor::ALevelSequenceActor(const FObjectInitializer& Init)
	: Super(Init)
{
#if WITH_EDITORONLY_DATA
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics() : DecalTexture(TEXT("/Engine/EditorResources/S_LevelSequence")) {}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
			SpriteComponent->AttachParent = RootComponent;
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->bAbsoluteScale = true;
			SpriteComponent->bReceivesDecals = false;
			SpriteComponent->bHiddenInGame = true;
			SetRootComponent(SpriteComponent);
		}
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = true;
	bAutoPlay = false;
}

void ALevelSequenceActor::BeginPlay()
{
	Super::BeginPlay();
	InitializePlayer();
}

#if WITH_EDITOR

bool ALevelSequenceActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	ULevelSequence* LevelSequenceAsset = Cast<ULevelSequence>(LevelSequence.TryLoad());
	if (LevelSequenceAsset)
	{
		Objects.Add(LevelSequenceAsset);
	}

	Super::GetReferencedContentObjects(Objects);

	return true;
}

#endif // WITH_EDITOR

void ALevelSequenceActor::Tick(float DeltaSeconds)
{
	if (SequencePlayer)
	{
		SequencePlayer->Update(DeltaSeconds);
	}
}

void ALevelSequenceActor::SetSequence(ULevelSequence* InSequence)
{
	if (!SequencePlayer || !SequencePlayer->IsPlaying())
	{
		LevelSequence = InSequence;
		InitializePlayer();
	}
}

void ALevelSequenceActor::InitializePlayer()
{
	ULevelSequence* LevelSequenceAsset = Cast<ULevelSequence>(LevelSequence.TryLoad());
	if (GetWorld()->IsGameWorld() && LevelSequenceAsset)
	{
		SequencePlayer = NewObject<ULevelSequencePlayer>(this, "AnimationPlayer");
		SequencePlayer->Initialize(LevelSequenceAsset, GetWorld(), PlaybackSettings);

		if (bAutoPlay)
		{
			SequencePlayer->Play();
		}
	}
}