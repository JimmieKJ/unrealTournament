/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Stereo.h
Content     :   Stereo rendering functions
Created     :   November 30, 2013
Authors     :   Tom Fosyth

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_Stereo_h
#define OVR_Stereo_h

#include "OVR_Device.h"

namespace OVR {

//-----------------------------------------------------------------------------------
// ***** Stereo Enumerations

// StereoEye specifies which eye we are rendering for; it is used to
// retrieve StereoEyeParams.
enum StereoEye
{
    StereoEye_Center,
    StereoEye_Left,
    StereoEye_Right    
};


//-----------------------------------------------------------------------------------
// ***** Viewport

// Viewport describes a rectangular area used for rendering, in pixels.
struct Viewport
{
    int x, y;
    int w, h;

    Viewport() {}
    Viewport(int x1, int y1, int w1, int h1) : x(x1), y(y1), w(w1), h(h1) { }

    bool operator == (const Viewport& vp) const
    { return (x == vp.x) && (y == vp.y) && (w == vp.w) && (h == vp.h); }
    bool operator != (const Viewport& vp) const
    { return !operator == (vp); }
};


//-----------------------------------------------------------------------------------
// ***** FovPort

// FovPort describes Field Of View (FOV) of a viewport.
// This class has values for up, down, left and right, stored in 
// tangent of the angle units to simplify calculations.
//
// As an example, for a standard 90 degree vertical FOV, we would 
// have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
//
// CreateFromRadians/Degrees helper functions can be used to
// access FOV in different units.

struct FovPort
{
    float UpTan;
    float DownTan;
    float LeftTan;
    float RightTan;

    FovPort ( float u = 0.0f, float d = 0.0f, float l = 0.0f, float r = 0.0f ) :
        UpTan(u), DownTan(d), LeftTan(l), RightTan(r) {}

    static FovPort CreateFromRadians(float horizontalFov, float verticalFov)
    {
        FovPort result;
        result.UpTan    = tanf (   verticalFov * 0.5f );
        result.DownTan  = tanf (   verticalFov * 0.5f );
        result.LeftTan  = tanf ( horizontalFov * 0.5f );
        result.RightTan = tanf ( horizontalFov * 0.5f );
        return result;
    }

    static FovPort CreateFromDegrees(float horizontalFovDegrees,
                                     float verticalFovDegrees)
    {
        return CreateFromRadians(DegreeToRad(horizontalFovDegrees),
                                 DegreeToRad(verticalFovDegrees));
    }

    //  Get Horizontal/Vertical components of Fov in radians.
    float GetVerticalFovRadians() const     { return atanf(UpTan)    + atanf(DownTan); }
    float GetHorizontalFovRadians() const   { return atanf(LeftTan)  + atanf(RightTan); }
    //  Get Horizontal/Vertical components of Fov in degrees.
    float GetVerticalFovDegrees() const     { return RadToDegree(GetVerticalFovRadians()); }
    float GetHorizontalFovDegrees() const   { return RadToDegree(GetHorizontalFovRadians()); }

};


//-----------------------------------------------------------------------------------
// ***** ScaleAndOffset

struct ScaleAndOffset2D
{
    Vector2f Scale;
    Vector2f Offset;

    ScaleAndOffset2D(float sx = 0.0f, float sy = 0.0f, float ox = 0.0f, float oy = 0.0f)
      : Scale(sx, sy), Offset(ox, oy)        
    { }
};


//-----------------------------------------------------------------------------------
// ***** Misc. utility functions.

// Inputs are 4 points (pFitX[0],pFitY[0]) through (pFitX[3],pFitY[3])
// Result is four coefficients in pResults[0] through pResults[3] such that
//      y = pResult[0] + x * ( pResult[1] + x * ( pResult[2] + x * ( pResult[3] ) ) );
// passes through all four input points.
// Return is true if it succeeded, false if it failed (because two control points
// have the same pFitX value).
bool FitCubicPolynomial ( float *pResult, const float *pFitX, const float *pFitY );

//-----------------------------------------------------------------------------------
// ***** LensConfig

// LensConfig describes the configuration of a single lens in an HMD.
// - Eqn and K[] describe a distortion function.
// - MetersPerTanAngleAtCenter is the relationship between distance on a
//   screen (at the center of the lens), and the angle variance of the light after it
//   has passed through the lens.
// - ChromaticAberration is an array of parameters for controlling
//   additional Red and Blue scaling in order to reduce chromatic aberration
//   caused by the Rift lenses.
struct LensConfig
{
    // The result is a scaling applied to the distance from the center of the lens.
    float    DistortionFnScaleRadiusSquared (float rsq) const;
    // x,y,z components map to r,g,b scales.
    Vector3f DistortionFnScaleRadiusSquaredChroma (float rsq) const;

    // DistortionFn applies distortion to the argument.
    // Input: the distance in TanAngle/NIC space from the optical center to the input pixel.
    // Output: the resulting distance after distortion.
    float DistortionFn(float r) const
    {
        return r * DistortionFnScaleRadiusSquared ( r * r );
    }

    // DistortionFnInverse computes the inverse of the distortion function on an argument.
    float DistortionFnInverse(float r) const;

    // Also computes the inverse, but using a polynomial approximation. Warning - it's just an approximation!
    float DistortionFnInverseApprox(float r) const;
    // Sets up InvK[].
    void SetUpInverseApprox();

    // Sets a bunch of sensible defaults.
    void SetToIdentity();



    enum { MaxCoefficients = 21 };

    DistortionEqnType   Eqn;
    float               K[MaxCoefficients];
    float               MaxR;       // The highest R you're going to query for - the curve is unpredictable beyond it.

    float               MetersPerTanAngleAtCenter;

    // Additional per-channel scaling is applied after distortion:
    //  Index [0] - Red channel constant coefficient.
    //  Index [1] - Red channel r^2 coefficient.
    //  Index [2] - Blue channel constant coefficient.
    //  Index [3] - Blue channel r^2 coefficient.
    float               ChromaticAberration[4];

    float               InvK[MaxCoefficients];
    float               MaxInvR;
};


//-----------------------------------------------------------------------------------
// ***** DistortionRenderDesc

// This describes distortion for a single eye in an HMD with a display, not just the lens by itself.
struct DistortionRenderDesc
{
    // The raw lens values.
    LensConfig          Lens;

    // These map from [-1,1] across the eye being rendered into TanEyeAngle space (but still distorted)
    Vector2f            LensCenter;
    Vector2f            TanEyeAngleScale;
    // Computed from device characteristics, IPD and eye-relief.
    // (not directly used for rendering, but very useful)
    Vector2f            PixelsPerTanAngleAtCenter;
};



//-----------------------------------------------------------------------------------
// ***** HmdRenderInfo

// All the parts of the HMD info that are needed to set up the rendering system.

struct HmdRenderInfo
{
    // The start of this sturucture is intentionally very similar to HMDInfo in OVER_Device.h
    // However to reduce interdependencies, one does not simply #include the other.

    HmdTypeEnum HmdType;

    // Size of the entire screen
    Size<int>   ResolutionInPixels;
    Size<float> ScreenSizeInMeters;
    float       ScreenGapSizeInMeters;

    // Characteristics of the lenses.
    float       CenterFromTopInMeters;
    float       LensSeparationInMeters;
    float       LensDiameterInMeters;
    float       LensSurfaceToMidplateInMeters;
    EyeCupType  EyeCups;

    // Timing & shutter data. All values in seconds.
    struct ShutterInfo
    {
        HmdShutterTypeEnum  Type;
        float               VsyncToNextVsync;                // 1/framerate
        float               VsyncToFirstScanline;            // for global shutter, vsync->shutter open.
        float               FirstScanlineToLastScanline;     // for global shutter, will be zero.
        float               PixelSettleTime;                 // estimated.
        float               PixelPersistence;                // Full persistence = 1/framerate.
    }           Shutter;


    // These are all set from the user's profile.
    struct EyeConfig
    {
        // Distance from center of eyeball to front plane of lens.
        float ReliefInMeters;
        // Distance from nose (technically, center of Rift) to the middle of the eye.
        float NoseToPupilInMeters;

        // Radial distortion.
        // See declaration of LensConfig for details.
        DistortionEqnType   DistortionEqn;
        float               DistortionK[4];
        float               ChromaticAberration[4];
        float               MetersPerTanAngleAtCenter;

    } EyeLeft, EyeRight;


    HmdRenderInfo()
    {
        HmdType = HmdType_None;
        ResolutionInPixels.Width = 0;
        ResolutionInPixels.Height = 0;
        ScreenSizeInMeters.Width = 0.0f;
        ScreenSizeInMeters.Height = 0.0f;
        ScreenGapSizeInMeters = 0.0f;
        CenterFromTopInMeters = 0.0f;
        LensSeparationInMeters = 0.0f;
        LensDiameterInMeters = 0.0f;
        LensSurfaceToMidplateInMeters = 0.0f;
        Shutter.Type = HmdShutter_LAST;
        Shutter.VsyncToNextVsync = 0.0f;
        Shutter.VsyncToFirstScanline = 0.0f;
        Shutter.FirstScanlineToLastScanline = 0.0f;
        Shutter.PixelSettleTime = 0.0f;
        Shutter.PixelPersistence = 0.0f;
        EyeCups = EyeCup_BlackA;
        EyeLeft.ReliefInMeters = 0.0f;
        EyeLeft.NoseToPupilInMeters = 0.0f;
        EyeLeft.DistortionEqn = Distortion_RecipPoly4;
        memset(EyeLeft.DistortionK, 0, sizeof(EyeLeft.DistortionK));
        EyeLeft.DistortionK[0] = 1.0f;
        EyeLeft.ChromaticAberration[0] = 0.0f;
        EyeLeft.ChromaticAberration[1] = 0.0f;
        EyeLeft.ChromaticAberration[2] = 0.0f;
        EyeLeft.ChromaticAberration[3] = 0.0f;
        EyeRight = EyeLeft;
    }

};




//-----------------------------------------------------------------------------------

// Stateless computation functions, in somewhat recommended execution order.
// For examples on how to use many of them, see the StereoConfig::UpdateComputedState function.

// profile may be NULL, in which case it uses the hard-coded defaults.
// eyeCupOverride can be EyeCup_LAST, in which case it uses the one in the profile.
HmdRenderInfo       GenerateHmdRenderInfoFromHmdInfo ( HMDInfo const &hmdInfo,
                                                       Profile const *profile = NULL,
                                                       EyeCupType eyeCupOverride = EyeCup_LAST );

LensConfig          GenerateLensConfigFromEyeRelief ( float eyeReliefInMeters, HmdRenderInfo const &hmd );

DistortionRenderDesc CalculateDistortionRenderDesc ( StereoEye eyeType, HmdRenderInfo const &hmd,
                                                     LensConfig const *pLensOverride = NULL );

FovPort             CalculateFovFromEyePosition ( float eyeReliefInMeters,
                                                  float offsetToRightInMeters,
                                                  float offsetDownwardsInMeters,
                                                  float lensDiameterInMeters,
                                                  float extraEyeRotationInRadians = 0.0f);

FovPort             CalculateFovFromHmdInfo ( StereoEye eyeType,
                                              DistortionRenderDesc const &distortion,
                                              HmdRenderInfo const &hmd,
                                              float extraEyeRotationInRadians = 0.0f );

FovPort             GetPhysicalScreenFov ( StereoEye eyeType, DistortionRenderDesc const &distortion );

FovPort             ClampToPhysicalScreenFov ( StereoEye eyeType, DistortionRenderDesc const &distortion,
                                               FovPort inputFovPort );

Sizei               CalculateIdealPixelSize ( DistortionRenderDesc const &distortion,
                                              FovPort fov, float pixelsPerDisplayPixel );

Viewport            GetFramebufferViewport ( StereoEye eyeType, HmdRenderInfo const &hmd );

Matrix4f            CreateProjection ( bool rightHanded, FovPort fov,
                                       float zNear = 0.01f, float zFar = 10000.0f );

Matrix4f            CreateOrthoSubProjection ( bool rightHanded, StereoEye eyeType,
                                               float tanHalfFovX, float tanHalfFovY,
                                               float unitsX, float unitsY, float distanceFromCamera,
                                               float interpupillaryDistance, Matrix4f const &projection,
                                               float zNear = 0.0f, float zFar = 0.0f );

ScaleAndOffset2D    CreateNDCScaleAndOffsetFromProjection ( bool rightHanded, Matrix4f projection );
ScaleAndOffset2D    CreateUVScaleAndOffsetfromNDCScaleandOffset ( ScaleAndOffset2D scaleAndOffsetNDC,
                                                                  Viewport renderedViewport,
                                                                  Sizei renderTargetSize );


//-----------------------------------------------------------------------------------
// ***** StereoEyeParams

// StereoEyeParams describes RenderDevice configuration needed to render
// the scene for one eye. 
struct StereoEyeParams
{
    StereoEye               Eye;
    Matrix4f                ViewAdjust;             // Translation to be applied to view matrix.

    // Distortion and the VP on the physical display - the thing to run the distortion shader on.
    DistortionRenderDesc    Distortion;
    Viewport                DistortionViewport;

    // Projection and VP of a particular view (you could have multiple of these).
    Viewport                RenderedViewport;       // Viewport that we render the standard scene to.
    FovPort                 Fov;                    // The FOVs of this scene.
    Matrix4f                RenderedProjection;     // Projection matrix used with this eye.
    ScaleAndOffset2D        EyeToSourceNDC;         // Mapping from TanEyeAngle space to [-1,+1] on the rendered image.
    ScaleAndOffset2D        EyeToSourceUV;          // Mapping from TanEyeAngle space to actual texture UV coords.
};


//-----------------------------------------------------------------------------------
// A set of "forward-mapping" functions, mapping from framebuffer space to real-world and/or texture space.
Vector2f TransformScreenNDCToTanFovSpace ( DistortionRenderDesc const &distortion,
                                           const Vector2f &framebufferNDC );
void TransformScreenNDCToTanFovSpaceChroma ( Vector2f *resultR, Vector2f *resultG, Vector2f *resultB, 
                                             DistortionRenderDesc const &distortion,
                                             const Vector2f &framebufferNDC );
Vector2f TransformTanFovSpaceToRendertargetTexUV ( StereoEyeParams const &eyeParams,
                                                   Vector2f const &tanEyeAngle );
Vector2f TransformTanFovSpaceToRendertargetNDC ( StereoEyeParams const &eyeParams,
                                                 Vector2f const &tanEyeAngle );
Vector2f TransformScreenPixelToScreenNDC( Viewport const &distortionViewport,
                                          Vector2f const &pixel );
Vector2f TransformScreenPixelToTanFovSpace ( Viewport const &distortionViewport,
                                             DistortionRenderDesc const &distortion,
                                             Vector2f const &pixel );
Vector2f TransformScreenNDCToRendertargetTexUV( DistortionRenderDesc const &distortion,
                                                StereoEyeParams const &eyeParams,
                                                Vector2f const &pixel );
Vector2f TransformScreenPixelToRendertargetTexUV( Viewport const &distortionViewport,
                                                  DistortionRenderDesc const &distortion,
                                                  StereoEyeParams const &eyeParams,
                                                  Vector2f const &pixel );

// A set of "reverse-mapping" functions, mapping from real-world and/or texture space back to the framebuffer.
// Be aware that many of these are significantly slower than their forward-mapping counterparts.
Vector2f TransformTanFovSpaceToScreenNDC( DistortionRenderDesc const &distortion,
                                          const Vector2f &tanEyeAngle, bool usePolyApprox = false );
Vector2f TransformRendertargetNDCToTanFovSpace( StereoEyeParams const &eyeParams,
                                                Vector2f const &textureUV );


} //namespace OVR

#endif // OVR_Stereo_h
