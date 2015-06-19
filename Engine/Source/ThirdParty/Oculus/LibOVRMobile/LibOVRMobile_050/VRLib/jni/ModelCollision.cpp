/************************************************************************************

Filename    :   ModelCollision.cpp
Content     :   Basic collision detection for scene walkthroughs.
Created     :   May 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "ModelCollision.h"

#include <math.h>


#include "Kernel/OVR_Alg.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"

namespace OVR {



//-----------------------------------------------------------------------------
//	CollisionPolytope
//-----------------------------------------------------------------------------

const float COLLISION_EPSILON = 0.01f;

bool CollisionPolytope::TestPoint( const Vector3f & p ) const
{
    for ( int i = 0; i < Planes.GetSizeI(); i++ )
	{
        if ( Planes[i].TestSide( p ) > 0.0f )
        {
            return false;
        }
	}
    return true;
}

bool CollisionPolytope::TestRay( const Vector3f & start, const Vector3f & dir, float & length, Planef * plane ) const
{
    const Vector3f end = start + dir * length;

    int crossing = -1;
    float cdot1 = 0.0f;
	float cdot2 = 0.0f;

    for ( int i = 0; i < Planes.GetSizeI(); i++ )
    {
        const float dot1 = Planes[i].TestSide( start );
        if ( dot1 > 0.0f )
        {
			const float dot2 = Planes[i].TestSide( end );
			if ( dot2 > 0.0f )
			{
				return false;
			}
            if ( dot2 <= 0.0f )
            {
				if ( crossing == -1 )
				{
					crossing = i;
					cdot1 = dot1;
					cdot2 = dot2;
				}
				else
				{
					if ( dot2 > cdot2 )
					{
						crossing = i;
						cdot1 = dot1;
						cdot2 = dot2;
					}
				}
            }
        }
    }

    if ( crossing < 0 )
    {
        return false;
    }

    length = length * ( cdot1 - COLLISION_EPSILON ) / ( cdot1 - cdot2 );
    if ( length < 0.0f )
    {
        length = 0.0f;
    }

    if ( plane != NULL )
    {
        *plane = Planes[crossing];
    }
    return true;
}

bool CollisionPolytope::PopOut( Vector3f & p ) const
{
	float minDist = FLT_MAX;
	int crossing = -1;
	for ( int i = 0; i < Planes.GetSizeI(); i++ )
	{
	    float dist = Planes[i].TestSide( p );
		if ( dist > 0.0f )
		{
			return false;
		}
		dist = fabs( dist );
		if ( dist < minDist )
		{
			minDist = dist;
			crossing = i;
		}
	}
	p += Planes[crossing].N * COLLISION_EPSILON;
	return true;
}

//-----------------------------------------------------------------------------
//	CollisionModel
//-----------------------------------------------------------------------------

bool CollisionModel::TestPoint( const Vector3f & p ) const
{
	for ( int i = 0; i < Polytopes.GetSizeI(); i++ )
	{
		if ( Polytopes[i].TestPoint( p ) )
		{
			return true;
		}
	}
	return false;
}

bool CollisionModel::TestRay( const Vector3f & start, const Vector3f & dir, float & length, Planef * plane ) const
{
	bool clipped = false;
	for ( int i = 0; i < Polytopes.GetSizeI(); i++ )
	{
		Planef clipPlane;
		float clipLength = length;
		if ( Polytopes[i].TestRay( start, dir, clipLength, &clipPlane ) )
		{
			if ( clipLength < length )
			{
				length = clipLength;
				if ( plane != NULL )
				{
					*plane = clipPlane;
				}
				clipped = true;
			}
		}
	}
	return clipped;
}

bool CollisionModel::PopOut( Vector3f & p ) const
{
	for ( int i = 0; i < Polytopes.GetSizeI(); i++ )
	{
		if ( Polytopes[i].PopOut( p ) )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//	SlideMove
//-----------------------------------------------------------------------------

const Vector3f	UpVector( 0.0f, 1.0f, 0.0f );
const float		RailHeight = 0.8f;

Vector3f SlideMove(
		const Vector3f & footPos,
		const float eyeHeight,
		const Vector3f & moveDirection,
		const float moveDistance,
		const CollisionModel & collisionModel,
		const CollisionModel & groundCollisionModel
	    )
{
	// Check for collisions at eye level to prevent slipping under walls.
	Vector3f eyePos = footPos + UpVector * eyeHeight;

	// Pop out of any collision models.
	collisionModel.PopOut( eyePos );

	{
		Planef fowardCollisionPlane;
		float forwardDistance = moveDistance;
		if ( !collisionModel.TestRay( eyePos, moveDirection, forwardDistance, &fowardCollisionPlane ) )
		{
			// No collision, move the full distance.
			eyePos += moveDirection * moveDistance;
		}
		else
		{
			// Move up to the point of collision.
			eyePos += moveDirection * forwardDistance;

			// Project the remaining movement onto the collision plane.
			const float COLLISION_BOUNCE = 0.001f;	// don't creep into the plane due to floating-point rounding
			const float intoPlane = moveDirection.Dot( fowardCollisionPlane.N ) - COLLISION_BOUNCE;
			const Vector3f slideDirection = ( moveDirection - fowardCollisionPlane.N * intoPlane );

			// Try to finish the move by sliding along the collision plane.
			float slideDistance = moveDistance;
			collisionModel.TestRay( eyePos - UpVector * RailHeight, slideDirection, slideDistance, NULL );

			eyePos += slideDirection * slideDistance;
		}
	}

	if ( groundCollisionModel.Polytopes.GetSizeI() != 0 )
	{
		// Check for collisions at foot level, which allows following terrain.
		float downDistance = 10.0f;
		groundCollisionModel.TestRay( eyePos, - UpVector, downDistance, NULL );

		// Maintain the minimum camera height.
		if ( eyeHeight - downDistance < 1.0f )
		{
			eyePos += UpVector * ( eyeHeight - downDistance );
		}
	}

	return eyePos - UpVector * eyeHeight;
}

}	// namespace OVR
