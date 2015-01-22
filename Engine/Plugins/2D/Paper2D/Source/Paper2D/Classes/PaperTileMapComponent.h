// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperSprite.h"

#include "PaperTileMapComponent.generated.h"

UCLASS(hideCategories=Object, ClassGroup=Paper2D, Experimental, meta=(BlueprintSpawnableComponent))
class PAPER2D_API UPaperTileMapComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

private:
	// Width of map (in tiles)
	UPROPERTY()
	int32 MapWidth_DEPRECATED;

	UPROPERTY()
	int32 MapHeight_DEPRECATED;

	UPROPERTY()
	int32 TileWidth_DEPRECATED;

	UPROPERTY()
	int32 TileHeight_DEPRECATED;

	// Default tile set to use for new layers
	UPROPERTY()
	UPaperTileSet* DefaultLayerTileSet_DEPRECATED;

	// Test material
	UPROPERTY()
	UMaterialInterface* Material_DEPRECATED;

	// The list of layers
	UPROPERTY()
	TArray<UPaperTileLayer*> TileLayers_DEPRECATED;

public:
	// The tile map used by this component
	UPROPERTY(Category=Setup, EditAnywhere, Instanced, BlueprintReadOnly)
	class UPaperTileMap* TileMap;

protected:
	friend class FPaperTileMapRenderSceneProxy;

	void RebuildRenderData(class FPaperTileMapRenderSceneProxy* Proxy);

public:
	// UObject interface
	virtual void PostInitProperties() override;
#if WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#endif
	// End of UObject interface

	// UActorComponent interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual const UObject* AdditionalStatObject() const override;
	// End of UActorComponent interface

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual class UBodySetup* GetBodySetup() override;
	// End of UPrimitiveComponent interface

	// Does this component own the tile map (is it instanced instead of being an asset reference)?
	bool OwnsTileMap() const;
};

// Allow the old name to continue to work for one release
DEPRECATED(4.7, "UPaperTileMapRenderComponent has been renamed to UPaperTileMapComponent")
typedef UPaperTileMapComponent UPaperTileMapRenderComponent;
