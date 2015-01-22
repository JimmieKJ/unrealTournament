// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeSplineSegment.h"

#include "LandscapeSplineControlPoint.generated.h"

struct FLandscapeSplineInterpPoint;

USTRUCT()
struct FLandscapeSplineConnection
{
	GENERATED_USTRUCT_BODY()

	FLandscapeSplineConnection() {}

	FLandscapeSplineConnection(class ULandscapeSplineSegment* InSegment, int32 InEnd)
		: Segment(InSegment)
		, End(InEnd)
	{
	}

	bool operator == (const FLandscapeSplineConnection& rhs) const
	{
		return Segment == rhs.Segment && End == rhs.End;
	}

	// Segment connected to this control point
	UPROPERTY()
	class ULandscapeSplineSegment* Segment;

	// Which end of the segment is connected to this control point
	UPROPERTY()
	uint32 End:1;

	LANDSCAPE_API struct FLandscapeSplineSegmentConnection& GetNearConnection() const;
	LANDSCAPE_API struct FLandscapeSplineSegmentConnection& GetFarConnection() const;
};

UCLASS(Within=LandscapeSplinesComponent,autoExpandCategories=LandscapeSplineControlPoint,MinimalAPI)
class ULandscapeSplineControlPoint : public UObject
{
	GENERATED_UCLASS_BODY()

// Directly editable data:
	/** Location in Landscape-space */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	FVector Location;

	/** Rotation of tangent vector at this point (in landscape-space) */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	FRotator Rotation;

	/** Width of the spline at this point. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	float Width;

	/** Falloff at the sides of the spline at this point. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	float SideFalloff;

	/** Falloff at the start/end of the spline (if this point is a start or end point, otherwise ignored). */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	float EndFalloff;

#if WITH_EDITORONLY_DATA
	/** Mesh to use on the control point */
	UPROPERTY(EditAnywhere, Category=Mesh)
	class UStaticMesh* Mesh;

	/** Overrides mesh's materials */
	UPROPERTY(EditAnywhere, Category=Mesh)
	TArray<class UMaterialInterface*> MaterialOverrides;

	/** Scale of the control point mesh */
	UPROPERTY(EditAnywhere, Category=Mesh)
	FVector MeshScale;

	/**  Max draw distance for the mesh used on this control point */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Mesh, meta = (DisplayName = "Max Draw Distance"))
	float LDMaxDrawDistance;

	/**
	 * Name of blend layer to paint when applying spline to landscape
	 * If "none", no layer is painted
	 */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	FName LayerName;

	/** If the spline is above the terrain, whether to raise the terrain up to the level of the spline when applying it to the landscape. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	uint32 bRaiseTerrain:1;

	/** If the spline is below the terrain, whether to lower the terrain down to the level of the spline when applying it to the landscape. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineControlPoint)
	uint32 bLowerTerrain:1;

	/** Whether to enable collision for the Control Point Mesh. */
	UPROPERTY(EditAnywhere, Category=Mesh)
	uint32 bEnableCollision:1;

	/** Whether the Control Point Mesh should cast a shadow. */
	UPROPERTY(EditAnywhere, Category=Mesh)
	uint32 bCastShadow:1;

protected:
	UPROPERTY(Transient)
	uint32 bSelected : 1;

	UPROPERTY(Transient)
	uint32 bNavDirty : 1;
#endif

// Procedural data:
public:
	UPROPERTY(TextExportTransient)
	TArray<FLandscapeSplineConnection> ConnectedSegments;

protected:
	/** Control point mesh */
	UPROPERTY(TextExportTransient)
	class UControlPointMeshComponent* MeshComponent;

	/** Spline points */
	UPROPERTY()
	TArray<struct FLandscapeSplineInterpPoint> Points;

	/** Bounds of points */
	UPROPERTY()
	FBox Bounds;

public:
	const FBox& GetBounds() const { return Bounds; }
	const TArray<struct FLandscapeSplineInterpPoint>& GetPoints() const { return Points; }

	// Get the name of the best connection point (socket) to use for a particular destination
	virtual FName GetBestConnectionTo(FVector Destination) const;

	// Get the location and rotation of a connection point (socket) in spline space
	virtual void GetConnectionLocalLocationAndRotation(FName SocketName, OUT FVector& OutLocation, OUT FRotator& OutRotation) const;

	// Get the location and rotation of a connection point (socket) in spline space
	virtual void GetConnectionLocationAndRotation(FName SocketName, OUT FVector& OutLocation, OUT FRotator& OutRotation) const;

#if WITH_EDITOR
	bool IsSplineSelected() const { return bSelected; }
	virtual void SetSplineSelected(bool bInSelected);

	/** Calculates rotation from connected segments */
	virtual void AutoCalcRotation();

	/**  */
	virtual void AutoFlipTangents();

	virtual void AutoSetConnections(bool bIncludingValid);

	/** Update spline points */
	virtual void UpdateSplinePoints(bool bUpdateCollision = true, bool bUpdateAttachedSegments = true);

	/** Delete spline points */
	virtual void DeleteSplinePoints();
#endif // WITH_EDITOR

	// Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject Interface
#endif // WITH_EDITOR

	void RegisterComponents();
	void UnregisterComponents();

	virtual bool OwnsComponent(const class UControlPointMeshComponent* ControlPointMeshComponent) const;

	friend class FEdModeLandscape;
};
