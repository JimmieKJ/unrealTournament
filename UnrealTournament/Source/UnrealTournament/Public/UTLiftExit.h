// place to define a place that bots can get off a lift/mover
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"

#include "UTLiftExit.generated.h"

UCLASS()
class AUTLiftExit : public AActor, public IUTPathBuilderInterface
{
	GENERATED_UCLASS_BODY()

	/** editor icon */
	UPROPERTY(VisibleAnywhere, Category = Sprite)
	TSubobjectPtr<UBillboardComponent> Icon;

	/** the lift that will take the bot places */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lift)
	class AUTLift* MyLift;

	/** utility for shared functionality that is also used by the UTLift automatic exit generation */
	static void AddLiftPathsShared(const FVector& ExitLoc, class AUTLift* TheLift, class AUTRecastNavMesh* NavData);

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData) override;
};