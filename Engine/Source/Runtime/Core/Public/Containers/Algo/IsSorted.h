// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace AlgoImpl
{
	template <typename T, typename PredType>
	bool IsSorted(const T* Range, int32 RangeSize, PredType Pred)
	{
		if (RangeSize == 0)
		{
			return true;
		}

		// When comparing N elements, we do N-1 comparisons
		--RangeSize;

		const T* Next = Range + 1;
		for (;;)
		{
			if (RangeSize == 0)
			{
				return true;
			}

			if (Pred(*Next, *Range))
			{
				return false;
			}

			++Range;
			++Next;
			--RangeSize;
		}
	}

	template <typename T>
	struct TLess
	{
		FORCEINLINE bool operator()(const T& Lhs, const T& Rhs) const
		{
			return Lhs < Rhs;
		}
	};
}

namespace Algo
{
	/**
	 * Tests is a range is sorted.
	 *
	 * @param  Array  The array to test for being sorted.
	 *
	 * @return true if the range is sorted, false otherwise.
	 */
	template <typename T, int32 ArraySize>
	FORCEINLINE bool IsSorted(const T (&Array)[ArraySize])
	{
		return AlgoImpl::IsSorted((const T*)Array, ArraySize, AlgoImpl::TLess<T>());
	}

	/**
	 * Tests is a range is sorted.
	 *
	 * @param  Array  The array to test for being sorted.
	 * @param  Pred   A binary sorting predicate which describes the ordering of the elements in the array.
	 *
	 * @return true if the range is sorted, false otherwise.
	 */
	template <typename T, int32 ArraySize, typename PredType>
	FORCEINLINE bool IsSorted(const T (&Array)[ArraySize], PredType Pred)
	{
		return AlgoImpl::IsSorted((const T*)Array, ArraySize, Pred);
	}

	/**
	 * Tests is a range is sorted.
	 *
	 * @param  Array      A pointer to the array to test for being sorted.
	 * @param  ArraySize  The number of elements in the array.
	 *
	 * @return true if the range is sorted, false otherwise.
	 */
	template <typename T>
	FORCEINLINE bool IsSorted(const T* Array, int32 ArraySize)
	{
		return AlgoImpl::IsSorted(Array, ArraySize, AlgoImpl::TLess<T>());
	}

	/**
	 * Tests is a range is sorted.
	 *
	 * @param  Array      A pointer to the array to test for being sorted.
	 * @param  ArraySize  The number of elements in the array.
	 * @param  Pred       A binary sorting predicate which describes the ordering of the elements in the array.
	 *
	 * @return true if the range is sorted, false otherwise.
	 */
	template <typename T, typename PredType>
	FORCEINLINE bool IsSorted(const T* Array, int32 ArraySize, PredType Pred)
	{
		return AlgoImpl::IsSorted(Array, ArraySize, Pred);
	}

	/**
	 * Tests is a range is sorted.
	 *
	 * @param  Container  The container to test for being sorted.
	 *
	 * @return true if the range is sorted, false otherwise.
	 */
	template <typename ContainerType>
	FORCEINLINE bool IsSorted(const ContainerType& Container)
	{
		return AlgoImpl::IsSorted(Container.GetData(), Container.Num(), AlgoImpl::TLess<typename ContainerType::ElementType>());
	}

	/**
	 * Tests is a range is sorted.
	 *
	 * @param  Container  The container to test for being sorted.
	 * @param  Pred       A binary sorting predicate which describes the ordering of the elements in the array.
	 *
	 * @return true if the range is sorted, false otherwise.
	 */
	template <typename ContainerType, typename PredType>
	FORCEINLINE bool IsSorted(const ContainerType& Container, PredType Pred)
	{
		return AlgoImpl::IsSorted(Container.GetData(), Container.Num(), Pred);
	}
}
