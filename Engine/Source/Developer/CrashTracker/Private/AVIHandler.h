// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Given a series of JPEG compressed images, this will wrap them with an AVI header
 * and write them out as a Motion JPEG AVI video.
 *
 * @param CompressedFrames	An ordered array of JPEG images
 * @param OutputPath		The path with which to dump out the video
 * @param Width				The width of the images
 * @param Height			The height of the images
 * @param FPS				The frame rate of the video
 */
void DumpOutMJPEGAVI(const TArray<class FCompressedDataFrame*>& CompressedFrames, FString OutputPath, int32 Width, int32 Height, int32 FPS);
