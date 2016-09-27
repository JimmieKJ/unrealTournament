#pragma  once

#include "UTInGameIntroZone.h"
#include "UTInGameIntroHelper.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTInGameIntroHelper : public UObject
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void HandleIntro(UWorld* World, InGameIntroZoneTypes IntermissionType);

	UFUNCTION()
	void HandleIntermission(UWorld* World, InGameIntroZoneTypes IntermissionType);

	UFUNCTION()
	void HandleEndMatchSummary(UWorld* World, InGameIntroZoneTypes SummaryType);

	UFUNCTION()
	void CleanUp();

	UPROPERTY(Replicated)
	bool bIsActive;

	UPROPERTY(Replicated)
	InGameIntroZoneTypes LastActiveType;

	static InGameIntroZoneTypes GetIntroTypeToPlay(UWorld* World);

	static AUTInGameIntroZoneTeamSpawnPointList* GetAppropriateSpawnList(UWorld* World, int TeamNum, InGameIntroZoneTypes IntermissionType);

protected:

	UFUNCTION()
	void SortPlayers();

	UFUNCTION()
	void MovePlayers(UWorld* World, InGameIntroZoneTypes ZoneType);

	UFUNCTION()
	void SpawnClone(UWorld* World, AUTPlayerState* PS, const FTransform& Location);
	
	UFUNCTION()
	void DestroySpawnedClones();

	UFUNCTION()
	void SpawnPlayerClones(UWorld* World, InGameIntroZoneTypes IntroType);

	TWeakPtr<AUTCharacter> SelectedCharacter;

	FTimerHandle MatchSummaryHandle;

	/** preview actors */
	TArray<class AUTCharacter*> PlayerPreviewCharacters;

	/** preview weapon */
	TArray<class AUTWeaponAttachment*> PreviewWeapons;

	TArray<class UAnimationAsset*> PreviewAnimations;

};