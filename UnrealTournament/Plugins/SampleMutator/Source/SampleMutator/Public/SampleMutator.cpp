#include "SampleMutator.h"

#include "UTPickupWeapon.h"

ASampleMutator::ASampleMutator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool ASampleMutator::CheckRelevance_Implementation(AActor* Other)
{
	// Find the rocket launcher blueprint
	if (RocketLauncherClass == nullptr)
	{
		RocketLauncherClass = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, TEXT("Blueprint'/Game/RestrictedAssets/Weapons/RocketLauncher/BP_RocketLauncher.BP_RocketLauncher_C'"));
	}

	// If a weapon pickup has a current weapon type, replace it with the rocket launcher
	AUTPickupWeapon *PickupWeapon = Cast<AUTPickupWeapon>(Other);
	if (PickupWeapon)
	{
		if (PickupWeapon->WeaponType != nullptr)
		{
			PickupWeapon->WeaponType = RocketLauncherClass;
		}
	}

	return Super::CheckRelevance_Implementation(Other);
}