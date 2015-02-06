/************************************************************************************

Filename    :   AppRender.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <math.h>
#include <jni.h>

#include <sys/time.h>

#include "GlUtils.h"
#include "GlTexture.h"
#include "VrCommon.h"
#include "App.h"

#include "AppLocal.h"
#include "BitmapFont.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "GazeCursor.h"
#include "VRMenu/VRMenuMgr.h"
#include "VRMenu/GuiSys.h"
#include "DebugLines.h"



//#define BASIC_FOLLOW 1

namespace OVR
{

const int FPS_NUM_FRAMES_TO_AVERAGE = 30;

// Debug tool to draw outlines of a 3D bounds
void AppLocal::DrawBounds( const Vector3f &mins, const Vector3f &maxs, const Matrix4f &mvp, const Vector3f &color )
{
	Matrix4f	scaled = mvp * Matrix4f::Translation( mins ) * Matrix4f::Scaling( maxs - mins );
	const GlProgram & prog = untexturedMvpProgram;
	glUseProgram(prog.program);
	glLineWidth( 1.0f );
	glUniform4f(prog.uColor, color.x, color.y, color.z, 1);
	glUniformMatrix4fv(prog.uMvp, 1, GL_FALSE /* not transposed */,
			scaled.Transposed().M[0] );
	glBindVertexArrayOES_( unitCubeLines.vertexArrayObject );
	glDrawElements(GL_LINES, unitCubeLines.indexCount, GL_UNSIGNED_SHORT, NULL);
	glBindVertexArrayOES_( 0 );
}

void AppLocal::DrawDialog( const Matrix4f & mvp )
{
	// draw the pop-up dialog
	const float now = TimeInSeconds();
	if ( now >= dialogStopSeconds )
	{
		return;
	}
	const Matrix4f dialogMvp = mvp * dialogMatrix;

	const float fadeSeconds = 0.5f;
	const float f = now - ( dialogStopSeconds - fadeSeconds );
	const float clampF = f < 0.0f ? 0.0f : f;
	const float alpha = 1.0f - clampF;

	DrawPanel( dialogTexture->textureId, dialogMvp, alpha );

}

// DJB: This function displays the 2D android activities. By showing
// android dialogs within VrActivity, this code is executed.
//
// Test by way of displaying Android activity UI.
void AppLocal::DrawActivity( const Matrix4f & mvp )
{
	if ( !activityPanel.Visible )
	{
		return;
	}

#if FANCY_FOLLOW || BASIC_FOLLOW
	Matrix4f targetMatrix = lastViewMatrix.Inverted()
			* Matrix4f::Translation( 0.0f, -0.0f, -2.5 /* screenDist */ )
			* Matrix4f::Scaling( (float)activityPanel.Width / 768.0f,
				(float)activityPanel.Height / 768.0f, 1.0f );
#if FANCY_FOLLOW
	activityPanel.Matrix = MatrixInterpolation( activityPanel.Matrix, targetMatrix, 0.20 );
#elif BASIC_FOLLOW
	activityPanel.Matrix = targetMatrix;
#endif // FANCY or BASIC

#endif // FOLLOW

	const Matrix4f dialogMvp = mvp * activityPanel.Matrix;

	DrawPanel( activityPanel.Texture->textureId, dialogMvp, 1.0f );
}

void AppLocal::DrawPanel( const GLuint externalTextureId, const Matrix4f & dialogMvp, const float alpha )
{
	const GlProgram & prog = externalTextureProgram2;
	glUseProgram( prog.program );
	glUniform4f(prog.uColor, 1, 1, 1, alpha );

	glUniformMatrix4fv(prog.uTexm, 1, GL_FALSE, Matrix4f::Identity().Transposed().M[0]);
	glUniformMatrix4fv(prog.uMvp, 1, GL_FALSE, dialogMvp.Transposed().M[0] );

	// It is important that panels write to destination alpha, or they
	// might get covered by an overlay plane/cube in TimeWarp.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalTextureId);
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	panelGeometry.Draw();
	glDisable( GL_BLEND );
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0 );	// don't leave it bound
}

Matrix4f CalculateCameraTimeWarpMatrix( const Quatf &from, const Quatf &to )
{
    Matrix4f		lastSensorMatrix = Matrix4f( to ).Transposed();
    Matrix4f		lastViewMatrix = Matrix4f( from ).Transposed();
	Matrix4f		timeWarp = ( lastSensorMatrix * lastViewMatrix.Inverted() );

	return timeWarp;
}

void AppLocal::DrawPassThroughCamera( const float bufferFov, const Quatf &orientation )
{
    if ( cameraFovHorizontal == 0 || cameraFovVertical == 0 )
    {
    	return;
    }

//    const Matrix4f		viewMatrix = Matrix4f( orientation ).Transposed();

	glDisable( GL_CULL_FACE );

	// flipped for portrait mode
	const float znear = 0.5f;
	const float zfar = 150.0f;
	const Matrix4f projectionMatrix = Matrix4f::PerspectiveRH(
			DegreeToRad(bufferFov), 1.0f, znear, zfar);

	const Matrix4f modelMatrix = Matrix4f(
			tan( DegreeToRad( cameraFovHorizontal/2 ) ), 0, 0, 0,
			0, tan( DegreeToRad( cameraFovVertical/2 ) ), 0, 0,
			0, 0, 0, -1,
			0, 0, 0, 1 );

	const GlProgram & prog = interpolatedCameraWarp;
	glUseProgram( prog.program );

	// interpolated time warp to de-waggle the rolling shutter camera image
	// enableCameraTimeWarp 0 = none
	// enableCameraTimeWarp 1 = full frame
	// enableCameraTimeWarp 2 = interpolated
	for ( int i = 0 ; i < 2 ; i++ )
	{
		const Matrix4f	timeWarp = enableCameraTimeWarp ?
				CalculateCameraTimeWarpMatrix( cameraFramePose[ enableCameraTimeWarp == 2 ? i : 0].Predicted.Pose.Orientation,
											   orientation )
				: Matrix4f::Identity();

		const Matrix4f cameraMvp = projectionMatrix * timeWarp * modelMatrix;

		glUniformMatrix4fv( i ? prog.uTexm : prog.uMvp, 1, GL_FALSE,
				cameraMvp.Transposed().M[0]);
	}

	glUniform4f(prog.uColor, 1, 1, 1, 1 );
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, cameraTexture->textureId);
	panelGeometry.Draw();
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0 );	// don't leave it bound
}

void AppLocal::DrawEyeViewsPostDistorted( Matrix4f const & centerViewMatrix, const int numPresents )
{
	const float TEXT_SCALE = 1.0f;

	// update vr lib systems after the app frame, but before rendering anything
	GetGuiSys().Frame( this, vrFrame, GetVRMenuMgr(), GetDefaultFont(), GetMenuFontSurface() );
	GetGazeCursor().Frame( this->lastViewMatrix, vrFrame.DeltaSeconds );

	if ( ShowFPS )
	{
		static double  LastFrameTime = TimeInSeconds();
		static double  AccumulatedFrameInterval = 0.0;
		static int   NumAccumulatedFrames = 0;
		static float LastFrameRate = 60.0f;

		double currentFrameTime = TimeInSeconds();
		double frameInterval = currentFrameTime - LastFrameTime;
		AccumulatedFrameInterval += frameInterval;
		NumAccumulatedFrames++;
		if ( NumAccumulatedFrames > FPS_NUM_FRAMES_TO_AVERAGE ) {
			double interval = ( AccumulatedFrameInterval / NumAccumulatedFrames );  // averaged
			AccumulatedFrameInterval = 0.0;
			NumAccumulatedFrames = 0;
			LastFrameRate = 1.0f / float( interval > 0.000001 ? interval : 0.00001 );
		}    
		fontParms_t fontParms;
		fontParms.CenterHoriz = true;
		fontParms.Billboard = true;
        fontParms.TrackRoll = true;
		const Vector3f viewPos( GetViewMatrixPosition( centerViewMatrix ) );
		const Vector3f viewFwd( GetViewMatrixForward( centerViewMatrix ) );
		const Vector3f textPos( viewPos + viewFwd * 1.5f );
		GetWorldFontSurface().DrawTextBillboarded3Df( GetDefaultFont(), 
                fontParms, textPos, TEXT_SCALE, Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), "%.1f fps", LastFrameRate );
		LastFrameTime = currentFrameTime;
	}

	if ( InfoTextEndFrame >= vrFrame.FrameNumber )
	{
		fontParms_t fontParms;
		fontParms.CenterHoriz = true;
		fontParms.Billboard = true;
        fontParms.TrackRoll = true;
		const Vector3f viewPos( GetViewMatrixPosition( centerViewMatrix ) );
		const Vector3f viewFwd( GetViewMatrixForward( centerViewMatrix ) );
		const Vector3f textPos( viewPos + viewFwd * 1.5f );
		GetWorldFontSurface().DrawTextBillboarded3Df( GetDefaultFont(), 
                fontParms, textPos, TEXT_SCALE, Vector4f( 1.0f, 1.0f, 1.0f, 1.0f ), InfoText.ToCStr() );
	}

	GetMenuFontSurface().Finish( centerViewMatrix );
	GetWorldFontSurface().Finish( centerViewMatrix );
	GetVRMenuMgr().Finish( centerViewMatrix );

	// Increase the fov by about 10 degrees if we are not holding 60 fps so
	// there is less black pull-in at the edges.
	//
	// Doing this dynamically based just on time causes visible flickering at the
	// periphery when the fov is increased, so only do it if minimumVsyncs is set.
	const float fovDegrees = hmdInfo.SuggestedEyeFov +
			( ( ( SwapParms.MinimumVsyncs > 1 ) || ovr_GetPowerLevelStateThrottled() ) ? 10.0f : 0.0f ) +
			( ( !showVignette ) ? 5.0f : 0.0f );

	// DisplayMonoMode uses a single eye rendering for speed improvement
	// and / or high refresh rate double-scan hardware modes.
	const int numEyes = renderMonoMode ? 1 : 2;

	// Flush out and report any errors
	GL_CheckErrors("FrameStart");

	if ( drawCalibrationLines && calibrationLinesDrawn )
	{
		// doing a time warp test, don't generate new images
		LOG( "drawCalibrationLines && calibrationLinesDrawn" );
	}
	else
	{
		// possibly change the buffer parameters
		EyeTargets->BeginFrame( vrParms );

		for (int eye = 0; eye < numEyes; eye++)
		{
			EyeTargets->BeginRenderingEye( eye );

			// Call back to the app for drawing.
			const Matrix4f mvp = appInterface->DrawEyeView( eye, fovDegrees );

			DrawActivity( mvp );

			DrawPassThroughCamera( fovDegrees, vrFrame.PoseState.Pose.Orientation );

			GetVRMenuMgr().RenderSubmitted( mvp.Transposed() );
			GetMenuFontSurface().Render3D( GetDefaultFont(), mvp.Transposed() );
			GetWorldFontSurface().Render3D( GetDefaultFont(), mvp.Transposed() );

			glDisable( GL_DEPTH_TEST );
			glDisable( GL_CULL_FACE );

			// Optionally draw thick calibration lines into the texture,
			// which will be overlayed by the thinner origin cross when
			// distorted to the window.
			if ( drawCalibrationLines )
			{
				EyeDecorations.DrawEyeCalibrationLines(fovDegrees, eye);
				calibrationLinesDrawn = true;
			}
			else
			{
				calibrationLinesDrawn = false;
			}

			DrawDialog( mvp );

			GetGazeCursor().Render( eye, mvp );

			GetDebugLines().Render( mvp.Transposed() );

			if ( showVignette )
			{
				// Draw a thin vignette at the edges of the view so clamping will give black
				// This will not be reflected correctly in overlay planes.
				// EyeDecorations.DrawEyeVignette();

				EyeDecorations.FillEdge( vrParms.resolution, vrParms.resolution );
			}

			EyeTargets->EndRenderingEye( eye );
		}
	}

	// This eye set is complete, use it now.
	if ( numPresents > 0 )
	{
		const CompletedEyes eyes = EyeTargets->GetCompletedEyes();

		for ( int eye = 0 ; eye < TimeWarpParms::MAX_WARP_EYES ; eye++ )
		{
			SwapParms.Images[eye][0].TexCoordsFromTanAngles = TanAngleMatrixFromFov( fovDegrees );
			SwapParms.Images[eye][0].TexId = eyes.Textures[renderMonoMode ? 0 : eye ];
			SwapParms.Images[eye][0].Pose = SensorForNextWarp.Predicted;
		}

		ovr_WarpSwap( OvrMobile, &SwapParms );
	}
}

// Draw a screen to an eye buffer the same way it would be drawn as a
// time warp overlay.
void DrawScreenDirect( const GLuint texid, const ovrMatrix4f & mvp )
{
	const OVR::Matrix4f mvpMatrix( mvp );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, texid );

	static OVR::GlProgram prog;
	if ( !prog.program )
	{
		prog = OVR::BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"attribute vec2 TexCoord;\n"
			"varying  highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oTexCoord = TexCoord;\n"
			"}\n"
		,
			"uniform sampler2D Texture0;\n"
			"varying highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = texture2D( Texture0, oTexCoord );\n"
			"}\n"
		);

	}
	glUseProgram( prog.program );

	glUniformMatrix4fv( prog.uMvp, 1, GL_FALSE, mvpMatrix.Transposed().M[0] );

	static OVR::GlGeometry	unitSquare;
	if ( unitSquare.vertexArrayObject == 0 )
	{
		unitSquare = OVR::BuildTesselatedQuad( 1, 1 );
	}

	glBindVertexArrayOES_( unitSquare.vertexArrayObject );
	glDrawElements( GL_TRIANGLES, unitSquare.indexCount, GL_UNSIGNED_SHORT, NULL );

	glBindTexture( GL_TEXTURE_2D, 0 );	// don't leave it bound
}

OVR::GlGeometry BuildFadedScreenMask( const float xFraction, const float yFraction )
{
	const float posx[] = { -1.001f, -1.0f + xFraction * 0.25f, -1.0f + xFraction, 1.0f - xFraction, 1.0f - xFraction * 0.25f, 1.001f };
	const float posy[] = { -1.001f, -1.0f + yFraction * 0.25f, -1.0f + yFraction, 1.0f - yFraction, 1.0f - yFraction * 0.25f, 1.001f };

	const int vertexCount = 6 * 6;

	OVR::VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y < 6; y++ )
	{
		for ( int x = 0; x < 6; x++ )
		{
			const int index = y * 6 + x;
			attribs.position[index].x = posx[x];
			attribs.position[index].y = posy[y];
			attribs.position[index].z = 0.0f;
			attribs.uv0[index].x = 0.0f;
			attribs.uv0[index].y = 0.0f;
			// the outer edges will have 0 color
			const float c = ( y <= 1 || y >= 4 || x <= 1 || x >= 4 ) ? 0.0f : 1.0f;
			for ( int i = 0; i < 3; i++ )
			{
				attribs.color[index][i] = c;
			}
			attribs.color[index][3] = 1.0f;	// solid alpha
		}
	}

	OVR::Array< OVR::TriangleIndex > indices;
	indices.Resize( 25 * 6 );

	// Should we flip the triangulation on the corners?
	int index = 0;
	for ( int x = 0; x < 5; x++ )
	{
		for ( int y = 0; y < 5; y++ )
		{
			indices[index + 0] = y * 6 + x;
			indices[index + 1] = y * 6 + x + 1;
			indices[index + 2] = (y + 1) * 6 + x;
			indices[index + 3] = (y + 1) * 6 + x;
			indices[index + 4] = y * 6 + x + 1;
			indices[index + 5] = (y + 1) * 6 + x + 1;
			index += 6;
		}
	}

	return OVR::GlGeometry( attribs, indices );
}

// draw a zero to destination alpha
void DrawScreenMask( const ovrMatrix4f & mvp, const float fadeFracX, const float fadeFracY )
{
	OVR::Matrix4f mvpMatrix( mvp );

	static OVR::GlProgram prog;
	if ( !prog.program )
	{
		prog = OVR::BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 VertexColor;\n"
			"attribute vec4 Position;\n"
			"varying  lowp vec4 oColor;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oColor = vec4( 1.0, 1.0, 1.0, 1.0 - VertexColor.x );\n"
			"}\n"
		,
			"varying lowp vec4	oColor;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = oColor;\n"
			"}\n"
		);

	}
	glUseProgram( prog.program );

	glUniformMatrix4fv( prog.uMvp, 1, GL_FALSE, mvpMatrix.Transposed().M[0] );

	static OVR::GlGeometry	VignetteSquare;
	if ( VignetteSquare.vertexArrayObject == 0 )
	{
		VignetteSquare = BuildFadedScreenMask( fadeFracX, fadeFracY );
	}

	glColorMask( 0, 0, 0, 1 );
	VignetteSquare.Draw();
	glColorMask( 1, 1, 1, 1 );
}

}	// namespace OVR
