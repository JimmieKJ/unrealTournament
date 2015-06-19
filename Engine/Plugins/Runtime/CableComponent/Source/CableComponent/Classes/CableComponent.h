// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CableComponent.generated.h"

/** Struct containing information about a point along the cable */
struct FCableParticle
{
	FCableParticle()
	: bFree(true)
	, Position(0,0,0)
	, OldPosition(0,0,0)
	{}

	/** If this point is free (simulating) or fixed to something */
	bool bFree;
	/** Current position of point */
	FVector Position;
	/** Position of point on previous iteration */
	FVector OldPosition;
};

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object, Physics, Collision, Activation, "Components|Activation"), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class CABLECOMPONENT_API UCableComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	// Begin UActorComponent interface.
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void SendRenderDynamicData_Concurrent() override;
	// Begin UActorComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// Begin USceneComponent interface.

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const override;
	// End UMeshComponent interface.


	/** Actor or Component that the end of the cable should be attached to */
	UPROPERTY(EditAnywhere, Category="Cable")
	FComponentReference AttachEndTo;

	/** End location of cable, relative to AttachEndTo if specified, otherwise relative to cable component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable")
	FVector EndLocation;

	/** Attaches the end of the cable to a specific Component within an Actor **/
	UFUNCTION(BlueprintCallable, Category = "Cable")
	void SetAttachEndTo(AActor* Actor, FName ComponentProperty);
	
	/** Gets the Actor that the cable is attached to **/
	UFUNCTION(BlueprintCallable, Category = "Cable")
	AActor* GetAttachedActor() const;

	/** Gets the specific USceneComponent that the cable is attached to **/
	UFUNCTION(BlueprintCallable, Category = "Cable")
	USceneComponent* GetAttachedComponent() const;

	/** Rest length of the cable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable", meta=(ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float CableLength;

	/** How many segments the cable has */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cable", meta=(ClampMin = "1", UIMin = "1", UIMax = "20"))
	int32 NumSegments;

	/** Controls the simulation substep time for the cable */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category="Cable", meta=(ClampMin = "0.005", UIMin = "0.005", UIMax = "0.1"))
	float SubstepTime;

	/** The number of solver iterations controls how 'stiff' the cable is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable", meta=(ClampMin = "1", ClampMax = "8"))
	int32 SolverIterations;


	/** How wide the cable geometry is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable Rendering", meta=(ClampMin = "0.01", UIMin = "0.01", UIMax = "50.0"))
	float CableWidth;

	/** Number of sides of the cable geometry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cable Rendering", meta=(ClampMin = "1", ClampMax = "16"))
	int32 NumSides;

	/** How many times to repeat the material along the length of the cable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable Rendering", meta=(UIMin = "0.1", UIMax = "8"))
	float TileMaterial;

private:

	/** Solve the cable spring constraints */
	void SolveConstraints();
	/** Integrate cable point positions */
	void VerletIntegrate(float SubstepTime, const FVector& Gravity);
	/** Perform a simulation substep */
	void PerformSubstep(float SubstepTime, const FVector& Gravity);
	/** Get start and end position for the cable */
	void GetEndPositions(FVector& OutStartPosition, FVector& OutEndPosition);
	/** Amount of time 'left over' from last tick */
	float TimeRemainder;
	/** Array of cable particles */
	TArray<FCableParticle>	Particles;


	friend class FCableSceneProxy;
};


