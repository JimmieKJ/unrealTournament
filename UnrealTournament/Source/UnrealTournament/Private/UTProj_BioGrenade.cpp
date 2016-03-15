#include "UnrealTournament.h"
#include "UTProj_BioGrenade.h"
#include "UnrealNetwork.h"

AUTProj_BioGrenade::AUTProj_BioGrenade(const FObjectInitializer& OI)
	: Super(OI)
{
	ProximitySphere = OI.CreateOptionalDefaultSubobject<USphereComponent>(this, TEXT("ProxSphere"));
	if (ProximitySphere != NULL)
	{
		ProximitySphere->InitSphereRadius(200.0f);
		ProximitySphere->SetCollisionObjectType(COLLISION_PROJECTILE);
		ProximitySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ProximitySphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		ProximitySphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AUTProj_BioGrenade::ProximityOverlapBegin);
		ProximitySphere->bTraceComplexOnMove = false;
		ProximitySphere->bReceivesDecals = false;
		ProximitySphere->AttachParent = RootComponent;
	}

	TimeBeforeFuseStarts = 2.0f;
	FuseTime = 1.0f;
	ProximityDelay = 0.1f;
}

void AUTProj_BioGrenade::BeginPlay()
{
	Super::BeginPlay();

	SetTimerUFunc(this, FName(TEXT("StartFuseTimed")), TimeBeforeFuseStarts, false);
}

void AUTProj_BioGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTProj_BioGrenade, bBeginFuseWarning);
}

void AUTProj_BioGrenade::StartFuseTimed()
{
	if (!bBeginFuseWarning)
	{
		StartFuse();
	}
}
void AUTProj_BioGrenade::StartFuse()
{
	bBeginFuseWarning = true;
	
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->ProjectileGravityScale *= 0.5f;

	if (FuseEffect != NULL && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAttached(FuseEffect, RootComponent);
	}
	SetTimerUFunc(this, FName(TEXT("FuseExpired")), FuseTime, false);
	SetTimerUFunc(this, FName(TEXT("PlayFuseBeep")), 0.5f, true);
}
void AUTProj_BioGrenade::FuseExpired()
{
	if (!bExploded)
	{
		Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
}

void AUTProj_BioGrenade::PlayFuseBeep()
{
	if (!bExploded)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), FuseBeepSound, this, SRT_IfSourceNotReplicated);
	}
}

void AUTProj_BioGrenade::OnStop(const FHitResult& Hit)
{
	// intentionally blank
}

void AUTProj_BioGrenade::ProximityOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor != NULL && OtherActor != Instigator && !bBeginFuseWarning && GetWorld()->TimeSeconds - CreationTime >= ProximityDelay)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(OtherActor, InstigatorController))
		{
			FVector OtherLocation;
			if (bFromSweep)
			{
				OtherLocation = SweepResult.Location;
			}
			else
			{
				OtherLocation = OtherActor->GetActorLocation();
			}

			FCollisionQueryParams Params(FName(TEXT("PawnSphereOverlapTrace")), true, this);
			Params.AddIgnoredActor(OtherActor);

			// since PawnOverlapSphere doesn't hit blocking objects, it is possible it is touching a target through a wall
			// make sure that the hit is valid before proceeding
			if (!GetWorld()->LineTraceTestByChannel(OtherLocation, GetActorLocation(), COLLISION_TRACE_WEAPON, Params))
			{
				StartFuse();
			}
		}
	}
}