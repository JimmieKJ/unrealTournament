// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"

#include "UTTeleporter.generated.h"

/**
* jump pad for launching characters
*/
UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTTeleporter : public AActor, public IUTPathBuilderInterface
{
	GENERATED_UCLASS_BODY()

	/** The player will teleport when overlapping this box */
	UPROPERTY(VisibleAnywhere, Category = Teleporter)
	class UBoxComponent* TriggerBox;

	/** Sound played when something teleports from this teleporter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleporter)
		USoundBase* TeleportOutSound;

	/** Sound played when something teleports to this teleporter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleporter)
		USoundBase* TeleportInSound;

#if WITH_EDITORONLY_DATA
	/** arrow component to show exit direction defined in TeleportTarget if bSetRotation is true */
	UPROPERTY(VisibleAnywhere, Category = Teleporter)
	class UArrowComponent* ExitArrow;

	/** arrow component to show entry direction defined in TeleportTarget if bSetRotation is true */
	UPROPERTY(VisibleAnywhere, Category = Teleporter)
	class UArrowComponent* EntryArrow;

	/** if set then lock the TeleportTarget when moving/rotating the teleporter */
	UPROPERTY(EditAnywhere, Category = Editor)
	bool bLockTeleportTarget;
#endif

	/** The destination of this teleporter (note: scale is ignored)
	 * NOTE: local space; editor doesn't give the option for world space
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleporter, meta = (MakeEditWidget = ""))
	FTransform TeleportTarget;

	/** if true set rotation of target to the rotation in TeleportTarget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleporter)
	bool bSetRotation;

	/** returns whether we support teleporting OtherActor */
	UFUNCTION(BlueprintNativeEvent)
	bool CanTeleport(AActor* OtherActor);

	/** called after teleportation is complete
	 * note that this is still server only so any effects need to be replicated in some way
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void PostTeleport(AActor* OtherActor);

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OverlappingActor, AActor* OtherActor);

#if WITH_EDITOR
	void UpdateExitArrow();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
#endif

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData) override;
};