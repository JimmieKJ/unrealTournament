// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Default predicate class. Assumes < operator is defined for the template type.
 */
template<typename T>
struct TLess
{
	FORCEINLINE bool operator()(const T& A, const T& B) const { return A < B; }
};

/**
 * Utility predicate class. Assumes < operator is defined for the template type.
 */
template<typename T>
struct TGreater
{
	FORCEINLINE bool operator()(const T& A, const T& B) const { return B < A; }
};

/**
 * Helper class for dereferencing pointer types in Sort function
 */
template<typename T, class PREDICATE_CLASS> 
struct TDereferenceWrapper
{
	const PREDICATE_CLASS& Predicate;

	TDereferenceWrapper( const PREDICATE_CLASS& InPredicate )
		: Predicate( InPredicate ) {}
  
	/** Pass through for non-pointer types */
	FORCEINLINE bool operator()( T& A, T& B ) { return Predicate( A, B ); } 
	FORCEINLINE bool operator()( const T& A, const T& B ) const { return Predicate( A, B ); } 
};
/** Partially specialized version of the above class */
template<typename T, class PREDICATE_CLASS> 
struct TDereferenceWrapper<T*, PREDICATE_CLASS>
{
	const PREDICATE_CLASS& Predicate;

	TDereferenceWrapper( const PREDICATE_CLASS& InPredicate )
		: Predicate( InPredicate ) {}
  
	/** Dereferennce pointers */
	FORCEINLINE bool operator()( T* A, T* B ) const 
	{
		return Predicate( *A, *B ); 
	} 
};

/**
 * Sort elements using user defined predicate class. The sort is unstable, meaning that the ordering of equal items is not necessarily preserved.
 * This is the internal sorting function used by Sort overrides.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS> 
void SortInternal( T* First, const int32 Num, const PREDICATE_CLASS& Predicate )
{
	struct FStack
	{
		T* Min;
		T* Max;
	};

	if( Num < 2 )
	{
		return;
	}
	FStack RecursionStack[32]={{First,First+Num-1}}, Current, Inner;
	for( FStack* StackTop=RecursionStack; StackTop>=RecursionStack; --StackTop ) //-V625
	{
		Current = *StackTop;
	Loop:
		PTRINT Count = Current.Max - Current.Min + 1;
		if( Count <= 8 )
		{
			// Use simple bubble-sort.
			while( Current.Max > Current.Min )
			{
				T *Max, *Item;
				for( Max=Current.Min, Item=Current.Min+1; Item<=Current.Max; Item++ )
				{
					if( Predicate( *Max, *Item ) )
					{
						Max = Item;
					}
				}
				Exchange( *Max, *Current.Max-- );
			}
		}
		else
		{
			// Grab middle element so sort doesn't exhibit worst-cast behavior with presorted lists.
			Exchange( Current.Min[Count/2], Current.Min[0] );

			// Divide list into two halves, one with items <=Current.Min, the other with items >Current.Max.
			Inner.Min = Current.Min;
			Inner.Max = Current.Max+1;
			for( ; ; )
			{
				while( ++Inner.Min<=Current.Max && !Predicate( *Current.Min, *Inner.Min ) );
				while( --Inner.Max> Current.Min && !Predicate( *Inner.Max, *Current.Min ) );
				if( Inner.Min>Inner.Max )
				{
					break;
				}
				Exchange( *Inner.Min, *Inner.Max );
			}
			Exchange( *Current.Min, *Inner.Max );

			// Save big half and recurse with small half.
			if( Inner.Max-1-Current.Min >= Current.Max-Inner.Min )
			{
				if( Current.Min+1 < Inner.Max )
				{
					StackTop->Min = Current.Min;
					StackTop->Max = Inner.Max - 1;
					StackTop++;
				}
				if( Current.Max>Inner.Min )
				{
					Current.Min = Inner.Min;
					goto Loop;
				}
			}
			else
			{
				if( Current.Max>Inner.Min )
				{
					StackTop->Min = Inner  .Min;
					StackTop->Max = Current.Max;
					StackTop++;
				}
				if( Current.Min+1<Inner.Max )
				{
					Current.Max = Inner.Max - 1;
					goto Loop;
				}
			}
		}
	}
}

/**
 * Sort elements using user defined predicate class. The sort is unstable, meaning that the ordering of equal items is not necessarily preserved.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS> 
void Sort( T* First, const int32 Num, const PREDICATE_CLASS& Predicate )
{
	SortInternal( First, Num, TDereferenceWrapper<T, PREDICATE_CLASS>( Predicate ) );
}

/**
 * Specialized version of the above Sort function for pointers to elements.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS> 
void Sort( T** First, const int32 Num, const PREDICATE_CLASS& Predicate )
{
	SortInternal( First, Num, TDereferenceWrapper<T*, PREDICATE_CLASS>( Predicate ) );
}

/**
 * Sort elements. The sort is unstable, meaning that the ordering of equal items is not necessarily preserved.
 * Assumes < operator is defined for the template type.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T> 
void Sort( T* First, const int32 Num )
{
	SortInternal( First, Num, TDereferenceWrapper<T, TLess<T> >( TLess<T>() ) );
}

/**
 * Specialized version of the above Sort function for pointers to elements.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T> 
void Sort( T** First, const int32 Num )
{
	SortInternal( First, Num, TDereferenceWrapper<T*, TLess<T> >( TLess<T>() ) );
}

/**
 * Stable merge to perform sort below. Stable sort is slower than non-stable
 * algorithm.
 *
 * @param Out Pointer to the first element of output array.
 * @param In Pointer to the first element to sort.
 * @param Mid Middle point of the table, i.e. merge separator.
 * @param Num Number of elements in the whole table.
 * @param Predicate Predicate class.
 */
template<class T, class PREDICATE_CLASS>
void Merge(T* Out, T* In, const int32 Mid, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	int32 Merged = 0;
	int32 Picked;
	int32 A = 0, B = Mid;

	while (Merged < Num)
	{
		if (Merged != B && (B >= Num || !Predicate(In[B], In[A])))
		{
			Picked = A++;
		}
		else
		{
			Picked = B++;
		}

		Out[Merged] = In[Picked];

		++Merged;
	}
}

/**
 * Euclidean algorithm using modulo policy.
 */
class FEuclidDivisionGCD
{
public:
	/**
	 * Calculate GCD.
	 *
	 * @param A First parameter.
	 * @param B Second parameter.
	 *
	 * @returns Greatest common divisor of A and B.
	 */
	static int32 GCD(int32 A, int32 B)
	{
		while (B != 0)
		{
			int32 Temp = B;
			B = A % B;
			A = Temp;
		}

		return A;
	}
};

/**
 * Array rotation using juggling technique.
 *
 * @template_param TGCDPolicy Policy for calculating greatest common divisor.
 */
template <class TGCDPolicy>
class TJugglingRotation
{
public:
	/**
	 * Rotates array.
	 *
	 * @param First Pointer to the array.
	 * @param From Rotation starting point.
	 * @param To Rotation ending point.
	 * @param Amount Amount of steps to rotate.
	 */
	template <class T>
	static void Rotate(T* First, const int32 From, const int32 To, const int32 Amount)
	{
		if (Amount == 0)
		{
			return;
		}

		auto Num = To - From;
		auto GCD = TGCDPolicy::GCD(Num, Amount);
		auto CycleSize = Num / GCD;

		for (int32 Index = 0; Index < GCD; ++Index)
		{
			T BufferObject = MoveTemp(First[From + Index]);
			int32 IndexToFill = Index;

			for (int32 InCycleIndex = 0; InCycleIndex < CycleSize; ++InCycleIndex)
			{
				IndexToFill = (IndexToFill + Amount) % Num;
				Exchange(First[From + IndexToFill], BufferObject);
			}
		}
	}
};

/**
 * Merge policy for merge sort.
 *
 * @template_param TRotationPolicy Policy for array rotation algorithm.
 */
template <class TRotationPolicy>
class TRotationInPlaceMerge
{
public:
	/**
	 * Two sorted arrays merging function.
	 *
	 * @param First Pointer to array.
	 * @param Mid Middle point i.e. separation point of two arrays to merge.
	 * @param Num Number of elements in array.
	 * @param Predicate Predicate for comparison.
	 */
	template <class T, class PREDICATE_CLASS>
	static void Merge(T* First, const int32 Mid, const int32 Num, const PREDICATE_CLASS& Predicate)
	{
		int32 AStart = 0;
		int32 BStart = Mid;

		while (AStart < BStart && BStart < Num)
		{
			int32 NewAOffset = BinarySearchLast(First + AStart, BStart - AStart, First[BStart], Predicate) + 1;
			AStart += NewAOffset;

			if (AStart >= BStart) // done
				break;

			int32 NewBOffset = BinarySearchFirst(First + BStart, Num - BStart, First[AStart], Predicate);
			TRotationPolicy::Rotate(First, AStart, BStart + NewBOffset, NewBOffset);
			BStart += NewBOffset;
			AStart += NewBOffset + 1;
		}
	}

private:
	/**
	 * Performs binary search, resulting in position of the first element with given value in an array.
	 *
	 * @param First Pointer to array.
	 * @param Num Number of elements in array.
	 * @param Value Value to look for.
	 * @param Predicate Predicate for comparison.
	 *
	 * @returns Position of the first element with value Value.
	 */
	template <class T, class PREDICATE_CLASS>
	static int32 BinarySearchFirst(T* First, const int32 Num, const T& Value, const PREDICATE_CLASS& Predicate)
	{
		int32 Start = 0;
		int32 End = Num;
		
		while (End - Start > 1)
		{
			int32 Mid = (Start + End) / 2;
			bool bComparison = Predicate(First[Mid], Value);

			Start = bComparison ? Mid : Start;
			End = bComparison ? End : Mid;
		}

		return Predicate(First[Start], Value) ? Start + 1 : Start;
	}

	/**
	 * Performs binary search, resulting in position of the last element with given value in an array.
	 *
	 * @param First Pointer to array.
	 * @param Num Number of elements in array.
	 * @param Value Value to look for.
	 * @param Predicate Predicate for comparison.
	 *
	 * @returns Position of the last element with value Value.
	 */
	template <class T, class PREDICATE_CLASS>
	static int32 BinarySearchLast(T* First, const int32 Num, const T& Value, const PREDICATE_CLASS& Predicate)
	{
		int32 Start = 0;
		int32 End = Num;

		while (End - Start > 1)
		{
			int32 Mid = (Start + End) / 2;
			bool bComparison = !Predicate(Value, First[Mid]);
			
			Start = bComparison ? Mid : Start;
			End = bComparison ? End : Mid;
		}

		return Predicate(Value, First[Start]) ? Start - 1 : Start;
	}
};

/**
 * Merge sort class.
 *
 * @template_param TMergePolicy Merging policy.
 * @template_param MinMergeSubgroupSize Minimal size of the subgroup that should be merged.
 */
template <class TMergePolicy, int32 MinMergeSubgroupSize = 2>
class TMergeSort
{
public:
	/**
	 * Sort the array.
	 *
	 * @param First Pointer to the array.
	 * @param Num Number of elements in the array.
	 * @param Predicate Predicate for comparison.
	 */
	template<class T, class PREDICATE_CLASS>
	static void Sort(T* First, const int32 Num, const PREDICATE_CLASS& Predicate)
	{
		int32 SubgroupStart = 0;

		if (MinMergeSubgroupSize > 1)
		{
			if (MinMergeSubgroupSize > 2)
			{
				// First pass with simple bubble-sort.
				do
				{
					int32 GroupEnd = FPlatformMath::Min(SubgroupStart + MinMergeSubgroupSize, Num);
					do
					{
						for (int32 It = SubgroupStart; It < GroupEnd - 1; ++It)
						{
							if (Predicate(First[It + 1], First[It]))
							{
								Exchange(First[It], First[It + 1]);
							}
						}
						GroupEnd--;
					} while (GroupEnd - SubgroupStart > 1);

					SubgroupStart += MinMergeSubgroupSize;
				} while (SubgroupStart < Num);
			}
			else
			{
				for (int32 Subgroup = 0; Subgroup < Num; Subgroup += 2)
				{
					if (Subgroup + 1 < Num && Predicate(First[Subgroup + 1], First[Subgroup]))
					{
						Exchange(First[Subgroup], First[Subgroup + 1]);
					}
				}
			}
		}

		int32 SubgroupSize = MinMergeSubgroupSize;
		while (SubgroupSize < Num)
		{
			SubgroupStart = 0;
			do
			{
				TMergePolicy::Merge(
					First + SubgroupStart,
					SubgroupSize,
					FPlatformMath::Min(SubgroupSize << 1, Num - SubgroupStart),
					Predicate);
				SubgroupStart += SubgroupSize << 1;
			} while (SubgroupStart < Num);

			SubgroupSize <<= 1;
		}
	}
};

/**
 * Stable sort elements using user defined predicate class. The sort is stable,
 * meaning that the ordering of equal items is preserved, but it's slower than
 * non-stable algorithm.
 *
 * This is the internal sorting function used by StableSort overrides.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS>
void StableSortInternal(T* First, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	TMergeSort<TRotationInPlaceMerge<TJugglingRotation<FEuclidDivisionGCD> > >::Sort(First, Num, Predicate);
}

/**
 * Stable sort elements using user defined predicate class. The sort is stable,
 * meaning that the ordering of equal items is preserved, but it's slower than
 * non-stable algorithm.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS>
void StableSort(T* First, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T, PREDICATE_CLASS>(Predicate));
}

/**
 * Specialized version of the above StableSort function for pointers to elements.
 * Stable sort is slower than non-stable algorithm.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS>
void StableSort(T** First, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T*, PREDICATE_CLASS>(Predicate));
}

/**
 * Stable sort elements. The sort is stable, meaning that the ordering of equal
 * items is preserved, but it's slower than non-stable algorithm.
 *
 * Assumes < operator is defined for the template type.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T>
void StableSort(T* First, const int32 Num)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T, TLess<T> >(TLess<T>()));
}

/**
 * Specialized version of the above StableSort function for pointers to elements.
 * Stable sort is slower than non-stable algorithm.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T>
void StableSort(T** First, const int32 Num)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T*, TLess<T> >(TLess<T>()));
}

