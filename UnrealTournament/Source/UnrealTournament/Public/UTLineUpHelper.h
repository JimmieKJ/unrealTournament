#pragma  once

#include "UTLineUpZone.h"
#include "UTLineUpHelper.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTLineUpHelper : public UObject
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void HandleLineUp(UWorld* World, LineUpTypes IntroType);

	UFUNCTION()
	void CleanUp();

	UPROPERTY(Replicated)
	bool bIsActive;

	UPROPERTY(Replicated)
	LineUpTypes LastActiveType;

	static LineUpTypes GetLineUpTypeToPlay(UWorld* World);

	static AUTLineUpZone* GetAppropriateSpawnList(UWorld* World, LineUpTypes ZoneType);

protected:

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