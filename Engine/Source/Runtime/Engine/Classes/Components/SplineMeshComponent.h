// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"

#include "SplineMeshComponent.generated.h"

UENUM()
namespace ESplineMeshAxis
{
	enum Type
	{
		X,
		Y,
		Z,
	};
}

/** 
 * Structure that holds info about spline, passed to renderer to deform UStaticMesh.
 * Also used by Lightmass, so be sure to update Lightmass::FSplineMeshParams and the static lighting code if this changes!
 */
USTRUCT()
struct FSplineMeshParams
{
	GENERATED_USTRUCT_BODY()

	/** Start location of spline, in component space */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector StartPos;

	/** Start tangent of spline, in component space */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector StartTangent;

	/** X and Y scale applied to mesh at start of spline */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D StartScale;

	/** Roll around spline applied at start */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	float StartRoll;

	/** Starting offset of the mesh from the spline, in component space */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D StartOffset;

	/** End location of spline, in component space */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector EndPos;

	/** End tangent of spline, in component space */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector EndTangent;

	/** X and Y scale applied to mesh at end of spline */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D EndScale;

	/** Roll around spline applied at end */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	float EndRoll;

	/** Ending offset of the mesh from the spline, in component space */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D EndOffset;


	FSplineMeshParams()
		: StartPos(ForceInit)
		, StartTangent(ForceInit)
		, StartScale(ForceInit)
		, StartRoll(0)
		, StartOffset(ForceInit)
		, EndPos(ForceInit)
		, EndTangent(ForceInit)
		, EndScale(ForceInit)
		, EndRoll(0)
		, EndOffset(ForceInit)
	{
	}

};

/** 
 *	A Spline Mesh Component is a derivation of a Static Mesh Component which can be deformed using a spline. Only a start and end position (and tangent) can be specified.  
 *	@see https://docs.unrealengine.com/latest/INT/Resources/ContentExamples/Blueprint_Splines
 */
UCLASS(ClassGroup=Rendering, hidecategories=(Physics), meta=(BlueprintSpawnableComponent))
class ENGINE_API USplineMeshComponent : public UStaticMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

	/** Spline that is used to deform mesh */
	UPROPERTY(EditAnywhere, Category=SplineMesh, meta=(ShowOnlyInnerProperties))
	FSplineMeshParams SplineParams;

	/** Axis (in component space) that is used to determine X axis for co-ordinates along spline */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector SplineUpDir;

	/** If true, will use smooth interpolation (ease in/out) for Scale, Roll, and Offset along this section of spline. If false, uses linear */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	uint32 bSmoothInterpRollScale:1;

	/** Chooses the forward axis for the spline mesh orientation */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

	// Physics data.
	UPROPERTY()
	class UBodySetup* BodySetup;

#if WITH_EDITORONLY_DATA
	// Used to automatically trigger rebuild of collision data
	UPROPERTY()
	FGuid CachedMeshBodySetupGuid;

	UPROPERTY(transient)
	uint32 bSelected:1;
#endif

	//Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	//End UObject Interface

	//Begin USceneComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//End USceneComponent Interface

	//Begin UPrimitiveComponent Interface
	virtual void CreatePhysicsState() override;
	virtual class UBodySetup* GetBodySetup() override;
#if WITH_EDITOR
	virtual bool ShouldRenderSelected() const override
	{
		return Super::ShouldRenderSelected() || bSelected;
	}
#endif
	virtual bool DoCustomNavigableGeometryExport(struct FNavigableGeometryExport* GeomExport) const override;
	//End UPrimitiveComponent Interface

	//Begin UStaticMeshComponent Interface
	virtual class FStaticMeshStaticLightingMesh* AllocateStaticLightingMesh(int32 LODIndex, const TArray<ULightComponent*>& InRelevantLights);
	//End UStaticMeshComponent Interface

	// Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
	// End Interface_CollisionDataProvider Interface

	/** Called when spline params are changed, to notify render thread and possibly collision */
	void MarkSplineParamsDirty();

	/** Get the start position of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetStartPosition() const;

	/** Set the start position of spline in local space */
	UFUNCTION(BlueprintCallable, Category=SplineMesh)
	void SetStartPosition(FVector StartPos);

	/** Get the start tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetStartTangent() const;

	/** Set the start tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartTangent(FVector StartTangent);

	/** Get the end position of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetEndPosition() const;

	/** Set the end position of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndPosition(FVector EndPos);

	/** Get the end tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetEndTangent() const;

	/** Set the end tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndTangent(FVector EndTangent);

	/** Set the start and end, position and tangent, all in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartAndEnd(FVector StartPos, FVector StartTangent, FVector EndPos, FVector EndTangent);

	/** Get the start scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetStartScale() const;

	/** Set the start scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartScale(FVector2D StartScale = FVector2D(1,1));

	/** Get the start roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	float GetStartRoll() const;

	/** Set the start roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartRoll(float StartRoll);

	/** Get the start offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetStartOffset() const;

	/** Set the start offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartOffset(FVector2D StartOffset);

	/** Get the end scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetEndScale() const;

	/** Set the end scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndScale(FVector2D EndScale = FVector2D(1,1));

	/** Get the end roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	float GetEndRoll() const;

	/** Set the end roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndRoll(float EndRoll);

	/** Get the end offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetEndOffset() const;

	/** Set the end offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndOffset(FVector2D EndOffset);

	/** Get the forward axis */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	ESplineMeshAxis::Type GetForwardAxis() const;

	/** Set the forward axis */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetForwardAxis(ESplineMeshAxis::Type InForwardAxis);

	/** Get the spline up direction */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetSplineUpDir() const;

	/** Set the spline up direction */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetSplineUpDir(const FVector& InSplineUpDir);

	// Destroys the body setup, used to clear collision if the mesh goes missing
	void DestroyBodySetup();
#if WITH_EDITOR
	// Builds collision for the spline mesh (if collision is enabled)
	void RecreateCollision();
#endif

	/**
	 * Calculates the spline transform, including roll, scale, and offset along the spline at a specified distance
	 * @Note:  This is mirrored to Lightmass::CalcSliceTransform() and LocalVertexShader.usf.  If you update one of these, please update them all! 
	 */
	FTransform CalcSliceTransform(const float DistanceAlong) const;

	inline static const float& GetAxisValue(const FVector& InVector, ESplineMeshAxis::Type InAxis);
	inline static float& GetAxisValue(FVector& InVector, ESplineMeshAxis::Type InAxis);
};

const float& USplineMeshComponent::GetAxisValue(const FVector& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}

float& USplineMeshComponent::GetAxisValue(FVector& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}

