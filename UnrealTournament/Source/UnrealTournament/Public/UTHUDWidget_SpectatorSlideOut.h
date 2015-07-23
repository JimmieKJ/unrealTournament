// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_SpectatorSlideOut.generated.h"

/**
*	Holds the bounds and simulated key of a hud element when clicked
*/
USTRUCT()
struct FClickElement
{
	GENERATED_USTRUCT_BODY()

	//Keypress when the element is clicked
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoreboard")
	FKey Key;

	// Holds the X1/Y1/X2/Y2 bounds of this element.  
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoreboard")
	FVector4 Bounds;

	FClickElement()
	{
		Bounds = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	FClickElement(const FKey InKey, const FVector4& inBounds)
	{
		Key = InKey;
		Bounds = inBounds;
	}
};

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_SpectatorSlideOut : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	virtual void DrawPlayerHeader(float RenderDelta, float XOffset, float YOffset);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);
	virtual void DrawFlag(FName KeyName, FString FlagName, AUTCarriedObject* Flag, float RenderDelta, float XOffset, float YOffset);
	virtual void DrawCamBind(FName KeyName, FString ProjName, float RenderDelta, float XOffset, float YOffset, bool bCamSelected);
	virtual void DrawPowerup(class AUTPickup* Pickup, float XOffset, float YOffset);
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

	/**Called from Slate to set the mouse position*/
	virtual void TrackMouseMovement(FVector2D InMousePosition) { MousePosition = InMousePosition; }
	/**Called from Slate when the mouse has clicked*/
	virtual bool MouseClick(FVector2D InMousePosition);

	virtual void SetMouseInteractive(bool bNewInteractive) { bIsInteractive = bNewInteractive; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
	UTexture2D* TextureAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
	UTexture2D* FlagAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
	UTexture2D* WeaponAtlas;

	UPROPERTY()
	TArray<AUTPickup*> PowerupList;

	UPROPERTY()
	bool bPowerupListInitialized;

	virtual void InitPowerupList();
	virtual float GetDrawScaleOverride();

private:
	FVector2D MousePosition;

	/**Whether the mouse can interact with hud elements*/
	bool bIsInteractive;

	/** Holds a list of hud elements that have keybinds associated with them.  NOTE: this is regenerated every frame*/
	TArray<FClickElement> ClickElementStack;

	/**Returns the index of the FClickElement the mouse is under. -1 for none*/
	int32 MouseHitTest(FVector2D Position);
	FKey NumberToKey(int32 InNumber);

	void UpdateCameraBindOffset(float& DrawOffset, float& XOffset, bool& bOverflow, float StartOffset, float& EndCamOffset);
};
