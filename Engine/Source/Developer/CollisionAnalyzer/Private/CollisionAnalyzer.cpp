// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CollisionAnalyzerPCH.h"
#include "CollisionDebugDrawingPublic.h"


//////////////////////////////////////////////////////////////////////////


void FCollisionAnalyzer::CaptureQuery(	const FVector& Start, 
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
										double CPUTime) 
{
	if(bIsRecording)
	{
		int32 NewQueryId = Queries.AddZeroed();
		FCAQuery& NewQuery = Queries[NewQueryId];
		NewQuery.Start = Start;
		NewQuery.End = End;
		NewQuery.Rot = Rot;
		NewQuery.Type = QueryType;
		NewQuery.Shape = QueryShape;
		NewQuery.Dims = Dims;
		NewQuery.Channel = TraceChannel;
		NewQuery.Params = Params;
		NewQuery.ResponseParams = ResponseParams;
		NewQuery.ObjectParams = ObjectParams;
		NewQuery.Results = Results;
		NewQuery.TouchAllResults = TouchAllResults;
		NewQuery.FrameNum = CurrentFrameNum;
		NewQuery.CPUTime = CPUTime * 1000.f;		
		NewQuery.ID = NewQueryId;


		QueryAddedEvent.Broadcast();
	}
}

TSharedPtr<SWidget> FCollisionAnalyzer::SummonUI() 
{
	TSharedPtr<SWidget> ReturnWidget;

	UE_LOG(LogCollisionAnalyzer, Log, TEXT("Opening CollisionAnalyzer..."));

	if( IsInGameThread() )
	{
		// Make a window
		ReturnWidget = SNew(SCollisionAnalyzer, this);			
	}
	else
	{
		UE_LOG(LogCollisionAnalyzer, Warning, TEXT("FCollisionAnalyzer::DisplayUI: Not in game thread."));
	}

	return ReturnWidget;
}

bool FCollisionAnalyzer::IsRecording()
{
	return bIsRecording;
}

void FCollisionAnalyzer::TickAnalyzer(UWorld* World)
{
	if(bIsRecording)
	{
		// Increment frame number
		CurrentFrameNum++;
	}

	// Draw any queries requested
	for(int32 DrawIdx=0; DrawIdx<DrawQueryIndices.Num(); DrawIdx++)
	{
		int32 QueryIdx = DrawQueryIndices[DrawIdx];
		if(QueryIdx < Queries.Num())
		{
			FCAQuery& DrawQuery = Queries[QueryIdx];
			if(DrawQuery.Shape == ECAQueryShape::Raycast)
			{
				DrawLineTraces(World, DrawQuery.Start, DrawQuery.End, DrawQuery.Results, 0.f);
			}
			else if(DrawQuery.Shape == ECAQueryShape::SphereSweep)
			{
				DrawSphereSweeps(World, DrawQuery.Start, DrawQuery.End, DrawQuery.Dims.X, DrawQuery.Results, 0.f);
			}
			else if(DrawQuery.Shape == ECAQueryShape::BoxSweep)
			{
				DrawBoxSweeps(World, DrawQuery.Start, DrawQuery.End, DrawQuery.Dims, DrawQuery.Rot, DrawQuery.Results, 0.f);
			}
			else if(DrawQuery.Shape == ECAQueryShape::CapsuleSweep)
			{
				DrawCapsuleSweeps(World, DrawQuery.Start, DrawQuery.End, DrawQuery.Dims.Z, DrawQuery.Dims.X, DrawQuery.Rot, DrawQuery.Results, 0.f);
			}
		}
	}

	// Draw debug box if desired
	if(DrawBox.IsValid)
	{
		DrawDebugBox(World, DrawBox.GetCenter(), DrawBox.GetExtent(), FColor::White);
	}
}


void FCollisionAnalyzer::SetIsRecording(bool bNewRecording)
{
	if(bNewRecording != bIsRecording)
	{
		// If starting recording, reset queries and zero frame counter.
		if(bNewRecording)
		{
			Queries.Reset();
			DrawQueryIndices.Empty();
			CurrentFrameNum = 0;

			QueriesChangedEvent.Broadcast();
		}

		bIsRecording = bNewRecording;
	}
}

int32 FCollisionAnalyzer::GetNumFramesOfRecording()
{
	return CurrentFrameNum+1;
}
