#pragma once

#include "UTInGameIntroZoneVisualizationCharacter.h"

#include "UTInGameIntroZone.generated.h"

UENUM()
enum class InGameIntroZoneTypes : uint8
{
	Invalid,
	Intro,
	Intermission,
	Intermission_RedWin,
	Intermission_BlueWin,
	PostMatch,
	PostMatch_RedWin,
	PostMatch_BlueWin,
	None UMETA(Hidden)
};

/**This class represents a collection of spawn points to use for an In Game Intro Zone based on a particular TeamNum. Note multiple AUTInGameIntroZones might use the same TeamSpawnPointList. **/
UCLASS()
class AUTInGameIntroZone : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	InGameIntroZoneTypes ZoneType;

	//**Determines if Spawn Locations should snap to the floor when being placed in the editor **/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	bool bSnapToFloor;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (MakeEditWidget = ""))
	TArray<FTransform> RedTeamSpawnLocations;
	 
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (MakeEditWidget = ""))
	TArray<FTransform> BlueTeamSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (MakeEditWidget = ""))
	TArray<FTransform> FFATeamSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	ACameraActor* TeamCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Team Spawn Point List")
	TSubclassOf<AUTCharacter> EditorVisualizationCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* SceneRoot;

	UPROPERTY(BlueprintReadOnly, Category = "Intro Zone")
	TArray<UMeshComponent*> RenderedPlayerStates;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, meta = (MakeEditWidget = ""))
	TArray<AUTInGameIntroZoneVisualizationCharacter*> MeshVisualizations;
#endif

	UFUNCTION()
	void UpdateSpawnLocationsWithVisualizationMove();

	UFUNCTION()
	void SnapToFloor();

	UFUNCTION()
	void InitializeMeshVisualizations();

	UFUNCTION()
	void UpdateMeshVisualizations();

	UFUNCTION()
	void DeleteAllMeshVisualizations();

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;

	void DefaultCreateForTeamGame();
	void DefaultCreateForFFAGame();
#endif // WITH_EDITOR

};