// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Enum to describe shape of the query */
namespace ECAQueryShape
{
	enum Type
	{
		Raycast,
		SphereSweep,
		CapsuleSweep,
		BoxSweep,
		ConvexSweep
	};
}

/** Enum to describe the type of query performed */
namespace ECAQueryType
{
	enum Type
	{
		Test,
		Single,
		Multi
	};
}

class ICollisionAnalyzer
{
public:
	/** Virtual destructor */
	virtual ~ICollisionAnalyzer() {}

	/** */
	virtual void CaptureQuery(
		const FVector& Start, 
		const FVector& End, 
		const FQuat& Rot, 
		ECAQueryType::Type QueryType, 
		ECAQueryShape::Type QueryShape, 
		const FVector& Dims, 
		ECollisionChannel TraceChannel, 
		const struct FCollisionQueryParams& Params, 
		const FCollisionResponseParams&	ResponseParams,
		const FCollisionObjectQueryParams&	ObjectParams,
		const TArray<FHitResult>& Results, 
		const TArray<FHitResult>& TouchAllResults,
		double CPUTime) = 0;

	/** Returns a new Collision Analyzer widget. */
	virtual TSharedPtr<SWidget> SummonUI() = 0;

	/** */
	virtual void TickAnalyzer(UWorld* InWorld) = 0;

	/** */
	virtual bool IsRecording() = 0;
};