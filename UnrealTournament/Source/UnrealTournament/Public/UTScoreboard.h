// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTScoreboard.generated.h"

UCLASS()
class UUTScoreboard : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Reference to the UTHUD that owns this scoreboard.  ONLY VALID during the rendering phase
	UPROPERTY(BlueprintReadOnly, Category="Scoreboard")
	class AUTHUD* UTHUDOwner;

	// Reference to the UTGameState object.  ONLY VALID during the rendering phase
	UPROPERTY(BlueprintReadOnly, Category="Scoreboard")
	class AUTGameState* UTGameState;

	// Reference to the Canvas object.  ONLY VALID during the rendering phase
	UPROPERTY(BlueprintReadOnly, Category="Scoreboard")
	class UCanvas* Canvas;

	// How much to scale each cell to make sure it all fits.
	float CellScale;
	float CellHeight;
	float ResScale;

	/**
	 *	The Blueprint hook for the scoreboard.  Return true if you want to short-circuit the native drawing code.
	 *
	 * @param RenderDelta	The amount of time since the last frame
	 **/
	UFUNCTION(BlueprintImplementableEvent)
	bool eventDraw(float RenderDelta);

	virtual void DrawScoreboard(float RenderDelta);

protected:
	virtual void DrawHeader(float RenderDelta, float X, float Y, float ClipX, float ClipY);
	virtual void DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter = -1);
	virtual void DrawFooter(float RenderDelta, float X, float Y, float ClipX, float ClipY);
};

