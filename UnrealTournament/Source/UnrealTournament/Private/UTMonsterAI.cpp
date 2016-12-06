#include "UnrealTournament.h"
#include "UTMonsterAI.h"

void AUTMonsterAI::CheckWeaponFiring(bool bFromWeapon)
{
	// if invis, don't attack until close, under attack, or have flag
	if ( bFromWeapon || GetUTChar() == nullptr || !GetUTChar()->IsInvisible() || GetWorld()->TimeSeconds - GetUTChar()->LastTakeHitTime < 3.0f ||
		GetUTChar()->GetCarriedObject() != nullptr || GetTarget() == nullptr || (GetTarget()->GetActorLocation() - GetUTChar()->GetActorLocation()).Size() < 2000.0f )
	{
		Super::CheckWeaponFiring(bFromWeapon);
	}
}