// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

	/**
* Include the current implementation of a FTransform, depending on the vector processing mode
	 */

#include "ScalarRegister.h"

#if ENABLE_VECTORIZED_TRANSFORM
#include "TransformVectorized.h"
#else
#include "TransformNonVectorized.h"
#endif
