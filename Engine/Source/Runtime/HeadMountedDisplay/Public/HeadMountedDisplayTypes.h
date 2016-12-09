// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FFilterVertex;

/**
* The family of HMD device.  Register a new class of device here if you need to branch code for PostProcessing until
*/
namespace EHMDDeviceType
{
	enum Type
	{
		DT_OculusRift,
		DT_Morpheus,
		DT_ES2GenericStereoMesh,
		DT_SteamVR,
		DT_GearVR,
		DT_GoogleVR
	};
}

class HEADMOUNTEDDISPLAY_API FHMDViewMesh
{
public:

	enum EHMDMeshType
	{
		MT_HiddenArea,
		MT_VisibleArea
	};

	FHMDViewMesh();
	~FHMDViewMesh();

	bool IsValid() const
	{
		return NumTriangles > 0;
	}

	void BuildMesh(const FVector2D Positions[], uint32 VertexCount, EHMDMeshType MeshType);

	FFilterVertex* pVertices;
	uint16*   pIndices;
	unsigned  NumVertices;
	unsigned  NumIndices;
	unsigned  NumTriangles;
};
