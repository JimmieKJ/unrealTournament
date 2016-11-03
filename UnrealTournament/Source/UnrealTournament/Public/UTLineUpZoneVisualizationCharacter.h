#pragma once

#include "UTLineUpZoneVisualizationCharacter.generated.h"

UCLASS(transient)
class UNREALTOURNAMENT_API AUTLineUpZoneVisualizationCharacter : public ACharacter, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int TeamNum;
	
	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial;

	UPROPERTY()
	int TransformOwnerIndex;

	UFUNCTION()
	void OnChangeTeamNum();

	UPROPERTY(EditAnywhere)
	UMaterialInterface* Material;
	

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

#if WITH_EDITORONLY_DATA
	
	virtual void PostEditMove(bool bFinished) override;

#endif // WITH_EDITORONLY_DATA
};