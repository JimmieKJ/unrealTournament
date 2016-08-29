// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * A class that controls the process of generating manifests and chunk data from a build image.
 */
class FBuildDataGenerator
{
public:
	/**
	 * Processes a Build Image to determine new chunks and produce a chunk based manifest, all saved to the cloud.
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param Settings      Specifies the settings for the operation.  See FBuildPatchSettings documentation.
	 * @return True if no errors occurred.
	 */
	static bool GenerateChunksManifestFromDirectory(const FBuildPatchSettings& Settings);
};
