/************************************************************************************

Filename    :   RtIntersect.cpp
Content     :   Basic intersection routines.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "RtIntersect.h"

#include <math.h>


#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Alg.h"

using namespace OVR;

bool RtIntersect::RayBounds( const Vector3f & rayStart, const Vector3f & rayDir,
							const Vector3f & mins, const Vector3f & maxs,
							float & t0, float & t1 )
{
	const float rcpDirX = ( fabsf( rayDir.x ) > Math<float>::SmallestNonDenormal ) ? ( 1.0f / rayDir.x ) : Math<float>::HugeNumber;
	const float rcpDirY = ( fabsf( rayDir.y ) > Math<float>::SmallestNonDenormal ) ? ( 1.0f / rayDir.y ) : Math<float>::HugeNumber;
	const float rcpDirZ = ( fabsf( rayDir.z ) > Math<float>::SmallestNonDenormal ) ? ( 1.0f / rayDir.z ) : Math<float>::HugeNumber;

	const float sX = ( mins.x - rayStart.x ) * rcpDirX;
	const float sY = ( mins.y - rayStart.y ) * rcpDirY;
	const float sZ = ( mins.z - rayStart.z ) * rcpDirZ;

	const float tX = ( maxs.x - rayStart.x ) * rcpDirX;
	const float tY = ( maxs.y - rayStart.y ) * rcpDirY;
	const float tZ = ( maxs.z - rayStart.z ) * rcpDirZ;

	const float minX = Alg::Min( sX, tX );
	const float minY = Alg::Min( sY, tY );
	const float minZ = Alg::Min( sZ, tZ );

	const float maxX = Alg::Max( sX, tX );
	const float maxY = Alg::Max( sY, tY );
	const float maxZ = Alg::Max( sZ, tZ );

	t0 = Alg::Max( minX, Alg::Max( minY, minZ ) );
	t1 = Alg::Min( maxX, Alg::Min( maxY, maxZ ) );

	return ( t0 <= t1 );
}

bool RtIntersect::RayTriangle( const Vector3f & rayStart, const Vector3f & rayDir,
							const Vector3f & v0, const Vector3f & v1, const Vector3f & v2,
							float & t0, float & u, float & v )
{
	assert( rayDir.IsNormalized() );

	const Vector3f edge1 = v1 - v0;
	const Vector3f edge2 = v2 - v0;

	const Vector3f tv = rayStart - v0;
	const Vector3f pv = rayDir.Cross( edge2 );
	const Vector3f qv = tv.Cross( edge1 );
	const float det = edge1.Dot( pv );

	// If the determinant is negative then the triangle is backfacing.
	if ( det <= 0.0f )
	{
		return false;
	}

	// This code has been modified to only perform a floating-point
	// division if the ray actually hits the triangle. If back facing
	// triangles are not culled then the sign of 's' and 't' need to
	// be flipped. This can be accomplished by multiplying the values
	// with the determinant instead of the reciprocal determinant.

	const float s = tv.Dot( pv );
	const float t = rayDir.Dot( qv );

	if ( s >= 0.0f && s <= det )
	{
		if ( t >= 0.0f && s + t <= det )
		{
			// If the determinant is almost zero then the ray lies in the triangle plane.
			// This comparison is done last because it is usually rare for
			// the ray to lay in the triangle plane.
			if ( fabsf( det ) > Math<float>::SmallestNonDenormal )
			{
				const float rcpDet = 1.0f / det;
				t0 = edge2.Dot( qv ) * rcpDet;
				u = s * rcpDet;
				v = t * rcpDet;
				return true;
			}
		}
	}

	return false;
}
