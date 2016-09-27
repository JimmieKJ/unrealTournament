#pragma once

#include "UTInGameIntroZoneVisualizationCharacter.h"

#include "UTInGameIntroZone.generated.h"

UENUM()
enum class InGameIntroZoneTypes : uint8
{
	Invalid,
	FFA_Intro,
	FFA_PostMatch,
	FFA_Intermission,
	Team_Intro,
	Team_Intermission,
	Team_Intermission_RedWin,
	Team_Intermission_BlueWin,
	Team_PostMatch,
	Team_PostMatch_RedWin,
	Team_PostMatch_BlueWin,
	None UMETA(Hidden)
};

/**This class represents a collection of spawn points to use for an In Game Intro Zone based on a particular TeamNum. Note multiple AUTInGameIntroZones might use the same TeamSpawnPointList. **/
UCLASS()
class AUTInGameIntroZoneTeamSpawnPointList : public AActor
{
	GENERATED_UCLASS_BODY()

	//**Determines if Spawn Locations should snap to the floor when being placed in the editor **/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	bool bSnapToFloor;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	uint8 TeamNum;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (MakeEditWidget = ""))
	TArray<FTransform> PlayerSpawnLocations;
	 
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	ACameraActor* TeamCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Team Spawn Point List")
	TSubclassOf<AUTCharacter> EditorVisualizationCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* SceneRoot;

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
#endif // WITH_EDITOR

};

UCLASS()
class AUTInGameIntroZone : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "Intro Zone")
	InGameIntroZoneTypes ZoneType;

	UPROPERTY(EditAnywhere, Category = "Intro Zone")
	TArray<AUTInGameIntroZoneTeamSpawnPointList*> TeamSpawns;
	
	UPROPERTY(BlueprintReadOnly, Category = "Intro Zone")
	TArray<UMeshComponent*> RenderedPlayerStates;

#if WITH_EDITORONLY_DATA
	virtual void PostEditMove(bool bFinished) override;
#endif 
};