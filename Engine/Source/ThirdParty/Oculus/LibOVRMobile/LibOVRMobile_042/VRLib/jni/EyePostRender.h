/************************************************************************************

Filename    :   EyePostRender.h
Content     :   Render on top of an eye render, portable between native and Unity.
Created     :   May 23, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_EyePostRender_h
#define OVR_EyePostRender_h

#include "GlGeometry.h"
#include "GlProgram.h"

namespace OVR
{

// This class is for rendering things on top of a complete eye buffer that can
// be common between native code and Unity.

class EyePostRender
{
public:
	void		Init();
	void		Shutdown();

	void		DrawEyeCalibrationLines( const float bufferFovDegrees, const int eye );
	void 		DrawEyeVignette();

	// Draw a single pixel ( 0 0 0 1 ) border around the framebuffer using clear rects
	void		FillEdge( int fbWidth, int fbHeight );

	GlProgram	UntexturedMvpProgram;
	GlProgram	UntexturedScreenSpaceProgram;

	GlGeometry	CalibrationLines;
	GlGeometry	VignetteSquare;
};

}	// namespace OVR

#endif	// OVR_EyePostRender_h
