// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShowFlags.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugDrawService.generated.h"

class FSceneView;

// BEGIN - temporary solution for Orion, to draw all perception data correctly in client-server game. Changes to GameplayDebugger are coming on Main branch soon so it won't be merged back to Main.
UENUM()
enum class EDrawDebugShapeElement : uint8
{
	Invalid = 0,
	SinglePoint, // individual points. 
	Segment, // pairs of points 
	Box,
	Cone,
	Cylinder,
	Capsule,
	Polygon,
	String,
};

USTRUCT()
struct ENGINE_API FDrawDebugShapeElement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Description;

	UPROPERTY()
	TArray<FVector> Points;

	UPROPERTY()
	FMatrix TransformationMatrix;

	UPROPERTY()
	EDrawDebugShapeElement Type;

	UPROPERTY()
	uint8 Color;

	UPROPERTY()
	uint16 ThicknesOrRadius;

	FDrawDebugShapeElement(EDrawDebugShapeElement InType = EDrawDebugShapeElement::Invalid);
	FDrawDebugShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness);
	void SetColor(const FColor& InColor);
	EDrawDebugShapeElement GetType() const;
	void SetType(EDrawDebugShapeElement InType);
	FColor GetFColor() const;

	static FDrawDebugShapeElement MakeString(const FString& Description, const FVector& Location);
	static FDrawDebugShapeElement MakeLine(const FVector& Start, const FVector& End, FColor Color, float Thickness = 0);
	static FDrawDebugShapeElement MakePoint(const FVector& Location, FColor Color, float Radius = 2);
	static FDrawDebugShapeElement MakeCylinder(FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color);
};
// END - temporary solution for Orion

/** 
 * 
 */
DECLARE_DELEGATE_TwoParams(FDebugDrawDelegate, class UCanvas*, class APlayerController*);

UCLASS(config=Engine)
class ENGINE_API UDebugDrawService : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//void Register

	static FDelegateHandle Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate);
	static void Unregister(FDelegateHandle HandleToRemove);
	static void Draw(const FEngineShowFlags Flags, class UCanvas* Canvas);
	static void Draw(const FEngineShowFlags Flags, class FViewport* Viewport, FSceneView* View, FCanvas* Canvas);

private:
	static TArray<TArray<FDebugDrawDelegate> > Delegates;
	static FEngineShowFlags ObservedFlags;
};
