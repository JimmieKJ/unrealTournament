// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IntegerSequence.h"
#include "Templates/AndOr.h"

template <int32 N, typename... Types>
struct TNthTypeFromParameterPack;

template <int32 N, typename T, typename... OtherTypes>
struct TNthTypeFromParameterPack<N, T, OtherTypes...>
{
	typedef typename TNthTypeFromParameterPack<N - 1, OtherTypes...>::Type Type;
};

template <typename T, typename... OtherTypes>
struct TNthTypeFromParameterPack<0, T, OtherTypes...>
{
	typedef T Type;
};

template <typename T, uint32 Index>
struct TTupleElement
{
	template <typename... ArgTypes>
	explicit TTupleElement(ArgTypes&&... Args)
		: Value(Forward<ArgTypes>(Args)...)
	{
	}

	T Value;
};

template <uint32 IterIndex, uint32 Index, typename... Types>
struct TTupleElementHelperImpl;

template <uint32 IterIndex, uint32 Index, typename ElementType, typename... Types>
struct TTupleElementHelperImpl<IterIndex, Index, ElementType, Types...> : TTupleElementHelperImpl<IterIndex + 1, Index, Types...>
{
};

template <uint32 Index, typename ElementType, typename... Types>
struct TTupleElementHelperImpl<Index, Index, ElementType, Types...>
{
	typedef ElementType Type;

	template <typename TupleType>
	static FORCEINLINE ElementType& Get(TupleType& Tuple)
	{
		return static_cast<TTupleElement<ElementType, Index>&>(Tuple).Value;
	}

	template <typename TupleType>
	static FORCEINLINE const ElementType& Get(const TupleType& Tuple)
	{
		return Get((TupleType&)Tuple);
	}
};

template <uint32 WantedIndex, typename... Types>
struct TTupleElementHelper : TTupleElementHelperImpl<0, WantedIndex, Types...>
{
};

template <typename... Types>
struct TTuple;

template <typename Indices, typename... Types>
struct TTupleImpl;

template <uint32... Indices, typename... Types>
struct TTupleImpl<TIntegerSequence<uint32, Indices...>, Types...> : TTupleElement<Types, Indices>...
{
	template <typename... ArgTypes>
	explicit TTupleImpl(ArgTypes&&... Args)
		: TTupleElement<Types, Indices>(Forward<ArgTypes>(Args))...
	{
	}

	template <uint32 Index> FORCEINLINE const typename TTupleElementHelper<Index, Types...>::Type& Get() const { return TTupleElementHelper<Index, Types...>::Get(*this); }
	template <uint32 Index> FORCEINLINE       typename TTupleElementHelper<Index, Types...>::Type& Get()       { return TTupleElementHelper<Index, Types...>::Get(*this); }

	template <typename FuncType, typename... ArgTypes>
	auto ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Forward<ArgTypes>(Args)..., Get<Indices>()...))
	{
		return Func(Forward<ArgTypes>(Args)..., Get<Indices>()...);
	}

	template <typename FuncType, typename... ArgTypes>
	auto ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Get<Indices>()..., Forward<ArgTypes>(Args)...))
	{
		return Func(Get<Indices>()..., Forward<ArgTypes>(Args)...);
	}
};

#ifdef _MSC_VER

	// Not strictly necessary, but some VC versions give a 'syntax error: <fake-expression>' error
	// for empty tuples.
	template <>
	struct TTupleImpl<TIntegerSequence<uint32>>
	{
		explicit TTupleImpl()
		{
		}

		// Doesn't matter what these return, or even have a function body, but they need to be declared
		template <uint32 Index> FORCEINLINE const int32& Get() const;
		template <uint32 Index> FORCEINLINE       int32& Get();

		template <typename FuncType, typename... ArgTypes>
		auto ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Forward<ArgTypes>(Args)...))
		{
			return Func(Forward<ArgTypes>(Args)...);
		}

		template <typename FuncType, typename... ArgTypes>
		auto ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Forward<ArgTypes>(Args)...))
		{
			return Func(Forward<ArgTypes>(Args)...);
		}
	};

#endif

template <typename T, typename... Types>
struct TDecayedFrontOfParameterPackIsSameType
{
	enum { Value = TAreTypesEqual<T, typename TDecay<typename TNthTypeFromParameterPack<0, Types...>::Type>::Type>::Value };
};

template <typename... Types>
struct TTuple : TTupleImpl<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...>
{
	template <typename... ArgTypes, typename = typename TEnableIf<!TAnd<TBoolConstant<sizeof...(ArgTypes) == 1>, TDecayedFrontOfParameterPackIsSameType<TTuple, ArgTypes...>>::Value>::Type>
	explicit TTuple(ArgTypes&&... Args)
		: TTupleImpl<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...>(Forward<ArgTypes>(Args)...)
	{
		// This constructor is disabled for TTuple because VC is incorrectly instantiating it as a move/copy constructor.
	}
};


/**
 * Makes a TTuple from some arguments.  The type of the TTuple elements are the decayed versions of the arguments.
 *
 * @param  Args  The arguments used to construct the tuple.
 * @return A tuple containing a copy of the arguments.
 *
 * Example:
 *
 * void Func(const int32 A, FString&& B)
 * {
 *     // Equivalent to:
 *     // TTuple<int32, const TCHAR*, FString> MyTuple(A, TEXT("Hello"), MoveTemp(B));
 *     auto MyTuple = MakeTuple(A, TEXT("Hello"), MoveTemp(B));
 * }
 */
template <typename... Types>
TTuple<typename TDecay<Types>::Type...> MakeTuple(Types&&... Args)
{
	return TTuple<typename TDecay<Types>::Type...>(Forward<Types>(Args)...);
}



template <typename IntegerSequence>
struct TTransformTuple_Impl;

template <uint32... Indices>
struct TTransformTuple_Impl<TIntegerSequence<uint32, Indices...>>
{
	template <typename TupleType, typename FuncType>
	static auto Do(TupleType&& Tuple, FuncType Func) -> decltype(MakeTuple(Func(Forward<TupleType>(Tuple).template Get<Indices>())...))
	{
		return MakeTuple(Func(Forward<TupleType>(Tuple).template Get<Indices>())...);
	}
};


/**
 * Creates a new TTuple by applying a functor to each of the elements.
 *
 * @param  Tuple  The tuple to apply the functor to.
 * @param  Func   The functor to apply.
 *
 * @return A new tuple of the transformed elements.
 *
 * Example:
 *
 * float        Overloaded(int32 Arg);
 * char         Overloaded(const TCHAR* Arg);
 * const TCHAR* Overloaded(const FString& Arg);
 *
 * void Func(const TTuple<int32, const TCHAR*, FString>& MyTuple)
 * {
 *     // Equivalent to:
 *     // TTuple<float, char, const TCHAR*> TransformedTuple(Overloaded(MyTuple.Get<0>()), Overloaded(MyTuple.Get<1>()), Overloaded(MyTuple.Get<2>())));
 *     auto TransformedTuple = TransformTuple(MyTuple, [](const auto& Arg) { return Overloaded(Arg); });
 * }
 */
template <typename FuncType, typename... Types>
auto TransformTuple(TTuple<Types...>&& Tuple, FuncType Func) -> decltype(TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(MoveTemp(Tuple), MoveTemp(Func)))
{
	return TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(MoveTemp(Tuple), MoveTemp(Func));
}

template <typename FuncType, typename... Types>
auto TransformTuple(const TTuple<Types...>& Tuple, FuncType Func) -> decltype(TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(Tuple, MoveTemp(Func)))
{
	return TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(Tuple, MoveTemp(Func));
}
