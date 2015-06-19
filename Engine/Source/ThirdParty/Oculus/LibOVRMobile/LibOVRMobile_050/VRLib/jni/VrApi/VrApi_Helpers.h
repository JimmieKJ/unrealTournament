/************************************************************************************

Filename    :   VrApi_Helpers.h
Content     :   API helper functions.
Created     :   March 2, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Helpers_h
#define OVR_VrApi_Helpers_h

#include "math.h"
#include "string.h"		// for memset()
#include "VrApi.h"

//-----------------------------------------------------------------
// Matrix helper functions
//-----------------------------------------------------------------

// Use left-multiplication to accumulate transformations.
inline ovrMatrix4f ovrMatrix4f_Multiply( const ovrMatrix4f * a, const ovrMatrix4f * b )
{
	ovrMatrix4f out;
	out.M[0][0] = a->M[0][0] * b->M[0][0] + a->M[0][1] * b->M[1][0] + a->M[0][2] * b->M[2][0] + a->M[0][3] * b->M[3][0];
	out.M[1][0] = a->M[1][0] * b->M[0][0] + a->M[1][1] * b->M[1][0] + a->M[1][2] * b->M[2][0] + a->M[1][3] * b->M[3][0];
	out.M[2][0] = a->M[2][0] * b->M[0][0] + a->M[2][1] * b->M[1][0] + a->M[2][2] * b->M[2][0] + a->M[2][3] * b->M[3][0];
	out.M[3][0] = a->M[3][0] * b->M[0][0] + a->M[3][1] * b->M[1][0] + a->M[3][2] * b->M[2][0] + a->M[3][3] * b->M[3][0];

	out.M[0][1] = a->M[0][0] * b->M[0][1] + a->M[0][1] * b->M[1][1] + a->M[0][2] * b->M[2][1] + a->M[0][3] * b->M[3][1];
	out.M[1][1] = a->M[1][0] * b->M[0][1] + a->M[1][1] * b->M[1][1] + a->M[1][2] * b->M[2][1] + a->M[1][3] * b->M[3][1];
	out.M[2][1] = a->M[2][0] * b->M[0][1] + a->M[2][1] * b->M[1][1] + a->M[2][2] * b->M[2][1] + a->M[2][3] * b->M[3][1];
	out.M[3][1] = a->M[3][0] * b->M[0][1] + a->M[3][1] * b->M[1][1] + a->M[3][2] * b->M[2][1] + a->M[3][3] * b->M[3][1];

	out.M[0][2] = a->M[0][0] * b->M[0][2] + a->M[0][1] * b->M[1][2] + a->M[0][2] * b->M[2][2] + a->M[0][3] * b->M[3][2];
	out.M[1][2] = a->M[1][0] * b->M[0][2] + a->M[1][1] * b->M[1][2] + a->M[1][2] * b->M[2][2] + a->M[1][3] * b->M[3][2];
	out.M[2][2] = a->M[2][0] * b->M[0][2] + a->M[2][1] * b->M[1][2] + a->M[2][2] * b->M[2][2] + a->M[2][3] * b->M[3][2];
	out.M[3][2] = a->M[3][0] * b->M[0][2] + a->M[3][1] * b->M[1][2] + a->M[3][2] * b->M[2][2] + a->M[3][3] * b->M[3][2];

	out.M[0][3] = a->M[0][0] * b->M[0][3] + a->M[0][1] * b->M[1][3] + a->M[0][2] * b->M[2][3] + a->M[0][3] * b->M[3][3];
	out.M[1][3] = a->M[1][0] * b->M[0][3] + a->M[1][1] * b->M[1][3] + a->M[1][2] * b->M[2][3] + a->M[1][3] * b->M[3][3];
	out.M[2][3] = a->M[2][0] * b->M[0][3] + a->M[2][1] * b->M[1][3] + a->M[2][2] * b->M[2][3] + a->M[2][3] * b->M[3][3];
	out.M[3][3] = a->M[3][0] * b->M[0][3] + a->M[3][1] * b->M[1][3] + a->M[3][2] * b->M[2][3] + a->M[3][3] * b->M[3][3];
	return out;
}

inline ovrMatrix4f ovrMatrix4f_Transpose( const ovrMatrix4f * a )
{
	ovrMatrix4f out;
	out.M[0][0] = a->M[0][0]; out.M[0][1] = a->M[1][0]; out.M[0][2] = a->M[2][0]; out.M[0][3] = a->M[3][0];
	out.M[1][0] = a->M[0][1]; out.M[1][1] = a->M[1][1]; out.M[1][2] = a->M[2][1]; out.M[1][3] = a->M[3][1];
	out.M[2][0] = a->M[0][2]; out.M[2][1] = a->M[1][2]; out.M[2][2] = a->M[2][2]; out.M[2][3] = a->M[3][2];
	out.M[3][0] = a->M[0][3]; out.M[3][1] = a->M[1][3]; out.M[3][2] = a->M[2][3]; out.M[3][3] = a->M[3][3];
	return out;
}

// Returns a 3x3 minor of a 4x4 matrix.
inline float ovrMatrix4f_Minor( const ovrMatrix4f * m, int r0, int r1, int r2, int c0, int c1, int c2 )
{
	return	m->M[r0][c0] * ( m->M[r1][c1] * m->M[r2][c2] - m->M[r2][c1] * m->M[r1][c2] ) -
			m->M[r0][c1] * ( m->M[r1][c0] * m->M[r2][c2] - m->M[r2][c0] * m->M[r1][c2] ) +
			m->M[r0][c2] * ( m->M[r1][c0] * m->M[r2][c1] - m->M[r2][c0] * m->M[r1][c1] );
}
 
// Returns the inverse of a 4x4 matrix.
inline ovrMatrix4f ovrMatrix4f_Inverse( const ovrMatrix4f * m )
{
	const float rcpDet = 1.0f / (	m->M[0][0] * ovrMatrix4f_Minor( m, 1, 2, 3, 1, 2, 3 ) -
									m->M[0][1] * ovrMatrix4f_Minor( m, 1, 2, 3, 0, 2, 3 ) +
									m->M[0][2] * ovrMatrix4f_Minor( m, 1, 2, 3, 0, 1, 3 ) -
									m->M[0][3] * ovrMatrix4f_Minor( m, 1, 2, 3, 0, 1, 2 ) );
	ovrMatrix4f out;
	out.M[0][0] =  ovrMatrix4f_Minor( m, 1, 2, 3, 1, 2, 3 ) * rcpDet;
	out.M[0][1] = -ovrMatrix4f_Minor( m, 0, 2, 3, 1, 2, 3 ) * rcpDet;
	out.M[0][2] =  ovrMatrix4f_Minor( m, 0, 1, 3, 1, 2, 3 ) * rcpDet;
	out.M[0][3] = -ovrMatrix4f_Minor( m, 0, 1, 2, 1, 2, 3 ) * rcpDet;
	out.M[1][0] = -ovrMatrix4f_Minor( m, 1, 2, 3, 0, 2, 3 ) * rcpDet;
	out.M[1][1] =  ovrMatrix4f_Minor( m, 0, 2, 3, 0, 2, 3 ) * rcpDet;
	out.M[1][2] = -ovrMatrix4f_Minor( m, 0, 1, 3, 0, 2, 3 ) * rcpDet;
	out.M[1][3] =  ovrMatrix4f_Minor( m, 0, 1, 2, 0, 2, 3 ) * rcpDet;
	out.M[2][0] =  ovrMatrix4f_Minor( m, 1, 2, 3, 0, 1, 3 ) * rcpDet;
	out.M[2][1] = -ovrMatrix4f_Minor( m, 0, 2, 3, 0, 1, 3 ) * rcpDet;
	out.M[2][2] =  ovrMatrix4f_Minor( m, 0, 1, 3, 0, 1, 3 ) * rcpDet;
	out.M[2][3] = -ovrMatrix4f_Minor( m, 0, 1, 2, 0, 1, 3 ) * rcpDet;
	out.M[3][0] = -ovrMatrix4f_Minor( m, 1, 2, 3, 0, 1, 2 ) * rcpDet;
	out.M[3][1] =  ovrMatrix4f_Minor( m, 0, 2, 3, 0, 1, 2 ) * rcpDet;
	out.M[3][2] = -ovrMatrix4f_Minor( m, 0, 1, 3, 0, 1, 2 ) * rcpDet;
	out.M[3][3] =  ovrMatrix4f_Minor( m, 0, 1, 2, 0, 1, 2 ) * rcpDet;
	return out;
}

inline ovrMatrix4f ovrMatrix4f_CreateIdentity()
{
	ovrMatrix4f out;
	out.M[0][0] = 1.0f; out.M[0][1] = 0.0f; out.M[0][2] = 0.0f; out.M[0][3] = 0.0f;
	out.M[1][0] = 0.0f; out.M[1][1] = 1.0f; out.M[1][2] = 0.0f; out.M[1][3] = 0.0f;
	out.M[2][0] = 0.0f; out.M[2][1] = 0.0f; out.M[2][2] = 1.0f; out.M[2][3] = 0.0f;
	out.M[3][0] = 0.0f; out.M[3][1] = 0.0f; out.M[3][2] = 0.0f; out.M[3][3] = 1.0f;
	return out;
}

inline ovrMatrix4f ovrMatrix4f_CreateTranslation( const float x, const float y, const float z )
{
	ovrMatrix4f out;
	out.M[0][0] = 1.0f; out.M[0][1] = 0.0f; out.M[0][2] = 0.0f; out.M[0][3] = x;
	out.M[1][0] = 0.0f; out.M[1][1] = 1.0f; out.M[1][2] = 0.0f; out.M[1][3] = y;
	out.M[2][0] = 0.0f; out.M[2][1] = 0.0f; out.M[2][2] = 1.0f; out.M[2][3] = z;
	out.M[3][0] = 0.0f; out.M[3][1] = 0.0f; out.M[3][2] = 0.0f; out.M[3][3] = 1.0f;
	return out;
}

inline ovrMatrix4f ovrMatrix4f_CreateRotation( const float radiansX, const float radiansY, const float radiansZ )
{
    const float sinX = sinf( radiansX );
    const float cosX = cosf( radiansX );
	const ovrMatrix4f rotationX =
	{ {
		{ 1,    0,     0, 0 },
		{ 0, cosX, -sinX, 0 },
		{ 0, sinX,  cosX, 0 },
		{ 0,    0,     0, 1 }
	} };
    const float sinY = sinf( radiansY );
    const float cosY = cosf( radiansY );
	const ovrMatrix4f rotationY =
	{ {
		{  cosY, 0, sinY, 0 },
		{     0, 1,    0, 0 },
		{ -sinY, 0, cosY, 0 },
		{     0, 0,    0, 1 }
	} };
    const float sinZ = sinf( radiansZ );
    const float cosZ = cosf( radiansZ );
	const ovrMatrix4f rotationZ =
	{ {
		{ cosZ, -sinZ, 0, 0 },
		{ sinZ,  cosZ, 0, 0 },
		{    0,     0, 1, 0 },
		{    0,     0, 0, 1 }
	} };
	const ovrMatrix4f rotationXY = ovrMatrix4f_Multiply( &rotationY, &rotationX );
	return ovrMatrix4f_Multiply( &rotationZ, &rotationXY );
}

inline ovrMatrix4f ovrMatrix4f_CreateProjection( const float minX, const float maxX,
											float const minY, const float maxY, const float nearZ, const float farZ )
{
	const float width = maxX - minX;
	const float height = maxY - minY;
	const float offsetZ = nearZ;	// set to zero for a [0,1] clip space

	ovrMatrix4f out;
	if ( farZ <= nearZ )
	{
		// place the far plane at infinity
		out.M[0][0] = 2 * nearZ / width;
		out.M[0][1] = 0;
		out.M[0][2] = ( maxX + minX ) / width;
		out.M[0][3] = 0;

		out.M[1][0] = 0;
		out.M[1][1] = 2 * nearZ / height;
		out.M[1][2] = ( maxY + minY ) / height;
		out.M[1][3] = 0;

		out.M[2][0] = 0;
		out.M[2][1] = 0;
		out.M[2][2] = -1;
		out.M[2][3] = -( nearZ + offsetZ );

		out.M[3][0] = 0;
		out.M[3][1] = 0;
		out.M[3][2] = -1;
		out.M[3][3] = 0;
	}
	else
	{
		// normal projection
		out.M[0][0] = 2 * nearZ / width;
		out.M[0][1] = 0;
		out.M[0][2] = ( maxX + minX ) / width;
		out.M[0][3] = 0;

		out.M[1][0] = 0;
		out.M[1][1] = 2 * nearZ / height;
		out.M[1][2] = ( maxY + minY ) / height;
		out.M[1][3] = 0;

		out.M[2][0] = 0;
		out.M[2][1] = 0;
		out.M[2][2] = -( farZ + offsetZ ) / ( farZ - nearZ );
		out.M[2][3] = -( farZ * ( nearZ + offsetZ ) ) / ( farZ - nearZ );

		out.M[3][0] = 0;
		out.M[3][1] = 0;
		out.M[3][2] = -1;
		out.M[3][3] = 0;
	}
	return out;
}

inline ovrMatrix4f ovrMatrix4f_CreateProjectionFov( const float fovRadiansX, const float fovRadiansY,
												const float offsetX, const float offsetY, const float nearZ, const float farZ )
{
	const float halfWidth = nearZ * tanf( fovRadiansX * 0.5f );
	const float halfHeight = nearZ * tanf( fovRadiansY * 0.5f );

	const float minX = offsetX - halfWidth;
	const float maxX = offsetX + halfWidth;

	const float minY = offsetY - halfHeight;
	const float maxY = offsetY + halfHeight;

	return ovrMatrix4f_CreateProjection( minX, maxX, minY, maxY, nearZ, farZ );
}

// Returns the 4x4 rotation matrix for the given quaternion.
inline ovrMatrix4f ovrMatrix4f_CreateFromQuaternion( const ovrQuatf * q )
{
	const float ww = q->w * q->w;
	const float xx = q->x * q->x;
	const float yy = q->y * q->y;
	const float zz = q->z * q->z;

	ovrMatrix4f out;
	out.M[0][0] = ww + xx - yy - zz;
	out.M[0][1] = 2 * ( q->x * q->y - q->w * q->z );
	out.M[0][2] = 2 * ( q->x * q->z + q->w * q->y );
	out.M[0][3] = 0;
	out.M[1][0] = 2 * ( q->x * q->y + q->w * q->z );
	out.M[1][1] = ww - xx + yy - zz;
	out.M[1][2] = 2 * ( q->y * q->z - q->w * q->x );
	out.M[1][3] = 0;
	out.M[2][0] = 2 * ( q->x * q->z - q->w * q->y );
	out.M[2][1] = 2 * ( q->y * q->z + q->w * q->x );
	out.M[2][2] = ww - xx - yy + zz;
	out.M[2][3] = 0;
	out.M[3][0] = 0;
	out.M[3][1] = 0;
	out.M[3][2] = 0;
	out.M[3][3] = 1;
	return out;
}

//-----------------------------------------------------------------
// ovrTimeWarpParms initialization helper functions
//-----------------------------------------------------------------

// Convert a standard projection matrix into a TanAngle matrix for
// the primary time warp surface.
inline ovrMatrix4f TanAngleMatrixFromProjection( const ovrMatrix4f * projection )
{
	// A projection matrix goes from a view point to NDC, or -1 to 1 space.
	// Scale and bias to convert that to a 0 to 1 space.
	const ovrMatrix4f tanAngleMatrix =
	{ {
		{ 0.5f * projection->M[0][0], 0.5f * projection->M[0][1], 0.5f * projection->M[0][2] - 0.5f, 0.5f * projection->M[0][3] },
		{ 0.5f * projection->M[1][0], 0.5f * projection->M[1][1], 0.5f * projection->M[1][2] - 0.5f, 0.5f * projection->M[1][3] },
		{ 0.0f, 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f, 0.0f }
	} };
	return tanAngleMatrix;
}

// Trivial version of TanAngleMatrixFromProjection() for a symmetric field of view.
inline ovrMatrix4f TanAngleMatrixFromFov( const float fovDegrees )
{
	const float tanHalfFov = tanf( 0.5f * fovDegrees * ( M_PI / 180.0f ) );
	const ovrMatrix4f tanAngleMatrix =
	{ {
		{ 0.5f / tanHalfFov, 0.0f, -0.5f, 0.0f },
		{ 0.0f, 0.5f / tanHalfFov, -0.5f, 0.0f },
		{ 0.0f, 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f, 0.0f }
	} };
	return tanAngleMatrix;
}

// If a simple quad defined as a -1 to 1 XY unit square is transformed to
// the camera view with the given modelView matrix, it can alternately be
// drawn as a TimeWarp overlay image to take advantage of the full window
// resolution, which is usually higher than the eye buffer textures, and
// avoid resampling both into the eye buffer, and again to the screen.
// This is used for high quality movie screens and user interface planes.
//
// Note that this is NOT an MVP matrix -- the "projection" is handled
// by the distortion process.
//
// The exact composition of the overlay image and the base image is
// determined by the warpProgram, you may still need to draw the geometry
// into the eye buffer to punch a hole in the alpha channel to let the
// overlay/underlay show through.
//
// This utility functions converts a model-view matrix that would normally
// draw a -1 to 1 unit square to the view into a TanAngle matrix for an
// overlay surface.
//
// The resulting z value should be straight ahead distance to the plane.
// The x and y values will be pre-multiplied by z for projective texturing.
inline ovrMatrix4f TanAngleMatrixFromUnitSquare( const ovrMatrix4f * modelView )
{
	const ovrMatrix4f inv = ovrMatrix4f_Inverse( modelView );
	ovrMatrix4f m;
	m.M[0][0] = 0.5f * inv.M[2][0] - 0.5f * ( inv.M[0][0] * inv.M[2][3] - inv.M[0][3] * inv.M[2][0] );
	m.M[0][1] = 0.5f * inv.M[2][1] - 0.5f * ( inv.M[0][1] * inv.M[2][3] - inv.M[0][3] * inv.M[2][1] );
	m.M[0][2] = 0.5f * inv.M[2][2] - 0.5f * ( inv.M[0][2] * inv.M[2][3] - inv.M[0][3] * inv.M[2][2] );
	m.M[0][3] = 0.0f;
	m.M[1][0] = 0.5f * inv.M[2][0] + 0.5f * ( inv.M[1][0] * inv.M[2][3] - inv.M[1][3] * inv.M[2][0] );
	m.M[1][1] = 0.5f * inv.M[2][1] + 0.5f * ( inv.M[1][1] * inv.M[2][3] - inv.M[1][3] * inv.M[2][1] );
	m.M[1][2] = 0.5f * inv.M[2][2] + 0.5f * ( inv.M[1][2] * inv.M[2][3] - inv.M[1][3] * inv.M[2][2] );
	m.M[1][3] = 0.0f;
	m.M[2][0] = m.M[3][0] = inv.M[2][0];
	m.M[2][1] = m.M[3][1] = inv.M[2][1];
	m.M[2][2] = m.M[3][2] = inv.M[2][2];
	m.M[2][3] = m.M[3][3] = 0.0f;
	return m;
}

// Utility function to calculate external velocity for smooth stick yaw turning.
// To reduce judder in FPS style experiences when the application framerate is
// lower than the vsync rate, the rotation from a joypad can be applied to the
// view space distorted eye vectors before applying the time warp.
inline ovrMatrix4f CalculateExternalVelocity( const ovrMatrix4f * viewMatrix, const float yawRadiansPerSecond )
{
	const float angle = yawRadiansPerSecond * ( -1.0f / 60.0f );
	const float sinHalfAngle = sinf( angle * 0.5f );
	const float cosHalfAngle = cosf( angle * 0.5f );

	// Yaw is always going to be around the world Y axis
	ovrQuatf quat;
	quat.x = viewMatrix->M[0][1] * sinHalfAngle;
	quat.y = viewMatrix->M[1][1] * sinHalfAngle;
	quat.z = viewMatrix->M[2][1] * sinHalfAngle;
	quat.w = cosHalfAngle;
	return ovrMatrix4f_CreateFromQuaternion( &quat );
}

// Utility function to default initialize the ovrTimeWarpParms.

typedef enum
{
	WARP_INIT_DEFAULT,
	WARP_INIT_BLACK,
	WARP_INIT_LOADING_ICON,
	WARP_INIT_MESSAGE
} ovrWarpInit;

inline ovrTimeWarpParms InitTimeWarpParms( const ovrWarpInit init = WARP_INIT_DEFAULT, const unsigned int texId = 0 )
{
	const ovrMatrix4f tanAngleMatrix = TanAngleMatrixFromFov( 90.0f );

	ovrTimeWarpParms parms;
	memset( &parms, 0, sizeof( parms ) );

	for ( int eye = 0; eye < MAX_WARP_EYES; eye++ )
	{
		for ( int i = 0; i < MAX_WARP_IMAGES; i++ )
		{
			parms.Images[eye][i].TexCoordsFromTanAngles = tanAngleMatrix;
			parms.Images[eye][i].Pose.Pose.Orientation.w = 1.0f;
		}
	}
	parms.ExternalVelocity.M[0][0] = 1.0f;
	parms.ExternalVelocity.M[1][1] = 1.0f;
	parms.ExternalVelocity.M[2][2] = 1.0f;
	parms.ExternalVelocity.M[3][3] = 1.0f;
	parms.MinimumVsyncs = 1;
	parms.PreScheduleSeconds = 0.014f;
	parms.WarpProgram = WP_SIMPLE;
	parms.DebugGraphMode = DEBUG_PERF_OFF;
	parms.DebugGraphValue = DEBUG_VALUE_DRAW;

	switch ( init )
	{
		case WARP_INIT_DEFAULT:
		{
			break;
		}
		case WARP_INIT_BLACK:
		{
			parms.WarpOptions = SWAP_OPTION_INHIBIT_SRGB_FRAMEBUFFER | SWAP_OPTION_FLUSH | SWAP_OPTION_DEFAULT_IMAGES;
			parms.WarpProgram = WP_SIMPLE;
			for ( int eye = 0; eye < MAX_WARP_EYES; eye++ )
			{
				parms.Images[eye][0].TexId = 0;		// default replaced with a black texture
			}
			break;
		}
		case WARP_INIT_LOADING_ICON:
		{
			parms.WarpOptions = SWAP_OPTION_INHIBIT_SRGB_FRAMEBUFFER | SWAP_OPTION_FLUSH | SWAP_OPTION_DEFAULT_IMAGES;
			parms.WarpProgram = WP_LOADING_ICON;
			parms.ProgramParms[0] = 1.0f;		// rotation in radians per second
			parms.ProgramParms[1] = 16.0f;		// icon size factor smaller than fullscreen
			for ( int eye = 0; eye < MAX_WARP_EYES; eye++ )
			{
				parms.Images[eye][0].TexId = 0;		// default replaced with a black texture
				parms.Images[eye][1].TexId = texId;	// loading icon texture
			}
			break;
		}
		case WARP_INIT_MESSAGE:
		{
			parms.WarpOptions = SWAP_OPTION_INHIBIT_SRGB_FRAMEBUFFER | SWAP_OPTION_FLUSH | SWAP_OPTION_DEFAULT_IMAGES;
			parms.WarpProgram = WP_LOADING_ICON;
			parms.ProgramParms[0] = 0.0f;		// rotation in radians per second
			parms.ProgramParms[1] = 2.0f;		// message size factor smaller than fullscreen
			for ( int eye = 0; eye < MAX_WARP_EYES; eye++ )
			{
				parms.Images[eye][0].TexId = 0;		// default replaced with a black texture
				parms.Images[eye][1].TexId = texId;	// message texture
			}
			break;
		}
	}
	return parms;
}

#endif	// OVR_VrApi_Helpers_h
