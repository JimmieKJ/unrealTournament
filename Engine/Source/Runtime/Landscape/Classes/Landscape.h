// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"
#include "Casts.h"
#include "LazyObjectPtr.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"

#include "LandscapeProxy.h"
#include "LandscapeInfoMap.h"

#include "Landscape.generated.h"

UENUM()
enum ELandscapeSetupErrors
{
	LSE_None,
	/** No Landscape Info available. */
	LSE_NoLandscapeInfo,
	/** There was already component with same X,Y. */
	LSE_CollsionXY,
	/** No Layer Info, need to add proper layers. */
	LSE_NoLayerInfo,
	LSE_MAX,
};

UCLASS(MinimalAPI, showcategories=(Display, Movement, Collision, Lighting, LOD, Input))
class ALandscape : public ALandscapeProxy
{
	GENERATED_BODY()

public:
	ALandscape(const FObjectInitializer& ObjectInitializer);

	// Make a key for XYtoComponentMap
	static FIntPoint MakeKey( int32 X, int32 Y ) { return FIntPoint(X, Y); }
	static void UnpackKey( FIntPoint Key, int32& OutX, int32& OutY ) { OutX = Key.X; OutY = Key.Y; }

	//~ Begin ALandscapeProxy Interface
	LANDSCAPE_API virtual ALandscape* GetLandscapeActor() override;
#if WITH_EDITOR
	//~ End ALandscapeProxy Interface

	LANDSCAPE_API bool HasAllComponent(); // determine all component is in this actor
	
	// Include Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads, 
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	// Exclude Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesNoOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads,
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	static void SplitHeightmap(ULandscapeComponent* Comp, bool bMoveToCurrentLevel = false);
	
	//~ Begin UObject Interface.
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditImport() override;
#endif
	virtual void PostLoad() override;
	//~ End UObject Interface
};

LANDSCAPE_API DECLARE_LOG_CATEGORY_EXTERN(LogLandscape, Warning, All);

/**
 * Landscape stats
 */

DECLARE_CYCLE_STAT_EXTERN(TEXT("Landscape Dynamic Draw Time"), STAT_LandscapeDynamicDrawTime, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Landscape Static Draw LOD Time"), STAT_LandscapeStaticDrawLODTime, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LandscapeVF Draw Time"), STAT_LandscapeVFDrawTime, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Landscape Triangles"), STAT_LandscapeTriangles, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Landscape Render Passes"), STAT_LandscapeComponents, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Landscape DrawCalls"), STAT_LandscapeDrawCalls, STATGROUP_Landscape, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Landscape Vertex Mem"), STAT_LandscapeVertexMem, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Component Mem"), STAT_LandscapeComponentMem, STATGROUP_Landscape, );

#if WITH_EDITORONLY_DATA

/**
 * Gets landscape-specific data for given world.
 *
 * @param World A pointer to world that return data should be associated with.
 *
 * @returns Landscape-specific data associated with given world.
 */
LANDSCAPE_API ULandscapeInfoMap& GetLandscapeInfoMap(UWorld* World);

#endif // WITH_EDITORONLY_DATA