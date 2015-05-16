// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget.h"
#include "UTScoreboard.generated.h"

USTRUCT()
struct FSelectionObject
{
	GENERATED_USTRUCT_BODY()

	// Holds a reference to PS that is under this score element
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoreboard")
	TWeakObjectPtr<AUTPlayerState> ScoreOwner;

	// Holds the X1/Y1/X2/Y2 bounds of this score element.  
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoreboard")
	FVector4 ScoreBounds;

	FSelectionObject()
	{
		ScoreOwner.Reset();
		ScoreBounds = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	FSelectionObject(TWeakObjectPtr<AUTPlayerState> inScoreOwner, const FVector4& inScoreBounds)
	{
		ScoreOwner = inScoreOwner;
		ScoreBounds = inScoreBounds;
	}

};

UCLASS()
class UNREALTOURNAMENT_API UUTScoreboard : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	// The main drawing stub
	virtual void Draw_Implementation(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderPlayerX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderScoreX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderDeathsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderPingX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderY;

	// The offset of text data within the cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnY;

	// The total Height of a given cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float CellHeight;

	// How much space in between each column
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float CenterBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnMedalX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float FooterPosY;

	// Where to draw the flags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float FlagX;


	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);

	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	virtual void AdvancePage(int32 Increment);
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	virtual void SetPage(uint32 NewPage);

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	TArray<FVector2D> BadgeNumberUVs;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	TArray<FVector2D> BadgeUVs;

	/** number of 'pages' that can be flipped through on the scoreboard */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	uint32 NumPages;

	UFUNCTION(BlueprintNativeEvent, Category = "Scoreboard")
	void PageChanged();

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	uint32 ActualPlayerCount;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
		AUTPlayerState* ScoreBreakdownPS;

	virtual AUTPlayerState* GetNextScoringPlayer(int32 dir);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* TextureAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* FlagAtlas;

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return bShowScores;
	}

	virtual void DrawGamePanel(float RenderDelta, float& YOffset);
	virtual void DrawGameOptions(float RenderDelta, float& YOffset);

	virtual void DrawTeamPanel(float RenderDelta, float& YOffset);

	virtual void DrawScorePanel(float RenderDelta, float& YOffset);
	virtual void DrawScoreHeaders(float RenderDelta, float& DrawY);
	virtual void DrawPlayerScores(float RenderDelta, float& DrawY);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);


	virtual void DrawServerPanel(float RenderDelta, float YOffset);

protected:

	// Will be true when the scoreboard is interactive.  This will cause the
	// scoreboard to track the SelectionStack and display selected items.
	bool bIsInteractive;

	// Holds a list of selectable objects on the scoreboard.  NOTE: this is regenerated every frame
	TArray<FSelectionObject> SelectionStack;

	int32 SelectionHitTest(FVector2D Position);

	FVector2D CursorPosition;

	TWeakObjectPtr<AUTPlayerState> SelectedPlayer;

public:

	void BecomeInteractive();
	void BecomeNonInteractive();

	// Each time a mouse is moved, then 
	virtual void TrackMouseMovement(FVector2D NewMousePosition);

	// Attempts to select an item in the stack.  Returns true and flags the PS if it succeeds or false and clears all selections if fails.
	virtual bool AttemptSelection(FVector2D SelectionPosition);

	// Clears any selected PS
	virtual void ClearSelection();

	// Finds next selected PRI given an offset
	virtual void SelectNext(int32 Offset, bool bDoNoWrap=false);
	virtual void DefaultSelection(AUTGameState* GS, uint8 TeamIndex = 255);

	virtual void SelectionUp();
	virtual void SelectionDown();
	virtual void SelectionLeft();
	virtual void SelectionRight();
	virtual void SelectionClick();

};

