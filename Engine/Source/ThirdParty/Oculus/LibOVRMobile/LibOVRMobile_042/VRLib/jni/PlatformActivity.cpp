/************************************************************************************

Filename    :   PlatformActivity.cpp
Content     :   System UI that can pop up over any engine
Created     :   July 23, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#include <jni.h>

#include <android/keycodes.h>
#include <unistd.h>						// usleep, etc
#include "GlUtils.h"
#include "GlTexture.h"
#include "ModelRender.h"
#include "App.h"
#include "AppLocal.h"					// ExitOnDestroy
#include "BitmapFont.h"
#include "ModelView.h"
#include "PackageFiles.h"
#include "VRMenu/GuiSys.h"
#include "VRMenu/VRMenu.h"
#include "VRMenu/VRMenuComponent.h"
#include "VRMenu/VRMenuMgr.h"
#include "VRMenu/GlobalMenu.h"
#include "VRMenu/DefaultComponent.h"
#include "VRMenu/ActionComponents.h"
#include "VrApi/VrLocale.h"

namespace OVR {

char const * FloorVertexProgSrc =
	"uniform mat4 Mvpm;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main() {\n"
	"  gl_Position = Mvpm * Position;\n"
	"  oTexCoord = TexCoord;\n"
	"}\n";

char const * FloorFragmentProgSrc =
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main() {\n"
	"  gl_FragColor = texture2D( Texture0, oTexCoord );\n"
	"}\n";


//==============================================================
// OvrConfirmQuitMenu
class OvrConfirmQuitMenu : public VRMenu
{
public:
	static	VRMenuId_t	ID_QUIT_MESSAGE;
	static	VRMenuId_t	ID_YES;
	static	VRMenuId_t	ID_NO;
	static char const *	MENU_NAME;

	static	OvrConfirmQuitMenu *	Create( App * app, BitmapFont const & font );

	OvrConfirmQuitMenu();
	virtual ~OvrConfirmQuitMenu();

private:
	virtual void	OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event );
	virtual bool	OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType );
};

char const * OvrConfirmQuitMenu::MENU_NAME = "ConfirmQuit";
VRMenuId_t OvrConfirmQuitMenu::ID_QUIT_MESSAGE( VRMenu::GetRootId().Get() + 1 );
VRMenuId_t OvrConfirmQuitMenu::ID_YES( VRMenu::GetRootId().Get() + 2 );
VRMenuId_t OvrConfirmQuitMenu::ID_NO( VRMenu::GetRootId().Get() + 3 );


//===============================
// OvrConfirmQuitMenu::OvrConfirmQuitMenu
OvrConfirmQuitMenu::OvrConfirmQuitMenu() :
	VRMenu( MENU_NAME )
{
}

//===============================
// OvrConfirmQuitMenu::~OvrConfirmQuitMenu
OvrConfirmQuitMenu::~OvrConfirmQuitMenu() 
{ 
}

//==============================
// OvrConfirmQuitMenu::Create
OvrConfirmQuitMenu * OvrConfirmQuitMenu::Create( App * app, BitmapFont const & font )
{
	OvrConfirmQuitMenu * menu = new OvrConfirmQuitMenu;

	Vector3f fwd( 0.0f, 0.0f, 1.0f );
	Vector3f up( 0.0f, 1.0f, 0.0f );
	Vector3f right( fwd.Cross( up ) );

	Array< VRMenuObjectParms const * > parms;

	VRMenuFontParms fontParms( true, true, false, false, false, 1.0f );
	VRMenuObjectFlags_t itemFlags;
	VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
	Vector3f const hilightOfs( 0.0f, 0.0f, 0.05f );
	float const hilightScale = 1.0f;
	float const fadeDuration = 0.05f;
	float const fadeDelay = 0.05f;

	float const ROW_HEIGHT = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;

	String quitText;
	VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/exit_to_home", "@string/exit_to_home", quitText );

	{
		Posef quitPose( Quatf(), up * ROW_HEIGHT * 1.0f );
		Array< VRMenuComponent* > quitComps;
		VRMenuSurfaceParms quitSurfaceParms;
		VRMenuObjectParms * quitParms = new VRMenuObjectParms( VRMENU_STATIC, quitComps, quitSurfaceParms, quitText,
																quitPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), 
																fontParms, ID_QUIT_MESSAGE, itemFlags, initFlags );
		parms.PushBack( quitParms );
	}

	{
		// Yes
		String yesText;
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/yes", "@string/yes", yesText );
		Posef yesPose( Quatf(), up * ROW_HEIGHT * -1.0f + right * 0.15f );
		Array< VRMenuComponent* > yesComps;
		yesComps.PushBack( new OvrButton_OnUp( menu, OvrConfirmQuitMenu::ID_YES ) );
		yesComps.PushBack( new OvrDefaultComponent( hilightOfs, hilightScale, fadeDuration, fadeDelay, 
				OvrGuiSys::BUTTON_DEFAULT_TEXT_COLOR, OvrGuiSys::BUTTON_HILIGHT_TEXT_COLOR ) );
		VRMenuSurfaceParms yesSurfaceParms;
		VRMenuObjectParms * yesParms = new VRMenuObjectParms( VRMENU_BUTTON, yesComps, yesSurfaceParms, yesText,
																yesPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), 
																fontParms, ID_YES, itemFlags, initFlags );
		parms.PushBack( yesParms );
	}

	{
		// No
		String noText;
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/no", "@string/no", noText );
		Posef noPose( Quatf(), up * ROW_HEIGHT * -1.0f - right * 0.15f );
		Array< VRMenuComponent* > noComps;
		noComps.PushBack( new OvrButton_OnUp( menu, OvrConfirmQuitMenu::ID_NO ) );
		noComps.PushBack( new OvrDefaultComponent( hilightOfs, hilightScale, fadeDuration, fadeDelay, 
				OvrGuiSys::BUTTON_DEFAULT_TEXT_COLOR, OvrGuiSys::BUTTON_HILIGHT_TEXT_COLOR ) );
		VRMenuSurfaceParms noSurfaceParms;
		VRMenuObjectParms * noParms = new VRMenuObjectParms( VRMENU_BUTTON, noComps, noSurfaceParms, noText,
																noPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), 
																fontParms, ID_NO, itemFlags, initFlags );
		parms.PushBack( noParms );
	}

	menu->InitWithItems( app->GetVRMenuMgr(), font, 1.8f, VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ), parms );

	menuHandle_t messageHandle = menu->HandleForId( app->GetVRMenuMgr(), ID_QUIT_MESSAGE );
	if ( messageHandle.IsValid() )
	{
		VRMenuObject * obj = app->GetVRMenuMgr().ToObject( messageHandle );
		if ( obj != NULL )
		{
			obj->SetTextWordWrapped( quitText, font, 1.0f );
		}
	}

	DeletePointerArray( parms );

	return menu;
}

//==============================
// OvrConfirmQuitMenu::OnItemEvent_Impl
void OvrConfirmQuitMenu::OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event )
{
	if ( event.EventType == VRMENU_EVENT_TOUCH_UP )
	{
		if ( itemId == ID_YES )
		{
			app->GetGuiSys().CloseMenu( app, MENU_NAME, false );
			app->ReturnToLauncher();
		}
		else if ( itemId == ID_NO )
		{
			app->GetGuiSys().CloseMenu( app, MENU_NAME, false );
			app->ExitPlatformUI();
		}
		else
		{
			OVR_ASSERT( itemId == ID_YES || itemId == ID_NO );
		}
	}
}

//==============================
// OvrConfirmQuitMenu::OnKeyEvent_Impl
bool OvrConfirmQuitMenu::OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
{
	if ( keyCode == AKEYCODE_BACK && eventType == KeyState::KEY_EVENT_SHORT_PRESS )
	{
		// for now, pressing back again in the confirm quit menu is the same as answering "no"
		LOG( "Exiting ConfirmQuitMenu due to back key, event type %s", KeyState::EventNames[eventType] );
		app->ExitPlatformUI();
		return true;
	}
	return false;
}

//==============================================================
// OvrThrottleMenu
class OvrThrottleMenu : public VRMenu
{
public:
	static	VRMenuId_t	ID_THROTTLE_MESSAGE;
	static char const *	MENU_NAME;

	int		throttleState;

	static	OvrThrottleMenu *	Create( App * app, BitmapFont const & font );

	void	SetThrottleState( App * app, int const throttleState_ )
	{
		OVR_ASSERT( throttleState_ == 1 || throttleState_ == 2 );
		VRMenuObject * obj = app->GetVRMenuMgr().ToObject( HandleForId( app->GetVRMenuMgr(), ID_THROTTLE_MESSAGE ) );
		OVR_ASSERT( obj != NULL );	// menu items is simply missing if this happens -- why did you break it?
		if ( throttleState_ == 1 )		
		{
			String level1WarningText1;
			String level1WarningText2;
			VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/instr1_cool_down_l1", "@string/instr1_cool_down_l1", level1WarningText1 );
			VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/instr2_cool_down_l1", "@string/instr2_cool_down_l1", level1WarningText2 );
			BitmapFont & font = app->GetDefaultFont();
			font.WordWrapText( level1WarningText1, 1.25f );
			obj->SetText( level1WarningText1 + "\n" + level1WarningText2 );
		}
		else if ( throttleState_ == 2 )
		{
			String level2WarningText;
			VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/instr_cool_down_l2", "@string/instr_cool_down_l2", level2WarningText );
			BitmapFont & font = app->GetDefaultFont();
			font.WordWrapText( level2WarningText, 1.25f );
			obj->SetText( level2WarningText );
		}
		throttleState = throttleState_;
	}

	OvrThrottleMenu() : VRMenu( MENU_NAME ), throttleState( 0 ) { }
	virtual ~OvrThrottleMenu() { }

	void DismissWarning( App * app )
	{
		// Only Level 1 throttle warning message is dismissable
		if ( throttleState == 1 )
		{
			app->GetGuiSys().CloseMenu( app, MENU_NAME, false );
			app->ExitPlatformUI();
		}
	}

	virtual void    Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                    BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
	{
		bool touchPressed = ( vrFrame.Input.buttonPressed & ( BUTTON_TOUCH | BUTTON_A ) ) != 0;
		bool touchReleased = !touchPressed && ( vrFrame.Input.buttonReleased & ( BUTTON_TOUCH | BUTTON_A ) ) != 0;
		if ( touchReleased )
		{
			LOG( "Touch released" );
			DismissWarning( app );
		}
	}

	virtual bool	OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
	{
		if ( keyCode == AKEYCODE_BACK )
		{
			return true;	// just eat it
		}
		return false;
	}
};

VRMenuId_t OvrThrottleMenu::ID_THROTTLE_MESSAGE( VRMenu::GetRootId().Get() + 1 );
char const * OvrThrottleMenu::MENU_NAME = "Throttle";

OvrThrottleMenu * OvrThrottleMenu::Create( App * app, BitmapFont const & font )
{
	OvrThrottleMenu * menu = new OvrThrottleMenu;

	Vector3f fwd( 0.0f, 0.0f, 1.0f );
	Vector3f up( 0.0f, 1.0f, 0.0f );
	Vector3f right( fwd.Cross( up ) );

	Array< VRMenuObjectParms const * > parms;

	VRMenuFontParms fontParms( true, true, false, false, false, 1.0f );
	VRMenuObjectFlags_t itemFlags;
	VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
	Vector3f const hilightOfs( 0.0f, 0.0f, 0.05f );

	float const ROW_HEIGHT = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	{
		// LOCALIZE
		Posef pose( Quatf(), up * ROW_HEIGHT * 0.0f );
		Array< VRMenuComponent* > comps;
		VRMenuSurfaceParms surfParms;
		String level1WarningText;
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/instr_cool_down_l1", "@string/instr_cool_down_l1", level1WarningText );
		VRMenuObjectParms * textParms = new VRMenuObjectParms( VRMENU_STATIC, comps, surfParms, level1WarningText,
																pose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), 
																fontParms, ID_THROTTLE_MESSAGE, itemFlags, initFlags );
		parms.PushBack( textParms );
	}

	menu->InitWithItems( app->GetVRMenuMgr( ), font, 2.5f, VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ), parms );

	DeletePointerArray( parms );

	return menu;
}

//==============================================================
// OvrWarningMenu
class OvrWarningMenu : public VRMenu
{
public:
	static	VRMenuId_t	ID_TITLE;
	static	VRMenuId_t	ID_MESSAGE;
	static	VRMenuId_t	ID_ACCEPT;
	static char const *	MENU_NAME;

	static	OvrWarningMenu *	Create( App * app, BitmapFont const & font );

	OvrWarningMenu() : VRMenu( MENU_NAME ) { }
	virtual ~OvrWarningMenu() { }

	void DismissWarning( App * app )
	{
		app->GetGuiSys().CloseMenu( app, MENU_NAME, false );
		app->ExitPlatformUI();
	}

    virtual void    Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                    BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
	{
		bool touchPressed = ( vrFrame.Input.buttonPressed & ( BUTTON_TOUCH | BUTTON_A ) ) != 0;
		bool touchReleased = !touchPressed && ( vrFrame.Input.buttonReleased & ( BUTTON_TOUCH | BUTTON_A ) ) != 0;
		if ( touchReleased )
		{
			LOG( "Touch released" );
			DismissWarning( app );
			app->RecenterYaw( false );
		}
	}

	virtual bool	OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
	{
		if ( keyCode == AKEYCODE_BACK )
		{
			return true;	// just eat it
		}
		return false;
	}
};

VRMenuId_t OvrWarningMenu::ID_TITLE( VRMenu::GetRootId().Get() + 1 );
VRMenuId_t OvrWarningMenu::ID_MESSAGE( VRMenu::GetRootId().Get() + 2 );
VRMenuId_t OvrWarningMenu::ID_ACCEPT( VRMenu::GetRootId().Get() + 3 );
char const * OvrWarningMenu::MENU_NAME = "Warning";

OvrWarningMenu * OvrWarningMenu::Create( App * app, BitmapFont const & font )
{
	OvrWarningMenu * menu = new OvrWarningMenu;

	Vector3f fwd( 0.0f, 0.0f, 1.0f );
	Vector3f up( 0.0f, 1.0f, 0.0f );
	Vector3f right( fwd.Cross( up ) );

	Array< VRMenuObjectParms const * > parms;

	VRMenuFontParms fontParms( true, true, false, false, false, 6.0f );
	VRMenuObjectFlags_t itemFlags( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED );
	VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
	Vector3f const hilightOfs( 0.0f, 0.0f, 0.05f );

	{
		String text;
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/health_safty_warning_title", "@string/health_safty_warning_title", text );
		Posef pose( Quatf(), Vector3f( up * 4.0f ) );
		VRMenuSurfaceParms surfParms( "title" );
		VRMenuObjectParms * titleParms = new VRMenuObjectParms( VRMENU_STATIC, Array< VRMenuComponent* >(), surfParms, text, 
				pose, Vector3f( 1.70f, 1.70f, 1.70f ), Posef(), Vector3f( 1.0f ),
				fontParms, ID_TITLE, itemFlags, initFlags );
		parms.PushBack( titleParms );
	}

	{
		// LOCALIZE
		String text1;
		String text2;
		String text3;
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/health_safty_warning_description1", "@string/health_safty_warning_description1", text1 );
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/health_safty_warning_description2", "@string/health_safty_warning_description2", text2 );
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/health_safty_warning_description3", "@string/health_safty_warning_description3", text3 );
		font.WordWrapText( text1, 2.0f );

		Posef pose( Quatf(), up * 0.0f );
		Array< VRMenuComponent* > comps;
		VRMenuSurfaceParms surfParms;
		VRMenuObjectParms * textParms = new VRMenuObjectParms( VRMENU_STATIC, comps, surfParms, text1 + "\n\n" + text2 + "\n\n" + text3,
				pose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), 
				fontParms, ID_MESSAGE, itemFlags, initFlags );
		parms.PushBack( textParms );
	}

	menu->InitWithItems( app->GetVRMenuMgr( ), font, 20.0f, VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ), parms );

	DeletePointerArray( parms );

	return menu;
}

//==============================================================
// PlatformActivity
class PlatformActivity : public VrAppInterface
{
public:
						PlatformActivity();
	virtual				~PlatformActivity();

	virtual void		OneTimeInit( const char * launchIntent );
	virtual void		OneTimeShutdown();
	virtual void		NewIntent( const char * intent );
	virtual Matrix4f	DrawEyeView( const int eye, const float fovDegrees );
	virtual void		ConfigureVrMode( ovrModeParms & modeParms );
	virtual Matrix4f	Frame( VrFrame vrFrame );
	virtual void		Command( const char * msg );
	virtual bool		OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType );
	virtual void 		WindowDestroyed();

	OvrSceneView		Scene;

	VRMenu *			ActiveMenu;
	OvrConfirmQuitMenu *ConfirmQuitMenu;
	OvrGlobalMenu *		GlobalMenu;
	OvrThrottleMenu *	ThrottleMenu;
	OvrWarningMenu *	WarningMenu;
	GlGeometry			FloorQuad;
	GlProgram			FloorProg;
	GLuint				FloorTexId;
	bool				OpenedMenu;
	long long			OpenFrame;
};

//==============================
// PlatformActivity::PlatformActivity
PlatformActivity::PlatformActivity() :
	ActiveMenu( NULL ),
	ConfirmQuitMenu( NULL ),
	GlobalMenu( NULL ),
	ThrottleMenu( NULL ),
	WarningMenu( NULL ),
	FloorTexId( 0 ),
	OpenedMenu( false ),
	OpenFrame( -1 )
{
	LOG( "PlatformActivity()" );
}

//==============================
// PlatformActivity::~PlatformActivity
PlatformActivity::~PlatformActivity()
{
	LOG( "~PlatformActivity()" );

	// app->Shutdown() should already have been called and menus added to it already freed
	ActiveMenu = NULL;
	ConfirmQuitMenu = NULL;
	GlobalMenu = NULL;
	ThrottleMenu = NULL;
	WarningMenu = NULL;
}


//==============================
// PlatformActivity::OneTimeInit
void PlatformActivity::OneTimeInit( const char * launchIntent )
{
	LOG( "PlatformActivity::OneTimeInit( %s )", launchIntent );

	// Do NOT finish() on destroy
	((AppLocal *)app)->ExitOnDestroy = false;

	// build the floor quad
	FloorProg = BuildProgram( FloorVertexProgSrc, FloorFragmentProgSrc );

	FloorQuad = BuildTesselatedQuad( 1, 1 );

	int Width = 0;
	int Height = 0;
	FloorTexId = LoadTextureFromApplicationPackage( "res/raw/globalmenu_floor_plane.tga", TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), Width, Height );

	// don't load a scene model for now
	/*
		const char * scenePath = "/sdcard/Oculus/Tuscany.ovrscene";

		MaterialParms materialParms;
		materialParms.UseSrgbTextureFormats = ( app->GetVrParms().colorFormat == COLOR_8888_sRGB );
		Scene.LoadWorldModel( scenePath, materialParms );
		Scene.YawOffset = -M_PI / 2;
	*/
}

//==============================
// PlatformActivity::OneTimeShutdown
void PlatformActivity::OneTimeShutdown()
{
	LOG( "PlatformActivity::OneTimeShutdown" );

	// Free GL Resources
	FloorQuad.Free();
	DeleteProgram( FloorProg );
	if ( FloorTexId != 0 )
	{
		glDeleteTextures( 1, &FloorTexId );
		FloorTexId = 0;
	}
}

//==============================
// PlatformActivity::NewIntent
void PlatformActivity::NewIntent( const char * intent )
{
	LOG( "PlatformActivity::NewIntent( %s )", intent );
	VRMenu * newMenu = NULL;

	static const char * PUI_THROTTLED_1 = "throttled1";
	static const char * PUI_THROTTLED_2 = "throttled2";

	if ( ovr_GetPowerLevelStateMinimum() )
	{
		intent = PUI_THROTTLED_2;
		LOG( "PlatformActivity::NewIntent( %s ): OVERRIDE FOR MINIMUM MODE", intent );
	}

	if ( OVR_strcmp( intent, PUI_CONFIRM_QUIT ) == 0 )
	{
		ConfirmQuitMenu = (OvrConfirmQuitMenu*)app->GetGuiSys().GetMenu( OvrConfirmQuitMenu::MENU_NAME );
		if ( ConfirmQuitMenu == NULL )
		{
			ConfirmQuitMenu = OvrConfirmQuitMenu::Create( app, app->GetDefaultFont() );
			app->GetGuiSys().AddMenu( ConfirmQuitMenu );
		}
		newMenu = ConfirmQuitMenu;
	}
	else if ( ( OVR_strcmp( intent, PUI_THROTTLED_1 ) == 0 ) || ( OVR_strcmp( intent, PUI_THROTTLED_2 ) == 0 ) )
	{
		ThrottleMenu = (OvrThrottleMenu*)app->GetGuiSys().GetMenu( OvrThrottleMenu::MENU_NAME );
		if ( ThrottleMenu == NULL )
		{
			ThrottleMenu = OvrThrottleMenu::Create( app, app->GetDefaultFont() );
			app->GetGuiSys().AddMenu( ThrottleMenu );
		}
		newMenu = ThrottleMenu;
		if ( OVR_strcmp( intent, PUI_THROTTLED_1 ) == 0 ) 
		{
			ThrottleMenu->SetThrottleState( app, 1 );
		}
		else if ( OVR_strcmp( intent, PUI_THROTTLED_2 ) == 0 )
		{
			ThrottleMenu->SetThrottleState( app, 2 );
		}
	}
	else if ( OVR_strcmp( intent, PUI_WARNING ) == 0 )
	{
		WarningMenu = (OvrWarningMenu*)app->GetGuiSys().GetMenu( OvrWarningMenu::MENU_NAME );
		if ( WarningMenu == NULL )
		{
			WarningMenu = OvrWarningMenu::Create( app, app->GetDefaultFont() );
			app->GetGuiSys().AddMenu( WarningMenu );
		}
		newMenu = WarningMenu;
	}
	else // PUI_GLOBAL_MENU or PUI_GLOBAL_MENU_TUTORIAL
	{
		OVR_ASSERT( strstr( intent, PUI_GLOBAL_MENU ) == intent || strstr( intent, PUI_GLOBAL_MENU_TUTORIAL ) == intent );

		GlobalMenu = (OvrGlobalMenu*)app->GetGuiSys().GetMenu( OvrGlobalMenu::MENU_NAME );
		if ( GlobalMenu == NULL )
		{
			// create the menu
			GlobalMenu = OvrGlobalMenu::Create( app, app->GetVRMenuMgr(), app->GetDefaultFont() );
			app->GetGuiSys().AddMenu( GlobalMenu );
		}

		bool const showTutorial = OVR_strcmp( intent, PUI_GLOBAL_MENU_TUTORIAL ) == 0;
		GlobalMenu->SetShowTutorial( app->GetVRMenuMgr(), showTutorial );

		// clear so intent is not sent back if no launch package set
		GlobalMenu->SetLaunchPackage( "" );

		// Grab the package that launched this activity from the intent
		if ( strstr( intent, PUI_GLOBAL_MENU ) == intent )
		{
			String LaunchingPackage = String( intent + OVR_strlen( PUI_GLOBAL_MENU ) + 1 );
			LOG( "Launching Package %s", LaunchingPackage.ToCStr() );
			GlobalMenu->SetLaunchPackage( LaunchingPackage );
		}

		newMenu = GlobalMenu;
	}

	if ( ActiveMenu != newMenu && ActiveMenu != NULL )
	{
		ActiveMenu->Close( app, app->GetGazeCursor(), true );
	}

	OpenFrame = -1;
	OpenedMenu = false;
	ActiveMenu = newMenu;
}

//==============================
// PlatformActivity::Command
void PlatformActivity::Command( const char * msg )
{
}

//==============================
// PlatformActivity::DrawEyeView
Matrix4f PlatformActivity::DrawEyeView( const int eye, const float fovDegrees )
{
	glDisable( GL_SCISSOR_TEST );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	const Matrix4f mvp = Scene.DrawEyeView( eye, fovDegrees );

	glDisable( GL_SCISSOR_TEST );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	{
		glDisable( GL_BLEND );
		glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, FloorTexId );
		glUseProgram( FloorProg.program );

		Matrix4f floorMat = Matrix4f::Translation( Vector3f( 0.0f, -1.0f, 0.0f ) ) *
			Matrix4f::RotationX( Mathf::DegreeToRadFactor * -90.0f ) *
			Matrix4f::Scaling( 8.0f, 8.0f, 8.0f );

		Matrix4f mat = mvp * floorMat;
		glUniformMatrix4fv( FloorProg.uMvp, 1, GL_FALSE, mat.Transposed().M[0] );
		Vector4f color( 1.0f, 1.0f, 1.0f, 1.0f );
		glUniform4fv( FloorProg.uColor, 1, &color.x );
		FloorQuad.Draw();
	}
	return mvp;
}

void PlatformActivity::ConfigureVrMode( ovrModeParms & modeParms )
{
	LOG( "ConfigureVrMode: PlatformActivity only needs minimal clocks" );
	modeParms.CpuLevel = 1;
	modeParms.GpuLevel = 1;
	app->GetVrParms().multisamples = 2;
}

//==============================
// PlatformActivity::Frame
Matrix4f PlatformActivity::Frame( const VrFrame vrFrame )
{
	// disallow player movement
	VrFrame vrFrameWithoutMove = vrFrame;
	vrFrameWithoutMove.Input.sticks[0][0] = 0.0f;
	vrFrameWithoutMove.Input.sticks[0][1] = 0.0f;

	Scene.Frame( app->GetVrViewParms(), vrFrameWithoutMove, app->GetSwapParms().ExternalVelocity );

	//LOG( "App is %x, menu is %s", (int)app, app->GetGuiSys().IsMenuOpen( OvrGlobalMenu::MENU_NAME ) ? "open" : "closed" );
	if ( !OpenedMenu )
	{
		if ( OpenFrame < 0 )
		{
			OpenFrame = vrFrame.FrameNumber + 2;
		}
		if ( vrFrame.FrameNumber >= OpenFrame )
		{
			app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), ActiveMenu->GetName() );
			ActiveMenu->RepositionMenu( app );
			OpenedMenu = true;
		}
	}

	//-------------------------------------------
	// Render the two eye views, each to a separate texture, and TimeWarp
	// to the screen.
	//-------------------------------------------
	app->DrawEyeViewsPostDistorted( Scene.CenterViewMatrix() );

	return Scene.CenterViewMatrix();
}

//==============================
// PlatformActivity::OnKeyEvent
bool PlatformActivity::OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType )
{
	return false;
}

void PlatformActivity::WindowDestroyed()
{
	LOG( "PlatformActivity::WindowDestroyed()" );
}

} // namespace OVR

extern "C" {

JNIEXPORT jlong Java_com_oculusvr_vrlib_PlatformActivity_nativeSetAppInterface( JNIEnv * jni, jclass clazz, jobject activity )
{
	LOG( "nativeSetAppInterface");

	return ( new OVR::PlatformActivity() )->SetActivity( jni, clazz, activity );
}

} // extern "C"
