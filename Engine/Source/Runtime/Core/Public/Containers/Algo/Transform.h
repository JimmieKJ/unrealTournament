// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Algo
{
	/**
	 * Conditionally applies a transform to a range and stores the results into a container
	 *
	 * @param  Input  Any iterable type
	 * @param  Output  Container to hold the output
	 * @param  Predicate  Condition which returns true for elements that should be transformed and false for elements that should be skipped
	 * @param  Transform  Transformation operation
	 */
	template <typename InT, typename OutT, typename PredicateT, typename TransformT>
	FORCEINLINE void TransformIf(const InT& Input, OutT& Output, PredicateT Predicate, TransformT Transform)
	{
		for (auto&& Value : Input)
		{
			if (Predicate(Value))
			{
				Output.Add(Transform(Value));
			}
		}
	}

	/**
	 * Applies a transform to a range and stores the results into a container
	 *
	 * @param  Input  Any iterable type
	 * @param  Output  Container to hold the output
	 * @param  Transform  Transformation operation
	 */
	template <typename InT, typename OutT, typename TransformT>
	FORCEINLINE void Transform(const InT& Input, OutT& Output, TransformT Transform)
	{
		for (auto&& Value : Input)
		{
			Output.Add(Transform(Value));
		}
	}
}
