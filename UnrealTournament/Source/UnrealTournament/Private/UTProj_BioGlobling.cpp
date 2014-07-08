

#include "UnrealTournament.h"
#include "UTProj_BioGlob.h"
#include "UTProj_BioGlobling.h"


AUTProj_BioGlobling::AUTProj_BioGlobling(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	ProjectileMovement->InitialSpeed = 1000.0f;
	ProjectileMovement->MaxSpeed = 1000.0f;
	bCanHitInstigator = true;
}

void AUTProj_BioGlobling::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	//Pass through Globs
	if (OtherActor != NULL && Cast<AUTProj_BioGlob>(OtherActor) != NULL && OtherActor->Instigator == Instigator)
	{
		return;
	}
	else
	{
		Super::ProcessHit_Implementation( OtherActor, OtherComp, HitLocation, HitNormal);
	}
}

