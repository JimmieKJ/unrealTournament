// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"

#include "UTWallDodgePoint.generated.h"

/** place this point to tell AI about a good spot to do wall dodges for level traversal */
UCLASS()
class AUTWallDodgePoint : public AActor, public IUTPathBuilderInterface
{
	GENERATED_BODY()
public:

	AUTWallDodgePoint(const FObjectInitializer& OI);

	/** editor icon (don't make editoronly because it's the RootComponent - removing will cause transform to be lost) */
	UPROPERTY(VisibleAnywhere, Category = Sprite)
	UBillboardComponent* Icon;
#if WITH_EDITORONLY_DATA
	/** editor rotation indicator */
	UPROPERTY(VisibleAnywhere)
	UArrowComponent* EditorArrow;
#endif

	/** range to search for start/end points */
	UPROPERTY(EditAnywhere)
	float CheckRange;

	virtual bool IsPOI() const override
	{
		return false;
	}

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData) override;
};