// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "SplineComponent.generated.h"



/** Permitted spline point types for SplineComponent. */
UENUM()
namespace ESplinePointType
{
	enum Type
	{
		Linear,
		Curve,
		Constant,
		CurveClamped,
		CurveCustomTangent
	};
}

/** Types of coordinate space accepted by the functions. */
UENUM()
namespace ESplineCoordinateSpace
{
	enum Type
	{
		Local,
		World
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

	/** Spline built from position data. */
	UPROPERTY()//EditAnywhere, Category = Points)
	FInterpCurveVector SplineInfo;

	/** Spline built from rotation data. */
	UPROPERTY()
	FInterpCurveQuat SplineRotInfo;

	/** Spline built from scale data. */
	UPROPERTY()
	FInterpCurveVector SplineScaleInfo;

	/** Input, distance along curve, output, parameter that puts you there. */
	UPROPERTY()
	FInterpCurveFloat SplineReparamTable;

	UPROPERTY()
	bool bAllowSplineEditingPerInstance_DEPRECATED;

	/** Number of steps per spline segment to place in the reparameterization table */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Spline, meta=(ClampMin=4, UIMin=4, ClampMax=100, UIMax=100))
	int32 ReparamStepsPerSegment;

	/** Specifies the duration of the spline in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spline)
	float Duration;

	/** Whether the endpoints of the spline are considered stationary when traversing the spline at non-constant velocity.  Essentially this sets the endpoints' tangents to zero vectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Spline, meta=(EditCondition="!bClosedLoop"))
	bool bStationaryEndpoints;

	/** Whether the spline has been edited from its default by the spline component visualizer */
	UPROPERTY(EditAnywhere, Category = Spline, meta=(DisplayName="Override Construction Script"))
	bool bSplineHasBeenEdited;

private:
	/**
	 * Whether the spline is to be considered as a closed loop.
	 * Use SetClosedLoop() to set this property, and IsClosedLoop() to read it.
	 */
	UPROPERTY(EditAnywhere, Category = Spline)
	bool bClosedLoop;

public:
	/** Default up vector in local space to be used when calculating transforms along the spline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spline)
	FVector DefaultUpVector;

#if WITH_EDITORONLY_DATA
	/** Color of an unselected spline component segment in the editor */
	UPROPERTY(EditAnywhere, Category = Editor)
	FLinearColor EditorUnselectedSplineSegmentColor;

	/** Color of a selected spline component segment in the editor */
	UPROPERTY(EditAnywhere, Category = Editor)
	FLinearColor EditorSelectedSplineSegmentColor;

	/** Whether scale visualization should be displayed */
	UPROPERTY(EditAnywhere, Category = Editor)
	bool bShouldVisualizeScale;

	/** Width of spline in editor for use with scale visualization */
	UPROPERTY(EditAnywhere, Category = Editor)
	float ScaleVisualizationWidth;
#endif

	//~ Begin UActorComponent Interface.
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	//~ End UActorComponent Interface.

	void ApplyComponentInstanceData(class FSplineInstanceData* ComponentInstanceData, const bool bPostUCS);

	/** Update the spline tangents and SplineReparamTable */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void UpdateSpline();

	/** Get location along spline at the provided input key value */
	FVector GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get tangent along spline at the provided input key value */
	FVector GetTangentAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get unit direction along spline at the provided input key value */
	FVector GetDirectionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get rotator corresponding to rotation along spline at the provided input key value */
	FRotator GetRotationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get quaternion corresponding to rotation along spline at the provided input key value */
	FQuat GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get up vector at the provided input key value */
	FVector GetUpVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get up vector at the provided input key value */
	FVector GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get transform at the provided input key value */
	FTransform GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale = false) const;

	/** Get roll in degrees at the provided input key value */
	float GetRollAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get scale at the provided input key value */
	FVector GetScaleAtSplineInputKey(float InKey) const;

	DEPRECATED(4.9, "This has been replaced by GetNumberOfSplinePoints(), as it is a clearer name for Blueprint users.")
	int32 GetNumSplinePoints() const { return GetNumberOfSplinePoints(); }

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
	UFUNCTION(BlueprintCallable, Category = Spline)
	void ClearSplinePoints();

	/** Adds a point to the spline */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void AddSplinePoint(const FVector& Position, ESplineCoordinateSpace::Type CoordinateSpace);

	/** Adds a point to the spline at the specified index*/
	UFUNCTION(BlueprintCallable, Category = Spline)
	void AddSplinePointAtIndex(const FVector& Position, int32 Index, ESplineCoordinateSpace::Type CoordinateSpace);

	/** Removes point at specified index from the spline */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void RemoveSplinePoint(const int32 Index);

	/** Adds a world space point to the spline */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use AddSplinePoint, specifying SplineCoordinateSpace::World"))
	void AddSplineWorldPoint(const FVector& Position);

	/** Adds a local space point to the spline */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use AddSplinePoint, specifying SplineCoordinateSpace::Local"))
	void AddSplineLocalPoint(const FVector& Position);

	/** Sets the spline to an array of points */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetSplinePoints(const TArray<FVector>& Points, ESplineCoordinateSpace::Type CoordinateSpace);

	/** Sets the spline to an array of world space points */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use SetSplinePoints, specifying SplineCoordinateSpace::World"))
	void SetSplineWorldPoints(const TArray<FVector>& Points);

	/** Sets the spline to an array of local space points */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use SetSplinePoints, specifying SplineCoordinateSpace::Local"))
	void SetSplineLocalPoints(const TArray<FVector>& Points);

	/** Move an existing point to a new location */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetLocationAtSplinePoint(int32 PointIndex, const FVector& InLocation, ESplineCoordinateSpace::Type CoordinateSpace);

	/** Move an existing point to a new world location */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use SetLocationAtSplinePoint, specifying SplineCoordinateSpace::World"))
	void SetWorldLocationAtSplinePoint(int32 PointIndex, const FVector& InLocation);

	/** Specify the tangent at a given spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetTangentAtSplinePoint(int32 PointIndex, const FVector& InTangent, ESplineCoordinateSpace::Type CoordinateSpace);

	/** Get the type of a spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	ESplinePointType::Type GetSplinePointType(int32 PointIndex) const;

	/** Specify the type of a spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetSplinePointType(int32 PointIndex, ESplinePointType::Type Type);

	/** Get the number of points that make up this spline */
	UFUNCTION(BlueprintCallable, Category = Spline)
	int32 GetNumberOfSplinePoints() const;

	/** Get the location at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetLocationAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the world location at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetLocationAtSplinePoint, specifying SplineCoordinateSpace::World"))
	FVector GetWorldLocationAtSplinePoint(int32 PointIndex) const;

	/** Get the location at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetDirectionAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the tangent at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetTangentAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the rotation at spline point as a quaternion */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FQuat GetQuaternionAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the rotation at spline point as a rotator */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FRotator GetRotationAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the up vector at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetUpVectorAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the right vector at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetRightVectorAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the amount of roll at spline point, in degrees */
	UFUNCTION(BlueprintCallable, Category = Spline)
	float GetRollAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get the scale at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetScaleAtSplinePoint(int32 PointIndex) const;

	/** Get the transform at spline point */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FTransform GetTransformAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale = false) const;

	/** Get location and tangent at a spline point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	void GetLocationAndTangentAtSplinePoint(int32 PointIndex, FVector& Location, FVector& Tangent, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Get local location and tangent at a spline point */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetLocationAndTangentAtSplinePoint, specifying SplineCoordinateSpace::Local"))
	void GetLocalLocationAndTangentAtSplinePoint(int32 PointIndex, FVector& LocalLocation, FVector& LocalTangent) const;

	/** Get the distance along the spline at the spline point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	float GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const;

	/** Returns total length along this spline */
	UFUNCTION(BlueprintCallable, Category=Spline) 
	float GetSplineLength() const;

	/** Sets the default up vector used by this spline */
	UFUNCTION(BlueprintCallable, Category=Spline)
	void SetDefaultUpVector(const FVector& UpVector, ESplineCoordinateSpace::Type CoordinateSpace);

	/** Gets the default up vector used by this spline */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetDefaultUpVector(ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return the corresponding input key at that point */
	UFUNCTION(BlueprintCallable, Category=Spline)
	float GetInputKeyAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return the point in space where this puts you */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetLocationAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return the point in world space where this puts you */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetLocationAtDistanceAlongSpline, specifying SplineCoordinateSpace::World"))
	FVector GetWorldLocationAtDistanceAlongSpline(float Distance) const;
	
	/** Given a distance along the length of this spline, return a unit direction vector of the spline tangent there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetDirectionAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return a unit direction vector of the spline tangent there, in world space. */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetDirectionAtDistanceAlongSpline, specifying SplineCoordinateSpace::World"))
	FVector GetWorldDirectionAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return the tangent vector of the spline there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetTangentAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return the tangent vector of the spline there, in world space. */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetTangentAtDistanceAlongSpline, specifying SplineCoordinateSpace::World"))
	FVector GetWorldTangentAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return a quaternion corresponding to the spline's rotation there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FQuat GetQuaternionAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return a rotation corresponding to the spline's rotation there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FRotator GetRotationAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return a rotation corresponding to the spline's rotation there, in world space. */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetRotationAtDistanceAlongSpline, specifying SplineCoordinateSpace::World"))
	FRotator GetWorldRotationAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return a unit direction vector corresponding to the spline's up vector there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetUpVectorAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return a unit direction vector corresponding to the spline's right vector there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetRightVectorAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return the spline's roll there, in degrees. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	float GetRollAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a distance along the length of this spline, return the spline's scale there. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector GetScaleAtDistanceAlongSpline(float Distance) const;

	/** Given a distance along the length of this spline, return an FTransform corresponding to that point on the spline. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FTransform GetTransformAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale = false) const;

	/** Given a time from 0 to the spline duration, return the point in space where this puts you */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetLocationAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return the point in space where this puts you */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetLocationAtTime, specifying SplineCoordinateSpace::World"))
	FVector GetWorldLocationAtTime(float Time, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a unit direction vector of the spline tangent there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetDirectionAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a unit direction vector of the spline tangent there. */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetDirectionAtTime, specifying SplineCoordinateSpace::World"))
	FVector GetWorldDirectionAtTime(float Time, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return the spline's tangent there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetTangentAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a quaternion corresponding to the spline's rotation there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FQuat GetQuaternionAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a rotation corresponding to the spline's position and direction there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FRotator GetRotationAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return a rotation corresponding to the spline's position and direction there. */
	UFUNCTION(BlueprintCallable, Category = Spline, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetRotationAtTime, specifying SplineCoordinateSpace::World"))
	FRotator GetWorldRotationAtTime(float Time, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return the spline's up vector there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetUpVectorAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return the spline's right vector there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetRightVectorAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return the spline's transform at the corresponding position. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FTransform GetTransformAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false, bool bUseScale = false) const;

	/** Given a time from 0 to the spline duration, return the spline's roll there, in degrees. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	float GetRollAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity = false) const;

	/** Given a time from 0 to the spline duration, return the spline's scale there. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector GetScaleAtTime(float Time, bool bUseConstantVelocity = false) const;

    /** Given a location, in world space, return the input key closest to that location. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	float FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const;

	/** Given a location, in world space, return the point on the curve that is closest to the location. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector FindLocationClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a location, in world spcae, return a unit direction vector of the spline tangent closest to the location. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	FVector FindDirectionClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a location, in world space, return the tangent vector of the spline closest to the location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector FindTangentClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a location, in world space, return a quaternion corresponding to the spline's rotation closest to the location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FQuat FindQuaternionClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a location, in world space, return rotation corresponding to the spline's rotation closest to the location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FRotator FindRotationClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a location, in world space, return a unit direction vector corresponding to the spline's up vector closest to the location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector FindUpVectorClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

    /** Given a location, in world space, return a unit direction vector corresponding to the spline's right vector closest to the location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector FindRightVectorClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

    /** Given a location, in world space, return the spline's roll closest to the location, in degrees. */
	UFUNCTION(BlueprintCallable, Category = Spline)
    float FindRollClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Given a location, in world space, return the spline's scale closest to the location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FVector FindScaleClosestToWorldLocation(const FVector& WorldLocation) const;

	/** Given a location, in world space, return an FTransform closest to that location. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	FTransform FindTransformClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale = false) const;


	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
#if WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

private:
	/** Returns the length of the specified spline segment up to the parametric value given */
	float GetSegmentLength(const int32 Index, const float Param = 1.0f) const;

	/** Returns the parametric value t which would result in a spline segment of the given length between S(0)...S(t) */
	float GetSegmentParamFromLength(const int32 Index, const float Length, const float SegmentLength) const;
};


// Deprecated method definitions
inline void USplineComponent::AddSplineWorldPoint(const FVector& Position) { AddSplinePoint(Position, ESplineCoordinateSpace::World); }
inline void USplineComponent::AddSplineLocalPoint(const FVector& Position) { AddSplinePoint(Position, ESplineCoordinateSpace::Local); }
inline void USplineComponent::SetSplineWorldPoints(const TArray<FVector>& Points) { SetSplinePoints(Points, ESplineCoordinateSpace::World); }
inline void USplineComponent::SetSplineLocalPoints(const TArray<FVector>& Points) { SetSplinePoints(Points, ESplineCoordinateSpace::Local); }
inline void USplineComponent::SetWorldLocationAtSplinePoint(int32 PointIndex, const FVector& InLocation) { SetLocationAtSplinePoint(PointIndex, InLocation, ESplineCoordinateSpace::World); }
inline FVector USplineComponent::GetWorldLocationAtSplinePoint(int32 PointIndex) const { return GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World); }
inline void USplineComponent::GetLocalLocationAndTangentAtSplinePoint(int32 PointIndex, FVector& LocalLocation, FVector& LocalTangent) const { GetLocationAndTangentAtSplinePoint(PointIndex, LocalLocation, LocalTangent, ESplineCoordinateSpace::Local); }
inline FVector USplineComponent::GetWorldLocationAtDistanceAlongSpline(float Distance) const { return GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World); }
inline FVector USplineComponent::GetWorldDirectionAtDistanceAlongSpline(float Distance) const { return GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World); }
inline FVector USplineComponent::GetWorldTangentAtDistanceAlongSpline(float Distance) const { return GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World); }
inline FRotator USplineComponent::GetWorldRotationAtDistanceAlongSpline(float Distance) const { return GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World); }
inline FVector USplineComponent::GetWorldLocationAtTime(float Time, bool bUseConstantVelocity) const { return GetLocationAtTime(Time, ESplineCoordinateSpace::World, bUseConstantVelocity); }
inline FVector USplineComponent::GetWorldDirectionAtTime(float Time, bool bUseConstantVelocity) const { return GetDirectionAtTime(Time, ESplineCoordinateSpace::World, bUseConstantVelocity); }
inline FRotator USplineComponent::GetWorldRotationAtTime(float Time, bool bUseConstantVelocity) const { return GetRotationAtTime(Time, ESplineCoordinateSpace::World, bUseConstantVelocity); }
