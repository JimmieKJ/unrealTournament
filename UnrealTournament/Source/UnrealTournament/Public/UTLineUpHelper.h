#pragma  once

#include "UTLineUpZone.h"
#include "UTLineUpHelper.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTLineUpHelper : public AActor
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void HandleLineUp(UWorld* World, LineUpTypes IntroType);

	UFUNCTION()
	void OnPlayerChange();

	UFUNCTION()
	void CleanUp();

	UPROPERTY()
	bool bIsActive;

	UPROPERTY()
	LineUpTypes LastActiveType;

	UPROPERTY()
	bool bIsPlacingPlayers;

	static LineUpTypes GetLineUpTypeToPlay(UWorld* World);

	static AUTLineUpZone* GetAppropriateSpawnList(UWorld* World, LineUpTypes ZoneType);

	static AActor* GetCameraActorForLineUp(UWorld* World, LineUpTypes ZoneType);

protected:

	void ClientUpdatePlayerClones();

	UFUNCTION()
	void HandleIntro(UWorld* World, LineUpTypes ZoneType);

	UFUNCTION()
	void HandleIntermission(UWorld* World, LineUpTypes IntermissionType);

	UFUNCTION()
	void HandleEndMatchSummary(UWorld* World, LineUpTypes SummaryType);

	UFUNCTION()
	void SortPlayers();

	UFUNCTION()
	void MovePlayers(UWorld* World, LineUpTypes ZoneType);

	UFUNCTION()
	void SpawnClone(UWorld* World, AUTPlayerState* PS, const FTransform& Location);
	
	UFUNCTION()
	void DestroySpawnedClones();

	UFUNCTION()
	void SpawnPlayerClones(UWorld* World, LineUpTypes LineUpType);

	UFUNCTION()
	void MovePreviewCharactersToLineUpSpawns(UWorld* World, LineUpTypes LineUpType);

	TWeakPtr<AUTCharacter> SelectedCharacter;

	FTimerHandle MatchSummaryHandle;

	/** preview actors */
	TArray<class AUTCharacter*> PlayerPreviewCharacters;

	/** preview weapon */
	TArray<class AUTWeaponAttachment*> PreviewWeapons;

	TArray<class UAnimationAsset*> PreviewAnimations;
};