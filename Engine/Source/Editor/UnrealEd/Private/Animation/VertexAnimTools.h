// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

/** Vertex animation tools */
class FVertexAnimTools
{
public:
	/** Create a new vertex animation from the supplied fbx information */
	static void ImportVertexAnimtion(UnFbx::FFbxImporter* FFbxImporter, USkeletalMesh * SkelMesh, UPackage * Package, FString& Filename);

};
