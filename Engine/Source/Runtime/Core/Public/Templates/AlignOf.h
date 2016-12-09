// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreTypes.h"
#include "Templates/ChooseClass.h"
#include "Templates/IntegralConstant.h"
#include "Templates/IsClass.h"


/** Used to determine the alignment of an element type. */
template<typename ElementType, bool IsClass = TIsClass<ElementType>::Value>
class TElementAlignmentCalculator
{
	/**
	 * We use a dummy FAlignedElement type that's used to calculate the padding added between the byte and the element
	 * to fulfill the type's required alignment.
	 *
	 * Its default constructor and destructor are declared but never implemented to avoid the need for a ElementType default constructor.
	 */

private:
	/**
	 * In the case of class ElementTypes, we inherit it to allow abstract types to work.
	 */
	struct FAlignedElements : ElementType
	{
		uint8 MisalignmentPadding;

		FAlignedElements();
		~FAlignedElements();
	};

	// We calculate the alignment here and then handle the zero case in the result by forwarding it to the non-class variant.
	// This is necessary because the compiler can perform empty-base-optimization to eliminate a redundant ElementType state.
	// Forwarding it to the non-class implementation should always work because an abstract type should never be empty.
	enum { CalculatedAlignment = sizeof(FAlignedElements) - sizeof(ElementType) };

public:
	enum { Value = TChooseClass<CalculatedAlignment != 0, TIntegralConstant<SIZE_T, CalculatedAlignment>, TElementAlignmentCalculator<ElementType, false>>::Result::Value };
};

template<typename ElementType>
class TElementAlignmentCalculator<ElementType, false>
{
private:
	/**
	 * In the case of non-class ElementTypes, we contain it because non-class types cannot be inherited.
	 */
	struct FAlignedElements
	{
		uint8 MisalignmentPadding;
		ElementType Element;

		FAlignedElements();
		~FAlignedElements();
	};
public:
	enum { Value = sizeof(FAlignedElements) - sizeof(ElementType) };
};

#define ALIGNOF(T) (TElementAlignmentCalculator<T>::Value)
