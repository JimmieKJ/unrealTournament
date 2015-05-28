/************************************************************************************

Filename    :   ModelView.cpp
Content     :   Basic viewing and movement in a scene.
Created     :   December 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "ModelView.h"

#include "VrApi/VrApi.h"
#include "VrApi/VrApi_Helpers.h"

#include "Input.h"		// VrFrame, etc
#include "BitmapFont.h"
#include "DebugLines.h"

namespace OVR
{

void ModelInScene::SetModelFile( const ModelFile * mf ) 
{ 
	Definition = mf;
	State.modelDef = mf ? &mf->Def : NULL;
	State.Joints.Resize( mf->GetJointCount() );
};

void ModelInScene::AnimateJoints( const float timeInSeconds )
{
	if ( State.Flags.Pause )
	{
		return;
	}

	for ( int i = 0; i < Definition->GetJointCount(); i++ )
	{
		const ModelJoint * joint = Definition->GetJoint( i );
		if ( joint->animation == MODEL_JOINT_ANIMATION_NONE )
		{
			continue;
		}

		float time = ( timeInSeconds + joint->timeOffset ) * joint->timeScale;

		switch( joint->animation )
		{
			case MODEL_JOINT_ANIMATION_SWAY:
			{
				time = sinf( time * Math<float>::Pi );
				// NOTE: fall through
			}
			case MODEL_JOINT_ANIMATION_ROTATE:
			{
				const Vector3f angles = joint->parameters * ( Math<float>::DegreeToRadFactor * time );
				const Matrix4f matrix = joint->transform *
										Matrix4f::RotationY( angles.y ) *
										Matrix4f::RotationX( angles.x ) *
										Matrix4f::RotationZ( angles.z ) *
										joint->transform.Inverted();
				State.Joints[i] = matrix.Transposed();
				break;
			}
			case MODEL_JOINT_ANIMATION_BOB:
			{
				const float frac = sinf( time * Math<float>::Pi );
				const Vector3f offset = joint->parameters * frac;
				const Matrix4f matrix = joint->transform *
										Matrix4f::Translation( offset ) *
										joint->transform.Inverted();
				State.Joints[i] = matrix.Transposed();
				break;
			}
			case MODEL_JOINT_ANIMATION_NONE:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------
// The RHS coordinate system is defines as follows (as seen in perspective view):
//  Y - Up
//  Z - Back
//  X - Right
const Vector3f	UpVector( 0.0f, 1.0f, 0.0f );
const Vector3f	ForwardVector( 0.0f, 0.0f, -1.0f );
const Vector3f	RightVector( 1.0f, 0.0f, 0.0f );

OvrSceneView::OvrSceneView() :
	FreeWorldModelOnChange( false ),
	SceneId( 0 ),
	LoadedPrograms( false ),
	MoveSpeed( 3.0f ),
	Znear( 1.0f ),
	Zfar( 1000.0f ),
	ImuToEyeCenter( 0.06f, 0.0f, 0.03f ),
	YawOffset( 0.0f ),
	PitchOffset( 0.0f ),
	YawVelocity( 0.0f ),
	AllowPositionTracking( false ),
	LastHeadModelOffset( 0.0f ),
	EyeYaw( 0.0f ),
	EyePitch( 0.0f ),
	EyeRoll( 0.0f ),
	FootPos( 0.0f )
{
}

int OvrSceneView::AddModel( ModelInScene * model )
{
	const int modelsSize = Models.GetSizeI();

	// scan for a NULL entry
	for ( int i = 0; i < modelsSize; ++i )
	{
		if ( Models[i] == NULL )
		{
			Models[i] = model;
			return i;
		}
	}

	Models.PushBack( model );

	return Models.GetSizeI() - 1;
}

void OvrSceneView::RemoveModelIndex( int index )
{
	Models[index] = NULL;
}

ModelGlPrograms OvrSceneView::GetDefaultGLPrograms()
{
	ModelGlPrograms programs;

	if ( !LoadedPrograms )
	{
		ProgVertexColor				= BuildProgram( VertexColorVertexShaderSrc, VertexColorFragmentShaderSrc );
		ProgSingleTexture			= BuildProgram( SingleTextureVertexShaderSrc, SingleTextureFragmentShaderSrc );
		ProgLightMapped				= BuildProgram( LightMappedVertexShaderSrc, LightMappedFragmentShaderSrc );
		ProgReflectionMapped		= BuildProgram( ReflectionMappedVertexShaderSrc, ReflectionMappedFragmentShaderSrc );
		ProgSkinnedVertexColor		= BuildProgram( VertexColorSkinned1VertexShaderSrc, VertexColorFragmentShaderSrc );
		ProgSkinnedSingleTexture	= BuildProgram( SingleTextureSkinned1VertexShaderSrc, SingleTextureFragmentShaderSrc );
		ProgSkinnedLightMapped		= BuildProgram( LightMappedSkinned1VertexShaderSrc, LightMappedFragmentShaderSrc );
		ProgSkinnedReflectionMapped	= BuildProgram( ReflectionMappedSkinned1VertexShaderSrc, ReflectionMappedFragmentShaderSrc );
		LoadedPrograms = true;
	}

	programs.ProgVertexColor				= & ProgVertexColor;
	programs.ProgSingleTexture				= & ProgSingleTexture;
	programs.ProgLightMapped				= & ProgLightMapped;
	programs.ProgReflectionMapped			= & ProgReflectionMapped;
	programs.ProgSkinnedVertexColor			= & ProgSkinnedVertexColor;
	programs.ProgSkinnedSingleTexture		= & ProgSkinnedSingleTexture;
	programs.ProgSkinnedLightMapped			= & ProgSkinnedLightMapped;
	programs.ProgSkinnedReflectionMapped	= & ProgSkinnedReflectionMapped;

	return programs;
}

void OvrSceneView::LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms )
{
	LOG( "OvrSceneView::LoadScene( %s )", sceneFileName );

	if ( GlPrograms.ProgSingleTexture == NULL )
	{
		GlPrograms = GetDefaultGLPrograms();
	}

	// Load the scene we are going to draw
	ModelFile * model = LoadModelFile( sceneFileName, GlPrograms, materialParms );

	SetWorldModel( *model );

	FreeWorldModelOnChange = true;
}

void OvrSceneView::SetWorldModel( ModelFile & world )
{
	LOG( "OvrSceneView::SetWorldModel( %s )", world.FileName.ToCStr() );

	if ( FreeWorldModelOnChange && Models.GetSizeI() > 0 )
	{
		delete WorldModel.Definition;
		FreeWorldModelOnChange = false;
	}
	Models.Clear();

	WorldModel.SetModelFile( &world );
	AddModel( &WorldModel );

	// Projection matrix
	Znear = 0.01f;
	Zfar = 2000.0f;

	// Set the initial player position
	FootPos = Vector3f( 0.0f, 0.0f, 0.0f );
	YawOffset = 0;

	LastHeadModelOffset = Vector3f( 0.0f, 0.0f, 0.0f );
}

SurfaceDef * OvrSceneView::FindNamedSurface( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedSurface( name );
}

const ModelTexture * OvrSceneView::FindNamedTexture( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedTexture( name );
}

const ModelTag * OvrSceneView::FindNamedTag( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedTag( name );
}

Bounds3f OvrSceneView::GetBounds() const
{
	return ( WorldModel.Definition == NULL ) ?
			Bounds3f( Vector3f( 0, 0, 0 ), Vector3f( 0, 0, 0 ) ) :
			WorldModel.Definition->GetBounds();
}

Matrix4f OvrSceneView::CenterViewMatrix() const
{
	return ViewMatrix;
}

Matrix4f OvrSceneView::ViewMatrixForEye( const int eye ) const
{
	const float eyeOffset = ( eye ? -1 : 1 ) * 0.5f * ViewParms.InterpupillaryDistance;
	return Matrix4f::Translation( eyeOffset, 0.0f, 0.0f ) * ViewMatrix;
}

Matrix4f OvrSceneView::ProjectionMatrixForEye( const int eye, const float fovDegrees ) const
{
	// We may want to make per-eye projection matrices if we move away from
	// nearly-centered lenses.
	return Matrix4f::PerspectiveRH( DegreeToRad( fovDegrees ), 1.0f, Znear, Zfar );
}

Matrix4f OvrSceneView::MvpForEye( const int eye, const float fovDegrees ) const
{
	return ProjectionMatrixForEye( eye, fovDegrees ) * ViewMatrixForEye( eye );
}

Matrix4f OvrSceneView::DrawEyeView( const int eye, const float fovDegrees ) const
{
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glFrontFace( GL_CCW );

	const Matrix4f projectionMatrix = ProjectionMatrixForEye( eye, fovDegrees );
	const Matrix4f viewMatrix = ViewMatrixForEye( eye );

	const DrawSurfaceList & surfs = BuildDrawSurfaceList( RenderModels, viewMatrix, projectionMatrix );
	(void)RenderSurfaceList( surfs );

	// TODO: sort the emit surfaces with the model based surfaces
	if ( EmitList.GetSizeI() > 0 )
	{
		DrawSurfaceList	emits;
		emits.drawSurfaces = &EmitList[0];
		emits.numDrawSurfaces = EmitList.GetSizeI();
		emits.projectionMatrix = projectionMatrix;
		emits.viewMatrix = viewMatrix;

		const Matrix4f vpMatrix = ( projectionMatrix * viewMatrix ).Transposed();

		for ( int i = 0 ; i < EmitList.GetSizeI() ; i++ )
		{
			DrawMatrices & matrices = *(DrawMatrices *)EmitList[i].matrices;
			matrices.Mvp = matrices.Model * vpMatrix;
		}

		(void)RenderSurfaceList( emits );
	}

	return ( projectionMatrix * viewMatrix );
}

Vector3f OvrSceneView::Forward() const
{
	return Vector3f( -ViewMatrix.M[2][0], -ViewMatrix.M[2][1], -ViewMatrix.M[2][2] );
}

Vector3f OvrSceneView::CenterEyePos() const
{
	return Vector3f( FootPos.x, FootPos.y + ViewParms.EyeHeight, FootPos.z );
}

Vector3f OvrSceneView::ShiftedCenterEyePos() const
{
	return ShiftedEyePos;
}

Vector3f OvrSceneView::HeadModelOffset( float EyeRoll, float EyePitch, float EyeYaw, float HeadModelDepth, float HeadModelHeight )
{
	// head-on-a-stick model
	const Matrix4f rollPitchYaw = Matrix4f::RotationY( EyeYaw )
			* Matrix4f::RotationX( EyePitch )
			* Matrix4f::RotationZ( EyeRoll );
    Vector3f eyeCenterInHeadFrame( 0.0f, HeadModelHeight, -HeadModelDepth );
	Vector3f lastHeadModelOffset = rollPitchYaw.Transform( eyeCenterInHeadFrame );

	lastHeadModelOffset.y -= eyeCenterInHeadFrame.y; // Bring the head back down to original height

	return lastHeadModelOffset;
}

void OvrSceneView::UpdateViewMatrix(const VrFrame vrFrame )
{
	// Experiments with position tracking
	const bool	useHeadModel = !AllowPositionTracking ||
			( ( vrFrame.Input.buttonState & ( BUTTON_A | BUTTON_X ) ) == 0 );

	// Delta time in seconds since last frame.
	const float dt = vrFrame.DeltaSeconds;
	const float yawSpeed = 1.5f;

    Vector3f GamepadMove;

	// Allow up / down movement if there is no floor collision model
	if ( vrFrame.Input.buttonState & BUTTON_RIGHT_TRIGGER )
	{
		FootPos.y -= vrFrame.Input.sticks[0][1] * dt * MoveSpeed;
	}
	else
	{
		GamepadMove.z = vrFrame.Input.sticks[0][1];
	}
	GamepadMove.x = vrFrame.Input.sticks[0][0];

	// Turn based on the look stick
	// Because this can be predicted ahead by async TimeWarp, we apply
	// the yaw from the previous frame's controls, trading a frame of
	// latency on stick controls to avoid a bounce-back.
	YawOffset -= YawVelocity * dt;

	if ( !( vrFrame.OvrStatus & ovrStatus_OrientationTracked ) )
	{
		PitchOffset -= yawSpeed * vrFrame.Input.sticks[1][1] * dt;
		YawVelocity = yawSpeed * vrFrame.Input.sticks[1][0];
	}
	else
	{
		YawVelocity = 0.0f;
	}

	// We extract Yaw, Pitch, Roll instead of directly using the orientation
	// to allow "additional" yaw manipulation with mouse/controller.
	const Quatf quat = vrFrame.PoseState.Pose.Orientation;

	quat.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>( &EyeYaw, &EyePitch, &EyeRoll );

	EyeYaw += YawOffset;

	// If the sensor isn't plugged in, allow right stick up/down
	// to adjust pitch, which can be useful for debugging.  Never
	// do this when head tracking
	if ( !( vrFrame.OvrStatus & ovrStatus_OrientationTracked ) )
	{
		EyePitch += PitchOffset;
	}

	// Perform player movement.
	if ( GamepadMove.LengthSq() > 0.0f )
	{
		const Matrix4f yawRotate = Matrix4f::RotationY( EyeYaw );
		const Vector3f orientationVector = yawRotate.Transform( GamepadMove );

		// Don't let move get too crazy fast
		const float moveDistance = OVR::Alg::Min<float>( MoveSpeed * (float)dt, 1.0f );
		if ( WorldModel.Definition )
		{
			FootPos = SlideMove( FootPos, ViewParms.EyeHeight, orientationVector, moveDistance,
						WorldModel.Definition->Collisions, WorldModel.Definition->GroundCollisions );
		}
		else
		{	// no scene loaded, walk without any collisions
			CollisionModel collisionModel;
			CollisionModel groundCollisionModel;
			FootPos = SlideMove( FootPos, ViewParms.EyeHeight, orientationVector, moveDistance,
						collisionModel, groundCollisionModel );
		}
	}

	// Rotate and position View Camera, using YawPitchRoll in BodyFrame coordinates.
	Matrix4f rollPitchYaw = Matrix4f::RotationY( EyeYaw )
			* Matrix4f::RotationX( EyePitch )
			* Matrix4f::RotationZ( EyeRoll );
	const Vector3f up = rollPitchYaw.Transform( UpVector );
	const Vector3f forward = rollPitchYaw.Transform( ForwardVector );
	const Vector3f right = rollPitchYaw.Transform( RightVector );

	// Have sensorFusion zero the integration when not using it, so the
	// first frame is correct.
	if ( vrFrame.Input.buttonPressed & (BUTTON_A | BUTTON_X) )
	{
		LatchedHeadModelOffset = LastHeadModelOffset;
	}

	// Calculate the shiftedEyePos
	ShiftedEyePos = CenterEyePos();

	Vector3f headModelOffset = HeadModelOffset( EyeRoll, EyePitch, EyeYaw,
			ViewParms.HeadModelDepth, ViewParms.HeadModelHeight );
	if ( useHeadModel )
	{
		ShiftedEyePos += headModelOffset;
	}

	headModelOffset += forward * ImuToEyeCenter.z;
	headModelOffset += right * ImuToEyeCenter.x;

	LastHeadModelOffset = headModelOffset;

	if ( !useHeadModel )
	{
		// Use position tracking from the sensor system, which is in absolute
		// coordinates without the YawOffset
		ShiftedEyePos += Matrix4f::RotationY( YawOffset ).Transform( vrFrame.PoseState.Pose.Position );

		ShiftedEyePos -= forward * ImuToEyeCenter.z;
		ShiftedEyePos -= right * ImuToEyeCenter.x;

		ShiftedEyePos += LatchedHeadModelOffset;
	}

	ViewMatrix = Matrix4f::LookAtRH( ShiftedEyePos, ShiftedEyePos + forward, up );
}

void OvrSceneView::UpdateSceneModels( const VrFrame vrFrame, const long long supressModelsWithClientId  )
{
	// Build the packed array of ModelState to pass to the renderer for both eyes
	RenderModels.Resize( 0 );

	for ( int i = 0; i < Models.GetSizeI(); ++i )
	{
		if ( Models[i] != NULL && Models[i]->DontRenderForClientUid != supressModelsWithClientId )
		{
			Models[i]->AnimateJoints( vrFrame.PoseState.TimeInSeconds );
			RenderModels.PushBack( Models[i]->State );
		}
	}
}

void OvrSceneView::Frame( const VrViewParms viewParms_, const VrFrame vrFrame,
		ovrMatrix4f & timeWarpParmsExternalVelocity, const long long supressModelsWithClientId )
{
	ViewParms = viewParms_;
	UpdateViewMatrix( vrFrame );
	UpdateSceneModels( vrFrame, supressModelsWithClientId );

	// External systems can add surfaces to this list before drawing.
	EmitList.Clear();

	// Set the external velocity matrix so TimeWarp can smoothly rotate the
	// view even if we are dropping frames.
	const ovrMatrix4f localViewMatrix = ViewMatrix;
	timeWarpParmsExternalVelocity = CalculateExternalVelocity( &localViewMatrix, YawVelocity );
}

}	// namespace OVR
