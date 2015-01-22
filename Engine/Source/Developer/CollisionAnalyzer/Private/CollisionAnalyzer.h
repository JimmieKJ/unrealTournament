// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Stores information about one collision query */
struct FCAQuery
{
	FVector						Start;
	FVector						End;
	FQuat						Rot;
	ECAQueryType::Type			Type;
	ECAQueryShape::Type			Shape;
	FVector						Dims;
	ECollisionChannel			Channel;
	FCollisionQueryParams		Params;
	FCollisionResponseParams	ResponseParams;
	FCollisionObjectQueryParams	ObjectParams;
	TArray<FHitResult>			Results;
	TArray<FHitResult>			TouchAllResults;
	int32						FrameNum;
	float						CPUTime; /** In ms */
	int32						ID;
};

/** Actual implementation of CollisionAnalyzer, private inside module */
class FCollisionAnalyzer : public ICollisionAnalyzer
{
public:
	// Begin ICollisionAnalyzer interface
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
		double CPUTime) override;

	/** Returns a new Collision Analyzer widget. */
	virtual TSharedPtr<SWidget> SummonUI() override;

	virtual void TickAnalyzer(UWorld* InWorld) override;
	virtual bool IsRecording() override;
	// End ICollisionAnalyzer interface

	/** Change the current recording state */
	void SetIsRecording(bool bNewRecording);
	/** Get the current number of frames we have recorded */
	int32 GetNumFramesOfRecording();

	FCollisionAnalyzer()
		: DrawBox(0)		
		, CurrentFrameNum(0)
		, bIsRecording(false)
	{
	}

	virtual ~FCollisionAnalyzer() 
	{
	}

	/** All collected query data */
	TArray<FCAQuery>	Queries;
	/** Indices of queries in Queries array that we want to draw in 3D */
	TArray<int32>		DrawQueryIndices;
	/** AABB to draw in the world */
	FBox				DrawBox;

	DECLARE_EVENT( FCollisionAnalyzer, FQueriesChangedEvent );
	FQueriesChangedEvent& OnQueriesChanged() { return QueriesChangedEvent; }
	FQueriesChangedEvent& OnQueryAdded() { return QueryAddedEvent; }

private:
	/** The current frame number we are on while recording */
	int32				CurrentFrameNum;
	/** Whether we are currently recording */
	bool				bIsRecording;
	/** Event called when Queries array changes */
	FQueriesChangedEvent QueriesChangedEvent;
	/** Event called when a single query is added to array */
	FQueriesChangedEvent QueryAddedEvent;

};