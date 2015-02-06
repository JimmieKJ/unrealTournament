/************************************************************************************

Filename    :   CollisionPrimitive.h
Content     :   Generic collision class supporting ray / triangle intersection.
Created     :   September 10, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_CollisionPrimitive_h )
#define OVR_CollisionPrimitive_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_BitFlags.h"
#include "../GlGeometry.h" // For TriangleIndex

namespace OVR {

class OvrDebugLines;

enum eContentFlags
{
	CONTENT_SOLID,
	CONTENT_ALL = 0x7fffffff
};

typedef BitFlagsT< eContentFlags > ContentFlags_t;

//==============================================================
// OvrCollisionResult
// Structure that holds the result of a collision
class OvrCollisionResult
{
public:
	OvrCollisionResult() :
		t( FLT_MAX ),
		uv( 0.0f ),
		TriIndex( -1 )
	{
	}

	float			t;			// fraction along line where the intersection occurred
	Vector2f		uv;			// texture coordinate of intersection
	int64_t			TriIndex;	// index of triangle hit (local to collider)
};

//==============================================================
// OvrCollisionPrimitive
// Base class for a collision primitive.
class OvrCollisionPrimitive
{
public:
						OvrCollisionPrimitive() { }
						OvrCollisionPrimitive( ContentFlags_t const contents ) : Contents( contents ) { }
	virtual				~OvrCollisionPrimitive();

	virtual  bool		IntersectRay( Vector3f const & start, Vector3f const & dir, Posef const & pose,
								Vector3f const & scale, ContentFlags_t const testContents,
								OvrCollisionResult & result ) const = 0;
	// the ray should already be in local space
	virtual bool		IntersectRay( Vector3f const & localStart, Vector3f const & localDir,
								Vector3f const & scale, ContentFlags_t const testContents,
								OvrCollisionResult & result ) const = 0;

	// test for ray intersection against only the AAB 
	bool				IntersectRayBounds( Vector3f const & start, Vector3f const & dir, 
								Vector3f const & scale, ContentFlags_t const testContents, 
								float & t0, float & t1 ) const;

	virtual void		DebugRender( OvrDebugLines & debugLines, Posef & pose ) const = 0;

	ContentFlags_t		GetContents() const { return Contents; }
	void				SetContents( ContentFlags_t const contents ) { Contents = contents; }

	Bounds3f const &	GetBounds() const { return Bounds; }
	void				SetBounds( Bounds3f const & bounds ) { Bounds = bounds; }

protected:
	bool				IntersectRayBounds( Vector3f const & start, Vector3f const & dir, 
							    Vector3f const & scale, float & t0, float & t1 ) const;

private:
	ContentFlags_t		Contents;	// flags dictating what can hit this collider
	Bounds3f			Bounds;		// Axial-aligned bounds of the primitive

};

//==============================================================
// OvrTriCollisionPrimitive
// Collider that handles collision vs. polygons and stores those polygons itself.
class OvrTriCollisionPrimitive : public OvrCollisionPrimitive
{
public:
	OvrTriCollisionPrimitive();
	OvrTriCollisionPrimitive( Array< Vector3f > const & vertices, Array< TriangleIndex > const & indices, 
			ContentFlags_t const contents );

	virtual	~OvrTriCollisionPrimitive();

	void				Init( Array< Vector3f > const & vertices, Array< TriangleIndex > const & indices,
								ContentFlags_t const contents );

	virtual  bool		IntersectRay( Vector3f const & start, Vector3f const & dir, Posef const & pose,
								Vector3f const & scale, ContentFlags_t const testContents,
								OvrCollisionResult & result ) const;


	// the ray should already be in local space
	virtual bool		IntersectRay( Vector3f const & localStart, Vector3f const & localDir,
								Vector3f const & scale, ContentFlags_t const testContents,
								OvrCollisionResult & result ) const;

	virtual void		DebugRender( OvrDebugLines & debugLines, Posef & pose ) const;

private:
	Array< Vector3f >		Vertices;	// vertices for all triangles
	Array< TriangleIndex >	Indices;	// indices indicating which vertices make up each triangle
};

} // namespace OVR

#endif // OVR_CollisionPrimitive_h