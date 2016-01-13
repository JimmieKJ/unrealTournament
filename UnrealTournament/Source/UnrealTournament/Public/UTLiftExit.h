// place to define a place that bots can get off a lift/mover
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"

#include "UTLiftExit.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTLiftExit : public AActor, public IUTPathBuilderInterface
{
	GENERATED_UCLASS_BODY()

	/** editor icon */
	UPROPERTY(VisibleAnywhere, Category = Sprite)
	UBillboardComponent* Icon;

	/** the lift that will take the bot places */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lift)
	class AUTLift* MyLift;
	/** if set, reaching this point requires a lift jump */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lift)
	bool bLiftJump;
	/** if set, don't allow path from LiftExit to Lift (only for Lift to LiftExit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lift)
	bool bOnlyExit;

	/** utility for shared functionality that is also used by the UTLift automatic exit generation */
	static bool AddLiftPathsShared(const FVector& ExitLoc, class AUTLift* TheLift, bool bRequireLiftJump, bool bOnlyExitPath, class AUTRecastNavMesh* NavData);

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData) override;
};