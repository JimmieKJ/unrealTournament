/************************************************************************************

Filename    :   GlGeometry.h
Content     :   OpenGL geometry setup.
Created     :   October 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_Geometry_h
#define OVR_Geometry_h

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Math.h"

namespace OVR
{

struct VertexAttribs
{
	Array< Vector3f > position;
	Array< Vector3f > normal;
	Array< Vector3f > tangent;
	Array< Vector3f > binormal;
	Array< Vector4f > color;
	Array< Vector2f > uv0;
	Array< Vector2f > uv1;
	Array< Vector4i > jointIndices;
	Array< Vector4f > jointWeights;
};

typedef unsigned short TriangleIndex;
//typedef unsigned int TriangleIndex;

static const int MAX_GEOMETRY_VERTICES	= 1 << ( sizeof( TriangleIndex ) * 8 );
static const int MAX_GEOMETRY_INDICES	= 1024 * 1024 * 3;

class GlGeometry
{
public:
			GlGeometry() :
				vertexBuffer( 0 ),
				indexBuffer( 0 ),
				vertexArrayObject( 0 ),
				vertexCount( 0 ),
				indexCount( 0 ) {}

			GlGeometry( const VertexAttribs & attribs, const Array< TriangleIndex > & indices ) :
				vertexBuffer( 0 ),
				indexBuffer( 0 ),
				vertexArrayObject( 0 ),
				vertexCount( 0 ),
				indexCount( 0 ) { Create( attribs, indices ); }

	// Create the VAO and vertex and index buffers from arrays of data.
	void	Create( const VertexAttribs & attribs, const Array< TriangleIndex > & indices );
	void	Update( const VertexAttribs & attribs );

	// Assumes the correct program, uniforms, textures, etc, are all bound.
	// Leaves the VAO bound for efficiency, be careful not to inadvertently
	// modify any of the state.
	// You need to manually bind and draw if you want to use GL_LINES / GL_POINTS / GL_TRISTRIPS,
	// or only draw a subset of the indices.
	void	Draw() const;

	// Free the buffers and VAO, assuming that they are strictly for this geometry.
	// We could save some overhead by packing an entire model into a single buffer, but
	// it would add more coupling to the structures.
	// This is not in the destructor to allow objects of this class to be passed by value.
	void	Free();

public:
	unsigned 	vertexBuffer;
	unsigned 	indexBuffer;
	unsigned 	vertexArrayObject;
	int			vertexCount;
	int 		indexCount;
};

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
//
// A horizontal and vertical value of 1 will give a single quad.
//
// Texcoords range from 0 to 1.
//
// Color is 1, fades alpha to 0 along the outer edge.
GlGeometry BuildTesselatedQuad( const int horizontal, const int vertical, bool twoSided = false );

GlGeometry BuildFadedScreenMask( const float xFraction, const float yFraction );

// 8 quads making a thin border inside the -1 tp 1 square.
// The fractions are the total fraction that will be faded,
// half on one side, half on the other.
GlGeometry BuildVignette( const float xFraction, const float yFraction );

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
// Fades alpha to 0 along the outer edge.
GlGeometry BuildTesselatedCylinder( const float radius, const float height,
		const int horizontal, const int vertical, const float uScale, const float vScale );

GlGeometry BuildDome( const float latRads, const float uScale = 1.0f, const float vScale = 1.0f );
GlGeometry BuildGlobe( const float uScale = 1.0f, const float vScale = 1.0f );

// Make a square patch on a sphere that can rotate with the viewer
// so it always covers the screen.
GlGeometry BuildSpherePatch( const float fov );

// Builds a grid of lines inside the -1 to 1 bounds.
// Always has lines through the origin, plus extraLines on each side
// of the origin.
// Go from very slightly inside the -1 to 1 bounds so lines won't get
// clipped at the edge of the projection frustum.
//
// If not fullGrid, all lines but x=0 and y=0 will be short hashes
GlGeometry BuildCalibrationLines( const int extraLines, const bool fullGrid );

// 12 edges of a 0 to 1 unit cube.
GlGeometry BuildUnitCubeLines();

}	// namespace OVR

#endif	// OVR_Geometry_h
