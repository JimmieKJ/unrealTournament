// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_SpectatorSlideOut.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_SpectatorSlideOut : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);
	virtual void DrawFlag(FName KeyName, FString FlagName, AUTCarriedObject* Flag, float RenderDelta, float XOffset, float YOffset);
	virtual void DrawCamBind(FName KeyName, FString ProjName, float RenderDelta, float XOffset, float YOffset, bool bCamSelected);
	virtual void DrawPowerup(class AUTPickupInventory* Pickup, float XOffset, float YOffset);
	virtual void InitializeWidget(AUTHUD* Hud);

	// The total Height of a given cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		float CellHeight;

	// How much space in between each column
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		float CenterBuffer;

	UPROPERTY()
		UFont* SlideOutFont;

	/** Current slide offset. */
	float SlideIn;

	/** How fast to slide menu in/out */
	UPROPERTY()
		float SlideSpeed;

	// Where to draw the flags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		float FlagX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnHeaderPlayerX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnHeaderScoreX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnHeaderArmor;

	// The offset of text data within the cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnY;

	/** How long after last action to highlight active players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ActionHighlightTime;

	UPROPERTY(EditDefaultsOnly, Category = "RenderObject")
		FCanvasIcon UDamageHUDIcon;

	UPROPERTY(EditDefaultsOnly, Category = "RenderObject")
		FCanvasIcon HealthIcon;

	UPROPERTY(EditDefaultsOnly, Category = "RenderObject")
		FCanvasIcon ArmorIcon;

	UPROPERTY(EditDefaultsOnly, Category = "RenderObject")
		FCanvasIcon FlagIcon;

	/** Cached spectator bindings. */
	UPROPERTY()
	FName RedFlagBind;

	UPROPERTY()
	FName BlueFlagBind;

	UPROPERTY()
	FName AutoCamBind;

	UPROPERTY()
	FName DemoRestartBind;
	
	UPROPERTY()
	FName DemoLiveBind;

	UPROPERTY()
	FName DemoRewindBind;
	
	UPROPERTY()
	FName DemoFastForwardBind;

	UPROPERTY()
	FName DemoPauseBind;

	UPROPERTY()
	FName CameraBind[10];

	UPROPERTY()
	FString CameraString[10];


	UPROPERTY()
	int32 NumCameras;

	UPROPERTY()
	bool bCamerasInitialized;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		UTexture2D* TextureAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		UTexture2D* FlagAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		UTexture2D* WeaponAtlas;

	UPROPERTY()
		TArray<AUTPickupInventory*> PowerupList;

	UPROPERTY()
		bool bPowerupListInitialized;

	virtual void InitPowerupList();

private:
};
