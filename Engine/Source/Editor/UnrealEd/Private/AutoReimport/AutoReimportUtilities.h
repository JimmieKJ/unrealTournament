// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetData;
class IAssetRegistry;

/** An immutable string with a cached CRC for efficient comparison with other strings */
struct FImmutableString
{
	/** Constructible from a raw string */
	FImmutableString(FString InString = FString()) : String(MoveTemp(InString)), CachedHash(0) {}
	FImmutableString& operator=(FString InString) { String = MoveTemp(InString); CachedHash = 0; return *this; }

	FImmutableString(const FImmutableString&) = default;
	FImmutableString& operator=(const FImmutableString&) = default;

	/** Move construction/assignment */
	FImmutableString(FImmutableString&& In) : String(MoveTemp(In.String)), CachedHash(MoveTemp(In.CachedHash)) {}
	FImmutableString& operator=(FImmutableString&& In) { Swap(String, In.String); Swap(CachedHash, In.CachedHash); return *this; }

	/** Check for equality with another immutable string (case-sensitive) */
	friend bool operator==(const FImmutableString& A, const FImmutableString& B)
	{
		return GetTypeHash(A) == GetTypeHash(B) && A.String == B.String;
	}

	/** Calculate the type hash for this string */
	friend uint32 GetTypeHash(const FImmutableString& In)
	{
		if (In.CachedHash == 0 && In.String.Len() != 0)
		{
			In.CachedHash = GetTypeHash(In.String);
		}
		return In.CachedHash;
	}

	/** Get the underlying string */
	const FString& Get() const { return String; }

	/** Serialise this string */
	friend FArchive& operator<<(FArchive& Ar, FImmutableString& String)
	{
		Ar << String.String;
		if (Ar.IsSaving())
		{
			GetTypeHash(String);	
		}
		
		Ar << String.CachedHash;

		return Ar;
	}

private:
	/** The string itself. */
	FString String;

	/** The cached hash of our string. 0 until compared with another string. */
	mutable uint32 CachedHash;
};

/** A time limit that counts down from the time of construction, until it hits a given delay */
struct FTimeLimit
{
	/** Constructor specifying that we should never bail out early */
	FTimeLimit() : Delay(-1) { Reset(); }

	/** Constructor specifying not to run over the specified number of seconds */
	FTimeLimit(float NumSeconds) : Delay(NumSeconds) { Reset(); }

	/** Check whether we have exceeded the time limit */
	bool Exceeded() const { return Delay != -1 && FPlatformTime::Seconds() >= StartTime + Delay; }

	/** Reset the time limit to start timing again from the current time */
	void Reset() { StartTime = FPlatformTime::Seconds(); }

private:

	/** The delay specified by the user */
	float Delay;

	/** The time we started */
	double StartTime;
};

namespace Utils
{
	/** Reduce the array to the specified accumulator */
	template<typename T, typename P, typename A>
	A Reduce(const TArray<T>& InArray, P Predicate, A Accumulator)
	{
		for (const T& X : InArray)
		{
			Accumulator = Predicate(X, Accumulator);
		}
		return Accumulator;
	}

	/** Helper template function to remove duplicates from an array, given some predicate */
	template<typename T, typename P>
	void RemoveDuplicates(TArray<T>& Array, P Predicate)
	{
		if (Array.Num() == 0)
			return;

		const int32 NumElements = Array.Num();
		int32 Write = 0;
		for (int32 Read = 0; Read < NumElements; ++Read)
		{
			for (int32 DuplRead = Read + 1; DuplRead < NumElements; ++DuplRead)
			{
				if (Predicate(Array[Read], Array[DuplRead]))
				{
					goto next;
				}
			}

			if (Write != Read)
			{
				Swap(Array[Write++], Array[Read]);
			}
			else
			{
				Write++;
			}
	next:
			;
		}
		const int32 NumDuplicates = NumElements - Write;
		if (NumDuplicates != 0)
		{
			Array.RemoveAt(Write, NumDuplicates, false);
		}
	}

	/** Find a list of assets that were once imported from the specified filename */
	TArray<FAssetData> FindAssetsPertainingToFile(const IAssetRegistry& Registry, const FString& AbsoluteFilename);

	/** Extract any source file paths from the specified object */
	void ExtractSourceFilePaths(UObject* Object, TArray<FString>& OutSourceFiles);
}