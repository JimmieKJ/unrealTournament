#pragma once

#include "UTLineUpZoneVisualizationCharacter.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTLineUpZoneVisualizationCharacter : public AUTCharacter
{
	GENERATED_UCLASS_BODY()

	void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir) override;

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
	
#if WITH_EDITORONLY_DATA
	//HPropertyWidgetProxy* OwningFTransformProxy;

	virtual void PostEditMove(bool bFinished) override;
	//void OnObjectSelected(UObject* Object);

#endif // WITH_EDITORONLY_DATA
};