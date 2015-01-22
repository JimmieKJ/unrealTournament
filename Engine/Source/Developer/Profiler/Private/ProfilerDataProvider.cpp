// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"

/*-----------------------------------------------------------------------------
	IDataProvider
-----------------------------------------------------------------------------*/

void IDataProvider::InternalDuplicate( const IDataProviderRef& DataProvider, const uint32 FrameStartIndex, const uint32 FrameEndIndex )
{
	for( uint32 FrameIdx = FrameStartIndex; FrameIdx < FrameEndIndex; FrameIdx++ )
	{
		const uint32 SampleStartIndex = GetSamplesIndicesForFrame( FrameIdx ).X;
		const uint32 SampleEndIndex = GetSamplesIndicesForFrame( FrameIdx ).Y;

		for( uint32 SampleIndex = SampleStartIndex; SampleIndex < SampleEndIndex; SampleIndex++ )
		{
			FProfilerSample ProfilerSample( GetCollection()[ SampleIndex ] );

			if( FProfilerSample::IsIndexValid( ProfilerSample._ParentIndex ) )
			{
				ProfilerSample._ParentIndex -= SampleStartIndex;
			}

			for( int32 ChildIdx = 0; ChildIdx < ProfilerSample._ChildrenIndices.Num(); ChildIdx++ )
			{
				ProfilerSample._ChildrenIndices[ChildIdx] -= SampleStartIndex;
			}

			DataProvider->AddDuplicatedSample( ProfilerSample );
		}
		DataProvider->AdvanceFrame( GetFrameTimeMS( FrameIdx ) );
	}
}

const uint32 IDataProvider::AdvanceFrame( const float DeltaTimeMS )
{
	if( !bHasAddedSecondStartMarker )
	{
		bHasAddedSecondStartMarker = true;

		// Add the default values.
		FrameCounters.Add( LastSecondFrameCounter );
		AccumulatedFrameCounters.Add( GetNumFrames() );
	}

	ElapsedTimeMS += DeltaTimeMS;
	LastSecondFrameTimeMS += DeltaTimeMS;

	LastSecondFrameCounter ++;

	const uint32 SampleEndIndex = GetNumSamples();
	FrameIndices.Add( FIntPoint( FrameIndex, SampleEndIndex ) );

	FrameTimes.Add( DeltaTimeMS );
	ElapsedFrameTimes.Add( ElapsedTimeMS );

	// Update the values.
	FrameCounters.Last() = LastSecondFrameCounter;
	AccumulatedFrameCounters.Last() = GetNumFrames();

	int NumLongFrames = 0;
	while( LastSecondFrameTimeMS > 1000.0f )
	{
		if( NumLongFrames > 0 )
		{
			// Add the default values.
			FrameCounters.Add( LastSecondFrameCounter );
			AccumulatedFrameCounters.Add( GetNumFrames() );
		}

		LastSecondFrameTimeMS -= 1000.0f;
		bHasAddedSecondStartMarker = false;
		LastSecondFrameCounter = 0;
		NumLongFrames ++;
	}

	FrameIndex = SampleEndIndex;
	return FrameIndex;
}

/*-----------------------------------------------------------------------------
	FArrayDataProvider
-----------------------------------------------------------------------------*/

const uint32 FArrayDataProvider::AddHierarchicalSample
( 
	const uint32 InThreadID, 
	const uint32 InGroupID, 
	const uint32 InStatID, 
	const double InStartMS, 
	const double InDurationMS, 
	const uint32 InCallsPerFrame, 
	const uint32 InParentIndex /*= FProfilerSample::InvalidIndex */
)
{
	uint32 HierSampleIndex = Collection.Num();
	FProfilerSample HierarchicalSample( InThreadID, InGroupID, InStatID, InStartMS, InDurationMS, InCallsPerFrame, InParentIndex );
#if CHUNKED_PROFILER_DATA
	Collection.AddElement( HierarchicalSample );
#else
	Collection.Add( HierarchicalSample );
#endif

	if( FProfilerSample::IsIndexValid( InParentIndex ) )
	{
		FProfilerSample& Parent = Collection[ InParentIndex ];
		const uint32 InitialMemUsage = Parent.ChildrenIndices().GetAllocatedSize();
		Parent.AddChild( HierSampleIndex );
		const uint32 AccumulatedMemoryUsage = Parent.ChildrenIndices().GetAllocatedSize() - InitialMemUsage;
		ChildrenIndicesMemoryUsage += AccumulatedMemoryUsage;
	}

	return HierSampleIndex;
}

void FArrayDataProvider::AddCounterSample
( 
	const uint32 InGroupID, 
	const uint32 InStatID, 
	const double InCounter, 
	const EProfilerSampleTypes::Type InProfilerSampleType 
)
{
	FProfilerSample CounterSample( InGroupID, InStatID, InCounter, InProfilerSampleType );
#if CHUNKED_PROFILER_DATA
	Collection.AddElement( CounterSample );
#else
	Collection.Add( CounterSample );
#endif
}

const uint32 FArrayDataProvider::AddDuplicatedSample( const FProfilerSample& ProfilerSample )
{
	uint32 DuplicateSampleIndex = Collection.Num();
#if CHUNKED_PROFILER_DATA
	Collection.AddElement( ProfilerSample );
#else
	Collection.Add( ProfilerSample );
#endif
	return DuplicateSampleIndex;
}

const uint32 FArrayDataProvider::GetNumSamples() const
{
	return Collection.Num();
}

const uint64 FArrayDataProvider::GetMemoryUsage() const 
{
	uint64 MemoryUsage = FrameTimes.GetAllocatedSize();
	MemoryUsage += FrameIndices.GetAllocatedSize();
	MemoryUsage += Collection.GetAllocatedSize();
	MemoryUsage += ChildrenIndicesMemoryUsage;
	return MemoryUsage;
}
