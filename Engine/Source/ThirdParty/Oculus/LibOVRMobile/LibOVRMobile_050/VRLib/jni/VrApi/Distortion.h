/************************************************************************************

Filename    :   Distortion.h
Content     :   Distortion file
Created     :   September 12, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_Distortion_h
#define OVR_Distortion_h

#include "Kernel/OVR_MemBuffer.h"
#include "HmdInfo.h"

namespace OVR
{

/*
 The entire screen is covered with a mesh of verts,
 doubled up at the centerline.  This allows us to, in theory,
 build personalized meshes that account for asymmetric eye offsets.

 For the original 32x32 block eye meshes,
 the mesh dimensions would be 66x33.

 For the 64x72 block mesh optimized for camera processing (20x20 pixels per block)
 the mesh dimensions would be 130x73.

 For sliced rendering, the number of blocks wide should be a multiple of 4.

 The buffer will contain:

 int	magic
 int	eyeBlocksWide
 int	eyeBlocksHigh

 // xy vectors for red, green, and blue channels
 // Vertex order goes from -1, -1 (lower left) to 1, -1 (lower right)
 // then up by rows to the top of the screen.
 float	tanAngles[meshWidth*meshHeight][6]

 */


static const int DISTORTION_BUFFER_MAGIC = 0x56347805;

MemBuffer BuildDistortionBuffer( const hmdInfoInternal_t & hmdInfo,
		int eyeBlocksWide, int eyeBlocksHigh );

}	// namespace OVR

#endif	// OVR_Distortion_h
