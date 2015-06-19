// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "SplineComponent.generated.h"



/** Permitted spline point types for SplineComponent */
UENUM()
namespace ESplinePointType
{
	enum Type
	{
		Linear,
		Curve,
		Constant,
		CurveClamped
	};
}


/** 
 *	A spline component is a spline shape which can be used for other purposes (e.g. animating objects). It does not contain rendering capabilities itself (outside the editor) 
 *	@see https://docs.unrealengine.com/latest/INT/Resources/ContentExamples/Blueprint_Splines
 */
UCLASS(ClassGroup=Utility, meta=(BlueprintSpawnableComponent))
class ENGINE_API USplineComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** Actual data for spline. Locations and tangents are in world space. */
	UPROPERTY()
	FInterpCurveVector SplineInfo;

	/** Input, distance along curve, output, parameter that puts you there. */
	UPROPERTY(Transient, TextExportTransient)
	FInterpCurveFloat SplineReparamTable;

	/** If true, spline keys may be edited per instance in the level viewport. Otherwise, the spline should be initialized in the construction script. */
	UPROPERTY(EditDefaultsOnly, Category = Spline)
	bool bAllowSplineEditingPerInstance;

	/** Number of steps per spline segment to place in the reparameterization table */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Spline, meta=(ClampMin=4, UIMin=4, ClampMax=100, UIMax=100))
	int32 ReparamStepsPerSegment;

	/** Specifies the duration of the spline in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spline)
	float Duration;

	/** Whether the endpoints of the spline are considered stationary when traversing the spline at non-constant velocity.  Essentially this sets the endpoints' tangents to zero vectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Spline, meta=(EditCondition="!bClosedLoop"))
	bool bStationaryEndpoints;

private:
	/**
	 * Whether the spline is to be considered as a closed loop.
	 * Use SetClosedLoop() to set this property, and IsClosedLoop() to read it.
	 */
	UPROPERTY(EditAnywhere, Category = Spline)
	bool bClosedLoop;

public:
#if WITH_EDITORONLY_DATA
	/** Color of an unselected spline component segment in the editor */
	UPROPERTY(EditAnywhere, Category = Editor)
	FLinearColor EditorUnselectedSplineSegmentColor;

	/** Color of a selected spline component segment in the editor */
	UPROPERTY(EditAnywhere, Category = Editor)
	FLinearColor EditorSelectedSplineSegmentColor;
#endif

	// Begin UActorComponent interface.
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	virtual FName GetComponentInstanceDataType() const override;
	// End UActorComponent interface.

	void ApplyComponentInstanceData(class FSplineInstanceData* ComponentInstanceData);

	/** Update the spline tangents and SplineReparamTable */
	void UpdateSpline();

	/** Specify unselected spline component segment color in the editor */
	UFUNCTION(BlueprintCallable, Category = Editor)
	void SetUnselectedSplineSegmentColor(const FLinearColor& SegmentColor);

	/** Specify selected spline component segment color in the editor */
	UFUNCTION(BlueprintCallable, Category = Editor)
	void SetSelectedSplineSegmentColor(const FLinearColor& SegmentColor);

	/** Specify whether the spline is a closed loop or not */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetClosedLoop(bool bInClosedLoop);

	/** Check whether the spline is a closed loop or not */
	UFUNCTION(BlueprintCallable, Category = Spline)
	bool IsClosedLoop() const;

	/** Clears all the points in the spline */
	UFUNCTION(BlueprintCallable, Category=Spline)
	void ClearSplinePoints();

	/** Adds a world space point to the spline */
	UFUNCTION(BlueprintCallable, Category=Spline)
	void AddSplineWorldPoint(const FVector& Position);

	/** Adds a local space point to the spline */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void AddSplineLocalPoint(const FVector& Position);

	/** Sets the spline to an array of world space points */
	UFUNCTION(BlueprintCallable, Category=Spline)
	void SetSplineWorldPoints(const TArray<FVector>& Points);

	/** Sets the spline to an array of local space points */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetSplineLocalPoints(const TArray<FVector>& Points);

	/** Move an existing point to a new world location */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetWorldLocationAtSplinePoint(int32 PointIndex, const FVector& InLocation);

	/** Get the type of a spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	ESplinePointType::Type GetSplinePointType(int32 PointIndex) const;

	/** Specify the type of a spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetSplinePointType(int32 PointIndex, ESplinePointType::Type Type);

	/** Get the number of points that make up this spline */
	UFUNCTION(BlueprintCallable, Category=Spline)
	int32 GetNumSplinePoints() const;

	/** Get the world location at spline point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetWorldLocationAtSplinePoint(int32 PointIndex) const;

	/** Get local location and tangent at a spline point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	void GetLocalLocationAndTangentAtSplinePoint(int32 PointIndex, FVector& LocalLocation, FVector& LocalTangent) const;

	/** Get the distance along the spline at the spline point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	float GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const;

	/** Returns total length along this spline */
	UFUNCTION(BlueprintCallable, Category=Spline) 
	float GetSplineLength() const;

	/** Given a distance along the length of this spline, return the corresponding input key at that point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	float GetInputKeyAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return the point in space where this puts you */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetWorldLocationAtDistanceAlongSpline(float Distance) const;
	
	/** Given a distance along the length of this spline, return a unit direction vector of the spline tangent there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetWorldDirectionAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return the tangent vector of the spline there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetWorldTangentAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return a rotation corresponding to the spline's position and direction there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FRotator GetWorldRotationAtDistanceAlongSpline(float Distance) const;

	/** Given a time from 0 to the spline duration, return the point in space where this puts you */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetWorldLocationAtTime(float Time, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a unit direction vector of the spline tangent there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetWorldDirectionAtTime(float Time, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a rotation corresponding to the spline's position and direction there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FRotator GetWorldRotationAtTime(float Time, bool bUseConstantVelocity = false) const;

	// UObject interface
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
	// End of UObject interface
#endif

	/** Walk through keys and set time for each one */
	void RefreshSplineInputs();

private:
	/** Returns the length of the specified spline segment up to the parametric value given */
	float GetSegmentLength(const int32 Index, const float Param = 1.0f) const;

	/** Returns the parametric value t which would result in a spline segment of the given length between S(0)...S(t) */
	float GetSegmentParamFromLength(const int32 Index, const float Length, const float SegmentLength) const;

	/** Updates the spline points to reflect a closed loop or not */
	void UpdateLoopEndpoint(bool bInClosedLoop);

	/** Adds an additional point to the end of the spline, whose OutValue and tangents coincides with the first point */
	void AddLoopEndpoint();

	/** Removes the final point from the spline, assuming it to be a duplicate of the first point */
	void RemoveLoopEndpoint();
};



