// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Type definitions
-----------------------------------------------------------------------------*/
#define CHUNKED_PROFILER_DATA 1

/** Type definition for an array of profiler samples.*/
#if CHUNKED_PROFILER_DATA
typedef TChunkedArray< class FProfilerSample, 1024 * 64 > FProfilerSampleArray;
#else
typedef TArray< class FProfilerSample > FProfilerSampleArray;
#endif

/** Type definition for an array of pointers to a profiler sample.*/
typedef TArray< class FProfilerSample* > FProfilerSampleArrayPtr;

/** Type definition for an array of indices.*/
typedef TArray< uint32 > FIndicesArray;

/** Type definition for shared pointers to instances of FProfilerSample. */
typedef TSharedPtr<class FProfilerSample> FProfilerSamplePtr;

/** Type definition for shared references to instances of FProfilerSample. */
typedef TSharedRef<class FProfilerSample> FProfilerSampleRef;

/*-----------------------------------------------------------------------------
	Enumerators
-----------------------------------------------------------------------------*/

// TODO: Feedback, is this grouping sufficient for the profiler?

/** Enumerates profiler sample types. */
struct EProfilerSampleTypes
{
	enum Type
	{
		/** Hierarchical - Displayed as a time, used by hierarchical stats, as "%.3f ms". */
		HierarchicalTime,

		/** Numerical - Displayed as a integer number, as "%i". */
		NumberInt,

		/** Numerical - Displayed as a floating number, as "%.2f". */
		NumberFloat,

		/** Memory - Displayed as a human readable data counter, as "%.2f kb". */
		Memory,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};

	/**
	 * @param EProfilerSampleTypes - The value to get the string for.
	 *
	 * @return string representation of the specified EProfilerSampleTypes value.
	 */
	static FString ToName( EProfilerSampleTypes::Type ProfilerSampleType );

	/**
	 * @param EProfilerSampleTypes - The value to get the string for.
	 *
	 * @return string representation with more detailed explanation of the specified EProfilerSampleTypes value.
	 */
	static FString ToDescription( EProfilerSampleTypes::Type ProfilerSampleType );
};

/** Enumerates types where profiler samples can be stored. */
namespace EStorageType
{
	enum Type
	{
		/** Stored in a local memory. */
		LocalMemory,

		/** Stored in a file. */
		BackedUpByFile,

		/** Stored in a global cache. */
		GlobalCache,
		
		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** 
 *	A base profiler sample, should be sufficient to store and visualize all range of profiling samples. 
 */
class FProfilerSample
{	
	friend class IDataProvider;

protected:
	/** Child samples of this profiler sample, as indices to profiler samples. */
	FIndicesArray _ChildrenIndices;

	/** The parent of this profiler sample, as an index to a profiler sample. */
	uint32 _ParentIndex;	

	/** The ID of the thread that this profiler sample was captured on. */
	const uint32 _ThreadID;

	/** The ID of the stat group this profiler sample belongs to, eg. Engine */
	const uint32 _GroupID;

	/** The ID of the stat of the profiler sample, eg. Frametime */
	const uint32 _StatID;

	/** Start time of this profiler sample, in milliseconds. */
	const double _StartMS;

	/** Duration of this profiler sample, in milliseconds. */
	const double _DurationMS;

	/**
	 *	Counter value of this profiler sample. 
	 *	The numerical value for NumberFloat, NumberInt or Memory. 
	 *	The number of times this counter was called for HierarchicalTime. 
	 */
	const double _Counter;

	/** Type of this profiler sample. */
	TEnumAsByte<EProfilerSampleTypes::Type> _Type;	

public:
	static SIZE_T SizeOf()
	{
		return sizeof(FProfilerSample);
	}

	/** The ID of the thread that this profiler sample was captured on. */
	const uint32 ThreadID() const 
	{ 
		return _ThreadID; 
	}

	/** Type of this profiler sample. */
	const EProfilerSampleTypes::Type Type() const 
	{ 
		return _Type; 
	};

	/** The ID of the stat group this profiler sample belongs to, eg. Engine */
	const uint32 GroupID() const 
	{ 
		return _GroupID; 
	}

	/** The ID of the stat of the profiler sample, eg. Frametime */
	const uint32 StatID() const 
	{ 
		return _StatID; 
	}

	/** Start time of the profiler sample, in milliseconds. */
	const double StartMS() const 
	{ 
		return _StartMS; 
	}

	/** Duration of the profiler sample, in milliseconds. */
	const double DurationMS() const 
	{ 
		return _DurationMS; 
	}

	/**
	 *	Counter value of this profiler sample. 
	 *	The numerical value for NumberFloat, NumberInt or Memory. 
	 *	The number of times this counter was called for Time. 
	 */
	const double CounterAsFloat() const 
	{ 
		return _Counter; 
	}

	/**
	 *	Counter value of this profiler sample. 
	 *	The numerical value for NumberFloat, NumberInt or Memory. 
	 *	The number of times this counter was called for HierarchicalTime. 
	 */
	const int64 CounterAsInt() const 
	{ 
		return (int64)_Counter;
	}

	/** A parent of this profiler sample, as an index to a profiler sample. */
	const uint32 ParentIndex() const 
	{ 
		return _ParentIndex; 
	}
	 
	/** Child samples of this profiler sample, as indices to profiler samples. */
	const FIndicesArray& ChildrenIndices() const 
	{ 
		return _ChildrenIndices; 
	}

	/** Adds a child to this profiler sample. */
	void AddChild( const uint32 ChildIndex ) 
	{
		_ChildrenIndices.Add( ChildIndex );
	}

	/** Fixes children ordering for the hierarchical representation. */
	void FixupChildrenOrdering( const TMap<uint32,uint32>& ChildrenOrderingIndices );

public:
	/** Constant, invalid index. */
	static const uint32 InvalidIndex = (uint32)(-1);
	/** Constant, invalid profiler sample, will be use in situation when a profiler sample can't be found or doesn't exist. */
	static const FProfilerSample Invalid;

public:
	/**
	 * Initialization constructor for hierarchical samples.
	 *
	 * @param InThreadID		- The ID of the thread that this profiler sample was captured on
	 * @param InGroupID			- The ID of the stat group that this profiler sample belongs to
	 * @param InStatID			- The ID of the stat of this profiler sample
	 * @param InStartMs			- The start time of this profiler sample, in milliseconds
	 * @param InDurationMs		- The duration of this profiler sample, in milliseconds
	 * @param InCallsPerFrame	- The number of times this counter was called in the frame it was captured in
	 * @param InParentIndex		- The parent of this profiler sample, as an index to a profiler sample
	 *
	 */
	FProfilerSample( uint32 InThreadID, uint32 InGroupID, uint32 InStatID, double InStartMs, double InDurationMs, uint32 InCallsPerFrame, uint32 InParentIndex = InvalidIndex )
		: _ParentIndex( InParentIndex )
		, _ThreadID( InThreadID )
		, _GroupID( InGroupID )
		, _StatID( InStatID )
		, _StartMS( InStartMs )
		, _DurationMS( InDurationMs )
		, _Counter( (double)InCallsPerFrame )
		, _Type( EProfilerSampleTypes::HierarchicalTime )
	{}

	/**
	 * Initialization constructor for non-hierarchical samples.
	 *
	 * @param InGroupID				- The ID of the stat group that this profiler sample belongs to
	 * @param InStatID				- The ID of the stat of this profiler sample
	 * @param InCounter				- The counter value for this profiler sample
	 * @param InProfilerSampleType	- The profiler sample type of this profiler sample
	 *
	 */
	FProfilerSample( uint32 InGroupID, uint32 InStatID, double InCounter, const EProfilerSampleTypes::Type InProfilerSampleType )
		: _ParentIndex( InvalidIndex )
		, _ThreadID( 0 )
		, _GroupID( InGroupID )
		, _StatID( InStatID )
		, _StartMS( 0.0f )
		, _DurationMS( 0.0f )
		, _Counter( InCounter )
		, _Type( InProfilerSampleType )
	{}

	/** Copy constructor. */
	explicit FProfilerSample( const FProfilerSample& Other )
		: _ChildrenIndices( Other._ChildrenIndices )
		, _ParentIndex( Other._ParentIndex )
		, _ThreadID( Other._ThreadID )
		, _GroupID( Other._GroupID )
		, _StatID( Other._StatID )
		, _StartMS( Other._StartMS )
		, _DurationMS( Other._DurationMS )
		, _Counter( Other._Counter )
		, _Type( Other._Type )
	{}

	/** Default constructor, creates an invalid profiler sample. */
	FProfilerSample()
		: _ParentIndex( InvalidIndex )
		, _ThreadID( 0 )
		, _GroupID( InvalidIndex )
		, _StatID( InvalidIndex )
		, _StartMS( 0.0f )
		, _DurationMS( 0.0f )
		, _Counter( 0.0f )
		, _Type( EProfilerSampleTypes::InvalidOrMax )
	{}

	/** Updates the duration of this sample, should be only used to update the root stat. */
	void SetDurationMS( double InDurationMs )
	{
		double& MutableDurationRef = const_cast<double&>(_DurationMS);
		MutableDurationRef = InDurationMs;
	}

	void SetStartAndEndMS( double InStartMS, double InEndMS )
	{
		double& MutableStartRef = const_cast<double&>(_StartMS);
		MutableStartRef = InStartMS;

		double& MutableDurationRef = const_cast<double&>(_DurationMS);
		MutableDurationRef = InEndMS - InStartMS;
	}

	/** @return end time of this profiler sample, in milliseconds. */
	const double GetEndMS() const
	{
		return _StartMS + _DurationMS;
	}

	/** @return frame rate of this profiler sample. */
	const float GetFramerate() const
	{
		const double Framerate = 1000.0 / _DurationMS;
		return (float)Framerate;
	}

	/** @return true if this profiler sample is valid. */
	const bool IsValid() const
	{
		return _Type != EProfilerSampleTypes::InvalidOrMax;
	}

	/** @return true if IndexToCheck is valid. */
	static const bool IsIndexValid( const uint32 IndexToCheck ) 
	{
		return IndexToCheck != InvalidIndex;
	}
};

class IHistogramDataSource
{
public:
	virtual int32 GetCount(float MinVal, float MaxVal) = 0;
	virtual int32 GetTotalCount() = 0;
};
