/************************************************************************************

Filename    :   Distortion.cpp
Content     :   Distortion file
Created     :   September 12, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "Distortion.h"


namespace OVR
{

/*
 * The distortion calculation
 *
 		hmdInfo->ResolutionInPixels.Width = 1920;
		hmdInfo->ResolutionInPixels.Height = 1080;
		hmdInfo->ScreenSizeInMeters.Width = 0.11047f;
		hmdInfo->ScreenSizeInMeters.Height = 0.06214f;

		distortions[0].Config.LensMetersPerTanDegreeAtCenter = Sizef(0.043875f);
		distortions[0].Config.Eqn = Distortion_RecipPoly4;
		distortions[0].Config.K[0] = 1.0f;
		distortions[0].Config.K[1] = -0.3999f;
		distortions[0].Config.K[2] =  0.2408f;
		distortions[0].Config.K[3] = -0.4589f;
		distortions[0].Config.ChromaticAberration[0] = -0.006f;
		distortions[0].Config.ChromaticAberration[2] =  0.014f;
		distortions[0].Config.ChromaticAberration[3] =  0.0f;

		case Distortion_RecipPoly4:
            scale = 1.0f / ( K[0] + rsq * ( K[1] + rsq * ( K[2] + rsq * K[3] ) ) );

		The GS5 lens IPD is 59 mm, or 1025 pixels round down to 1024

				   ^
				   5
				   4
				   0
				   v
		<---448---> <---512---><---512---> <---448--->
				   ^
				   5
				   4
				   0
				   v

		The farthest side is 540 pixels, or 0.0311 meters from the center
		The farthest corner is 540,512 = 744 pixels, or 0.0428 meters from the center
		The farthest corner on a symetric viewport is 540,540 = 764 pixels, or 0.0440 meters from the center

		TanAngleForPixelDistance( 0.0440 ) = 2.677309 = 70 degrees half angle along diagonal

		For a pixel location, calculate the meters to the center point
		Scale that by LensMetersPerTanDegreeAtCenter then apply distortion
		to get an actual tanAngle.
		Scale the tanAngle to get the FBO texture coordinate.

 *
 */

#if 0	// unused
// Takes 0 to 1 range in a square viewport covering 1080 x 1080 pixels
// that are partially scissored off (asymetrically) on the sides to 960x1080
// Returns a -1 tp 1 mapping.
static void	WarpTexCoord2( const hmdInfoInternal_t & hmdInfo, const float in[2], float out[2] ) {
	float	theta[2];
	for ( int i = 0; i < 2; i++ ) {
		const float unit = in[i];
		const float ndc = 2.0f * ( unit - 0.5f );
		const float pixels = ndc * hmdInfo.heightPixels * 0.5f;
		const float meters = pixels * hmdInfo.widthMeters / hmdInfo.widthPixels;
		const float tanAngle = meters / hmdInfo.lens.MetersPerTanAngleAtCenter;
		theta[i] = tanAngle;
	}

	const float rsq = theta[0] * theta[0] + theta[1] * theta[1];
	const float scale = hmdInfo.lens.DistortionFnScaleRadiusSquared(rsq);

	for ( int i = 0; i < 2; i++ ) {
		const float tanAngle = scale * theta[i];
		const float textureNdc = tanAngle;
		out[i] = textureNdc;
	}
}
#endif

static void WarpTexCoordChroma( const hmdInfoInternal_t & hmdInfo, const float in[2],
			float red[2], float green[2], float blue[2] ) {
	float theta[2];
	for ( int i = 0; i < 2; i++ ) {
		const float unit = in[i];
		const float ndc = 2.0f * ( unit - 0.5f );
		const float pixels = ndc * hmdInfo.heightPixels * 0.5f;
		const float meters = pixels * hmdInfo.widthMeters / hmdInfo.widthPixels;
		const float tanAngle = meters / hmdInfo.lens.MetersPerTanAngleAtCenter;
		theta[i] = tanAngle;
	}

	const float rsq = theta[0] * theta[0] + theta[1] * theta[1];

	const Vector3f chromaScale = hmdInfo.lens.DistortionFnScaleRadiusSquaredChroma (rsq);

	for ( int i = 0; i < 2; i++ ) {
		red[i] = chromaScale[0] * theta[i];
		green[i] = chromaScale[1] * theta[i];
		blue[i] = chromaScale[2] * theta[i];
	}
}


MemBuffer BuildDistortionBuffer( const hmdInfoInternal_t & hmdInfo,
		int eyeBlocksWide, int eyeBlocksHigh )
{
	const int vertexCount = 2 * ( eyeBlocksWide + 1 ) * ( eyeBlocksHigh + 1 );
	MemBuffer	buf( 12 + 4 * vertexCount * 6 );
	((int *)buf.Buffer)[0] = DISTORTION_BUFFER_MAGIC;
	((int *)buf.Buffer)[1] = eyeBlocksWide;
	((int *)buf.Buffer)[2] = eyeBlocksHigh;

	// the centers are offset horizontal in each eye
	const float aspect = hmdInfo.widthPixels * 0.5 / hmdInfo.heightPixels;

	const float	horizontalShiftLeftMeters =  -(( hmdInfo.lensSeparation / 2 ) - ( hmdInfo.widthMeters / 4 )) + hmdInfo.horizontalOffsetMeters;
    const float horizontalShiftRightMeters = (( hmdInfo.lensSeparation / 2 ) - ( hmdInfo.widthMeters / 4 )) + hmdInfo.horizontalOffsetMeters;
	const float	horizontalShiftViewLeft = 2 * aspect * horizontalShiftLeftMeters / hmdInfo.widthMeters;
    const float	horizontalShiftViewRight = 2 * aspect * horizontalShiftRightMeters / hmdInfo.widthMeters;

	for ( int eye = 0; eye < 2; eye++ )
	{
		for ( int y = 0; y <= eyeBlocksHigh; y++ )
		{
			const float	yf = (float)y / (float)eyeBlocksHigh;
			for ( int x = 0; x <= eyeBlocksWide; x++ )
			{
				int	vertNum = y * ( eyeBlocksWide+1 ) * 2 +
						eye * (eyeBlocksWide+1) + x;
				const float	xf = (float)x / (float)eyeBlocksWide;
				float * v = &((float *)buf.Buffer)[3+vertNum*6];
				const float inTex[2] = { ( eye ? horizontalShiftViewLeft : horizontalShiftViewRight ) +
						xf *aspect + (1.0f-aspect) * 0.5f, yf };
				WarpTexCoordChroma( hmdInfo, inTex, &v[0], &v[2], &v[4] );
			}
		}
	}
	return buf;
}

}
