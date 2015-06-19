/************************************************************************************

Filename    :   OVR_Stereo.cpp
Content     :   Stereo rendering functions
Created     :   November 30, 2013
Authors     :   Tom Fosyth

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_Stereo.h"
#include "OVR_Profile.h"

namespace OVR {


//-----------------------------------------------------------------------------------

// Inputs are 4 points (pFitX[0],pFitY[0]) through (pFitX[3],pFitY[3])
// Result is four coefficients in pResults[0] through pResults[3] such that
//      y = pResult[0] + x * ( pResult[1] + x * ( pResult[2] + x * ( pResult[3] ) ) );
// passes through all four input points.
// Return is true if it succeeded, false if it failed (because two control points
// have the same pFitX value).
bool FitCubicPolynomial ( float *pResult, const float *pFitX, const float *pFitY )
{
    float d0 = ( ( pFitX[0]-pFitX[1] ) * ( pFitX[0]-pFitX[2] ) * ( pFitX[0]-pFitX[3] ) );
    float d1 = ( ( pFitX[1]-pFitX[2] ) * ( pFitX[1]-pFitX[3] ) * ( pFitX[1]-pFitX[0] ) );
    float d2 = ( ( pFitX[2]-pFitX[3] ) * ( pFitX[2]-pFitX[0] ) * ( pFitX[2]-pFitX[1] ) );
    float d3 = ( ( pFitX[3]-pFitX[0] ) * ( pFitX[3]-pFitX[1] ) * ( pFitX[3]-pFitX[2] ) );

    if ( ( d0 == 0.0f ) || ( d1 == 0.0f ) || ( d2 == 0.0f ) || ( d3 == 0.0f ) )
    {
        return false;
    }

    float f0 = pFitY[0] / d0;
    float f1 = pFitY[1] / d1;
    float f2 = pFitY[2] / d2;
    float f3 = pFitY[3] / d3;

    pResult[0] = -( f0*pFitX[1]*pFitX[2]*pFitX[3]
                  + f1*pFitX[0]*pFitX[2]*pFitX[3]
                  + f2*pFitX[0]*pFitX[1]*pFitX[3]
                  + f3*pFitX[0]*pFitX[1]*pFitX[2] );
    pResult[1] = f0*(pFitX[1]*pFitX[2] + pFitX[2]*pFitX[3] + pFitX[3]*pFitX[1])
               + f1*(pFitX[0]*pFitX[2] + pFitX[2]*pFitX[3] + pFitX[3]*pFitX[0])
               + f2*(pFitX[0]*pFitX[1] + pFitX[1]*pFitX[3] + pFitX[3]*pFitX[0])
               + f3*(pFitX[0]*pFitX[1] + pFitX[1]*pFitX[2] + pFitX[2]*pFitX[0]);
    pResult[2] = -( f0*(pFitX[1]+pFitX[2]+pFitX[3])
                  + f1*(pFitX[0]+pFitX[2]+pFitX[3])
                  + f2*(pFitX[0]+pFitX[1]+pFitX[3])
                  + f3*(pFitX[0]+pFitX[1]+pFitX[2]) );
    pResult[3] = f0 + f1 + f2 + f3;

    return true;
}



float EvalCatmullRomSpline ( float const *K, float scaledVal, int NumSegments )
{
    //int const NumSegments = LensConfig::NumCoefficients;

    float scaledValFloor = floorf ( scaledVal );
    scaledValFloor = Alg::Max ( 0.0f, Alg::Min ( (float)(NumSegments-1), scaledValFloor ) );
    float t = scaledVal - scaledValFloor;
    int k = (int)scaledValFloor;

    float p0 = 0.0f;
    float p1 = 0.0f;
    float m0 = 0.0f;
    float m1 = 0.0f;

    if (k == 0)
    {   // Curve starts at 1.0 with gradient K[1]-K[0]
        p0 = 1.0f;
        m0 = K[1] - K[0];
        p1 = K[1];
        m1 = 0.5f * ( K[2] - K[0] );
    }
    else if (k < NumSegments-2)
    {   // General case
        p0 = K[k];
        m0 = 0.5f * ( K[k+1] - K[k-1] );
        p1 = K[k+1];
        m1 = 0.5f * ( K[k+2] - K[k] );
    }
    else if (k == NumSegments-2)
    {   // Last tangent is just the slope of the last two points.
        p0 = K[k];
        m0 = 0.5f * ( K[k+1] - K[k-1] );
        p1 = K[k+1];
        m1 = K[k+1] - K[k];
    }
    else if (k == NumSegments-1)
    {   // Beyond the last segment it's just a straight line
        p0 = K[k];
        m0 = K[k] - K[k-1];
        p1 = p0 + m0;
        m1 = m0;
    }

    float omt = 1.0f - t;
    float res  = ( p0 * ( 1.0f + 2.0f *   t ) + m0 *   t ) * omt * omt
               + ( p1 * ( 1.0f + 2.0f * omt ) - m1 * omt ) *   t *   t;

    return res;
}

//-----------------------------------------------------------------------------------


// The result is a scaling applied to the distance.
float LensConfig::DistortionFnScaleRadiusSquared (float rsq) const
{
    float scale = 1.0f;
    switch ( Eqn )
    {
    case Distortion_Poly4:
        // This version is deprecated! Prefer one of the other two.
        scale = ( K[0] + rsq * ( K[1] + rsq * ( K[2] + rsq * K[3] ) ) );
        break;
    case Distortion_RecipPoly4:
        scale = 1.0f / ( K[0] + rsq * ( K[1] + rsq * ( K[2] + rsq * K[3] ) ) );
        break;
    case Distortion_CatmullRom10: {
        // A Catmull-Rom spline through the values 1.0, K[1], K[2] ... K[10]
        // evenly spaced in R^2 from 0.0 to MaxR^2
        // K[0] controls the slope at radius=0.0, rather than the actual value.
        const int NumSegments = 11;
        OVR_ASSERT ( NumSegments <= MaxCoefficients );
        float scaledRsq = (float)(NumSegments-1) * rsq / ( MaxR * MaxR );
        scale = EvalCatmullRomSpline ( K, scaledRsq, NumSegments );
        } break;

    case Distortion_CatmullRom20: {
		// A Catmull-Rom spline through the values 1.0, K[1], K[2] ... K[20]
		// evenly spaced in R^2 from 0.0 to MaxR^2
		// K[0] controls the slope at radius=0.0, rather than the actual value.
		const int NumSegments = 21;
		OVR_ASSERT ( NumSegments <= MaxCoefficients );
		float scaledRsq = (float)(NumSegments-1) * rsq / ( MaxR * MaxR );
		scale = EvalCatmullRomSpline ( K, scaledRsq, NumSegments );
        } break;

    default:
        OVR_ASSERT ( false );
        break;
    }
    return scale;
}

// x,y,z components map to r,g,b
Vector3f LensConfig::DistortionFnScaleRadiusSquaredChroma (float rsq) const
{
    float scale = DistortionFnScaleRadiusSquared ( rsq );
    Vector3f scaleRGB;
    scaleRGB.x = scale * ( 1.0f + ChromaticAberration[0] + rsq * ChromaticAberration[1] );     // Red
    scaleRGB.y = scale;                                                                        // Green
    scaleRGB.z = scale * ( 1.0f + ChromaticAberration[2] + rsq * ChromaticAberration[3] );     // Blue
    return scaleRGB;
}

// DistortionFnInverse computes the inverse of the distortion function on an argument.
float LensConfig::DistortionFnInverse(float r) const
{    
    OVR_ASSERT((r <= 20.0f));

    float s, d;
    float delta = r * 0.25f;

    // Better to start guessing too low & take longer to converge than too high
    // and hit singularities. Empirically, r * 0.5f is too high in some cases.
    s = r * 0.25f;
    d = fabs(r - DistortionFn(s));

    for (int i = 0; i < 20; i++)
    {
        float sUp   = s + delta;
        float sDown = s - delta;
        float dUp   = fabs(r - DistortionFn(sUp));
        float dDown = fabs(r - DistortionFn(sDown));

        if (dUp < d)
        {
            s = sUp;
            d = dUp;
        }
        else if (dDown < d)
        {
            s = sDown;
            d = dDown;
        }
        else
        {
            delta *= 0.5f;
        }
    }

    return s;
}



float LensConfig::DistortionFnInverseApprox(float r) const
{
    float rsq = r * r;
    float scale = 1.0f;
    switch ( Eqn )
    {
    case Distortion_Poly4:
        // Deprecated
        OVR_ASSERT ( false );
        break;
    case Distortion_RecipPoly4:
        scale = 1.0f / ( InvK[0] + rsq * ( InvK[1] + rsq * ( InvK[2] + rsq * InvK[3] ) ) );
        break;
    case Distortion_CatmullRom10: {
    	// A Catmull-Rom spline through the values 1.0, K[1], K[2] ... K[10]
        // evenly spaced in R^2 from 0.0 to MaxR^2
        // K[0] controls the slope at radius=0.0, rather than the actual value.
        const int NumSegments = 11;
        OVR_ASSERT ( NumSegments <= MaxCoefficients );
        float scaledRsq = (float)(NumSegments-1) * rsq / ( MaxInvR * MaxInvR );
        scale = EvalCatmullRomSpline ( InvK, scaledRsq, NumSegments );
        } break;

    case Distortion_CatmullRom20: {
            // A Catmull-Rom spline through the values 1.0, K[1], K[2] ... K[20]
            // evenly spaced in R^2 from 0.0 to MaxR^2
            // K[0] controls the slope at radius=0.0, rather than the actual value.
            const int NumSegments = 21;
            OVR_ASSERT ( NumSegments <= MaxCoefficients );
            float scaledRsq = (float)(NumSegments-1) * rsq / ( MaxInvR * MaxInvR );
            scale = EvalCatmullRomSpline ( InvK, scaledRsq, NumSegments );
            break;
        } break;

    default:
        OVR_ASSERT ( false );
        break;
    }
    return r * scale;
}

void LensConfig::SetUpInverseApprox()
{
    float maxR = MaxInvR;

    switch ( Eqn )
    {
    default:
    case Distortion_Poly4:
        // Deprecated
        OVR_ASSERT ( false );
        break;
    case Distortion_RecipPoly4:{

        float sampleR[4];
        float sampleRSq[4];
        float sampleInv[4];
        float sampleFit[4];

        // Found heuristically...
        sampleR[0] = 0.0f;
        sampleR[1] = maxR * 0.4f;
        sampleR[2] = maxR * 0.8f;
        sampleR[3] = maxR * 1.5f;
        for ( int i = 0; i < 4; i++ )
        {
            sampleRSq[i] = sampleR[i] * sampleR[i];
            sampleInv[i] = DistortionFnInverse ( sampleR[i] );
            sampleFit[i] = sampleR[i] / sampleInv[i];
        }
        sampleFit[0] = 1.0f;
        FitCubicPolynomial ( InvK, sampleRSq, sampleFit );

    #if 0
        // Should be a nearly exact match on the chosen points.
        OVR_ASSERT ( fabs ( DistortionFnInverse ( sampleR[0] ) - DistortionFnInverseApprox ( sampleR[0] ) ) / maxR < 0.0001f );
        OVR_ASSERT ( fabs ( DistortionFnInverse ( sampleR[1] ) - DistortionFnInverseApprox ( sampleR[1] ) ) / maxR < 0.0001f );
        OVR_ASSERT ( fabs ( DistortionFnInverse ( sampleR[2] ) - DistortionFnInverseApprox ( sampleR[2] ) ) / maxR < 0.0001f );
        OVR_ASSERT ( fabs ( DistortionFnInverse ( sampleR[3] ) - DistortionFnInverseApprox ( sampleR[3] ) ) / maxR < 0.0001f );
        // Should be a decent match on the rest of the range.
        const int maxCheck = 20;
        for ( int i = 0; i < maxCheck; i++ )
        {
            float checkR = (float)i * maxR / (float)maxCheck;
            float realInv = DistortionFnInverse       ( checkR );
            float testInv = DistortionFnInverseApprox ( checkR );
            float error = fabsf ( realInv - testInv ) / maxR;
            OVR_ASSERT ( error < 0.1f );
        }
    #endif

        }break;
    case Distortion_CatmullRom10:{

        const int NumSegments = 11;
        OVR_ASSERT ( NumSegments <= MaxCoefficients );
        for ( int i = 1; i < NumSegments; i++ )
        {
            float scaledRsq = (float)i;
            float rsq = scaledRsq * MaxInvR * MaxInvR / (float)( NumSegments - 1);
            float r = sqrtf ( rsq );
            float inv = DistortionFnInverse ( r );
            InvK[i] = inv / r;
            InvK[0] = 1.0f;     // TODO: fix this.
        }

#if 0
        const int maxCheck = 20;
        for ( int i = 0; i <= maxCheck; i++ )
        {
            float checkR = (float)i * MaxInvR / (float)maxCheck;
            float realInv = DistortionFnInverse       ( checkR );
            float testInv = DistortionFnInverseApprox ( checkR );
            float error = fabsf ( realInv - testInv ) / MaxR;
            OVR_ASSERT ( error < 0.01f );
        }
#endif

        }break;

    case Distortion_CatmullRom20:{

		const int NumSegments = 21;
		OVR_ASSERT ( NumSegments <= MaxCoefficients );
		for ( int i = 1; i < NumSegments; i++ )
		{
			float scaledRsq = (float)i;
			float rsq = scaledRsq * MaxInvR * MaxInvR / (float)( NumSegments - 1);
			float r = sqrtf ( rsq );
			float inv = DistortionFnInverse ( r );
			InvK[i] = inv / r;
			InvK[0] = 1.0f;     // TODO: fix this.
		}

		}break;
    }
}


void LensConfig::SetToIdentity()
{
    for ( int i = 0; i < MaxCoefficients; i++ )
    {
        K[i] = 0.0f;
        InvK[i] = 0.0f;
    }
    Eqn = Distortion_RecipPoly4;
    K[0] = 1.0f;
    InvK[0] = 1.0f;
    MaxR = 1.0f;
    MaxInvR = 1.0f;
    ChromaticAberration[0] = 0.0f;
    ChromaticAberration[1] = 0.0f;
    ChromaticAberration[2] = 0.0f;
    ChromaticAberration[3] = 0.0f;
    MetersPerTanAngleAtCenter = 0.05f;
}

//-----------------------------------------------------------------------------------


struct EyePos
{
    float DistanceToCenterInMeters;
    float OffsetToRightInMeters;
};


EyePos CalculateEyePositonFromPixelBoundaries ( DistortionRenderDesc const &distortion, Viewport const &distortionViewport, float pixelEdgeLeft, float pixelEdgeRight, float lensDiameter )
{
    // pixelEdgeLeft and pixelEdgeRight are actual on-screen pixel counts for lines that the user can
    // ONLY JUST see, i.e. their sight lines exactly hit the edges of the lens.
    // Note that because the eye is rotated towards the lines when the user is calibrating, this geometry
    // actually finds the center of rotation of the eyeball, not the position of the pupil.

    float pixelHeightMiddle = (float)distortionViewport.y + (float)distortionViewport.h * 0.5f;
    Vector2f edgeTanFovLeft  = TransformScreenPixelToTanFovSpace ( distortionViewport, distortion, Vector2f ( pixelEdgeLeft,  pixelHeightMiddle ) );
    Vector2f edgeTanFovRight = TransformScreenPixelToTanFovSpace ( distortionViewport, distortion, Vector2f ( pixelEdgeRight, pixelHeightMiddle ) );

    EyePos retVal;
    // |==L1====L2=====|   <--- lens surface
    //  \    |       _/
    //   \   R     _/
    //    \  |   _/
    //     \t|p_/
    //      \|/
    //       O  <--- eyeball
    //
    // So we have angles theta and phi, or rather their tangents.
    // R = eye relief
    // L1+L1 = lens_diameter
    // tan(theta) = L1/R
    // tan(phi)   = L2/R
    // => tan(theta)+tan(phi) = L1+L2/R
    // => R = lens_diameter/(tan(theta)+tan(phi))
    retVal.DistanceToCenterInMeters = lensDiameter / ( -edgeTanFovLeft.x + edgeTanFovRight.x );
    // L2 = tan(phi) * R
    // retVal.OffsetToRightInMeters = lensDiameter * 0.5 - L2
    //                              = lensDiameter * 0.5 - tan(phi) * (lens_diameter/(tan(theta)+tan(phi)))
    //                              = lensDiameter * ( 0.5 - tan(phi) / (tan(theta)+tan(phi)) )
    retVal.OffsetToRightInMeters = lensDiameter * ( 0.5f - ( edgeTanFovRight.x / ( -edgeTanFovLeft.x + edgeTanFovRight.x ) ) );
    return retVal;
}



// profile may be NULL, in which case it uses the hard-coded defaults.
HmdRenderInfo GenerateHmdRenderInfoFromHmdInfo ( HMDInfo const &hmdInfo, 
                                                 Profile const *profile /*=NULL*/,
                                                 EyeCupType eyeCupOverride /*= EyeCup_LAST*/ )
{
    HmdRenderInfo renderInfo;

    renderInfo.HmdType                              = hmdInfo.HmdType;
    renderInfo.ResolutionInPixels                   = hmdInfo.ResolutionInPixels;
    renderInfo.ScreenSizeInMeters                   = hmdInfo.ScreenSizeInMeters;
    renderInfo.CenterFromTopInMeters                = hmdInfo.CenterFromTopInMeters;
    renderInfo.ScreenGapSizeInMeters                = hmdInfo.ScreenGapSizeInMeters;
    renderInfo.LensSeparationInMeters               = hmdInfo.LensSeparationInMeters;

    OVR_ASSERT ( sizeof(renderInfo.Shutter) == sizeof(hmdInfo.Shutter) );   // Try to keep the files in sync!
    renderInfo.Shutter.Type                         = hmdInfo.Shutter.Type;
    renderInfo.Shutter.VsyncToNextVsync             = hmdInfo.Shutter.VsyncToNextVsync;
    renderInfo.Shutter.VsyncToFirstScanline         = hmdInfo.Shutter.VsyncToFirstScanline;
    renderInfo.Shutter.FirstScanlineToLastScanline  = hmdInfo.Shutter.FirstScanlineToLastScanline;
    renderInfo.Shutter.PixelSettleTime              = hmdInfo.Shutter.PixelSettleTime;
    renderInfo.Shutter.PixelPersistence             = hmdInfo.Shutter.PixelPersistence;

    renderInfo.LensDiameterInMeters                 = 0.035f;
    renderInfo.LensSurfaceToMidplateInMeters        = 0.025f;
    renderInfo.EyeCups                              = EyeCup_BlackA;

#if 0       // Device settings are out of date - don't use them.
    if (Contents & Contents_Distortion)
    {
        memcpy(renderInfo.DistortionK, DistortionK, sizeof(float)*4);
        renderInfo.DistortionEqn = Distortion_RecipPoly4;
    }
#endif

    // Defaults in case of no user profile.
    renderInfo.EyeLeft.NoseToPupilInMeters   = 0.032f;
    renderInfo.EyeLeft.ReliefInMeters        = 0.012f;

    // 10mm eye-relief laser numbers for DK1 lenses.
    // These are a decent seed for finding eye-relief and IPD.
    // These are NOT used for rendering!
    // Rendering distortions are now in GenerateLensConfigFromEyeRelief()
    // So, if you're hacking in new distortions, don't do it here!
    renderInfo.EyeLeft.MetersPerTanAngleAtCenter = 0.0449f;
	renderInfo.EyeLeft.DistortionEqn       = Distortion_RecipPoly4;
	renderInfo.EyeLeft.DistortionK[0]      =  1.0f;
	renderInfo.EyeLeft.DistortionK[1]      = -0.494165344f;
	renderInfo.EyeLeft.DistortionK[2]      = 0.587046423f;
	renderInfo.EyeLeft.DistortionK[3]      = -0.841887126f;

	renderInfo.EyeLeft.ChromaticAberration[0] = -0.006f;
	renderInfo.EyeLeft.ChromaticAberration[1] =  0.0f;
	renderInfo.EyeLeft.ChromaticAberration[2] =  0.014f;
	renderInfo.EyeLeft.ChromaticAberration[3] =  0.0f;

    renderInfo.EyeRight = renderInfo.EyeLeft;


    // Obtain data from profile.
    HMDProfile* hmd_profile = NULL;
    if ( profile != NULL )
    {
        float ipd = profile->GetIPD();
        renderInfo.EyeLeft.NoseToPupilInMeters = 0.5f * ipd;
        renderInfo.EyeRight.NoseToPupilInMeters = 0.5f * ipd;

        hmd_profile = (HMDProfile*)profile;

        renderInfo.EyeCups = hmd_profile->GetEyeCup();
    }

    switch ( hmdInfo.HmdType )
    {
    case HmdType_None:
    case HmdType_DKProto:
    case HmdType_DK1:
        // Slight hack to improve usability.
        // If you have a DKHD-style lens profile enabled,
        // but you plug in DK1 and forget to change the profile,
        // obviously you don't want those lens numbers.
        if ( ( renderInfo.EyeCups != EyeCup_BlackA ) &&
             ( renderInfo.EyeCups != EyeCup_BlackB ) &&
             ( renderInfo.EyeCups != EyeCup_BlackC ) )
        {
            renderInfo.EyeCups = EyeCup_BlackA;
        }
        break;
    default:
        break;
    }

    if ( eyeCupOverride != EyeCup_LAST )
    {
        renderInfo.EyeCups = eyeCupOverride;
    }



    switch ( renderInfo.EyeCups )
    {
    case EyeCup_BlackA:
    case EyeCup_BlackB:
    case EyeCup_BlackC:
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.035f;
        renderInfo.LensSurfaceToMidplateInMeters          = 0.02357f;
        // Not strictly lens-specific, but still wise to set a reasonable default for relief.
        renderInfo.EyeLeft.ReliefInMeters                 = 0.008f;
        renderInfo.EyeRight.ReliefInMeters                = 0.008f;
        break;
    case EyeCup_OrangeA:
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.03955f;
        renderInfo.LensSurfaceToMidplateInMeters          = 0.01485f;
        // Not strictly lens-specific, but still wise to set a reasonable default for relief.
        renderInfo.EyeLeft.ReliefInMeters                 = 0.012f;
        renderInfo.EyeRight.ReliefInMeters                = 0.012f;
        break;
    case EyeCup_RedA:
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.04f;      // approximate
        renderInfo.LensSurfaceToMidplateInMeters          = 0.01725f;
        // Not strictly lens-specific, but still wise to set a reasonable default for relief.
        renderInfo.EyeLeft.ReliefInMeters                 = 0.012f;
        renderInfo.EyeRight.ReliefInMeters                = 0.012f;
        break;
    case EyeCup_BlueA:
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.04f;      // approximate
        renderInfo.LensSurfaceToMidplateInMeters          = 0.02294f;
        // Not strictly lens-specific, but still wise to set a reasonable default for relief.
        renderInfo.EyeLeft.ReliefInMeters                 = 0.012f;
        renderInfo.EyeRight.ReliefInMeters                = 0.012f;
        break;
    case EyeCup_DelilahA:
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.04f;      // approximate
        renderInfo.LensSurfaceToMidplateInMeters          = 0.00895f;
        // Not strictly lens-specific, but still wise to set a reasonable default for relief.
        renderInfo.EyeLeft.ReliefInMeters                 = 0.012f;
        renderInfo.EyeRight.ReliefInMeters                = 0.012f;
        break;
    case EyeCup_JamesA:
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.04f;      // approximate
        renderInfo.LensSurfaceToMidplateInMeters          = 0.02635f;
        // Not strictly lens-specific, but still wise to set a reasonable default for relief.
        renderInfo.EyeLeft.ReliefInMeters                 = 0.012f;
        renderInfo.EyeRight.ReliefInMeters                = 0.012f;
        break;
    case EyeCup_SunMandalaA:
        // Not yet measured - use some sane defaults.
        renderInfo.LensSeparationInMeters                 = 0.0635f;
        renderInfo.LensDiameterInMeters                   = 0.04f;
        renderInfo.LensSurfaceToMidplateInMeters          = 0.015f;
        renderInfo.EyeLeft.ReliefInMeters                 = 0.014f;
        renderInfo.EyeRight.ReliefInMeters                = 0.014f;
        break;
    default: OVR_ASSERT ( false ); break;
    }

    if ( hmd_profile != NULL )
    {
        int eyeLEdgeL = hmd_profile->GetLL();
        int eyeLEdgeR = hmd_profile->GetLR();
        int eyeREdgeL = hmd_profile->GetRL();
        int eyeREdgeR = hmd_profile->GetRR();
        if (eyeLEdgeR != 0 && eyeREdgeL != 0 && eyeREdgeR != 0)
        {
            // The user HAS calibrated for this device, so we can do a bit better...

            // TODO: use EyeCup type to set up faceplate->pupil distance so we can place the virtual eyes correctly.
            for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
            {
                StereoEye eyeType = ( eyeNum == 0 ) ? StereoEye_Left : StereoEye_Right;
                int edgeLeft = eyeLEdgeL;
                int edgeRight = eyeLEdgeR;
                if ( eyeType == StereoEye_Right )
                {
                    edgeLeft  = eyeREdgeL - ( renderInfo.ResolutionInPixels.Width / 2 );
                    edgeRight = eyeREdgeR - ( renderInfo.ResolutionInPixels.Width / 2 );
                }

                // First time round, distortion is taken from the defaults set above.
                LensConfig *pDistortionOverride = NULL;
                LensConfig distortionOverride;
                float offsetToRightInMeters = 0.0f;
                float reliefInMeters = 0.0f;
                float firstReliefInMeters = 0.0f;
                float firstOffsetToRightInMeters = 0.0f;
                for ( int iteration = 0; iteration < 10; iteration++ )
                {
                    DistortionRenderDesc distortion = CalculateDistortionRenderDesc ( eyeType, renderInfo, pDistortionOverride );
                    Viewport distortionViewport ( 0, 0, renderInfo.ResolutionInPixels.Width / 2, renderInfo.ResolutionInPixels.Height );
                    EyePos eyePos = CalculateEyePositonFromPixelBoundaries ( distortion, distortionViewport, (float)edgeLeft, (float)edgeRight, renderInfo.LensDiameterInMeters );
                    // But that finds the center of the eyeball. What we want is the eye relief - the distance from Rift lens to human pupil.
                    // Fortunately, human eyes have a very consistent size - 24mm - varying by only a millimeter.
                    // Additionally, the optical center of the eyeball is roughly at the middle of the pupil.
                    // Although this is physically right on the curve of the eye (and then the cornea bulges over it),
                    // optically it is about 1mm back from its pysical location. Thus, the optical center of the eyeball
                    // is about 11mm in front of the center.
                    float const centerOfEyeballToOpticalCenter = 0.011f;
                    offsetToRightInMeters = eyePos.OffsetToRightInMeters;
                    reliefInMeters        = eyePos.DistanceToCenterInMeters - centerOfEyeballToOpticalCenter;
                    if ( iteration == 0 )
                    {
                        firstReliefInMeters = reliefInMeters;
                        firstOffsetToRightInMeters = offsetToRightInMeters;
                    }
                    // From this new relief, calculate a new distortion.
                    distortionOverride = GenerateLensConfigFromEyeRelief ( reliefInMeters, renderInfo );
                    // Now use this new setting, and find the eye reliefs again.
                    pDistortionOverride = &distortionOverride;
                }

                if ( ( fabsf ( firstReliefInMeters - reliefInMeters ) > 0.005f ) || ( reliefInMeters < 0.002f ) || ( reliefInMeters > 0.05f ) )
                {
                    // Something went wrong, and it's not converging, or it's not a sensible result.
                    OVR_ASSERT ( !"Eye-relief not converging - going with first guess" );
                    reliefInMeters = firstReliefInMeters;
                    offsetToRightInMeters = firstOffsetToRightInMeters;
                }


                if ( eyeType == StereoEye_Left )
                {
                    renderInfo.EyeLeft.NoseToPupilInMeters   = renderInfo.LensSeparationInMeters * 0.5f - offsetToRightInMeters;
                    renderInfo.EyeLeft.ReliefInMeters        = reliefInMeters;
                }
                else
                {
                    renderInfo.EyeRight.NoseToPupilInMeters  = renderInfo.LensSeparationInMeters * 0.5f + offsetToRightInMeters;
                    renderInfo.EyeRight.ReliefInMeters       = reliefInMeters;
                }


            }
        }
    }

    // Now we know where the eyes are relative to the lenses, we can compute a distortion for each.
    // Currently we only use the eye relief, and since we only have one distortion mapping, we average the two.
    // TODO: incorporate lateral offset in distortion generation.
    // TODO: we used a distortion to calculate eye-relief, and now we're making a distortion from that eye-relief. Close the loop!
    for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
    {
        HmdRenderInfo::EyeConfig *pHmdEyeConfig = ( eyeNum == 0 ) ? &(renderInfo.EyeLeft) : &(renderInfo.EyeRight);
        LensConfig distortionConfig = GenerateLensConfigFromEyeRelief ( pHmdEyeConfig->ReliefInMeters, renderInfo );
        pHmdEyeConfig->MetersPerTanAngleAtCenter = distortionConfig.MetersPerTanAngleAtCenter;
        pHmdEyeConfig->DistortionEqn          = distortionConfig.Eqn;
        pHmdEyeConfig->DistortionK[0]         = distortionConfig.K[0];
        pHmdEyeConfig->DistortionK[1]         = distortionConfig.K[1];
        pHmdEyeConfig->DistortionK[2]         = distortionConfig.K[2];
        pHmdEyeConfig->DistortionK[3]         = distortionConfig.K[3];
        pHmdEyeConfig->ChromaticAberration[0] = distortionConfig.ChromaticAberration[0];
        pHmdEyeConfig->ChromaticAberration[1] = distortionConfig.ChromaticAberration[1];
        pHmdEyeConfig->ChromaticAberration[2] = distortionConfig.ChromaticAberration[2];
        pHmdEyeConfig->ChromaticAberration[3] = distortionConfig.ChromaticAberration[3];
    }

    return renderInfo;
}


LensConfig GenerateLensConfigFromEyeRelief ( float eyeReliefInMeters, HmdRenderInfo const &hmd )
{
    struct DistortionDescriptor
    {
        float EyeRelief;
        // The three places we're going to sample & lerp the curve at.
        // One sample is always at 0.0, and the distortion scale should be 1.0 or else!
        float SampleRadius[3];
        // Where the distortion has actually been measured/calibrated out to.
        // Don't try to hallucinate data out beyond here.
        float MaxRadius;
        // The config itself.
        LensConfig Config;
    };

    DistortionDescriptor distortions[10];
    int numDistortions = 0;

    if ( ( hmd.EyeCups == EyeCup_BlackA ) ||
         ( hmd.EyeCups == EyeCup_BlackB ) ||
         ( hmd.EyeCups == EyeCup_BlackC ) )
    {

#if 1
        // Brant numbers, 2013-12-05  "DK1 Partially Edge Corrected"
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0425f;
        distortions[numDistortions].Config.K[1]                          = -0.2965f;
        distortions[numDistortions].Config.K[2]                          = -0.16f;
        distortions[numDistortions].Config.K[3]                          = -0.14f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.512249443f;
        distortions[numDistortions].SampleRadius[2]                      = 0.712694878f;
        distortions[numDistortions].MaxRadius                            = 0.812917595f;
        numDistortions++;

        distortions[numDistortions] = distortions[0];
        distortions[numDistortions].EyeRelief = 0.020f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.006f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.014f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }


#elif 0
        // Brant numbers, 2013-12-05  "DK1 flat"
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0425f;
        distortions[numDistortions].Config.K[1]                          = -0.2965f;
        distortions[numDistortions].Config.K[2]                          = -0.145f;
        distortions[numDistortions].Config.K[3]                          = -0.09f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.512249443f;
        distortions[numDistortions].SampleRadius[2]                      = 0.712694878f;
        distortions[numDistortions].MaxRadius                            = 0.812917595f;
        numDistortions++;

        distortions[numDistortions] = distortions[0];
        distortions[numDistortions].EyeRelief = 0.020f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.006f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.014f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }

#elif 0
        // Brant numbers, 2013-11-12
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.042f;
        distortions[numDistortions].Config.K[1]                          = -0.49f;
        distortions[numDistortions].Config.K[2]                          = 0.42f;
        distortions[numDistortions].Config.K[3]                          = -0.78f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.512249443f;
        distortions[numDistortions].SampleRadius[2]                      = 0.712694878f;
        distortions[numDistortions].MaxRadius                            = 0.812917595f;
        numDistortions++;

        distortions[numDistortions] = distortions[0];
        distortions[numDistortions].EyeRelief = 0.020f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.006f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.014f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }

#elif 0
        // Brant numbers, 2013-11-06
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0425f;
        distortions[numDistortions].Config.K[1]                          = -0.4246f;
        distortions[numDistortions].Config.K[2]                          = 0.2f;
        distortions[numDistortions].Config.K[3]                          = -0.43f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.512249443f;
        distortions[numDistortions].SampleRadius[2]                      = 0.712694878f;
        distortions[numDistortions].MaxRadius                            = 0.812917595f;
        numDistortions++;

        distortions[numDistortions] = distortions[0];
        distortions[numDistortions].EyeRelief = 0.020f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.006f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.014f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }

#elif 0

        // DK1
        // Use our tried-and-tested single set of numbers, splatting them all over the eye-relief range so the "interpolation" does nothing.
        distortions[0].EyeRelief = 0.005f;
        distortions[0].Config.MetersPerTanAngleAtCenter = 0.043875f;
        distortions[0].Config.Eqn = Distortion_RecipPoly4;
        distortions[0].Config.K[0] = 1.0f;
        distortions[0].Config.K[1] = -0.3999f;
        distortions[0].Config.K[2] =  0.2408f;
        distortions[0].Config.K[3] = -0.4589f;
        distortions[0].Config.ChromaticAberration[0] = -0.006f;
        distortions[0].Config.ChromaticAberration[1] =  0.0f;
        distortions[0].Config.ChromaticAberration[2] =  0.014f;
        distortions[0].Config.ChromaticAberration[3] =  0.0f;
        distortions[0].SampleRadius[0] = 0.2f;
        distortions[0].SampleRadius[1] = 0.4f;
        distortions[0].SampleRadius[2] = 0.6f;
        distortions[0].MaxRadius = 0.8f;

        distortions[1] = distortions[0];
        distortions[1].EyeRelief = 0.010f;

        numDistortions = 2;

#else
        // DK1 lens laser numbers, attempt 4 (2013-31-10)
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.005f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0449f;
        distortions[numDistortions].Config.K[1]                          = -0.457111396f;
        distortions[numDistortions].Config.K[2]                          = 0.501588058f;
        distortions[numDistortions].Config.K[3]                          = -0.524455056f;
        distortions[numDistortions].SampleRadius[0]                      = 0.334075724f;
        distortions[numDistortions].SampleRadius[1]                      = 0.556792873f;
        distortions[numDistortions].SampleRadius[2]                      = 0.779510022f;
        distortions[numDistortions].MaxRadius                            = 0.857461024f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0449f;
        distortions[numDistortions].Config.K[1]                          = -0.494165344f;
        distortions[numDistortions].Config.K[2]                          = 0.587046423f;
        distortions[numDistortions].Config.K[3]                          = -0.841887126f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.512249443f;
        distortions[numDistortions].SampleRadius[2]                      = 0.712694878f;
        distortions[numDistortions].MaxRadius                            = 0.812917595f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.015f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0449f;
        distortions[numDistortions].Config.K[1]                          = -0.479541917f;
        distortions[numDistortions].Config.K[2]                          = 0.283471421f;
        distortions[numDistortions].Config.K[3]                          = -0.665178736f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.512249443f;
        distortions[numDistortions].SampleRadius[2]                      = 0.712694878f;
        distortions[numDistortions].MaxRadius                            = 0.775055679f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.020f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0449f;
        distortions[numDistortions].Config.K[1]                          = -0.622750763f;
        distortions[numDistortions].Config.K[2]                          = 1.59574614f;
        distortions[numDistortions].Config.K[3]                          = -3.867592287f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.445434298f;
        distortions[numDistortions].SampleRadius[2]                      = 0.534521158f;
        distortions[numDistortions].MaxRadius                            = 0.556792873f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.025f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0449f;
        distortions[numDistortions].Config.K[1]                          = -0.569929784f;
        distortions[numDistortions].Config.K[2]                          = 0.443343798f;
        distortions[numDistortions].Config.K[3]                          = -2.103056792f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222717149f;
        distortions[numDistortions].SampleRadius[1]                      = 0.378619154f;
        distortions[numDistortions].SampleRadius[2]                      = 0.456570156f;
        distortions[numDistortions].MaxRadius                            = 0.489977728f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.030f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0449f;
        distortions[numDistortions].Config.K[1]                          = -0.684349862f;
        distortions[numDistortions].Config.K[2]                          = 0.915836408f;
        distortions[numDistortions].Config.K[3]                          = -6.914391511f;
        distortions[numDistortions].SampleRadius[0]                      = 0.178173719f;
        distortions[numDistortions].SampleRadius[1]                      = 0.334075724f;
        distortions[numDistortions].SampleRadius[2]                      = 0.400890869f;
        distortions[numDistortions].MaxRadius                            = 0.423162584f;
        numDistortions++;

        // THE MOST HORRIBLE KLUDGE IN THE WORLD.
        // The laser says scale of 0.0449f, but we visually like a scale of 0.0425f, so...
        float bodgeFactor = 0.0425f / 0.0449f;
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.MetersPerTanAngleAtCenter *= bodgeFactor;
        }

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.006f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.014f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }
#endif

    }
    else if ( hmd.EyeCups == EyeCup_OrangeA )
    {
#if 0
        // Brant numbers, 2013-11-12
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0330f;
        distortions[numDistortions].Config.K[1]                          = -0.2848f;
        distortions[numDistortions].Config.K[2]                          = 0.0364f;
        distortions[numDistortions].Config.K[3]                          = -0.2627f;
        distortions[numDistortions].SampleRadius[0]                      = 0.289017341f;
        distortions[numDistortions].SampleRadius[1]                      = 0.578034682f;
        distortions[numDistortions].SampleRadius[2]                      = 0.751445087f;
        distortions[numDistortions].MaxRadius                            = 0.838150289f;
        numDistortions++;

        distortions[numDistortions] = distortions[0];
        distortions[numDistortions].EyeRelief = 0.020f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.0078f;
            distortions[i].Config.ChromaticAberration[1]        = -0.02f;
            distortions[i].Config.ChromaticAberration[2]        =  0.02f;
            distortions[i].Config.ChromaticAberration[3]        =  0.04f;
        }
#else
        // Orange lens laser numbers, attempt 2 (2013-31-10)
        numDistortions = 0;
        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.005f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.373746053f;
        distortions[numDistortions].Config.K[2]                          = 0.171521748f;
        distortions[numDistortions].Config.K[3]                          = -0.206820921f;
        distortions[numDistortions].SampleRadius[0]                      = 0.289017341f;
        distortions[numDistortions].SampleRadius[1]                      = 0.578034682f;
        distortions[numDistortions].SampleRadius[2]                      = 0.809248555f;
        distortions[numDistortions].MaxRadius                            = 0.867052023f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.371433289f;
        distortions[numDistortions].Config.K[2]                          = 0.149036491f;
        distortions[numDistortions].Config.K[3]                          = -0.269099895f;
        distortions[numDistortions].SampleRadius[0]                      = 0.289017341f;
        distortions[numDistortions].SampleRadius[1]                      = 0.578034682f;
        distortions[numDistortions].SampleRadius[2]                      = 0.751445087f;
        distortions[numDistortions].MaxRadius                            = 0.838150289f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.015f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.301787291f;
        distortions[numDistortions].Config.K[2]                          = 0.019093807f;
        distortions[numDistortions].Config.K[3]                          = -0.211039469f;
        distortions[numDistortions].SampleRadius[0]                      = 0.231213873f;
        distortions[numDistortions].SampleRadius[1]                      = 0.49132948f;
        distortions[numDistortions].SampleRadius[2]                      = 0.693641618f;
        distortions[numDistortions].MaxRadius                            = 0.780346821f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.020f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.319692497f;
        distortions[numDistortions].Config.K[2]                          = 0.396798687f;
        distortions[numDistortions].Config.K[3]                          = -1.011206732f;
        distortions[numDistortions].SampleRadius[0]                      = 0.231213873f;
        distortions[numDistortions].SampleRadius[1]                      = 0.462427746f;
        distortions[numDistortions].SampleRadius[2]                      = 0.664739884f;
        distortions[numDistortions].MaxRadius                            = 0.722543353f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.025f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.346702556f;
        distortions[numDistortions].Config.K[2]                          = 0.731044617f;
        distortions[numDistortions].Config.K[3]                          = -2.123657702f;
        distortions[numDistortions].SampleRadius[0]                      = 0.202312139f;
        distortions[numDistortions].SampleRadius[1]                      = 0.433526012f;
        distortions[numDistortions].SampleRadius[2]                      = 0.578034682f;
        distortions[numDistortions].MaxRadius                            = 0.621387283f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.030f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.351078753f;
        distortions[numDistortions].Config.K[2]                          = 0.909798021f;
        distortions[numDistortions].Config.K[3]                          = -3.878715667f;
        distortions[numDistortions].SampleRadius[0]                      = 0.202312139f;
        distortions[numDistortions].SampleRadius[1]                      = 0.433526012f;
        distortions[numDistortions].SampleRadius[2]                      = 0.505780347f;
        distortions[numDistortions].MaxRadius                            = 0.534682081f;
        numDistortions++;

        // TODO: get 35mm numbers.

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.040f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0346f;
        distortions[numDistortions].Config.K[1]                          = -0.896205416f;
        distortions[numDistortions].Config.K[2]                          = 4.478857342f;
        distortions[numDistortions].Config.K[3]                          = -20.39841899f;
        distortions[numDistortions].SampleRadius[0]                      = 0.144508671f;
        distortions[numDistortions].SampleRadius[1]                      = 0.289017341f;
        distortions[numDistortions].SampleRadius[2]                      = 0.375722543f;
        distortions[numDistortions].MaxRadius                            = 0.39017341f;
        numDistortions++;

        // THE MOST HORRIBLE KLUDGE IN THE WORLD.
        float bodgeFactor = 0.033f / 0.0342f;
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.MetersPerTanAngleAtCenter *= bodgeFactor;
        }

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.017f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.026f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }
#endif
    }
    else if ( hmd.EyeCups == EyeCup_RedA )
    {
#if 1
        // Brant numbers, 2013-11-12
        numDistortions = 0;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.036f;
        distortions[numDistortions].Config.K[1]                          = -0.30f;
        distortions[numDistortions].Config.K[2]                          = 0.26f;
        distortions[numDistortions].Config.K[3]                          = -0.32f;
        distortions[numDistortions].SampleRadius[0]                      = 0.405405405f;
        distortions[numDistortions].SampleRadius[1]                      = 0.675675676f;
        distortions[numDistortions].SampleRadius[2]                      = 0.945945946f;
        distortions[numDistortions].MaxRadius                            = 0.972972973f;
        numDistortions++;

        distortions[numDistortions] = distortions[0];
        distortions[numDistortions].EyeRelief = 0.020f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            distortions[i].Config.ChromaticAberration[0]        = -0.0078f;
            distortions[i].Config.ChromaticAberration[1]        = -0.02f;
            distortions[i].Config.ChromaticAberration[2]        =  0.01f;
            distortions[i].Config.ChromaticAberration[3]        =  0.04f;
        }

#elif 0
        // Red lens laser numbers, 2013-09-20
        numDistortions = 0;
        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.005f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.037f;
        distortions[numDistortions].Config.K[1]                          = -0.195900886f;
        distortions[numDistortions].Config.K[2]                          = -0.105440872f;
        distortions[numDistortions].Config.K[3]                          = 0.06670699f;
        distortions[numDistortions].SampleRadius[0]                      = 0.405405405f;
        distortions[numDistortions].SampleRadius[1]                      = 0.675675676f;
        distortions[numDistortions].SampleRadius[2]                      = 0.810810811f;
        distortions[numDistortions].MaxRadius                            = 0.864864865f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.037f;
        distortions[numDistortions].Config.K[1]                          = -0.216159735f;
        distortions[numDistortions].Config.K[2]                          = -0.149234833f;
        distortions[numDistortions].Config.K[3]                          = 0.052725977f;
        distortions[numDistortions].SampleRadius[0]                      = 0.405405405f;
        distortions[numDistortions].SampleRadius[1]                      = 0.675675676f;
        distortions[numDistortions].SampleRadius[2]                      = 0.945945946f;
        distortions[numDistortions].MaxRadius                            = 0.972972973f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.015f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.037f;
        distortions[numDistortions].Config.K[1]                          = -0.211908151f;
        distortions[numDistortions].Config.K[2]                          = -0.076791767f;
        distortions[numDistortions].Config.K[3]                          = -0.031451767f;
        distortions[numDistortions].SampleRadius[0]                      = 0.405405405f;
        distortions[numDistortions].SampleRadius[1]                      = 0.621621622f;
        distortions[numDistortions].SampleRadius[2]                      = 0.837837838f;
        distortions[numDistortions].MaxRadius                            = 0.905405405f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.020f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0372f;
        distortions[numDistortions].Config.K[1]                          = -0.181396897f;
        distortions[numDistortions].Config.K[2]                          = -0.137846159f;
        distortions[numDistortions].Config.K[3]                          = 0.024650434f;
        distortions[numDistortions].SampleRadius[0]                      = 0.403225806f;
        distortions[numDistortions].SampleRadius[1]                      = 0.537634409f;
        distortions[numDistortions].SampleRadius[2]                      = 0.698924731f;
        distortions[numDistortions].MaxRadius                            = 0.758064516f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.025f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0372f;
        distortions[numDistortions].Config.K[1]                          = -0.158458174f;
        distortions[numDistortions].Config.K[2]                          = -0.023984681f;
        distortions[numDistortions].Config.K[3]                          = -0.332836904f;
        distortions[numDistortions].SampleRadius[0]                      = 0.268817204f;
        distortions[numDistortions].SampleRadius[1]                      = 0.510752688f;
        distortions[numDistortions].SampleRadius[2]                      = 0.61827957f;
        distortions[numDistortions].MaxRadius                            = 0.658602151f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.030f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0375f;
        distortions[numDistortions].Config.K[1]                          = -0.326507277f;
        distortions[numDistortions].Config.K[2]                          = 0.854552504f;
        distortions[numDistortions].Config.K[3]                          = -1.976786691f;
        distortions[numDistortions].SampleRadius[0]                      = 0.266666667f;
        distortions[numDistortions].SampleRadius[1]                      = 0.4f;
        distortions[numDistortions].SampleRadius[2]                      = 0.533333333f;
        distortions[numDistortions].MaxRadius                            = 0.546666667f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.035f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.038f;
        distortions[numDistortions].Config.K[1]                          = -0.736972223f;
        distortions[numDistortions].Config.K[2]                          = 4.261697956f;
        distortions[numDistortions].Config.K[3]                          = -12.05019047f;
        distortions[numDistortions].SampleRadius[0]                      = 0.210526316f;
        distortions[numDistortions].SampleRadius[1]                      = 0.315789474f;
        distortions[numDistortions].SampleRadius[2]                      = 0.434210526f;
        distortions[numDistortions].MaxRadius                            = 0.460526316f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.040f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.038f;
        distortions[numDistortions].Config.K[1]                          = -0.685650346f;
        distortions[numDistortions].Config.K[2]                          = 3.005334399f;
        distortions[numDistortions].Config.K[3]                          = -8.978884459f;
        distortions[numDistortions].SampleRadius[0]                      = 0.184210526f;
        distortions[numDistortions].SampleRadius[1]                      = 0.263157895f;
        distortions[numDistortions].SampleRadius[2]                      = 0.368421053f;
        distortions[numDistortions].MaxRadius                            = 0.394736842f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            // Tuned by Brant, 2013-11-12
            distortions[i].Config.ChromaticAberration[0]        = -0.0078f;
            distortions[i].Config.ChromaticAberration[1]        = -0.02f;
            distortions[i].Config.ChromaticAberration[2]        =  0.01f;
            distortions[i].Config.ChromaticAberration[3]        =  0.04f;
        }
#else
        // Red lens laser numbers, 2013-11-08
        numDistortions = 0;
        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.005f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0373f;
        distortions[numDistortions].Config.K[1]                          = -0.265605582f;
        distortions[numDistortions].Config.K[2]                          = -0.071544245f;
        distortions[numDistortions].Config.K[3]                          = 0.009034775f;
        distortions[numDistortions].SampleRadius[0]                      = 0.402144772f;
        distortions[numDistortions].SampleRadius[1]                      = 0.670241287f;
        distortions[numDistortions].SampleRadius[2]                      = 0.91152815f;
        distortions[numDistortions].MaxRadius                            = 1.045576408f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0372f;
        distortions[numDistortions].Config.K[1]                          = -0.179810563f;
        distortions[numDistortions].Config.K[2]                          = -0.24995779f;
        distortions[numDistortions].Config.K[3]                          = 0.11597192f;
        distortions[numDistortions].SampleRadius[0]                      = 0.403225806f;
        distortions[numDistortions].SampleRadius[1]                      = 0.672043011f;
        distortions[numDistortions].SampleRadius[2]                      = 0.860215054f;
        distortions[numDistortions].MaxRadius                            = 0.981182796f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.015f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0372f;
        distortions[numDistortions].Config.K[1]                          = -0.081520008f;
        distortions[numDistortions].Config.K[2]                          = -0.433201256f;
        distortions[numDistortions].Config.K[3]                          = 0.260741567f;
        distortions[numDistortions].SampleRadius[0]                      = 0.268817204f;
        distortions[numDistortions].SampleRadius[1]                      = 0.537634409f;
        distortions[numDistortions].SampleRadius[2]                      = 0.752688172f;
        distortions[numDistortions].MaxRadius                            = 0.819892473f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.020f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0375f;
        distortions[numDistortions].Config.K[1]                          = -0.16583793f;
        distortions[numDistortions].Config.K[2]                          = -0.109181126f;
        distortions[numDistortions].Config.K[3]                          = -0.001817286f;
        distortions[numDistortions].SampleRadius[0]                      = 0.266666667f;
        distortions[numDistortions].SampleRadius[1]                      = 0.533333333f;
        distortions[numDistortions].SampleRadius[2]                      = 0.666666667f;
        distortions[numDistortions].MaxRadius                            = 0.733333333f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.025f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0379f;
        distortions[numDistortions].Config.K[1]                          = -0.186935607f;
        distortions[numDistortions].Config.K[2]                          = -0.377033154f;
        distortions[numDistortions].Config.K[3]                          = 0.812164188f;
        distortions[numDistortions].SampleRadius[0]                      = 0.211081794f;
        distortions[numDistortions].SampleRadius[1]                      = 0.36939314f;
        distortions[numDistortions].SampleRadius[2]                      = 0.474934037f;
        distortions[numDistortions].MaxRadius                            = 0.540897098f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.030f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0379f;
        distortions[numDistortions].Config.K[1]                          = -0.1614458f;
        distortions[numDistortions].Config.K[2]                          = -1.462027722f;
        distortions[numDistortions].Config.K[3]                          = 5.063980488f;
        distortions[numDistortions].SampleRadius[0]                      = 0.18469657f;
        distortions[numDistortions].SampleRadius[1]                      = 0.316622691f;
        distortions[numDistortions].SampleRadius[2]                      = 0.395778364f;
        distortions[numDistortions].MaxRadius                            = 0.448548813f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.035f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0379f;
        distortions[numDistortions].Config.K[1]                          = -0.371962642f;
        distortions[numDistortions].Config.K[2]                          = 0.349554064f;
        distortions[numDistortions].Config.K[3]                          = -0.903929217f;
        distortions[numDistortions].SampleRadius[0]                      = 0.18469657f;
        distortions[numDistortions].SampleRadius[1]                      = 0.316622691f;
        distortions[numDistortions].SampleRadius[2]                      = 0.395778364f;
        distortions[numDistortions].MaxRadius                            = 0.435356201f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.040f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0379f;
        distortions[numDistortions].Config.K[1]                          = -1.035961064f;
        distortions[numDistortions].Config.K[2]                          = 9.177145865f;
        distortions[numDistortions].Config.K[3]                          = -42.91082923f;
        distortions[numDistortions].SampleRadius[0]                      = 0.131926121f;
        distortions[numDistortions].SampleRadius[1]                      = 0.211081794f;
        distortions[numDistortions].SampleRadius[2]                      = 0.316622691f;
        distortions[numDistortions].MaxRadius                            = 0.343007916f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            // Tuned by Brant, 2013-11-12
            distortions[i].Config.ChromaticAberration[0]        = -0.0078f;
            distortions[i].Config.ChromaticAberration[1]        = -0.02f;
            distortions[i].Config.ChromaticAberration[2]        =  0.01f;
            distortions[i].Config.ChromaticAberration[3]        =  0.04f;
        }
#endif
    }
    else if ( hmd.EyeCups == EyeCup_SunMandalaA )
    {
        // Mandala numbers, 2013-11-08
        numDistortions = 0;
        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.005f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0451f;
        distortions[numDistortions].Config.K[1]                          = -0.37984519f;
        distortions[numDistortions].Config.K[2]                          = 0.301627061f;
        distortions[numDistortions].Config.K[3]                          = -0.308654237f;
        distortions[numDistortions].SampleRadius[0]                      = 0.332594235f;
        distortions[numDistortions].SampleRadius[1]                      = 0.554323725f;
        distortions[numDistortions].SampleRadius[2]                      = 0.720620843f;
        distortions[numDistortions].MaxRadius                            = 0.776053215f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.010f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0447f;
        distortions[numDistortions].Config.K[1]                          = -0.396485344f;
        distortions[numDistortions].Config.K[2]                          = 0.073236652f;
        distortions[numDistortions].Config.K[3]                          = -0.168224626f;
        distortions[numDistortions].SampleRadius[0]                      = 0.33557047f;
        distortions[numDistortions].SampleRadius[1]                      = 0.559284116f;
        distortions[numDistortions].SampleRadius[2]                      = 0.727069351f;
        distortions[numDistortions].MaxRadius                            = 0.782997763f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.015f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0452f;
        distortions[numDistortions].Config.K[1]                          = -0.607042446f;
        distortions[numDistortions].Config.K[2]                          = 0.661034018f;
        distortions[numDistortions].Config.K[3]                          = -0.789863929f;
        distortions[numDistortions].SampleRadius[0]                      = 0.331858407f;
        distortions[numDistortions].SampleRadius[1]                      = 0.553097345f;
        distortions[numDistortions].SampleRadius[2]                      = 0.719026549f;
        distortions[numDistortions].MaxRadius                            = 0.774336283f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.020f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0452f;
        distortions[numDistortions].Config.K[1]                          = -0.651840571f;
        distortions[numDistortions].Config.K[2]                          = 1.018617336f;
        distortions[numDistortions].Config.K[3]                          = -1.609478086f;
        distortions[numDistortions].SampleRadius[0]                      = 0.221238938f;
        distortions[numDistortions].SampleRadius[1]                      = 0.442477876f;
        distortions[numDistortions].SampleRadius[2]                      = 0.60840708f;
        distortions[numDistortions].MaxRadius                            = 0.663716814f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.025f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.0458f;
        distortions[numDistortions].Config.K[1]                          = -0.768524783f;
        distortions[numDistortions].Config.K[2]                          = 1.293914907f;
        distortions[numDistortions].Config.K[3]                          = -1.792073986f;
        distortions[numDistortions].SampleRadius[0]                      = 0.218340611f;
        distortions[numDistortions].SampleRadius[1]                      = 0.3930131f;
        distortions[numDistortions].SampleRadius[2]                      = 0.545851528f;
        distortions[numDistortions].MaxRadius                            = 0.633187773f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.030f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.045f;
        distortions[numDistortions].Config.K[1]                          = -0.551867596f;
        distortions[numDistortions].Config.K[2]                          = 0.885754254f;
        distortions[numDistortions].Config.K[3]                          = -2.063126867f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222222222f;
        distortions[numDistortions].SampleRadius[1]                      = 0.4f;
        distortions[numDistortions].SampleRadius[2]                      = 0.533333333f;
        distortions[numDistortions].MaxRadius                            = 0.577777778f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.035f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.045f;
        distortions[numDistortions].Config.K[1]                          = -0.658729444f;
        distortions[numDistortions].Config.K[2]                          = 1.376641349f;
        distortions[numDistortions].Config.K[3]                          = -3.525644954f;
        distortions[numDistortions].SampleRadius[0]                      = 0.222222222f;
        distortions[numDistortions].SampleRadius[1]                      = 0.333333333f;
        distortions[numDistortions].SampleRadius[2]                      = 0.511111111f;
        distortions[numDistortions].MaxRadius                            = 0.551111111f;
        numDistortions++;

        distortions[numDistortions].Config.Eqn = Distortion_RecipPoly4;
        distortions[numDistortions].Config.K[0] =  1.0f;
        distortions[numDistortions].EyeRelief                            = 0.040f;
        distortions[numDistortions].Config.MetersPerTanAngleAtCenter     = 0.046f;
        distortions[numDistortions].Config.K[1]                          = -0.807388157f;
        distortions[numDistortions].Config.K[2]                          = 1.799661536f;
        distortions[numDistortions].Config.K[3]                          = -6.748063625f;
        distortions[numDistortions].SampleRadius[0]                      = 0.173913043f;
        distortions[numDistortions].SampleRadius[1]                      = 0.326086957f;
        distortions[numDistortions].SampleRadius[2]                      = 0.434782609f;
        distortions[numDistortions].MaxRadius                            = 0.47826087f;
        numDistortions++;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            // These are placeholder, they have not been tuned!
            distortions[i].Config.ChromaticAberration[0]        = -0.017f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.026f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }
    }
    else
    {
        // Unknown lens.
        // Use DK1 black lens settings, just so we can continue to run with something.
        distortions[0].EyeRelief = 0.005f;
        distortions[0].Config.MetersPerTanAngleAtCenter = 0.043875f;
        distortions[0].Config.Eqn = Distortion_RecipPoly4;
        distortions[0].Config.K[0] = 1.0f;
        distortions[0].Config.K[1] = -0.3999f;
        distortions[0].Config.K[2] =  0.2408f;
        distortions[0].Config.K[3] = -0.4589f;
        distortions[0].SampleRadius[0] = 0.2f;
        distortions[0].SampleRadius[1] = 0.4f;
        distortions[0].SampleRadius[2] = 0.6f;

        distortions[1] = distortions[0];
        distortions[1].EyeRelief = 0.010f;
        numDistortions = 2;

        // Chromatic aberration doesn't seem to change with eye relief.
        for ( int i = 0; i < numDistortions; i++ )
        {
            // These are placeholder, they have not been tuned!
            distortions[i].Config.ChromaticAberration[0]        =  0.0f;
            distortions[i].Config.ChromaticAberration[1]        =  0.0f;
            distortions[i].Config.ChromaticAberration[2]        =  0.0f;
            distortions[i].Config.ChromaticAberration[3]        =  0.0f;
        }
    }

    OVR_ASSERT ( numDistortions < (int)(sizeof(distortions)/sizeof(distortions[0])) );


    DistortionDescriptor *pUpper = NULL;
    DistortionDescriptor *pLower = NULL;
    float lerpVal = 0.0f;
    for ( int i = 0; i < numDistortions-1; i++ )
    {
        OVR_ASSERT ( distortions[i].EyeRelief < distortions[i+1].EyeRelief );
        if ( ( distortions[i].EyeRelief <= eyeReliefInMeters ) && ( distortions[i+1].EyeRelief > eyeReliefInMeters ) )
        {
            pLower = &(distortions[i]);
            pUpper = &(distortions[i+1]);
            lerpVal = ( eyeReliefInMeters - pLower->EyeRelief ) / ( pUpper->EyeRelief - pLower->EyeRelief );
            // No break here - I want the ASSERT to check everything every time!
        }
    }
    if ( pUpper == NULL )
    {
#if 0
        // Outside the range, so extrapolate rather than interpolate.
        if ( distortions[0].EyeRelief > eyeReliefInMeters )
        {
            pLower = &(distortions[0]);
            pUpper = &(distortions[1]);
        }
        else
        {
            OVR_ASSERT ( distortions[numDistortions-1].EyeRelief <= eyeReliefInMeters );
            pLower = &(distortions[numDistortions-2]);
            pUpper = &(distortions[numDistortions-1]);
        }
        lerpVal = ( eyeReliefInMeters - pLower->EyeRelief ) / ( pUpper->EyeRelief - pLower->EyeRelief );
#else
        // Do not extrapolate, just clamp - slightly worried about people putting in bogus settings.
        if ( distortions[0].EyeRelief > eyeReliefInMeters )
        {
            pLower = &(distortions[0]);
            pUpper = &(distortions[0]);
        }
        else
        {
            OVR_ASSERT ( distortions[numDistortions-1].EyeRelief <= eyeReliefInMeters );
            pLower = &(distortions[numDistortions-1]);
            pUpper = &(distortions[numDistortions-1]);
        }
        lerpVal = 0.0f;
#endif
    }
    float invLerpVal = 1.0f - lerpVal;

    // Lerp control points and fit an equation to them.
    float fitX[4];
    float fitY[4];
    fitX[0] = 0.0f;
    fitY[0] = 1.0f;
    for ( int ctrlPt = 1; ctrlPt < 4; ctrlPt ++ )
    {
        float radiusLerp = invLerpVal * pLower->SampleRadius[ctrlPt-1] + lerpVal * pUpper->SampleRadius[ctrlPt-1];
        float radiusLerpSq = radiusLerp * radiusLerp;
        float fitYLower = pLower->Config.DistortionFnScaleRadiusSquared ( radiusLerpSq );
        float fitYUpper = pUpper->Config.DistortionFnScaleRadiusSquared ( radiusLerpSq );
        fitX[ctrlPt] = radiusLerpSq;
        fitY[ctrlPt] = 1.0f / ( invLerpVal * fitYLower + lerpVal * fitYUpper );
    }
    // Where is the edge of the lens - no point modelling further than this.
    // float maxValidRadius = invLerpVal * pLower->MaxRadius + lerpVal * pUpper->MaxRadius;

    LensConfig result;
    result.Eqn = Distortion_RecipPoly4;
    result.ChromaticAberration[0] = invLerpVal * pLower->Config.ChromaticAberration[0] + lerpVal * pUpper->Config.ChromaticAberration[0];
    result.ChromaticAberration[1] = invLerpVal * pLower->Config.ChromaticAberration[1] + lerpVal * pUpper->Config.ChromaticAberration[1];
    result.ChromaticAberration[2] = invLerpVal * pLower->Config.ChromaticAberration[2] + lerpVal * pUpper->Config.ChromaticAberration[2];
    result.ChromaticAberration[3] = invLerpVal * pLower->Config.ChromaticAberration[3] + lerpVal * pUpper->Config.ChromaticAberration[3];
    result.MetersPerTanAngleAtCenter =  pLower->Config.MetersPerTanAngleAtCenter * invLerpVal +
                                        pUpper->Config.MetersPerTanAngleAtCenter * lerpVal;
    bool bSuccess = FitCubicPolynomial ( result.K, fitX, fitY );
    OVR_ASSERT ( bSuccess );
    OVR_UNUSED ( bSuccess );

    // Set up the fast inverse.
    //float maxRDist = result.DistortionFn ( maxValidRadius );
    //result.SetUpInverseApprox ( maxRDist );

    return result;
}





DistortionRenderDesc CalculateDistortionRenderDesc ( StereoEye eyeType, HmdRenderInfo const &hmd,
                                                     const LensConfig *pLensOverride /*= NULL */ )
{
    // From eye relief, IPD and device characteristics, we get the distortion mapping.
    // This distortion does the following things:
    // 1. It undoes the distortion that happens at the edges of the lens.
    // 2. It maps the undistorted field into "retina" space.
    // So the input is a pixel coordinate - the physical pixel on the display itself.
    // The output is the real-world direction of the ray from this pixel as it comes out of the lens and hits the eye.
    // However we typically think of rays "coming from" the eye, so the direction (TanAngleX,TanAngleY,1) is the direction
    //      that the pixel appears to be in real-world space, where AngleX and AngleY are relative to the straight-ahead vector.
    // If your renderer is a raytracer, you can use this vector directly (normalize as appropriate).
    // However in standard rasterisers, we have rendered a 2D image and are putting it in front of the eye,
    //      so we then need a mapping from this space to the [-1,1] UV coordinate space, which depends on exactly
    //      where "in space" the app wants to put that rendertarget.
    //      Where in space, and how large this rendertarget is, is completely up to the app and/or user,
    //      though of course we can provide some useful hints.

    // TODO: Use IPD and eye relief to modify distortion (i.e. non-radial component)
    // TODO: cope with lenses that don't produce collimated light.
    //       This means that IPD relative to the lens separation changes the light vergence,
    //       and so we actually need to change where the image is displayed.

    const HmdRenderInfo::EyeConfig &hmdEyeConfig = ( eyeType == StereoEye_Left ) ? hmd.EyeLeft : hmd.EyeRight;

    DistortionRenderDesc localDistortion;
    localDistortion.Lens.Eqn = hmdEyeConfig.DistortionEqn;
    for ( int i = 0; i < 4; i++ )
    {
        localDistortion.Lens.K[i] = hmdEyeConfig.DistortionK[i];
    }
    for ( int i = 0; i < 4; i++ )
    {
        localDistortion.Lens.ChromaticAberration[i] = hmdEyeConfig.ChromaticAberration[i];
    }
    localDistortion.Lens.MetersPerTanAngleAtCenter = hmdEyeConfig.MetersPerTanAngleAtCenter;

    if ( pLensOverride != NULL )
    {
        localDistortion.Lens = *pLensOverride;
    }

    Sizef pixelsPerMeter(hmd.ResolutionInPixels.Width / ( hmd.ScreenSizeInMeters.Width - hmd.ScreenGapSizeInMeters ),
                         hmd.ResolutionInPixels.Height / hmd.ScreenSizeInMeters.Height);

    localDistortion.PixelsPerTanAngleAtCenter = (pixelsPerMeter * localDistortion.Lens.MetersPerTanAngleAtCenter).ToVector();
    // Same thing, scaled to [-1,1] for each eye, rather than pixels.

    localDistortion.TanEyeAngleScale = Vector2f(0.25f, 0.5f).EntrywiseMultiply(
                                       (hmd.ScreenSizeInMeters / localDistortion.Lens.MetersPerTanAngleAtCenter).ToVector());
    
    // <--------------left eye------------------><-ScreenGapSizeInMeters-><--------------right eye----------------->
    // <------------------------------------------ScreenSizeInMeters.Width----------------------------------------->
    //                            <----------------LensSeparationInMeters--------------->
    // <--centerFromLeftInMeters->
    //                            ^
    //                      Center of lens

    // Find the lens centers in scale of [-1,+1] (NDC) in left eye.
    float visibleWidthOfOneEye = 0.5f * ( hmd.ScreenSizeInMeters.Width - hmd.ScreenGapSizeInMeters );
    float centerFromLeftInMeters = ( hmd.ScreenSizeInMeters.Width - hmd.LensSeparationInMeters ) * 0.5f;
    localDistortion.LensCenter.x = (     centerFromLeftInMeters / visibleWidthOfOneEye          ) * 2.0f - 1.0f;
    localDistortion.LensCenter.y = ( hmd.CenterFromTopInMeters  / hmd.ScreenSizeInMeters.Height ) * 2.0f - 1.0f;
    if ( eyeType == StereoEye_Right )
    {
        localDistortion.LensCenter.x = -localDistortion.LensCenter.x;
    }

    return localDistortion;
}


FovPort CalculateFovFromEyePosition ( float eyeReliefInMeters,
                                      float offsetToRightInMeters,
                                      float offsetDownwardsInMeters,
                                      float lensDiameterInMeters,
                                      float extraEyeRotationInRadians /*= 0.0f*/ )
{
    // 2D view of things:
    //       |-|            <--- offsetToRightInMeters (in this case, it is negative)
    // |=======C=======|    <--- lens surface (C=center)
    //  \    |       _/
    //   \   R     _/
    //    \  |   _/
    //     \ | _/
    //      \|/
    //       O  <--- center of pupil

    // (technically the lens is round rather than square, so it's not correct to
    // separate vertical and horizontal like this, but it's close enough)
    float halfLensDiameter = lensDiameterInMeters * 0.5f;
    FovPort fovPort;
    fovPort.UpTan    = ( halfLensDiameter + offsetDownwardsInMeters ) / eyeReliefInMeters;
    fovPort.DownTan  = ( halfLensDiameter - offsetDownwardsInMeters ) / eyeReliefInMeters;
    fovPort.LeftTan  = ( halfLensDiameter + offsetToRightInMeters   ) / eyeReliefInMeters;
    fovPort.RightTan = ( halfLensDiameter - offsetToRightInMeters   ) / eyeReliefInMeters;

    if ( extraEyeRotationInRadians > 0.0f )
    {
        // That's the basic looking-straight-ahead eye position relative to the lens.
        // But if you look left, the pupil moves left as the eyeball rotates, which
        // means you can see more to the right than this geometry suggests.
        // So add in the bounds for the extra movement of the pupil.

        // Beyond 30 degrees does not increase FOV because the pupil starts moving backwards more than sideways.
        extraEyeRotationInRadians = Alg::Min ( DegreeToRad ( 30.0f ), Alg::Max ( 0.0f, extraEyeRotationInRadians ) );
        const float eyeballCenterToPupil = 0.011f; // 11mm for almost every human.
        float extraTranslation = eyeballCenterToPupil * sinf ( extraEyeRotationInRadians );
        float extraRelief = eyeballCenterToPupil * ( 1.0f - cosf ( extraEyeRotationInRadians ) );

        fovPort.UpTan    = Alg::Max ( fovPort.UpTan   , ( halfLensDiameter + offsetDownwardsInMeters + extraTranslation ) / ( eyeReliefInMeters + extraRelief ) );
        fovPort.DownTan  = Alg::Max ( fovPort.DownTan , ( halfLensDiameter - offsetDownwardsInMeters + extraTranslation ) / ( eyeReliefInMeters + extraRelief ) );
        fovPort.LeftTan  = Alg::Max ( fovPort.LeftTan , ( halfLensDiameter + offsetToRightInMeters   + extraTranslation ) / ( eyeReliefInMeters + extraRelief ) );
        fovPort.RightTan = Alg::Max ( fovPort.RightTan, ( halfLensDiameter - offsetToRightInMeters   + extraTranslation ) / ( eyeReliefInMeters + extraRelief ) );
    }

    return fovPort;
}



FovPort CalculateFovFromHmdInfo ( StereoEye eyeType,
                                  DistortionRenderDesc const &distortion,
                                  HmdRenderInfo const &hmd,
                                  float extraEyeRotationInRadians /*= 0.0f*/ )
{
    FovPort fovPort;
    float eyeReliefInMeters;
    float offsetToRightInMeters;
    if ( eyeType == StereoEye_Right )
    {
        eyeReliefInMeters     = hmd.EyeRight.ReliefInMeters;
        offsetToRightInMeters = hmd.EyeRight.NoseToPupilInMeters - 0.5f * hmd.LensSeparationInMeters;
    }
    else
    {
        eyeReliefInMeters     = hmd.EyeLeft.ReliefInMeters;
        offsetToRightInMeters = -(hmd.EyeLeft.NoseToPupilInMeters - 0.5f * hmd.LensSeparationInMeters);
    }

    // Central view.
    fovPort = CalculateFovFromEyePosition ( eyeReliefInMeters,
                                                      offsetToRightInMeters,
                                                      0.0f,
                                                      hmd.LensDiameterInMeters,
                                                      extraEyeRotationInRadians );

    fovPort = ClampToPhysicalScreenFov ( eyeType, distortion, fovPort );
    return fovPort;
}



FovPort GetPhysicalScreenFov ( StereoEye eyeType, DistortionRenderDesc const &distortion )
{
    OVR_UNUSED1 ( eyeType );

    FovPort resultFovPort;

    // Figure out the boundaries of the screen. We take the middle pixel of the screen,
    // move to each of the four screen edges, and transform those back into TanAngle space.
    Vector2f dmiddle = distortion.LensCenter;

    // The gotcha is that for some distortion functions, the map will "wrap around"
    // for screen pixels that are not actually visible to the user (especially on DK1,
    // which has a lot of invisible pixels), and map to pixels that are close to the middle.
    // This means the edges of the screen will actually be
    // "closer" than the visible bounds, so we'll clip too aggressively.

    // Solution - step gradually towards the boundary, noting the maximum distance.
    struct FunctionHider
    {
        static FovPort FindRange ( Vector2f from, Vector2f to, int numSteps,
                                          DistortionRenderDesc const &distortion )
        {
            FovPort result;
            result.UpTan    = 0.0f;
            result.DownTan  = 0.0f;
            result.LeftTan  = 0.0f;
            result.RightTan = 0.0f;

            float stepScale = 1.0f / ( numSteps - 1 );
            for ( int step = 0; step < numSteps; step++ )
            {
                float    lerpFactor  = stepScale * (float)step;
                Vector2f sample      = from + (to - from) * lerpFactor;
                Vector2f tanEyeAngle = TransformScreenNDCToTanFovSpace ( distortion, sample );

                result.LeftTan  = Alg::Max ( result.LeftTan,  -tanEyeAngle.x );
                result.RightTan = Alg::Max ( result.RightTan,  tanEyeAngle.x );
                result.UpTan    = Alg::Max ( result.UpTan,    -tanEyeAngle.y );
                result.DownTan  = Alg::Max ( result.DownTan,   tanEyeAngle.y );
            }
            return result;
        }
    };

    FovPort leftFovPort  = FunctionHider::FindRange( dmiddle, Vector2f( -1.0f, dmiddle.y ), 10, distortion );
    FovPort rightFovPort = FunctionHider::FindRange( dmiddle, Vector2f( 1.0f, dmiddle.y ),  10, distortion );
    FovPort upFovPort    = FunctionHider::FindRange( dmiddle, Vector2f( dmiddle.x, -1.0f ), 10, distortion );
    FovPort downFovPort  = FunctionHider::FindRange( dmiddle, Vector2f( dmiddle.x, 1.0f ),  10, distortion );
    
    resultFovPort.LeftTan  = leftFovPort.LeftTan;
    resultFovPort.RightTan = rightFovPort.RightTan;
    resultFovPort.UpTan    = upFovPort.UpTan;
    resultFovPort.DownTan  = downFovPort.DownTan;

    return resultFovPort;
}

FovPort ClampToPhysicalScreenFov( StereoEye eyeType, DistortionRenderDesc const &distortion,
                                         FovPort inputFovPort )
{
    FovPort resultFovPort;
    FovPort phsyicalFovPort = GetPhysicalScreenFov ( eyeType, distortion );
    resultFovPort.LeftTan  = Alg::Min ( inputFovPort.LeftTan,  phsyicalFovPort.LeftTan );
    resultFovPort.RightTan = Alg::Min ( inputFovPort.RightTan, phsyicalFovPort.RightTan );
    resultFovPort.UpTan    = Alg::Min ( inputFovPort.UpTan,    phsyicalFovPort.UpTan );
    resultFovPort.DownTan  = Alg::Min ( inputFovPort.DownTan,  phsyicalFovPort.DownTan );

    return resultFovPort;
}

Sizei CalculateIdealPixelSize ( DistortionRenderDesc const &distortion,
                                FovPort tanHalfFov, float pixelsPerDisplayPixel )
{
    Sizei result;
    // TODO: if the app passes in a FOV that doesn't cover the centre, use the distortion values for the nearest edge/corner to match pixel size.
    result.Width  = (int)(0.5f + pixelsPerDisplayPixel * distortion.PixelsPerTanAngleAtCenter.x * ( tanHalfFov.LeftTan + tanHalfFov.RightTan ) );
    result.Height = (int)(0.5f + pixelsPerDisplayPixel * distortion.PixelsPerTanAngleAtCenter.y * ( tanHalfFov.UpTan   + tanHalfFov.DownTan  ) );
    return result;
}

Viewport GetFramebufferViewport ( StereoEye eyeType, HmdRenderInfo const &hmd )
{
    Viewport result;
    result.w = hmd.ResolutionInPixels.Width/2;
    result.h = hmd.ResolutionInPixels.Height;
    result.x = 0;
    result.y = 0;
    if ( eyeType == StereoEye_Right )
    {
        result.x = (hmd.ResolutionInPixels.Width+1)/2;      // Round up, not down.
    }
    return result;
}

Matrix4f CreateProjection( bool rightHanded, FovPort tanHalfFov,
                           float zNear /*= 0.01f*/, float zFar /*= 10000.0f*/ )
{
    // Cribbed from Matrix4f::PerspectiveRH()
    Matrix4f projection;
    // Produces X result, mapping clip edges to [-w,+w]
    float projXScale = 2.0f / ( tanHalfFov.LeftTan + tanHalfFov.RightTan );
    float projXOffset = ( tanHalfFov.RightTan - tanHalfFov.LeftTan ) * projXScale * 0.5f;
    projection.M[0][0] = projXScale;
    projection.M[0][1] = 0.0f;
    projection.M[0][2] = projXOffset;
    projection.M[0][3] = 0.0f;

    // Produces Y result, mapping clip edges to [-w,+w]
    float projYScale = 2.0f / ( tanHalfFov.UpTan + tanHalfFov.DownTan );
    float projYOffset = ( tanHalfFov.UpTan - tanHalfFov.DownTan ) * projYScale * 0.5f;
    projection.M[1][0] = 0.0f;
    projection.M[1][1] = projYScale;
    projection.M[1][2] = projYOffset;
    projection.M[1][3] = 0.0f;

    // Produces Z-buffer result - app needs to fill this in with whatever Z range it wants.
    // We'll just use some defaults for now.
    projection.M[2][0] = 0.0f;
    projection.M[2][1] = 0.0f;
    projection.M[2][2] = zFar / (zNear - zFar);
    projection.M[2][3] = (zFar * zNear) / (zNear - zFar);

    // Produces W result (= -Z in)
    projection.M[3][0] = 0.0f;
    projection.M[3][1] = 0.0f;
    if ( rightHanded )
    {
        projection.M[3][2] = -1.0f;
    }
    else
    {
        projection.M[3][2] = 1.0f;
    }
    projection.M[3][3] = 0.0f;

    return projection;
}


ScaleAndOffset2D CreateNDCScaleAndOffsetFromProjection ( bool rightHanded, Matrix4f projection )
{
    float offsetScale = 1.0f;
    if ( rightHanded )
    {
        offsetScale = -1.0f;
    }
    OVR_ASSERT ( offsetScale == projection.M[3][2] );

    // Given a projection matrix, create the scale&offset for the shader code.
    // This is very simple, since this is already what projection matrices almost do.
    // The exception is NDC wants [-1,-1] to be at top-left, not bottom-left.
    ScaleAndOffset2D result;
    result.Scale    = Vector2f(projection.M[0][0], projection.M[1][1]);
    result.Offset   = Vector2f(projection.M[0][2] * offsetScale, projection.M[1][2] * -offsetScale);
    // Hey - why is that Y.Offset negated?
    // It's because a projection matrix transforms from world coords with Y=up,
    // whereas this is from NDC which is Y=down.

    return result;
}



ScaleAndOffset2D CreateUVScaleAndOffsetfromNDCScaleandOffset ( ScaleAndOffset2D scaleAndOffsetNDC,
                                                               Viewport renderedViewport,
                                                               Sizei renderTargetSize )
{
    // scaleAndOffsetNDC takes you to NDC space [-1,+1] within the given viewport on the rendertarget.
    // We want a scale to instead go to actual UV coordinates you can sample with,
    // which need [0,1] and ignore the viewport.
    ScaleAndOffset2D result;
    // Scale [-1,1] to [0,1]
    result.Scale  = scaleAndOffsetNDC.Scale * 0.5f;
    result.Offset = scaleAndOffsetNDC.Offset * 0.5f + Vector2f(0.5f);
    
    // ...but we will have rendered to a subsection of the RT, so scale for that.
    Vector2f scale(  (float)renderedViewport.w / (float)renderTargetSize.Width,
                     (float)renderedViewport.h / (float)renderTargetSize.Height );
    Vector2f offset( (float)renderedViewport.x / (float)renderTargetSize.Width,
                     (float)renderedViewport.y / (float)renderTargetSize.Height );

	result.Scale  = result.Scale.EntrywiseMultiply(scale);
    result.Offset  = result.Offset.EntrywiseMultiply(scale) + offset;
    return result;
}



Matrix4f CreateOrthoSubProjection ( bool rightHanded, StereoEye eyeType,
                                    float tanHalfFovX, float tanHalfFovY,
                                    float unitsX, float unitsY,
                                    float distanceFromCamera, float interpupillaryDistance,
                                    Matrix4f const &projection,
                                    float zNear /*= 0.0f*/, float zFar /*= 0.0f*/ )
{
    OVR_UNUSED1 ( rightHanded );

    float orthoHorizontalOffset = interpupillaryDistance * 0.5f / distanceFromCamera;
    switch ( eyeType )
    {
    case StereoEye_Center:
        orthoHorizontalOffset = 0.0f;
        break;
    case StereoEye_Left:
        break;
    case StereoEye_Right:
        orthoHorizontalOffset = -orthoHorizontalOffset;
        break;
    default: OVR_ASSERT ( false ); break;
    }

    // Current projection maps real-world vector (x,y,1) to the RT.
    // We want to find the projection that maps the range [-FovPixels/2,FovPixels/2] to
    // the physical [-orthoHalfFov,orthoHalfFov]
    // Note moving the offset from M[0][2]+M[1][2] to M[0][3]+M[1][3] - this means
    // we don't have to feed in Z=1 all the time.
    // The horizontal offset math is a little hinky because the destination is
    // actually [-orthoHalfFov+orthoHorizontalOffset,orthoHalfFov+orthoHorizontalOffset]
    // So we need to first map [-FovPixels/2,FovPixels/2] to
    //                         [-orthoHalfFov+orthoHorizontalOffset,orthoHalfFov+orthoHorizontalOffset]:
    // x1 = x0 * orthoHalfFov/(FovPixels/2) + orthoHorizontalOffset;
    //    = x0 * 2*orthoHalfFov/FovPixels + orthoHorizontalOffset;
    // But then we need the sam mapping as the existing projection matrix, i.e.
    // x2 = x1 * Projection.M[0][0] + Projection.M[0][2];
    //    = x0 * (2*orthoHalfFov/FovPixels + orthoHorizontalOffset) * Projection.M[0][0] + Projection.M[0][2];
    //    = x0 * Projection.M[0][0]*2*orthoHalfFov/FovPixels +
    //      orthoHorizontalOffset*Projection.M[0][0] + Projection.M[0][2];
    // So in the new projection matrix we need to scale by Projection.M[0][0]*2*orthoHalfFov/FovPixels and
    // offset by orthoHorizontalOffset*Projection.M[0][0] + Projection.M[0][2].

    float orthoScaleX = 2.0f * tanHalfFovX / unitsX;
    float orthoScaleY = 2.0f * tanHalfFovY / unitsY;
    Matrix4f ortho;
    ortho.M[0][0] = projection.M[0][0] * orthoScaleX;
    ortho.M[0][1] = 0.0f;
    ortho.M[0][2] = 0.0f;
    ortho.M[0][3] = -projection.M[0][2] + ( orthoHorizontalOffset * projection.M[0][0] );

    ortho.M[1][0] = 0.0f;
    ortho.M[1][1] = -projection.M[1][1] * orthoScaleY;       // Note sign flip (text rendering uses Y=down).
    ortho.M[1][2] = 0.0f;
    ortho.M[1][3] = -projection.M[1][2];

    if ( fabsf ( zNear - zFar ) < 0.001f )
    {
        ortho.M[2][0] = 0.0f;
        ortho.M[2][1] = 0.0f;
        ortho.M[2][2] = 0.0f;
        ortho.M[2][3] = zFar;
    }
    else
    {
        ortho.M[2][0] = 0.0f;
        ortho.M[2][1] = 0.0f;
        ortho.M[2][2] = zFar / (zNear - zFar);
        ortho.M[2][3] = (zFar * zNear) / (zNear - zFar);
    }

    // No perspective correction for ortho.
    ortho.M[3][0] = 0.0f;
    ortho.M[3][1] = 0.0f;
    ortho.M[3][2] = 0.0f;
    ortho.M[3][3] = 1.0f;

    return ortho;
}


//-----------------------------------------------------------------------------------
// A set of "forward-mapping" functions, mapping from framebuffer space to real-world and/or texture space.

// This mimics the first half of the distortion shader's function.
Vector2f TransformScreenNDCToTanFovSpace( DistortionRenderDesc const &distortion,
                                          const Vector2f &framebufferNDC )
{
    // Scale to TanHalfFov space, but still distorted.
    Vector2f tanEyeAngleDistorted;
    tanEyeAngleDistorted.x = ( framebufferNDC.x - distortion.LensCenter.x ) * distortion.TanEyeAngleScale.x;
    tanEyeAngleDistorted.y = ( framebufferNDC.y - distortion.LensCenter.y ) * distortion.TanEyeAngleScale.y;
    // Distort.
    float radiusSquared = ( tanEyeAngleDistorted.x * tanEyeAngleDistorted.x )
                        + ( tanEyeAngleDistorted.y * tanEyeAngleDistorted.y );
    float distortionScale = distortion.Lens.DistortionFnScaleRadiusSquared ( radiusSquared );
    Vector2f tanEyeAngle;
    tanEyeAngle.x = tanEyeAngleDistorted.x * distortionScale;
    tanEyeAngle.y = tanEyeAngleDistorted.y * distortionScale;

    return tanEyeAngle;
}

// Same, with chromatic aberration correction.
void TransformScreenNDCToTanFovSpaceChroma ( Vector2f *resultR, Vector2f *resultG, Vector2f *resultB, 
                                             DistortionRenderDesc const &distortion,
                                             const Vector2f &framebufferNDC )
{
    // Scale to TanHalfFov space, but still distorted.
    Vector2f tanEyeAngleDistorted;
    tanEyeAngleDistorted.x = ( framebufferNDC.x - distortion.LensCenter.x ) * distortion.TanEyeAngleScale.x;
    tanEyeAngleDistorted.y = ( framebufferNDC.y - distortion.LensCenter.y ) * distortion.TanEyeAngleScale.y;
    // Distort.
    float radiusSquared = ( tanEyeAngleDistorted.x * tanEyeAngleDistorted.x )
                        + ( tanEyeAngleDistorted.y * tanEyeAngleDistorted.y );
    Vector3f distortionScales = distortion.Lens.DistortionFnScaleRadiusSquaredChroma ( radiusSquared );
    *resultR = tanEyeAngleDistorted * distortionScales.x;
    *resultG = tanEyeAngleDistorted * distortionScales.y;
    *resultB = tanEyeAngleDistorted * distortionScales.z;
}

// This mimics the second half of the distortion shader's function.
Vector2f TransformTanFovSpaceToRendertargetTexUV( StereoEyeParams const &eyeParams,
                                                  Vector2f const &tanEyeAngle )
{
    Vector2f textureUV;
    textureUV.x = tanEyeAngle.x * eyeParams.EyeToSourceUV.Scale.x + eyeParams.EyeToSourceUV.Offset.x;
    textureUV.y = tanEyeAngle.y * eyeParams.EyeToSourceUV.Scale.y + eyeParams.EyeToSourceUV.Offset.y;
    return textureUV;
}

Vector2f TransformTanFovSpaceToRendertargetNDC( StereoEyeParams const &eyeParams,
                                                Vector2f const &tanEyeAngle )
{
    Vector2f textureNDC;
    textureNDC.x = tanEyeAngle.x * eyeParams.EyeToSourceNDC.Scale.x + eyeParams.EyeToSourceNDC.Offset.x;
    textureNDC.y = tanEyeAngle.y * eyeParams.EyeToSourceNDC.Scale.y + eyeParams.EyeToSourceNDC.Offset.y;
    return textureNDC;
}

Vector2f TransformScreenPixelToScreenNDC( Viewport const &distortionViewport,
                                          Vector2f const &pixel )
{
    // Move to [-1,1] NDC coords.
    Vector2f framebufferNDC;
    framebufferNDC.x = -1.0f + 2.0f * ( ( pixel.x - (float)distortionViewport.x ) / (float)distortionViewport.w );
    framebufferNDC.y = -1.0f + 2.0f * ( ( pixel.y - (float)distortionViewport.y ) / (float)distortionViewport.h );
    return framebufferNDC;
}

Vector2f TransformScreenPixelToTanFovSpace( Viewport const &distortionViewport,
                                            DistortionRenderDesc const &distortion,
                                            Vector2f const &pixel )
{
    return TransformScreenNDCToTanFovSpace( distortion,
                TransformScreenPixelToScreenNDC( distortionViewport, pixel ) );
}

Vector2f TransformScreenNDCToRendertargetTexUV( DistortionRenderDesc const &distortion,
                                                StereoEyeParams const &eyeParams,
                                                Vector2f const &pixel )
{
    return TransformTanFovSpaceToRendertargetTexUV ( eyeParams,
                TransformScreenNDCToTanFovSpace ( distortion, pixel ) );
}

Vector2f TransformScreenPixelToRendertargetTexUV( Viewport const &distortionViewport,
                                                  DistortionRenderDesc const &distortion,
                                                  StereoEyeParams const &eyeParams,
                                                  Vector2f const &pixel )
{
    return TransformTanFovSpaceToRendertargetTexUV ( eyeParams,
                TransformScreenPixelToTanFovSpace ( distortionViewport, distortion, pixel ) );
}


//-----------------------------------------------------------------------------------
// A set of "reverse-mapping" functions, mapping from real-world and/or texture space back to the framebuffer.

Vector2f TransformTanFovSpaceToScreenNDC( DistortionRenderDesc const &distortion,
                                          const Vector2f &tanEyeAngle, bool usePolyApprox /*= false*/ )
{
    float tanEyeAngleRadius = tanEyeAngle.Length();
    float tanEyeAngleDistortedRadius = distortion.Lens.DistortionFnInverseApprox ( tanEyeAngleRadius );
    if ( !usePolyApprox )
    {
        tanEyeAngleDistortedRadius = distortion.Lens.DistortionFnInverse ( tanEyeAngleRadius );
    }
    Vector2f tanEyeAngleDistorted = tanEyeAngle;
    if ( tanEyeAngleRadius > 0.0f )
    {   
        tanEyeAngleDistorted = tanEyeAngle * ( tanEyeAngleDistortedRadius / tanEyeAngleRadius );
    }

    Vector2f framebufferNDC;
    framebufferNDC.x = ( tanEyeAngleDistorted.x / distortion.TanEyeAngleScale.x ) + distortion.LensCenter.x;
    framebufferNDC.y = ( tanEyeAngleDistorted.y / distortion.TanEyeAngleScale.y ) + distortion.LensCenter.y;

    return framebufferNDC;
}

Vector2f TransformRendertargetNDCToTanFovSpace( StereoEyeParams const &eyeParams,
                                                Vector2f const &textureNDC )
{
    Vector2f tanEyeAngle;
    tanEyeAngle.x = ( textureNDC.x - eyeParams.EyeToSourceNDC.Offset.x ) / eyeParams.EyeToSourceNDC.Scale.x;
    tanEyeAngle.y = ( textureNDC.y - eyeParams.EyeToSourceNDC.Offset.y ) / eyeParams.EyeToSourceNDC.Scale.y;
    return tanEyeAngle;
}



} //namespace OVR



