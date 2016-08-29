// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace AlgoImpl
{
	template <typename RangeType, typename PredicateType>
	typename const RangeType::ElementType* FindByPredicate(const RangeType& Range, PredicateType Predicate)
	{
		for (const auto& Elem : Range)
		{
			if (Predicate(Elem))
			{
				return &Elem;
			}
		}

		return nullptr;
	}
}

namespace Algo
{
	/**
	 * Returns a pointer to the first element matching a value in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Value  The value to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename ValueType>
	FORCEINLINE typename RangeType::ElementType* Find(RangeType& Range, const ValueType& Value)
	{
		return const_cast<typename RangeType::ElementType*>(Find(const_cast<const RangeType&>(Range), Value));
	}

	/**
	 * Returns a pointer to the first element matching a value in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Value  The value to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename ValueType>
	FORCEINLINE const typename RangeType::ElementType* Find(const RangeType& Range, const ValueType& Value)
	{
		return AlgoImpl::FindByPredicate(Range, [&](const typename RangeType::ElementType& Elem){
			return Elem == Value;
		});
	}

	/**
	 * Returns a pointer to the first element matching a predicate in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Pred   The predicate to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename PredicateType>
	FORCEINLINE typename RangeType::ElementType* FindByPredicate(RangeType& Range, PredicateType Pred)
	{
		return const_cast<typename RangeType::ElementType*>(FindByPredicate(const_cast<const RangeType&>(Range), Pred));
	}

	/**
	 * Returns a pointer to the first element matching a predicate in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Pred   The predicate to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename PredicateType>
	FORCEINLINE const typename RangeType::ElementType* FindByPredicate(const RangeType& Range, PredicateType Pred)
	{
		return AlgoImpl::FindByPredicate(Range, Pred);
	}
}
