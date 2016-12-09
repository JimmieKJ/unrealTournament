// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusShaders.h"


IMPLEMENT_SHADER_TYPE(, FOculusVertexShader, TEXT("OculusShaders"), TEXT("MainVertexShader"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FOculusWhiteShader, TEXT("OculusShaders"), TEXT("MainWhiteShader"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOculusBlackShader, TEXT("OculusShaders"), TEXT("MainBlackShader"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOculusAlphaInverseShader, TEXT("OculusShaders"), TEXT("MainAlphaInverseShader"), SF_Pixel);
