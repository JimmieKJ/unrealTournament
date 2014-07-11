// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTJumpPad.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"

AUTJumpPad::AUTJumpPad(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	PrimaryActorTick.bCanEverTick = true;

	TSubobjectPtr<USceneComponent> SceneComponent = PCIP.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComponent"));
	RootComponent = SceneComponent;
	RootComponent->bShouldUpdatePhysicsVolume = true;

	// Setup the mesh
	Mesh = PCIP.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("JumpPadMesh"));
	Mesh->AttachParent = RootComponent;

	TriggerBox = PCIP.CreateDefaultSubobject<UBoxComponent>(this, TEXT("TriggerBox"));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->AttachParent = RootComponent;
	

	JumpSound = nullptr;
	JumpTarget = FVector(100.0f, 0.0f, 0.0f);
	JumpTime = 1.0f;
	bMaintainVelocity = false;

	AreaClass = UNavArea_Default::StaticClass();
#if WITH_EDITORONLY_DATA
	JumpPadComp = PCIP.CreateDefaultSubobject<UUTJumpPadRenderingComponent>(this, TEXT("JumpPadComp"));
	JumpPadComp->PostPhysicsComponentTick.bCanEverTick = false;
	JumpPadComp->AttachParent = RootComponent;
#endif // WITH_EDITORONLY_DATA
}

void AUTJumpPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Launch the pending actors
	if (PendingJumpActors.Num() > 0)
	{
		for (AActor* Actor : PendingJumpActors)
		{
			Launch(Actor);
		}
		PendingJumpActors.Reset();
	}
}

void AUTJumpPad::CheckForErrors()
{
	Super::CheckForErrors();

	FVector JumpVelocity = CalculateJumpVelocity(this);
	// figure out default game mode from which we will derive the default character
	TSubclassOf<AGameMode> GameClass = GetWorld()->GetWorldSettings()->DefaultGameMode;
	if (GameClass == NULL)
	{
		// horrible config hack around unexported function UGameMapsSettings::GetGlobalDefaultGameMode()
		FString GameClassPath;
		GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), GameClassPath, GEngineIni);
		GameClass = LoadClass<AGameMode>(NULL, *GameClassPath, NULL, 0, NULL);
	}
	const ACharacter* DefaultChar = (GameClass != NULL) ? Cast<ACharacter>(GameClass.GetDefaultObject()->DefaultPawnClass.GetDefaultObject()) : GetDefault<AUTCharacter>();
	if (DefaultChar != NULL && DefaultChar->CharacterMovement != NULL)
	{
		JumpVelocity *= FMath::Sqrt(DefaultChar->CharacterMovement->GravityScale);
	}
	// check if velocity is faster than physics will allow
	APhysicsVolume* PhysVolume = (RootComponent != NULL) ? RootComponent->GetPhysicsVolume() : GetWorld()->GetDefaultPhysicsVolume();
	if (JumpVelocity.Size() > PhysVolume->TerminalVelocity)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		Arguments.Add(TEXT("Speed"), FText::AsNumber(JumpVelocity.Size()));
		Arguments.Add(TEXT("TerminalVelocity"), FText::AsNumber(PhysVolume->TerminalVelocity));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("UTJumpPad", "TerminalVelocityWarning", "{ActorName} : Jump pad speed on default character would be {Speed} but terminal velocity is {TerminalVelocity}!"), Arguments)))
			->AddToken(FMapErrorToken::Create(FName(TEXT("JumpPadTerminalVelocity"))));
	}
}

bool AUTJumpPad::CanLaunch_Implementation(AActor* TestActor)
{
	return (Cast<ACharacter>(TestActor) != NULL && TestActor->Role >= ROLE_AutonomousProxy);
}

void AUTJumpPad::Launch_Implementation(AActor* Actor)
{
	// For now just filter for ACharacter. Maybe certain projectiles/vehicles/ragdolls/etc should bounce in the future
	ACharacter* Char = Cast<ACharacter>(Actor);
	if (Char != NULL)
	{
		//Launch the character to the target
		Char->LaunchCharacter(CalculateJumpVelocity(Char), !bMaintainVelocity, true);

		// Play Jump sound if we have one
		UUTGameplayStatics::UTPlaySound(GetWorld(), JumpSound, Char, SRT_AllButOwner, false);
	}
}

void AUTJumpPad::ReceiveActorBeginOverlap(AActor* OtherActor)
{
	Super::ReceiveActorBeginOverlap(OtherActor);

	// Add the actor to be launched if it hasn't already
	if (!PendingJumpActors.Contains(OtherActor) && CanLaunch(OtherActor))
	{
		PendingJumpActors.Add(OtherActor);
	}
}

FVector AUTJumpPad::CalculateJumpVelocity(AActor* JumpActor)
{
	FVector Target = ActorToWorld().TransformPosition(JumpTarget) - JumpActor->GetActorLocation();

	float SizeZ = Target.Z / JumpTime + 0.5f * -GetWorld()->GetGravityZ() * JumpTime;
	float SizeXY = Target.Size2D() / JumpTime;

	FVector Velocity = Target.SafeNormal2D() * SizeXY + FVector(0.0f, 0.0f, SizeZ);

	// Scale the velocity if Character has gravity scaled
	ACharacter* Char = Cast<ACharacter>(JumpActor);
	if (Char != NULL && Char->CharacterMovement != NULL && Char->CharacterMovement->GravityScale != 1.0f)
	{
		Velocity *= FMath::Sqrt(Char->CharacterMovement->GravityScale);
	}
	return Velocity;
}


#if WITH_EDITOR
void AUTJumpPad::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateNavigationRelevancy();

	if (PropertyChangedEvent.Property && IsNavigationRelevant() && GetWorld() && GetWorld()->GetNavigationSystem())
	{
		GetWorld()->GetNavigationSystem()->UpdateNavOctree(this);
	}
}
#endif // WITH_EDITOR


bool AUTJumpPad::UpdateNavigationRelevancy()
{
	PointLink = FNavigationLink(FVector(0.0f), JumpTarget);
	PointLink.Direction = ENavLinkDirection::LeftToRight;
	PointLink.AreaClass = AreaClass;

	SetNavigationRelevancy(true);
	return true;
}

bool AUTJumpPad::GetNavigationRelevantData(struct FNavigationRelevantData& Data) const
{
	TArray<FNavigationLink> TempList; 
	TempList.Add(PointLink);

	NavigationHelper::ProcessNavLinkAndAppend(&Data.Modifiers, this, TempList);
	return false;
}

bool AUTJumpPad::GetNavigationLinksClasses(TArray<TSubclassOf<class UNavLinkDefinition> >& OutClasses) const
{
	return false;
}

bool AUTJumpPad::GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const
{
	OutLink.Add(PointLink);
	return true;
}
