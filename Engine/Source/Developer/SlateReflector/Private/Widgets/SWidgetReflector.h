// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWidgetReflector.h"

/**
 * Widget reflector implementation.
 * User widget to enable iteration without recompilation.
 */
class SLATEREFLECTOR_API SWidgetReflector
	: public SUserWidget
	, public IWidgetReflector
{
public:

	SLATE_USER_ARGS(SWidgetReflector) 
	{ }
	SLATE_END_ARGS()

	virtual void Construct( const FArguments& InArgs ) = 0;
};

