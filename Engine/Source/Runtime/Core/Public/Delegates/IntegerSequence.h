// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

template <uint32... Indices>
struct TIntegerSequence;

// Doxygen can't parse recursive template definitions; just skip it.
#if !UE_BUILD_DOCS

template <uint32 ToAdd, uint32... Values>
struct TMakeIntegerSequenceImpl : TMakeIntegerSequenceImpl<ToAdd - 1, Values..., sizeof...(Values)>
{
};

template <uint32... Values>
struct TMakeIntegerSequenceImpl<0, Values...>
{
	typedef TIntegerSequence<Values...> Type;
};

#endif

template <uint32 ToAdd>
using TMakeIntegerSequence = typename TMakeIntegerSequenceImpl<ToAdd>::Type;
