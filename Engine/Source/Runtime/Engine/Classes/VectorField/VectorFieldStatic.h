// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VectorField: A 3D grid of vectors.
=============================================================================*/

#pragma once
#include "VectorField/VectorField.h"
#include "VectorFieldStatic.generated.h"

UCLASS(hidecategories=VectorFieldBounds, MinimalAPI)
class UVectorFieldStatic : public UVectorField
{
	GENERATED_UCLASS_BODY()

	/** Size of the vector field volume. */
	UPROPERTY(Category=VectorFieldStatic, VisibleAnywhere)
	int32 SizeX;

	/** Size of the vector field volume. */
	UPROPERTY(Category=VectorFieldStatic, VisibleAnywhere)
	int32 SizeY;

	/** Size of the vector field volume. */
	UPROPERTY(Category=VectorFieldStatic, VisibleAnywhere)
	int32 SizeZ;


public:
	/** The resource for this vector field. */
	class FVectorFieldResource* Resource;

	/** Source vector data. */
	FByteBulkData SourceData;

#if WITH_EDITORONLY_DATA
	/** Path to the resource used to construct this vector field. Relative to the object's package, BaseDir() or absolute. */
	UPROPERTY(Category=SourceAsset, VisibleAnywhere)
	FString SourceFilePath;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY(Category=SourceAsset, VisibleAnywhere)
	FString SourceFileTimestamp;
#endif // WITH_EDITORONLY_DATA

	// Begin UObject interface.
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITORONLY_DATA
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
	// End UObject interface.

	// Begin UVectorField Interface
	virtual void InitInstance(class FVectorFieldInstance* Instance, bool bPreviewInstance) override;
	// End UVectorField Interface

	/**
	 * Initialize resources.
	 */
	ENGINE_API void InitResource();
private:

	/** Permit the factory class to update and release resources externally. */
	friend class UVectorFieldStaticFactory;

	/**
	 * Update resources. This must be implemented by subclasses as the Resource
	 * pointer must always be valid.
	 */
	ENGINE_API void UpdateResource();

	/**
	 * Release the static vector field resource.
	 */
	ENGINE_API void ReleaseResource();

};

