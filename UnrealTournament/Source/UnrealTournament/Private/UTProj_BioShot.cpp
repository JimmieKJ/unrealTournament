// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_BioShot.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"
#include "UTLift.h"
#include "UTWeap_LinkGun.h"
#include "UTProjectileMovementComponent.h"
#include "StatNames.h"
#include "UTRewardMessage.h"
#include "UTAvoidMarker.h"

AUTProj_BioShot::AUTProj_BioShot(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProjectileMovement->InitialSpeed = 4000.0f;
	ProjectileMovement->MaxSpeed = 6000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;
	ProjectileMovement->ProjectileGravityScale = 0.9f;

	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->Bounciness = 0.1f;
	ProjectileMovement->Friction = 0.008f;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 140.f;
	ProjectileMovement->Buoyancy = 1.1f;

	BioMesh = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, TEXT("BioGooComp"));
	if (BioMesh != NULL)
	{
		BioMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
		BioMesh->CanCharacterStepUpOn = ECB_No;
		BioMesh->bReceivesDecals = false;
		BioMesh->CastShadow = false;
		BioMesh->AttachParent = RootComponent;
	}

	CollisionComp->bAbsoluteScale = true;
	DamageParams.BaseDamage = 21.0f;

	Momentum = 40000.0f;
	MaxSpeedUnderWater = 3000.f;
	SurfaceNormal = FVector(0.0f, 0.0f, 1.0f);
	SurfaceType = EHitType::HIT_None;
	SurfaceWallThreshold = 0.3f;

	RestTime = 7.f;
	bAlwaysShootable = true;

	GlobStrength = 1.f;
	MaxRestingGlobStrength = 6;
	DamageRadiusGain = 60.0f;
	InitialLifeSpan = 0.f;
	ExtraRestTimePerStrength = 1.5f;

	SplashSpread = 0.8f;
	bSpawningGloblings = false;
	bLanded = false;
	bHasMerged = false;
	bCanTrack = false;
	WebLifeBoost = 10.f; 
	WebLifeLowThreshold = 4.f;
	MaxLinkDistance = 1000.f;

	LandedOverlapRadius = 16.f;
	LandedOverlapScaling = 16.f;
	MaxSlideSpeed = 1500.f;
	TrackingRange = 2000.f;
	MaxTrackingSpeed = 1200.f;
	ProjectileMovement->HomingAccelerationMagnitude = 1200.f;
	UUTProjectileMovementComponent* UTProjMovement = Cast<UUTProjectileMovementComponent>(ProjectileMovement);
	if (UTProjMovement)
	{
		UTProjMovement->bPreventZHoming = true;
	}
	GlobRadiusScaling = 4.f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	BlobPulseTime = 0.f;
	InitialBlobPulseRate = 3.f;
	MaxBlobPulseRate = 8.f;
	BlobOverCharge = 0.f;
	BlobWebThreshold = 100.f;
	BlobPulseTime = 0.f;
	BlobPulseScaling = 0.1f;
	bFoundWebLink = false;

	BaseBeamColor = FLinearColor(0.2f, 0.33f, 0.07f, 1.f);
	LowLifeBeamColor = FLinearColor(0.1f, 0.5f, 0.f, 0.01f);
	ChargingBeamColor = FLinearColor(100.f, 500.f, 100.f, 1.f);
	FriendlyBeamColor = FLinearColor(200.f, 300.f, 0.f, 1.f);
	WebCollisionRadius = 20.f;
	bReplaceLinkTwo = false;
	LastFriendlyHitTime = 0.f;
}

void AUTProj_BioShot::BeginPlay()
{
	Super::BeginPlay();
	if (!IsPendingKillPending())
	{
		SetGlobStrength(GlobStrength);

		// failsafe if never land
		RemainingLife = RestTime;
		bSpawningGloblings = false;
		SurfaceNormal = GetActorRotation().Vector();
/*
		if (GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
			if (PC != NULL && PC->MyHUD != NULL)
			{
				PC->MyHUD->AddPostRenderedActor(this);
			}
		}*/
	}
}

void AUTProj_BioShot::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	Super::OnBounce(ImpactResult, ImpactVelocity);
	ProjectileMovement->MaxSpeed = MaxSlideSpeed;
	bCanTrack = true;

	// only one bounce sound
	BounceSound = NULL;
}

bool AUTProj_BioShot::DisableEmitterLights() const
{
	return true;
}

void AUTProj_BioShot::Destroyed()
{
	Super::Destroyed();

	if (FearSpot != NULL)
	{
		FearSpot->Destroy();
	}

	while (WebLinks.Num() > 0)
	{
		RemoveWebLink(WebLinks[0].LinkedBio);
	}

/*
	if (GetWorld()->GetNetMode() != NM_DedicatedServer && GEngine->GetWorldContextFromWorld(GetWorld()) != NULL) // might not be able to get world context when exiting PIE
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->RemovePostRenderedActor(this);
		}
	}*/

	// @TODO FIXMESTEVE - fix up WebChild list?
}

void AUTProj_BioShot::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	float XL, YL;

	float Scale = Canvas->ClipX / 1920;

	UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->MediumFont;
	Canvas->TextSize(TinyFont, *GetName(), XL, YL, Scale, Scale);

	FVector ScreenPosition = Canvas->Project(GetActorLocation() + FVector(0, 0, 15.f));
	float XPos = ScreenPosition.X - (XL * 0.5);
	if (XPos < Canvas->ClipX || XPos + XL < 0.0f)
	{
		FCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + ScreenPosition.Y - YL)), FText::FromString(GetName()), TinyFont, FLinearColor::White);
		TextItem.Scale = FVector2D(Scale, Scale);
		TextItem.BlendMode = SE_BLEND_Translucent;
		TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
		Canvas->DrawItem(TextItem);
	}
}

void AUTProj_BioShot::ShutDown()
{
	Super::ShutDown();
	if (!bPendingKillPending)
	{
		while (WebLinks.Num() > 0)
		{
			RemoveWebLink(WebLinks[0].LinkedBio);
		}
	}
}

void AUTProj_BioShot::RemoveWebLink(AUTProj_BioShot* LinkedBio)
{
	if (bRemovingWebLink)
	{
		// prevent re-entry
		return;
	}

	// find the associated link
	bool bFoundLink = false;
	int32 i = 0;
	for (i=0; i<WebLinks.Num(); i++)
	{
		if (WebLinks[i].LinkedBio == LinkedBio)
		{
			bFoundLink = true;
			break;
		}
	}
	if (WebLinkOne == LinkedBio)
	{
		WebLinkOne = NULL;
	}
	if (WebLinkTwo == LinkedBio)
	{
		WebLinkTwo = NULL;
	}

	if (bFoundLink)
	{
		// remove the link
		bRemovingWebLink = true;
		if (WebLinks[i].LinkedBio && !WebLinks[i].LinkedBio->IsPendingKillPending())
		{
			WebLinks[i].LinkedBio->RemoveWebLink(this);
			WebLinks[i].LinkedBio = NULL;
		}
		if (WebLinks[i].WebLink)
		{
			WebLinks[i].WebLink->DeactivateSystem();
			WebLinks[i].WebLink->bAutoDestroy = true;
		}
		if (WebLinks[i].WebCapsule != NULL)
		{
			WebLinks[i].WebCapsule->DestroyComponent();
			WebLinks[i].WebCapsule = NULL;
		}
		WebLinks.RemoveAt(i);
		bRemovingWebLink = false;
	}
}

void AUTProj_BioShot::OnRep_WebLinkOne()
{
	if (SavedWebLinkOne)
	{
		RemoveWebLink(SavedWebLinkOne);
		SavedWebLinkOne = NULL;
	}
	if (WebLinkOne && !WebLinkOne->IsPendingKillPending() && !WebLinkOne->bExploded)
	{
		AddWebLink(WebLinkOne);
		SavedWebLinkOne = WebLinkOne;
	}
}

void AUTProj_BioShot::OnRep_WebLinkTwo()
{
	if (SavedWebLinkTwo)
	{
		RemoveWebLink(SavedWebLinkTwo);
		SavedWebLinkTwo = NULL;
	}
	if (WebLinkTwo && !WebLinkTwo->IsPendingKillPending() && !WebLinkTwo->bExploded)
	{
		AddWebLink(WebLinkTwo);
		SavedWebLinkTwo = WebLinkTwo;
	}
}

void AUTProj_BioShot::OnRep_WebMaster()
{
	if (WebMaster && !WebMaster->IsPendingKillPending() && !WebMaster->bExploded)
	{
		AddWebLink(WebMaster);
	}
}

bool AUTProj_BioShot::AddWebLink(AUTProj_BioShot* LinkedBio)
{
	if (bAddingWebLink || !LinkedBio)
	{
		// no re-entry
		return false;
	}
	static FName NAME_HitLocation(TEXT("HitLocation"));
	static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));

	// find already existing associated link
	int32 i = 0;
	for (i = 0; i<WebLinks.Num(); i++)
	{
		if (WebLinks[i].LinkedBio == LinkedBio)
		{
			return false;
		}
	}

	bCanTrack = false;
	bAddingWebLink = true;
	if (WebLinks.Num() == 0)
	{
		// lifespan boost for being connected to web
		RemainingLife += WebLifeBoost;
	}
	UParticleSystemComponent* NewWebLinkEffect = NULL;
	UCapsuleComponent* NewCapsule = NULL;
	if (this != LinkedBio->WebMaster)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), WebLinkSound, this, ESoundReplicationType::SRT_None);
	}

	if (!LinkedBio->bAddingWebLink)
	{
		float Dist2D = (GetActorLocation() - LinkedBio->GetActorLocation()).Size2D();
		NewWebLinkEffect = UGameplayStatics::SpawnEmitterAttached(WebLinkEffect, RootComponent, NAME_None, GetActorLocation(), (LinkedBio->GetActorLocation() - GetActorLocation()).Rotation(), EAttachLocation::KeepWorldPosition, false);
		if (NewWebLinkEffect)
		{
			NewWebLinkEffect->bAutoActivate = false;
			NewWebLinkEffect->bAutoDestroy = false;
			NewWebLinkEffect->SecondsBeforeInactive = 0.0f;
			NewWebLinkEffect->ActivateSystem();
			NewWebLinkEffect->SetVectorParameter(NAME_HitLocation, LinkedBio->GetActorLocation());
			NewWebLinkEffect->SetVectorParameter(NAME_LocalHitLocation, NewWebLinkEffect->ComponentToWorld.InverseTransformPositionNoScale(LinkedBio->GetActorLocation()));
			NewWebLinkEffect->SetWorldRotation((LinkedBio->GetActorLocation() - GetActorLocation()).Rotation());
			static FName NAME_BeamColor("BeamColor");
			CurrentBeamColor = BaseBeamColor;
			NewWebLinkEffect->SetColorParameter(NAME_BeamColor, CurrentBeamColor);
		}
		if (Role == ROLE_Authority)
		{
			NewCapsule = NewObject<UCapsuleComponent>(this);
			NewCapsule->SetCapsuleSize(WebCollisionRadius, 0.5f*(LinkedBio->GetActorLocation() - GetActorLocation()).Size(), false);
			NewCapsule->RegisterComponent();
			NewCapsule->OnComponentBeginOverlap.AddDynamic(this, &AUTProj_BioShot::OnWebOverlapBegin);
			FRotator WebRot = (LinkedBio->GetActorLocation() - GetActorLocation()).Rotation();
			WebRot.Pitch += 90.f;
			NewCapsule->SetWorldLocationAndRotation(0.5f*(LinkedBio->GetActorLocation() + GetActorLocation()), WebRot);
			NewCapsule->BodyInstance.SetCollisionProfileName("Projectile");
			NewCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		// replication support (temp)
		if ((Role == ROLE_Authority) && (this != LinkedBio->WebMaster) && (WebMaster != LinkedBio))
		{
			if (!WebLinkOne)
			{
				WebLinkOne = LinkedBio;
			}
			else if (!WebLinkTwo)
			{
				WebLinkTwo = LinkedBio;
			}
			else if (!LinkedBio->WebLinkOne)
			{
				LinkedBio->WebLinkOne = this;
			}
			else if (!LinkedBio->WebLinkTwo)
			{
				LinkedBio->WebLinkTwo = this;
			}
			else if (bReplaceLinkTwo)
			{
				RemoveWebLink(WebLinkTwo);
				WebLinkTwo = LinkedBio;
				bReplaceLinkTwo = false;
			}
			else
			{
				RemoveWebLink(WebLinkOne);
				WebLinkOne = LinkedBio;
				bReplaceLinkTwo = true;
			}
		}
	}

	new(WebLinks) FBioWebLink(LinkedBio, NewWebLinkEffect, NewCapsule);
	LinkedBio->AddWebLink(this);
	bAddingWebLink = false;
	return true;
}

void AUTProj_BioShot::OnWebOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	OnOverlapBegin(OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

bool AUTProj_BioShot::CanWebLinkTo(AUTProj_BioShot* LinkedBio)
{
	if (LinkedBio && (LinkedBio != this) && !LinkedBio->IsPendingKillPending() && (GetActorLocation() - LinkedBio->GetActorLocation()).Size() < MaxLinkDistance)
	{
		// verify line of sight
		FHitResult Hit;
		static FName NAME_BioLinkTrace(TEXT("BioLinkTrace"));

		bool bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + 2.f*SurfaceNormal, LinkedBio->GetActorLocation() + 2.f*LinkedBio->SurfaceNormal, COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_BioLinkTrace, true, this));
		return (!bBlockingHit || Cast<AUTProj_BioShot>(Hit.Actor.Get()));
	}
	return false;
}

bool AUTProj_BioShot::ComponentCanBeDamaged(const FHitResult& Hit, float DamageRadius)
{
	if ((Hit.Component == CollisionComp) || (Hit.Component == PawnOverlapSphere))
	{
		return true;
	}
	return ((GetActorLocation() - Hit.ImpactPoint).SizeSquared() < FMath::Square(0.5f*DamageRadius + 22.f));
}

float AUTProj_BioShot::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	if (Role == ROLE_Authority)
	{
		if (InstigatorController && EventInstigator && Cast<AUTWeap_LinkGun>(DamageCauser))
		{
			if ((GlobStrength >= 1.f) && bLanded)
			{
				AUTGameState* GameState = InstigatorController->GetWorld()->GetGameState<AUTGameState>();
				if ((EventInstigator == InstigatorController) || (GameState && GameState->OnSameTeam(InstigatorController, EventInstigator)))
				{
					// ignore link damage from teammates and start linking process
					AUTWeap_LinkGun *Linker = Cast<AUTWeap_LinkGun>(DamageCauser);
					if (Linker->LinkedBio && CanWebLinkTo(Linker->LinkedBio))
					{
						if (AddWebLink(Linker->LinkedBio))
						{
							if (Linker->LinkedBio->WebLinkOne && (Linker->LinkedBio->WebLinkOne != this))
							{
								AddWebLink(Linker->LinkedBio->WebLinkOne);
							}
							else if (Linker->LinkedBio->WebLinkTwo && (Linker->LinkedBio->WebLinkTwo != this))
							{
								AddWebLink(Linker->LinkedBio->WebLinkTwo);
							}
							else
							{
								for (int32 i = 0; i<Linker->LinkedBio->WebLinks.Num(); i++)
								{
									if ((Linker->LinkedBio->WebLinks[i].LinkedBio != this) && (Linker->LinkedBio->WebLinks[i].LinkedBio != NULL))
									{
										AddWebLink(Linker->LinkedBio->WebLinks[i].LinkedBio);
										break;
									}
								}
							}
						}
					}
					if (GlobStrength >= MaxRestingGlobStrength)
					{
						BlobOverCharge += DamageAmount;
						if (BlobOverCharge >= BlobWebThreshold)
						{
							BlobOverCharge = 0.f;
							FVector ShotDirection = (DamageEvent.IsOfType(FUTPointDamageEvent::ClassID))
								? ((const FUTPointDamageEvent&)DamageEvent).ShotDirection
								: FVector_NetQuantizeNormal(0.f, 0.f, 1.f);
							BlobToWeb(ShotDirection);
							return DamageAmount;
						}
					}
					else if (Linker && (WebLinks.Num() > 0))
					{
						PropagateCharge(WebLifeBoost * DamageAmount * 0.01f);
					}

					Linker->LinkedBio = this;
					return 0.f;
				}
			}
		}
		if (WebLinks.Num() > 0)
		{
			if (DamageEvent.IsOfType(FUTPointDamageEvent::ClassID) && !ComponentCanBeDamaged(((const FUTPointDamageEvent&)DamageEvent).HitInfo, 0.f))
			{
				return 0.f;
			}
			else if (DamageEvent.IsOfType(FUTRadialDamageEvent::ClassID))
			{
				bool bFoundCollisionComp = false;
				for (int32 i = 0; i < ((const FUTRadialDamageEvent&)DamageEvent).ComponentHits.Num(); i++)
				{
					if (ComponentCanBeDamaged(((const FUTRadialDamageEvent&)DamageEvent).ComponentHits[i], ((const FUTRadialDamageEvent&)DamageEvent).Params.OuterRadius))
					{
						bFoundCollisionComp = true;
						break;
					}
				}
				if (!bFoundCollisionComp)
				{
					return 0.f;
				}
			}
		}
		if ((bLanded || TrackedPawn) && !bExploded && (DamageAmount > 12.f))
		{
			Explode(GetActorLocation(), SurfaceNormal);
		}
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AUTProj_BioShot::PropagateCharge(float ChargeAmount)
{
	if (GetWorld()->GetTimeSeconds() == LastWebChargeTime)
	{
		return;
	}
	LastWebChargeTime = GetWorld()->GetTimeSeconds();

	// link charging
	if (RemainingLife < WebLifeBoost + RestTime)
	{
		bIsLinkCharging = true;
		LinkChargeEndTime = GetWorld()->GetTimeSeconds() + 0.3f;
		RemainingLife = FMath::Min(WebLifeBoost + RestTime, RemainingLife + ChargeAmount);
	}
	for (int32 i = 0; i < WebLinks.Num(); i++)
	{
		if (WebLinks[i].LinkedBio && !WebLinks[i].LinkedBio->IsPendingKillPending() && (WebLinks[i].LinkedBio->RemainingLife < WebLifeBoost + RestTime))
		{
			WebLinks[i].LinkedBio->PropagateCharge(ChargeAmount);
		}
	}
}

void AUTProj_BioShot::BlobToWeb(const FVector& NormalDir)
{
	FActorSpawnParameters Params;
	Params.Instigator = Instigator;
	Params.Owner = Instigator;

	//Spawn globlings for as many globs above 1, and weblink them
	bSpawningGloblings = true;
	//Adjust a bit so globlings don't spawn in the floor
	FVector FloorOffset = GetActorLocation() + (SurfaceNormal * 10.0f);
	int32 NumGloblings = int32(GlobStrength);
	FVector CrossVector = (NormalDir ^ SurfaceNormal).GetSafeNormal();

	for (int32 i = 0; i<NumGloblings; i++)
	{
		// 2d splash in plane with link beam as normal 
		float Offset = -0.3f + 1.5f*float(NumGloblings - 2.f*i) / float(NumGloblings);
		FVector Dir = SurfaceNormal * FMath::Cos(Offset) + CrossVector * FMath::Sin(Offset);
		AUTProj_BioShot* Globling = GetWorld()->SpawnActor<AUTProj_BioShot>(GetClass(), FloorOffset, Dir.Rotation(), Params);
		if (Globling)
		{
			Globling->bSpawningGloblings = true; 
			Globling->ProjectileMovement->InitialSpeed *= 0.4f;
			Globling->ProjectileMovement->Velocity *= 0.4f;
			Globling->WebMaster = this;
		}
	}
	bSpawningGloblings = false;
	SetGlobStrength(1.f);
	RemainingLife = RestTime + 0.5f;  // to match this blobs remaining life with web
}

void AUTProj_BioShot::BioStabilityTimer()
{
	if (!bExploded && (!bFakeClientProjectile || !MasterProjectile) && (Role== ROLE_Authority))
	{
		Explode(GetActorLocation(), SurfaceNormal);
	}
}

void AUTProj_BioShot::OnRep_BioLanded()
{
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->Velocity = FVector::ZeroVector;
}

void AUTProj_BioShot::Landed(UPrimitiveComponent* HitComp, const FVector& HitLocation)
{
	if (!bLanded)
	{
		bCanTrack = true;
		bLanded = true;
		bCanHitInstigator = true;

		//Change the collision so that weapons make it explode
		CollisionComp->SetCollisionProfileName("ProjectileShootable");

		//Rotate away from the floor
		FRotator NormalRotation = (SurfaceNormal).Rotation();
		NormalRotation.Roll = FMath::FRand() * 360.0f;	//Random spin
		SetActorRotation(NormalRotation);

		//Spawn Effects
		OnLanded();
		if ((GetNetMode() != NM_DedicatedServer) && !MyFakeProjectile)
		{
			if (GlobStrength > 2.5f)
			{
				LandedEffects = LargeLandedEffects;
			}
			if (LandedEffects)
			{
				LandedEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(NormalRotation, HitLocation), HitComp, this, InstigatorController);
			}
		}

		//Start the explosion timer
		if (GlobStrength < 1.f)
		{
			BioStabilityTimer();
		}
		else if (!bPendingKillPending)
		{
			//Stop the projectile
			ProjectileMovement->ProjectileGravityScale = 0.0f;
			ProjectileMovement->Velocity = FVector::ZeroVector;

			if (Role == ROLE_Authority)
			{
				bReplicateUTMovement = true;
				RemainingLife = RestTime + (GlobStrength - 1.f) * ExtraRestTimePerStrength;
				SetGlobStrength(GlobStrength);
				FActorSpawnParameters Params;
				Params.Owner = this;
				FearSpot = GetWorld()->SpawnActor<AUTAvoidMarker>(GetActorLocation(), GetActorRotation(), Params);
			}
			else if (BioMesh)
			{
				float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
				FVector ScalingMesh = FVector(1.5f*GlobScalingSqrt);
				if (bLanded)
				{
					ScalingMesh.X *= 0.85f;
				}
				BioMesh->SetRelativeScale3D(ScalingMesh);
			}

			if (WebMaster && CanWebLinkTo(WebMaster))
			{
				AddWebLink(WebMaster);

				if (Role == ROLE_Authority)
				{
					WebChild = WebMaster->WebChild;
					WebMaster->WebChild = this;
					FindWebLink();

					if (!bFoundWebLink)
					{
						// try later
						FTimerHandle TempHandle;
						GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_BioShot::FindWebLink, 1.f, false);
					}
				}
			}
		}
	}
	// uncomment to easily test tracking (also remove instigator checks in Track())
	//Track(Cast<AUTCharacter>(Instigator));
}

void AUTProj_BioShot::FindWebLink()
{
	// connect to furthest with no weblink yet - skip if on same surface
	AUTProj_BioShot* FurthestBio = NULL;
	float FurthestDist = 0.f;
	AUTProj_BioShot* NextBio = WebChild;
	while (NextBio)
	{
		if (!NextBio->IsPendingKillPending() && ((NextBio->SurfaceNormal | SurfaceNormal) < 0.98f))
		{
			float Dist = (NextBio->GetActorLocation() - GetActorLocation()).SizeSquared();
			if (!FurthestBio || ((Dist > FurthestDist) && CanWebLinkTo(NextBio)))
			{
				FurthestBio = NextBio;
				FurthestDist = Dist;
			}
		}
		NextBio = NextBio->WebChild;
	}
	if (FurthestBio)
	{
		AddWebLink(FurthestBio);
		bFoundWebLink = true;
	}
}

void AUTProj_BioShot::OnLanded_Implementation()
{
}

bool AUTProj_BioShot::CanInteractWithBio()
{
	return (!bExploded && !bHasMerged && !bSpawningGloblings && !bFakeClientProjectile && (Role == ROLE_Authority));
}

void AUTProj_BioShot::MergeWithGlob(AUTProj_BioShot* OtherBio)
{
	if (OtherBio == NULL || bPendingKillPending || OtherBio->bPendingKillPending || !CanInteractWithBio() || !OtherBio->CanInteractWithBio() || (GlobStrength < 1.f))
	{
		//Let the globlings pass through so they dont explode the glob, ignore exploded bio
		return;
	}
	// let the landed glob grow
	if (!bLanded && OtherBio->bLanded)
	{
		return;
	}
	OtherBio->bHasMerged = true;

	if (OtherBio->GlobStrength > GlobStrength)
	{
		InstigatorController = OtherBio->InstigatorController;
		Instigator = OtherBio->Instigator;
	}

	SetGlobStrength(GlobStrength + OtherBio->GlobStrength);
	if (TrackedPawn != NULL && !TrackedPawn->bPendingKillPending && !TrackedPawn->GetCapsuleComponent()->IsPendingKill()) // we check in tick but it's still possible they get gibbed first
	{
		ProjectileMovement->bIsHomingProjectile = true;
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
		ProjectileMovement->MaxSpeed = MaxTrackingSpeed / FMath::Sqrt(GlobStrength);
		ProjectileMovement->HomingTargetComponent = TrackedPawn->GetCapsuleComponent(); // warning: TWeakObjectPtr will return NULL if this is already pending kill!
		ProjectileMovement->bShouldBounce = true;
		bLanded = false;
		if (ProjectileMovement->Velocity.Size() < 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold)
		{
			ProjectileMovement->Velocity = 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold * (ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).GetSafeNormal();
		}
	}
	else if (OtherBio->TrackedPawn)
	{
		Track(OtherBio->TrackedPawn);
	}
		
	OtherBio->Destroy();
}

void AUTProj_BioShot::Track(AUTCharacter* NewTrackedPawn)
{
	if (IsPendingKillPending() || !bCanTrack || !NewTrackedPawn || NewTrackedPawn->bPendingKillPending || ((NewTrackedPawn->GetActorLocation() - GetActorLocation()).SizeSquared() > FMath::Square(TrackingRange)) || (NewTrackedPawn == Instigator) || !ProjectileMovement)
	{
		return;
	}
	if (InstigatorController)
	{
		// don't track teammates
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->OnSameTeam(InstigatorController, NewTrackedPawn))
		{
			return;
		}
	}

	ProjectileMovement->ProjectileGravityScale = 0.9f;
	// track closest
	if (!TrackedPawn || !ProjectileMovement->HomingTargetComponent.IsValid() || ((ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).SizeSquared() > (NewTrackedPawn->GetCapsuleComponent()->GetComponentLocation() - GetActorLocation()).SizeSquared()))
	{
		TrackedPawn = NewTrackedPawn;
		ProjectileMovement->bIsHomingProjectile = true;
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
		ProjectileMovement->MaxSpeed = MaxTrackingSpeed / FMath::Sqrt(GlobStrength);
		ProjectileMovement->HomingTargetComponent = TrackedPawn->GetCapsuleComponent();
		ProjectileMovement->bShouldBounce = true;
		bLanded = false;

		if (ProjectileMovement->Velocity.Size() < 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold)
		{
			ProjectileMovement->Velocity = 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold * (ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).GetSafeNormal();
		}

		if (FearSpot != NULL)
		{
			FearSpot->Destroy();
		}
		// warn AI of incoming projectile
		AUTBot* B = Cast<AUTBot>(TrackedPawn->Controller);
		if (B != NULL)
		{
			B->ReceiveProjWarning(this);
		}
	}
}

void AUTProj_BioShot::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	if (Role == ROLE_Authority)
	{
		RemainingLife -= DeltaTime;
		if (RemainingLife <= 0.f)
		{
			BioStabilityTimer();
			return;
		}
		bWebLifeLow = (RemainingLife < WebLifeLowThreshold);
		bFriendlyHit = (LastFriendlyHitTime > GetWorld()->GetTimeSeconds() - 0.5f);
		bIsLinkCharging = (LinkChargeEndTime > GetWorld()->GetTimeSeconds());
	}
	if (ProjectileMovement->bIsHomingProjectile)
	{
		if (!ProjectileMovement->HomingTargetComponent.IsValid() || ((ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).SizeSquared() > FMath::Square(1.5f * TrackingRange)))
		{
			ProjectileMovement->HomingTargetComponent = NULL;
			ProjectileMovement->bIsHomingProjectile = false;
			TrackedPawn = NULL;
			bLanded = true;
			if (FearSpot == NULL)
			{
				FActorSpawnParameters Params;
				Params.Owner = this;
				FearSpot = GetWorld()->SpawnActor<AUTAvoidMarker>(GetActorLocation(), GetActorRotation(), Params);
			}
		}
	}
	else
	{
		if (GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			static FName NAME_BeamColor("BeamColor");
			float PulseMag = FMath::Sin(GetWorld()->GetTimeSeconds()*8.f);
			for (int32 i = 0; i<WebLinks.Num(); i++)
			{
				if (WebLinks[i].WebLink)
				{
					if (bFriendlyHit || WebLinks[i].LinkedBio->bFriendlyHit)
					{
						CurrentBeamColor = FriendlyBeamColor;
					}
					else if (bIsLinkCharging || WebLinks[i].LinkedBio->bIsLinkCharging)
					{
						CurrentBeamColor = (PulseMag > 0.f) ? BaseBeamColor : ChargingBeamColor;
					}
					else if (bWebLifeLow || WebLinks[i].LinkedBio->bWebLifeLow)
					{
						CurrentBeamColor = (PulseMag > 0.f) ? BaseBeamColor : LowLifeBeamColor;
					}
					else
					{
						CurrentBeamColor = BaseBeamColor;
					}
					if (CurrentBeamColor != WebLinks[i].SetBeamColor)
					{
						WebLinks[i].WebLink->SetColorParameter(NAME_BeamColor, CurrentBeamColor);
						WebLinks[i].SetBeamColor = CurrentBeamColor;
					}
				}
			}
		}
	}

	if (BioMesh && ((GlobStrength >= MaxRestingGlobStrength) || ProjectileMovement->bIsHomingProjectile))
	{
		float PulseRate = InitialBlobPulseRate + BlobOverCharge * (MaxBlobPulseRate - InitialBlobPulseRate)/BlobWebThreshold;
		BlobPulseTime += DeltaTime * PulseRate;

		float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
		FVector GlobScaling(GlobScalingSqrt);
		GlobScaling.X *= 0.85f*(1.f + BlobPulseScaling*FMath::Square(FMath::Cos(BlobPulseTime)));
		GlobScaling.Z *= 1.3f*(1.f + BlobPulseScaling*FMath::Square(FMath::Sin(BlobPulseTime)));
		GlobScaling.Y = GlobScaling.Z;
		BioMesh->SetRelativeScale3D(1.5f*GlobScaling);
	}
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}


void AUTProj_BioShot::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// FIXME: temporary? workaround for synchronization issues on clients where it explodes on client but not on server
	if (GetNetMode() == NM_Client && (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL))
	{
		return;
	}

	APhysicsVolume* WaterVolume = Cast<APhysicsVolume>(OtherActor);
	if (WaterVolume && WaterVolume->bWaterVolume)
	{
		ProjectileMovement->MaxSpeed = MaxSpeedUnderWater;
	}

	if (bTriggeringWeb || ShouldIgnoreHit(OtherActor, OtherComp))
	{
		return;
	}
	if (bHasMerged)
	{
		ShutDown(); 
		return;
	}

	AUTProj_BioShot* OtherBio = Cast<AUTProj_BioShot>(OtherActor);
	if (OtherBio)
	{
		if (!bAddingWebLink && !OtherBio->bAddingWebLink)
		{
			MergeWithGlob(OtherBio);
		}
	}
	else if (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL)
	{
		AUTCharacter* TargetCharacter = Cast<AUTCharacter>(OtherActor);
		if (TargetCharacter && ((TargetCharacter != Instigator) || bCanHitInstigator))
		{
			if (WebLinks.Num() > 0)
			{
				// web ignores teammates and instigator
				if (TargetCharacter == Instigator)
				{
					if (Role == ROLE_Authority)
					{
						UUTGameplayStatics::UTPlaySound(GetWorld(), WebLinkSound, this, ESoundReplicationType::SRT_All);
					}
					LastFriendlyHitTime = GetWorld()->GetTimeSeconds();
					return;
				}
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS && GS->OnSameTeam(InstigatorController, TargetCharacter))
				{
					if (Role == ROLE_Authority)
					{
						UUTGameplayStatics::UTPlaySound(GetWorld(), WebLinkSound, this, ESoundReplicationType::SRT_All);
					}
					LastFriendlyHitTime = GetWorld()->GetTimeSeconds();
					return;
				}
			}
			if (!bFakeClientProjectile && (Role == ROLE_Authority) && (TargetCharacter != Instigator))
			{
				// tell nearby bio that is on ground @TODO FIXMESTEVE OPTIMIZE
				for (TActorIterator<AUTProj_BioShot> It(GetWorld()); It; ++It)
				{
					AUTProj_BioShot* Glob = *It;
					if (Glob != this)
					{
						Glob->Track(TargetCharacter);
					}
				}
			}
			bTriggeringWeb = true;
			for (int32 i = 0; i<WebLinks.Num(); i++)
			{
				if (WebLinks[i].LinkedBio)
				{
					WebLinks[i].LinkedBio->ProcessHit(OtherActor, OtherComp, HitLocation, HitNormal);
				}
			}
			// set different damagetype for charged shots
			MyDamageType = (GlobStrength > 1.f) ? ChargedDamageType : GetClass()->GetDefaultObject<AUTProjectile>()->MyDamageType;
			float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
			DamageParams = GetClass()->GetDefaultObject<AUTProjectile>()->DamageParams;
			DamageParams.BaseDamage *= GlobStrength;
			DamageParams.OuterRadius += DamageRadiusGain * (GlobStrength - 1.0f);
			Momentum = GetClass()->GetDefaultObject<AUTProjectile>()->Momentum * GlobScalingSqrt;
		}
		if ((Cast<AUTProjectile>(OtherActor) == NULL) || (WebLinks.Num() == 0))
		{
			Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
		}
	}
	else if (!bLanded)
	{
		//Determine if we hit a Wall/Floor/Ceiling
		SurfaceNormal = HitNormal;
		if (FMath::Abs(SurfaceNormal.Z) > SurfaceWallThreshold) // A wall will have a low Z in the HitNormal since it's a unit vector
		{
			SurfaceType = (SurfaceNormal.Z >= 0) ? HIT_Floor : HIT_Ceiling;
		}
		else
		{
			SurfaceType = HIT_Wall;
		}

		SetActorLocation(HitLocation + HitNormal);
		Landed(OtherComp, HitLocation);

		AUTLift* Lift = Cast<AUTLift>(OtherActor);
		if (Lift && Lift->GetEncroachComponent())
		{
			AttachRootComponentTo(Lift->GetEncroachComponent(), NAME_None, EAttachLocation::KeepWorldPosition);
		}
	}
}

void AUTProj_BioShot::DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	AUTCharacter* HitCharacter = Cast<AUTCharacter>(OtherActor);
	bool bPossibleReward = !bLanded && !bCanTrack && HitCharacter && AirSnotRewardClass && (HitCharacter->Health > 0) && (GlobStrength >= MaxRestingGlobStrength) && (Role == ROLE_Authority);
	bool bPossibleAirSnot = bPossibleReward && HitCharacter->GetCharacterMovement() != NULL && (HitCharacter->GetCharacterMovement()->MovementMode == MOVE_Falling) && (GetWorld()->GetTimeSeconds() - HitCharacter->FallingStartTime > 0.3f);
	int32 PreHitStack = HitCharacter ? HitCharacter->Health + HitCharacter->ArmorAmount : 0.f;
	Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	if (bPossibleReward && HitCharacter && (HitCharacter->Health <= 0))
	{
		// Air Snot reward
		AUTPlayerController* PC = Cast<AUTPlayerController>(InstigatorController);
		if (PC != NULL)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
			if (PS)
			{
				PS->ModifyStatsValue(NAME_AirSnot, 1);
			}
			int32 SnotRanking = 0;
			float SnotSkill = (GetWorld()->GetTimeSeconds() - CreationTime) * 4.f;
			if (bPossibleAirSnot)
			{
				SnotSkill += 2.f;
			}
			if (PreHitStack > 100)
			{
				SnotSkill += 2.f;
				if (PreHitStack > 200)
				{
					SnotSkill += 2.f;
					if (PreHitStack > 250)
					{
						SnotSkill += 2.f;
					}
				}
			}
			if (SnotSkill > 4.f)
			{
				SnotRanking = (SnotSkill > 6.f) ? 5 : 1;
			}
			PC->SendPersonalMessage(AirSnotRewardClass, SnotRanking);
		}
	}
}


void AUTProj_BioShot::OnRep_GlobStrength()
{
	if (Cast<AUTProj_BioShot>(MyFakeProjectile))
	{
		Cast<AUTProj_BioShot>(MyFakeProjectile)->SetGlobStrength(GlobStrength);
	}
	else
	{
		SetGlobStrength(GlobStrength);
	}
}

void AUTProj_BioShot::SetGlobStrength(float NewStrength)
{
	if (bHasMerged)
	{
		return;
	}
	float OldStrength = GlobStrength;
	GlobStrength = NewStrength;
	ProjectileMovement->bShouldBounce = (GlobStrength < 1.f);

	if (!bLanded && !TrackedPawn)
	{
		//Set the projectile speed here so the client can mirror the strength speed
		ProjectileMovement->InitialSpeed = GetClass()->GetDefaultObject<AUTProjectile>()->ProjectileMovement->InitialSpeed * (0.4f + GlobStrength) / (1.35f * GlobStrength);
		ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
	}
	// don't reduce remaining time for strength lost (i.e. SplashGloblings())
	else if (GlobStrength > OldStrength)
	{
		if (Role == ROLE_Authority)
		{
			RemainingLife = RemainingLife + (GlobStrength - OldStrength) * ExtraRestTimePerStrength;
		}
		// Glob merge effects
		UUTGameplayStatics::UTPlaySound(GetWorld(), MergeSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
		if (GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MergeEffect, GetActorLocation() - CollisionComp->GetUnscaledSphereRadius(), SurfaceNormal.Rotation(), true);
		}
	}

	//Increase The collision of the flying Glob if over a certain strength
	float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
	if (GlobStrength > 3.f)
	{
		CollisionComp->SetSphereRadius(GlobRadiusScaling * GlobScalingSqrt);
		if (!bLargeGlobLit)
		{
			bLargeGlobLit = true;
			TArray<ULightComponent*> LightComponents;
			GetComponents<ULightComponent>(LightComponents);
			for (int32 i = 0; i < LightComponents.Num(); i++)
			{
				LightComponents[i]->SetVisibility(true);
			}
		}
	}

	if (bLanded && (GlobStrength > MaxRestingGlobStrength))
	{
		if (Role == ROLE_Authority)
		{
			FActorSpawnParameters Params;
			Params.Instigator = Instigator;
			Params.Owner = Instigator;

			//Adjust a bit so globlings don't spawn in the floor
			FVector FloorOffset = GetActorLocation() + (SurfaceNormal * 10.0f);

			//Spawn globlings for as many Glob's above MaxRestingGlobStrength
			bSpawningGloblings = true;
			int32 NumGloblings = int32(GlobStrength) - MaxRestingGlobStrength;
			for (int32 i = 0; i<NumGloblings; i++)
			{
				FVector Dir = SurfaceNormal + FMath::VRand() * SplashSpread;
				AUTProj_BioShot* Globling = GetWorld()->SpawnActor<AUTProj_BioShot>(GetClass(), FloorOffset, Dir.Rotation(), Params);
				if (Globling)
				{
					Globling->bSpawningGloblings = true;
					Globling->ProjectileMovement->InitialSpeed *= 0.2f;
					Globling->ProjectileMovement->Velocity *= 0.2f;
				}
			}
			bSpawningGloblings = false;
		}
		GlobStrength = MaxRestingGlobStrength;
	}

	//Update any effects
	OnSetGlobStrength();

	if (bLanded)
	{
		PawnOverlapSphere->SetSphereRadius(LandedOverlapRadius + LandedOverlapScaling*GlobScalingSqrt, false);
		//PawnOverlapSphere->bHiddenInGame = false;
		//PawnOverlapSphere->bVisible = true;
	}
	if (BioMesh)
	{
		FVector ScalingMesh = FVector(1.5f*GlobScalingSqrt);
		if (bLanded)
		{
			ScalingMesh.X *= 0.85f;
		}
		BioMesh->SetRelativeScale3D(ScalingMesh);
	}
}

void AUTProj_BioShot::OnSetGlobStrength_Implementation()
{
}

void AUTProj_BioShot::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_BioShot, GlobStrength, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, WebLinkOne, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, WebLinkTwo, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, WebMaster, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, bIsLinkCharging, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, bWebLifeLow, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, bFriendlyHit, COND_None);
	DOREPLIFETIME(AUTProj_BioShot, bLanded);
}