/************************************************************************************

Filename    :   ModelCollision.h
Content     :   Basic collision detection for scene walkthroughs.
Created     :   May 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef MODELCOLLISION_H
#define MODELCOLLISION_H

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"

namespace OVR
{

class CollisionPolytope
{
public:
	void	Add( const Planef & p ) { Planes.PushBack( p ); }

	// Returns true if the given point is inside this polytope.
	bool	TestPoint( const Vector3f & p ) const;

	// Returns true if the ray hits the polytope.
	// The length of the ray is clipped to the point where the ray enters the polytope.
	// Optionally the polytope boundary plane that is hit is returned.
	bool	TestRay( const Vector3f & start, const Vector3f & dir, float & length, Planef * plane ) const;

	// Pops the given point out of the polytope if inside.
	bool	PopOut( Vector3f & p ) const;

public:
	String			Name;
	Array< Planef > Planes;
};

class CollisionModel
{
public:
	// Returns true if the given point is inside solid.
	bool	TestPoint( const Vector3f & p ) const;

	// Returns true if the ray hits solid.
	// The length of the ray is clipped to the point where the ray enters solid.
	// Optionally the solid boundary plane that is hit is returned.
	bool	TestRay( const Vector3f & start, const Vector3f & dir, float & length, Planef * plane ) const;

	// Pops the given point out of any collision geometry the point may be inside of.
	bool	PopOut( Vector3f & p ) const;

public:
	Array< CollisionPolytope > Polytopes;
};

Vector3f SlideMove(
		const Vector3f & footPos,
		const float eyeHeight,
		const Vector3f & moveDirection,
		const float moveDistance,
		const CollisionModel & collisionModel,
		const CollisionModel & groundCollisionModel
	    );

}	// namespace OVR

#endif	// MODELCOLLISION_H
