#pragma once

#include "UTLineUpZoneVisualizationCharacter.h"

#include "UTLineUpZone.generated.h"

UENUM()
enum class LineUpTypes : uint8
{
	Invalid,
	Intro,
	Intermission,
	PostMatch,
	None UMETA(Hidden)
};

/**This class represents a collection of spawn points to use for an In Game Intro Zone based on a particular TeamNum. Note multiple AUTInGameIntroZones might use the same TeamSpawnPointList. **/
UCLASS()
class AUTLineUpZone : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	LineUpTypes ZoneType;

	//**Determines if Spawn Locations should snap to the floor when being placed in the editor **/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	bool bSnapToFloor;

	/**Extra offset we add when snapping objects to the floor. Should be equal to the visualization capsule half height. **/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	float SnapFloorOffset;

	/** Determines if this list is holding Team Spawn Locations or FFA Spawn Locations. True = Team Spawn Locations. False = FFA Spawn Locations.**/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	bool bIsTeamSpawnList;

	/** During any game mode that wants to display a winning team, this holds the winning team spawn locations. Otherwise it is used for the Red Team. **/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (EditCondition="bIsTeamSpawnList",MakeEditWidget=""))
	TArray<FTransform> RedAndWinningTeamSpawnLocations;
	 
	/** During any game mode that wants to display a winning team, this holds the losing team spawn locations. Otherwise it is used for the Blue Team. **/
	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (EditCondition="bIsTeamSpawnList",MakeEditWidget=""))
	TArray<FTransform> BlueAndLosingTeamSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List", meta = (EditCondition="!bIsTeamSpawnList",MakeEditWidget=""))
	TArray<FTransform> FFATeamSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Team Spawn Point List")
	ACameraActor* Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Team Spawn Point List")
	TSubclassOf<AUTLineUpZoneVisualizationCharacter> EditorVisualizationCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* SceneRoot;

	UPROPERTY(BlueprintReadOnly, Category = "Intro Zone")
	TArray<UMeshComponent*> RenderedPlayerStates;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, VisibleAnywhere, meta = (MakeEditWidget = ""))
	TArray<AUTLineUpZoneVisualizationCharacter*> MeshVisualizations;

	UPROPERTY()
	class UBillboardComponent* SpriteComponent;

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
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostRegisterAllComponents() override;

	void DefaultCreateForTeamIntro();
	void DefaultCreateForTeamIntermission();
	void DefaultCreateForTeamEndMatch();
	void DefaultCreateForFFAIntro();
	void DefaultCreateForFFAIntermission();
	void DefaultCreateForFFAEndMatch();
#endif // WITH_EDITOR

};