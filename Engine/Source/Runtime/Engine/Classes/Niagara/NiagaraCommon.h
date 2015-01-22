// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.generated.h"

UENUM()
enum class ENiagaraDataType : uint8
{
	Scalar,
	Vector,
	Matrix,
	Unknown,
	Invalid,
};
