// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTCTFCapturePoint.h"
#include "UTCTFFlagBase.h"
#include "UTResetInterface.h"
#include "UTCTFFlagBaseCapturePoint.generated.h"

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTCTFFlagBaseCapturePoint : public AUTCTFFlagBase, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
	AUTCTFCapturePoint* CapturePoint;

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	UFUNCTION()
	virtual void OnCapturePointFinished();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Game)
	void Reset() override;

	UFUNCTION(BlueprintNativeEvent, Category = CapturePoint)
	void OnCapturePointActivated();

protected:
	UPROPERTY()
	AUTCTFFlag* DeliveredFlag;
	
	bool bIsCapturePointActive;
};