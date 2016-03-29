// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** data for updating cloth section from the results of clothing simulation */
struct FClothSimulData
{
	TArray<FVector4> ClothSimulPositions;
	TArray<FVector4> ClothSimulNormals;
};
