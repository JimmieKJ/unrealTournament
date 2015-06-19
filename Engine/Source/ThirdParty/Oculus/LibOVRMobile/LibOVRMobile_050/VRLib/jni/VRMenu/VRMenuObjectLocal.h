/************************************************************************************

Filename    :   VRMenuObject.h
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuObjectLocal_h )
#define OVR_VRMenuObjectLocal_h

#include "VRMenuObject.h"
#include "VRMenuMgr.h"
#include "CollisionPrimitive.h"
#include "../BitmapFont.h"

namespace OVR {

typedef SInt16 guiIndex_t;

struct textMetrics_t {
	textMetrics_t() :
        w( 0.0f ),
        h( 0.0f ),
        ascent( 0.0f ),
		descent( 0.0f ),
		fontHeight( 0.0f )
	{
    }

	float w;
	float h;
	float ascent;
	float descent;
	float fontHeight;
};

class App;

//==============================================================
// VRMenuSurfaceTexture
class VRMenuSurfaceTexture
{
public:
	VRMenuSurfaceTexture();

	bool	LoadTexture( eSurfaceTextureType const type, char const * imageName, bool const allowDefault );
	void 	LoadTexture( eSurfaceTextureType const type, const GLuint texId, const int width, const int height );
	void	Free();
	void	SetOwnership( const bool isOwner )	{ OwnsTexture = isOwner; }

	GLuint				GetHandle() const { return Handle; }
	int					GetWidth() const { return Width; }
	int					GetHeight() const { return Height; }
	eSurfaceTextureType	GetType() const { return Type; }

private:
	GLuint				Handle;	// GL texture handle
	int					Width;	// width of the image
	int					Height;	// height of the image
	eSurfaceTextureType	Type;	// specifies how this image is used for rendering
    bool                OwnsTexture;    // if true, free texture on a reload or deconstruct
};

//==============================================================
// SubmittedMenuObject
class SubmittedMenuObject
{
public:
	SubmittedMenuObject() :
		SurfaceIndex( -1 ),
		DistanceIndex( -1 ),
		Scale( 1.0f ),
		Color( 1.0f ),
		ColorTableOffset( 0.0f ),
		SkipAdditivePass( false ),
		FadeDirection( 0.0f )
	{
	}

	menuHandle_t				Handle;				// handle of the object
	int							SurfaceIndex;		// surface of the object
	int							DistanceIndex;		// use the position at this index to calc sort distance
	Posef						Pose;				// pose in model space
	Vector3f					Scale;				// scale of the object
	Vector4f					Color;				// color of the object
	Vector2f					ColorTableOffset;	// color table offset for color ramp fx
	bool						SkipAdditivePass;	// true to skip any additive multi-texture pass
	VRMenuRenderFlags_t			Flags;				// various flags
	Vector2f					Offsets;			// offsets based on anchors (width / height * anchor.x / .y)
	Vector3f					FadeDirection;		// Fades vertices based on direction - default is zero vector which indicates off
#if defined( OVR_BUILD_DEBUG )
	String						SurfaceName;		// for debugging only
#endif
};


//==============================================================
// VRMenuSurface
class VRMenuSurface
{
public:
    // artificial bounds in Z, which is technically 0 for surfaces that are an image
    static const float Z_BOUNDS;

	VRMenuSurface();
	~VRMenuSurface();

	void							CreateFromSurfaceParms( VRMenuSurfaceParms const & parms );

	void							Free();

	void 							RegenerateSurfaceGeometry();

	void							Render( OvrVRMenuMgr const & menuMgr, Matrix4f const & mvp, SubmittedMenuObject const & sub ) const;

	Bounds3f const &				GetLocalBounds() const { return Tris.GetBounds(); }

	bool							IsRenderable() const { return ( Geo.vertexCount > 0 && Geo.indexCount > 0 ) && Visible; }

    bool							IntersectRay( Vector3f const & start, Vector3f const & dir, Posef const & pose,
									        Vector3f const & scale, ContentFlags_t const testContents, OvrCollisionResult & result ) const;
    // the ray should already be in local space
    bool							IntersectRay( Vector3f const & localStart, Vector3f const & localDir, 
									        Vector3f const & scale, ContentFlags_t const testContents, OvrCollisionResult & result ) const;
    void							LoadTexture( int const textureIndex, eSurfaceTextureType const type, 
									        const GLuint texId, const int width, const int height );

	Vector4f const&					GetColor() const { return Color; }
	void							SetColor( Vector4f const & color ) { Color = color; }

	Vector2f const &				GetDims() const { return Dims; }
	void							SetDims( Vector2f const &dims ) { Dims = dims; } // requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

	Vector2f const &				GetAnchors() const { return Anchors; }
	void							SetAnchors( Vector2f const & a ) { Anchors = a; }

	Vector2f						GetAnchorOffsets() const;

	Vector4f const &				GetBorder() const { return Border; }
	void							SetBorder( Vector4f const & a ) { Border = a; }	// requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

	VRMenuSurfaceTexture const &	GetTexture( int const index )  const { return Textures[index]; }
	void							SetOwnership( int const index, bool const isOwner );
	
	bool							GetVisible() const { return Visible; }
	void							SetVisible( bool const v ) { Visible = v; }

	String const &					GetName() const { return SurfaceName; }

private:
	VRMenuSurfaceTexture			Textures[VRMENUSURFACE_IMAGE_MAX];
	GlGeometry						Geo;				// VBO for this surface
	OvrTriCollisionPrimitive		Tris;				// per-poly collision object
	Vector4f						Color;				// Color, modulated with object color
	Vector2f						TextureDims;		// texture width and height
	Vector2f						Dims;				// width and height
	Vector2f						Anchors;			// anchors
	Vector4f						Border;				// border size for sliced sprite
	String      					SurfaceName;		// name of the surface for debugging
	ContentFlags_t					Contents;
	bool							Visible;			// must be true to render -- used to animate between different surfaces

	eGUIProgramType					ProgramType;

private:
	void							CreateImageGeometry(  int const textureWidth, int const textureHeight, const Vector2f &dims, const Vector4f &border, ContentFlags_t const content );
	// This searches the loaded textures for the specified number of matching types. For instance,
	// ( SURFACE_TEXTURE_DIFFUSE, 2 ) would only return true if two of the textures were of type
	// SURFACE_TEXTURE_DIFFUSE.  This is used to determine, based on surface types, which shaders
	// to use to render the surface, independent of how the texture maps are ordered.
	bool							HasTexturesOfType( eSurfaceTextureType const t, int const requiredCount ) const;
	// Returns the index in Textures[] of the n-th occurence of type t.
	int								IndexForTextureType( eSurfaceTextureType const t, int const occurenceCount ) const;
};

//==============================================================
// VRMenuObjectLocal
// base class for all menu objects

class VRMenuObjectLocal : public VRMenuObject
{
public:
	friend class VRMenuObject;
	friend class VRMenuMgr;
	friend class VRMenuMgrLocal;

	// Initialize the object after creation
	virtual void				Init( VRMenuObjectParms const & parms );

	// Frees all of this object's children
	virtual void				FreeChildren( OvrVRMenuMgr & menuMgr );

	// Adds a child to this menu object
	virtual void				AddChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle );
	// Removes a child from this menu object, but does not free the child object.
	virtual void				RemoveChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle );
	// Removes a child from tis menu object and frees it, along with any children it may have.
	virtual void				FreeChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle );
	// Returns true if the handle is in this menu's tree of children
	virtual bool				IsDescendant( OvrVRMenuMgr & menuMgr, menuHandle_t const handle ) const;

	// Update this menu for a frame, including testing the gaze direction against the bounds
	// of this menu and all its children
	virtual void				Frame( OvrVRMenuMgr & menuMgr, Matrix4f const & viewMatrix );

    // Tests a ray (in object's local space) for intersection.
    bool                        IntersectRay( Vector3f const & localStart, Vector3f const & localDir, Vector3f const & parentScale, Bounds3f const & bounds,
                                        float & bounds_t0, float & bounds_t1, ContentFlags_t const testContents, 
										OvrCollisionResult & result ) const;

	// Test the ray against this object and all child objects, returning the first object that was 
	// hit by the ray. The ray should be in parent-local space - for the current root menu this is 
	// always world space.
	virtual menuHandle_t		HitTest( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font, Posef const & worldPose, 
                                        Vector3f const & rayStart, Vector3f const & rayDir, ContentFlags_t const testContents, 
										HitTestResult & result ) const;

	//--------------------------------------------------------------
	// components
	//--------------------------------------------------------------
	virtual void				AddComponent( VRMenuComponent * component );
	virtual	void				RemoveComponent( VRMenuComponent * component );

	virtual Array< VRMenuComponent* > const & GetComponentList() const { return Components; }

	virtual VRMenuComponent *	GetComponentById_Impl( int id ) const;
	virtual VRMenuComponent *	GetComponentByName_Impl( const char * typeName ) const;

	//--------------------------------------------------------------
	// accessors
	//--------------------------------------------------------------
	virtual eVRMenuObjectType	GetType() const { return Type; }
	virtual menuHandle_t		GetHandle() const { return Handle; }
	virtual menuHandle_t		GetParentHandle() const { return ParentHandle; }
	virtual void				SetParentHandle( menuHandle_t const h ) { ParentHandle = h; }

	virtual	VRMenuObjectFlags_t const &	GetFlags() const { return Flags; }
	virtual	void				SetFlags( VRMenuObjectFlags_t const & flags ) { Flags = flags; }
	virtual	void				AddFlags( VRMenuObjectFlags_t const & flags ) { Flags |= flags; }
	virtual	void				RemoveFlags( VRMenuObjectFlags_t const & flags ) { Flags &= ~flags; }

	virtual OVR::String const &	GetText() const { return Text; }
	virtual void				SetText( char const * text ) { Text = text; TextDirty = true; }
	virtual void				SetTextWordWrapped( char const * text, class BitmapFont const & font, float const widthInMeters );

	virtual bool				IsHilighted() const { return Hilighted; }
	virtual void				SetHilighted( bool const b ) { Hilighted = b; }
	virtual bool				IsSelected() const { return Selected; }
	virtual void				SetSelected( bool const b ) { Selected = b; }
	virtual	int					NumChildren() const { return Children.GetSizeI(); }
	virtual menuHandle_t		GetChildHandleForIndex( int const index ) const { return Children[index]; }

	virtual Posef const &		GetLocalPose() const { return LocalPose; }
	virtual void				SetLocalPose( Posef const & pose ) { LocalPose = pose; }
	virtual Vector3f const &	GetLocalPosition() const { return LocalPose.Position; }
	virtual void				SetLocalPosition( Vector3f const & pos ) { LocalPose.Position = pos; }
	virtual Quatf const &		GetLocalRotation() const { return LocalPose.Orientation; }
	virtual void				SetLocalRotation( Quatf const & rot ) { LocalPose.Orientation = rot; }
	virtual Vector3f            GetLocalScale() const;
	virtual void				SetLocalScale( Vector3f const & scale ) { LocalScale = scale; }

    virtual Posef const &       GetHilightPose() const { return HilightPose; }
    virtual void                SetHilightPose( Posef const & pose ) { HilightPose = pose; }
    virtual float               GetHilightScale() const { return HilightScale; }
    virtual void                SetHilightScale( float const s ) { HilightScale = s; }

    virtual void                SetTextLocalPose( Posef const & pose ) { TextLocalPose = pose; }
    virtual Posef const &       GetTextLocalPose() const { return TextLocalPose; }
    virtual void                SetTextLocalPosition( Vector3f const & pos ) { TextLocalPose.Position = pos; }
    virtual Vector3f const &    GetTextLocalPosition() const { return TextLocalPose.Position; }
    virtual void                SetTextLocalRotation( Quatf const & rot ) { TextLocalPose.Orientation = rot; }
    virtual Quatf const &       GetTextLocalRotation() const { return TextLocalPose.Orientation; }
    virtual Vector3f            GetTextLocalScale() const;
    virtual void                SetTextLocalScale( Vector3f const & scale ) { TextLocalScale = scale; }

	virtual	void				SetLocalBoundsExpand( Vector3f const mins, Vector3f const & maxs );

	virtual Bounds3f			GetLocalBounds( BitmapFont const & font ) const;
    virtual Bounds3f            GetTextLocalBounds( BitmapFont const & font ) const;

	virtual Bounds3f const &	GetCullBounds() const { return CullBounds; }
	virtual void				SetCullBounds( Bounds3f const & bounds ) const { CullBounds = bounds; }

	virtual	Vector2f const &	GetColorTableOffset() const;
	virtual void				SetColorTableOffset( Vector2f const & ofs );

	virtual	Vector4f const &	GetColor() const;
	virtual	void				SetColor( Vector4f const & c );

    virtual	Vector4f const &	GetTextColor() const { return TextColor; }
    virtual	void				SetTextColor( Vector4f const & c ) { TextColor = c; }

	virtual VRMenuId_t			GetId() const { return Id; }
	virtual menuHandle_t		ChildHandleForId( OvrVRMenuMgr & menuMgr, VRMenuId_t const id ) const;

	virtual void				SetFontParms( VRMenuFontParms const & fontParms ) { FontParms = fontParms; }
	virtual VRMenuFontParms const & GetFontParms() const { return FontParms; }

	virtual	Vector3f const &	GetFadeDirection() const { return FadeDirection;  }
	virtual void				SetFadeDirection( Vector3f const & dir ) { FadeDirection = dir;  }

	virtual void				SetVisible( bool visible );

	// returns the index of the first surface with SURFACE_TEXTURE_ADDITIVE.
	// If singular is true, then the matching surface must have only one texture map and it must be of that type.
	virtual int					FindSurfaceWithTextureType( eSurfaceTextureType const type, bool const singular ) const;
	virtual	void				SetSurfaceColor( int const surfaceIndex, Vector4f const & color );
	virtual Vector4f const &	GetSurfaceColor( int const surfaceIndex ) const;
	virtual	void				SetSurfaceVisible( int const surfaceIndex, bool const v );
	virtual bool				GetSurfaceVisible( int const surfaceIndex ) const;
	virtual int					NumSurfaces() const;
	virtual int					AllocSurface();
	virtual void 				CreateFromSurfaceParms( int const surfaceIndex, VRMenuSurfaceParms const & parms );

    virtual void                SetSurfaceTexture( int const surfaceIndex, int const textureIndex, 
                                        eSurfaceTextureType const type, GLuint const texId, 
                                        int const width, int const height );

	virtual void                SetSurfaceTextureTakeOwnership( int const surfaceIndex, int const textureIndex,
										eSurfaceTextureType const type, GLuint const texId,
										int const width, int const height );

	virtual void 				RegenerateSurfaceGeometry( int const surfaceIndex, const bool freeSurfaceGeometry );

	virtual Vector2f const &	GetSurfaceDims( int const surfaceIndex ) const;
	virtual void				SetSurfaceDims( int const surfaceIndex, Vector2f const &dims );

	virtual Vector4f const &	GetSurfaceBorder( int const surfaceIndex );
	virtual void				SetSurfaceBorder( int const surfaceIndex, Vector4f const & border );

	//--------------------------------------------------------------
	// collision
	//--------------------------------------------------------------
	virtual void							SetCollisionPrimitive( OvrCollisionPrimitive * c );
	virtual OvrCollisionPrimitive *			GetCollisionPrimitive() { return CollisionPrimitive; }
	virtual OvrCollisionPrimitive const *	GetCollisionPrimitive() const { return CollisionPrimitive; }

	virtual ContentFlags_t		GetContents() const { return Contents; }
	virtual void				SetContents( ContentFlags_t const c ) { Contents = c; }

	//--------------------------------------------------------------
	// surfaces (non-virtual)
	//--------------------------------------------------------------
	VRMenuSurface const &			GetSurface( int const s ) const { return Surfaces[s]; }
	VRMenuSurface &					GetSurface( int const s ) { return Surfaces[s]; }
	Array< VRMenuSurface > const &	GetSurfaces() const { return Surfaces; }

	float						GetWrapWidth() const { return WrapWidth; }


private:
	eVRMenuObjectType			Type;			// type of this object
	menuHandle_t				Handle;			// handle of this object
	menuHandle_t				ParentHandle;	// handle of this object's parent
	VRMenuId_t					Id;				// opaque id that the creator of the menu can use to identify a menu object
	VRMenuObjectFlags_t			Flags;			// various bit flags
	Posef						LocalPose;		// local-space position and orientation
	Vector3f					LocalScale;		// local-space scale of this item
    Posef                       HilightPose;    // additional pose applied when hilighted
    float                       HilightScale;   // additional scale when hilighted
    Posef                       TextLocalPose;  // local-space position and orientation of text, local to this node (i.e. after LocalPose / LocalScale are applied)
    Vector3f                    TextLocalScale; // local-space scale of the text at this node
	OVR::String					Text;			// text to display on this object
	Array< menuHandle_t >		Children;		// array of direct children of this object
	Array< VRMenuComponent* >	Components;		// array of components on this object
	OvrCollisionPrimitive *		CollisionPrimitive;		// collision surface, if any
	ContentFlags_t				Contents;		// content flags for this object

	// may be cleaner to put texture and geometry in a separate surface structure
	Array< VRMenuSurface >		Surfaces;
	Vector4f					Color;				// color modulation
    Vector4f                    TextColor;          // color modulation for text
	Vector2f					ColorTableOffset;	// offset for color-ramp shader fx
	VRMenuFontParms				FontParms;			// parameters for text rendering
	Vector3f                    FadeDirection;		// Fades vertices based on direction - default is zero vector which indicates off

	bool						Hilighted;		// true if hilighted
	bool						Selected;		// true if selected
    mutable bool                TextDirty;      // if true, recalculate text bounds

    // cached state
	Vector3f					MinsBoundsExpand;	// amount to expand local bounds mins
	Vector3f					MaxsBoundsExpand;	// amount to expand local bounds maxs
	mutable Bounds3f			CullBounds;			// bounds of this object and all its children in the local space of its parent
    mutable textMetrics_t       TextMetrics;		// cached metrics for the text

	float						WrapWidth;

private:
	// only VRMenuMgrLocal static methods can construct and destruct a menu object.
								VRMenuObjectLocal( VRMenuObjectParms const & parms, menuHandle_t const handle );
	virtual						~VRMenuObjectLocal();

	// Render the specified surface.
	virtual void				RenderSurface( OvrVRMenuMgr const & menuMgr, Matrix4f const & mvp, 
                                        SubmittedMenuObject const & sub ) const;

	bool						IntersectRayBounds( Vector3f const & start, Vector3f const & dir,
										Vector3f const & mins, Vector3f const & maxs,
										ContentFlags_t const testContents, float & t0, float & t1 ) const;

	// Test the ray against this object and all child objects, returning the first object that was 
	// hit by the ray. The ray should be in parent-local space - for the current root menu this is 
	// always world space.
	virtual bool				HitTest_r( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font, Posef const & parentPose, Vector3f const & parentScale,
                                        Vector3f const & rayStart, Vector3f const & rayDir,  ContentFlags_t const testContents, 
                                        HitTestResult & result ) const;

	int							GetComponentIndex( VRMenuComponent * component ) const;
};

} // namespace OVR

#endif // OVR_VRMenuObjectLocal_h
