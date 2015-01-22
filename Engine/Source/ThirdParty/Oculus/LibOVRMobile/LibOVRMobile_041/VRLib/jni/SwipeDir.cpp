/************************************************************************************

Filename    :   SwipeDir.cpp
Content     :   Directory browsing with SwipeViews
Created     :   July 25, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


************************************************************************************/

#include "SwipeDir.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <android/keycodes.h>
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "SwipeView.h"
#include "Log.h"
#include "unzip.h"
#include "3rdParty/stb/stb_image.h"
#include "3rdParty/stb/stb_image_write.h"
#include "OVR_CAPI.h"
#include "VrCommon.h"
#include "Kernel/OVR_StringHash.h"
#include "SearchPaths.h"

// TODO:
// close swipedir with a non-selected click
// treat high velocity touches as swipes even if short
// Make directory icons out of the first issue in each subdirectory
// Show count in directory somehow?
// Faded background behind text
// edge blends over overlay screen aren't right
// Switch to chromatic when swipeview is up
// Gaze cursor ghosts look wrong in some directions
// gaze cursor constant depth
// sound pool glitch
// Screen lighting
// edge fades to help with whites
// end-of-book notices
// deghost
// save swipeview position in state file
// Mistaking quick swipes for double tap
// Touch glow
// Texture uploads and mip maps in pieces
// Match swipe view pop distance to swipe distance
// bad y range select in swipe view
// Slightly shift viewpointup to bias center up
// Double tap to put away swipe view?
// stick yaw in swipe view
// Show partially read flag on swipe view panels
// Don't allow moving when swipeview is up
// Consider page-at-a-time swiping
// last page detection
// don't include empty directories

// sort all existing thumbnail loads ahead of creates
// test with /sdcard root
// Directory refresh
// redundant directory entry on startup

namespace OVR
{

void * SwipeDir::ThumbnailThread( void * v )
{
	int result = pthread_setname_np( pthread_self(), "SwipeDir" );
	if ( result != 0 )
	{
		LOG( "SwipeDir: pthread_setname_np failed %s", strerror( result ) );
	}

	SwipeDir * swd = (SwipeDir *)v;
	SearchPaths sp;

	for ( ; ; )
	{
		swd->BackgroundCommands.SleepUntilMessage();
		const char * msg = swd->BackgroundCommands.GetNextMessage();
		LOG( "BackgroundCommands: %s", msg );
		if ( MessageQueue::MatchesHead( "load ", msg ) )
		{
			int panelId;
			Array<SwipePanel>	* panels;
			sscanf( msg, "load %p %i", &panels, &panelId );
			const char * fileName = strstr( msg, ":" ) + 1;

			const String fullPath = sp.GetFullPath( fileName );

			unsigned char * data;
			int		width;
			int		height;
			data = stbi_load( fullPath.ToCStr(), &width, &height, NULL, 4 );
			if ( !data )
			{
				LOG( "Couldn't load %s", fileName );
			}
			else
			{
				swd->TextureCommands.PostPrintf( "thumb %p %i %p %i %i",
					&( *panels ), panelId, data, width, height );
			}
			free( (void *)msg );
		}
		else if ( MessageQueue::MatchesHead( "create ", msg ) )
		{
			const String fullPath = sp.GetFullPath( msg + strlen( "create " ) );

			int	width = 0;
			int height = 0;
			unsigned char * data = swd->CreateThumbnail( fullPath.ToCStr( ), width, height );

			// Should we write out a trivial thumbnail if the create failed?
			if ( data )
			{
				// write it out
				const String	tn = swd->ThumbName( fullPath );

			//String bmpName( fileName );
			//bmpName += ".bmp";
			//LOG( "Write bmp %s", bmpName.ToCStr() );
			//stbi_write_bmp( bmpName.ToCStr(), outW, outH, 4, (unsigned char *)out.Buffer );

				WriteJpeg( tn.ToCStr(), data, width, height );
				free( data );
			}

		}
		else
		{
			LOG( "SwipeDir::ThumbnailThread received unhandled message: %s", msg );
			OVR_ASSERT( false );
			free( (void *)msg );
		}
	}

	return NULL;
}



// Change the size and texture of an existing panel now that the thumbnail
// image has been loaded by the background thread.
void LoadThumbnailToTexture( const char * thumbnailCommand )
{
	Array<SwipePanel> * panels;
	SwipePanel * panel = NULL;
	int panelId;
	unsigned char * data;
	int width;
	int height;

	sscanf( thumbnailCommand, "thumb %p %i %p %i %i", &panels, &panelId, &data, &width, &height );

	// find panel using panelId
	const UPInt numPanels = panels->GetSize( );
	for ( UPInt index = 0; index < numPanels; ++index )
	{
		SwipePanel& currentPanel = panels->At( index );
		if ( currentPanel.Id == panelId )
		{
			panel = &currentPanel;
			break;
		}
	}

	assert( panel );

	const int max = Alg::Max( width, height );
	panel->Size[0] *= (float)width / max;
	panel->Size[1] *= (float)height / max;

	panel->Texture = LoadRGBATextureFromMemory(
			data, width, height, true /* srgb */ ).texture;

	BuildTextureMipmaps( panel->Texture );
	MakeTextureTrilinear( panel->Texture );
	MakeTextureClamped( panel->Texture );

	free( data );
}

void SwipeDir::EnterDirectory( const SearchPaths & searchPaths, const char * dirName )
{
	LOG( "EnterDirectory( %s )", dirName );

	// See if it is in the current path
	const int topLevel = Path.GetSize()-1;
	for ( int level = topLevel ; level >= 0 ; level-- )
	{
		if ( Path[level]->FullPath == dirName )
		{	// remove everything after this in path
			const int remove = topLevel-level;
			LOG( "Removing %i levels from path", remove );
			if ( remove > 0 )
			{
				// close the topmost
				Path[topLevel]->Swipe->Close();

				Path.RemoveMultipleAt( level+1, remove );
			}
			// open this level
			Path[level]->Swipe->Activate( );
			return;
		}

	}

	// close the topmost
	if ( topLevel >= 0 )
	{
		Path[topLevel]->Swipe->Close();
	}

	// See if the directory is already created
	for ( int i = 0 ; i < Directories.GetSize() ; i++ )
	{
		if ( Directories[i]->FullPath == dirName )
		{	// push it on the path and open it
			LOG( "Adding existing directory to path" );
			Path.PushBack( Directories[i] );
			// open this level
			Path[Path.GetSize()-1]->Swipe->Activate( );
			return;
		}
	}

	// Create a new directory view
	LOG( "Adding new directory to path" );
	SwipeDirLevel * newLev = new SwipeDirLevel();
	newLev->Root = this;
	newLev->FullPath = dirName;
	newLev->Swipe = new SwipeView();
	newLev->Swipe->LayoutRows = PanelRows;
	newLev->Swipe->RowOffset = 0;
	newLev->Swipe->SlotSize.x = PanelWidth + 0.1;
	newLev->Swipe->SlotSize.y = PanelHeight + 0.05;

	// Find all the files - checks all search paths
	StringHash< String > uniqueFileList = RelativeDirectoryFileList( searchPaths, dirName );
	Array<String> fileList;
	for ( StringHash< String >::ConstIterator iter = uniqueFileList.Begin( ); iter != uniqueFileList.End( ); ++iter )
	{
		fileList.PushBack( iter->First );
	}
	SortStringArray( fileList );

#if 0
	Array<String> fileList = DirectoryFileList( dirName );
#endif 

	// Build panels and thumbnail commands
	for ( int i = 0 ; i < fileList.GetSize() ; i++ )
	{
		const String & s = fileList[i];

		// Create the panel for it
		const int panelIndex = newLev->Swipe->Panels.GetSize();

		if ( MatchesExtension( s, "/" ))
		{	// subdirectory
			SwipePanel	panel;
			// full name in Identifier, just base in Text
			panel.Identifier = strdup( s.ToCStr() );
			String title( ExtractFileBase( s ) );

			panel.Text = strdup( title.ToCStr() );

			panel.Size.x = PanelWidth;
			panel.Size.y = PanelHeight;
			panel.SelectState = 0.0f;
			panel.Texture = DefaultDirectoryTexture;

			panel.Id = i + 1000;

			newLev->Swipe->Panels.PushBack( panel );

			// load folder thumbnail
			const String thumbName = DirectoryThumbName(s);

			char	cmd[1024];
			sprintf( cmd, "load %p %i:%s", &newLev->Swipe->Panels, panel.Id, thumbName.ToCStr( ) );
			BackgroundCommands.PostString(String(cmd));

			continue;
		}

		// If this is a thumbnail, skip it

		// See if we want this loose-file
		if ( !ShouldAddFile( s.ToCStr() ) )
		{
			continue;
		}

		SwipePanel	panel;
		// full name in Identifier, just base in Text
		panel.Identifier = strdup( s.ToCStr() );
		panel.Text = strdup( ExtractFileBase( s ).ToCStr() );

		panel.Size.x = PanelWidth;
		panel.Size.y = PanelHeight;
		panel.SelectState = 0.0f;
		panel.Texture = DefaultFileTexture;

		panel.Id = i + 1000;

		newLev->Swipe->Panels.PushBack( panel );

		// If there is not a matching .jpg in the next several files
		// (there might be other extensions, like .bmp present)
		// make a create thumbnail command before making the
		// load thumbnail command.
		const String	thumbName = ThumbName( s );
		for ( int check = 1 ; check < 5 ; check++ )
		{
			if ( i + check < fileList.GetSize() && fileList[i+check] == thumbName )
			{	// don't need to create
				break;
			}
			if ( check == 4 )
			{
				String	cmd( "create " );
				cmd += s;
				BackgroundCommands.PostString( cmd );
			}
		}
		char	cmd[1024];
		sprintf( cmd, "load %p %i:%s", &newLev->Swipe->Panels, panel.Id, thumbName.ToCStr( ) );
		BackgroundCommands.PostString( String( cmd ) );
	}


	// Init the swipe view now that the panels have been populated
	newLev->Swipe->Init( ParentApp.GetGazeCursor() );

	// Add it to both the path and directories list
	Directories.PushBack( newLev );
	Path.PushBack( newLev );

	// open this level
	Path[Path.GetSize()-1]->Swipe->Activate( );

	// Allow subclasses to do any additional procedures
	PostEnterDirectory( newLev, Directories.GetSize() == 1 );
}

SwipeDir::SwipeDir( const char *	rootDir,	// Should include a trailing slash
		App & 			app,		// For playing sounds, gaze cursor, font, and fontSurface
		unsigned 		defaultFileTexture,
		unsigned 		defaultDirectoryTexture,
		float			panelWidth,		// in radians
		float			panelHeight,
		float			panelGap,
		int				panelRows )
: RelativeRootDir( rootDir ), ParentApp( app ), DefaultFileTexture( defaultFileTexture ),
  DefaultDirectoryTexture( defaultDirectoryTexture ), PanelWidth( panelWidth ),
  PanelHeight( panelHeight ), PanelGap( panelGap ), PanelRows( panelRows ), TextureCommands( 10000 ), BackgroundCommands( 10000 )
{
	// add trailing slash to rootDir if necessary
	const int l = strlen( rootDir );
	if ( l > 0 && rootDir[ l-1 ] != '/' )
	{
		RelativeRootDir = RelativeRootDir + "/";
	}

	// spawn the thumbnail loading thread with the command list
	pthread_t loadingThread;
	const int createErr = pthread_create( &loadingThread, NULL /* default attributes */,
			&ThumbnailThread, this );
	if ( createErr != 0 )
	{
		LOG( "pthread_create returned %i", createErr );
	}

	// We can't enter the root directory in the constructor, because the
	// vtbl hasn't been set up to allow calling ShouldAddFile() yet.
}

void SwipeDir::Activate( SearchPaths const & searchPaths )
{
	// activate on next frame
	if ( Path.GetSize() > 0 )
	{
		Path[Path.GetSize()-1]->Swipe->Activate( );
	}
	else
	{
		// Enter the root directory
		EnterDirectory( searchPaths, RelativeRootDir );
	}
}

bool	SwipeDir::AtRoot() const
{
	return RelativeRootDir == Path[Path.GetSize() - 1]->FullPath;
}

bool 	SwipeDir::OnKeyEvent(const int keyCode, const KeyState::eKeyEventType eventType)
{
	// if back button or controller button B pressed
	if (((keyCode == AKEYCODE_BACK) && (eventType == KeyState::KEY_EVENT_SHORT_PRESS)) ||
		((keyCode == KEYCODE_B) && (eventType == KeyState::KEY_EVENT_UP)))
	{
		if (IsActive() && AtRoot()) // if SwipeView shown and at root, prompt exit
		{
			return false;
		}
		else  // otherwise go up a level
		{
			BackDirectory();
			return true;
		}
	}

	return false;
}

// Back out one directory level
void	SwipeDir::BackDirectory()
{
	if ( Path.GetSize() > 0 )
	{
		Path.PopBack(1);
		if ( Path.GetSize() > 0 )
		{
			Path[Path.GetSize()-1]->Swipe->Activate( );
		}
	}
}


String SwipeDir::Frame( App * app, const VrFrame & vrFrame, const Matrix4f & view, SearchPaths const & searchPaths )
{
	// Check for thumbnail loads
	while( 1 )
	{
		const char * cmd = TextureCommands.GetNextMessage();
		if ( !cmd )
		{
			break;
		}

		LOG( "TextureCommands: %s", cmd );
		LoadThumbnailToTexture( cmd );
		free( (void *)cmd );
	}


	// Process active swipe view
	if ( Path.GetSize() == 0 )
	{
		return String("");
	}

	SwipeView * swipe = Path[Path.GetSize()-1]->Swipe;

	const SwipeAction swipeAct = swipe->Frame( app->GetGazeCursor(), app->GetDefaultFont(),
            app->GetWorldFontSurface(), vrFrame, view,
            !app->IsGuiOpen() && !app->IsPassThroughCameraEnabled() );

	if ( swipeAct.PlaySndTouchActive )
	{	// touch down on active panel
		app->PlaySound( "sv_touch_active" );
	}
	else if ( swipeAct.PlaySndTouchInactive )
	{	// touch down in inactive area for drag
		app->PlaySound( "sv_touch_inactive" );
	}
	else if ( swipeAct.PlaySndActiveToDrag )
	{	// dragged enough to change from active to drag
		app->PlaySound( "sv_active_to_drag" );
	}
	else if ( swipeAct.PlaySndRelease )
	{	// release wihout action
		app->PlaySound( "sv_release" );
	}
	else if ( swipeAct.PlaySndSelect )
	{	// gaze goes on panel
		app->PlaySound( "sv_select" );
	}
	else if ( swipeAct.PlaySndDeselect )
	{	// gaze goes off panel
		app->PlaySound( "sv_deselect" );
	}

	if ( swipeAct.PlaySndReleaseAction )
	{	// release and action
		app->PlaySound( "sv_release_active" );

		if ( swipeAct.ActivatePanelIndex != -1 )
		{
			swipe->Close();
			const String selected = (char *)swipe->Panels[swipeAct.ActivatePanelIndex].Identifier;

			if ( MatchesExtension( selected, "/" ) )
			{	// enter directory
				EnterDirectory( searchPaths, selected );
				return String("");
			}
			return selected;
		}
	}

	return String("");
}


// Called for each stereo eye
void	SwipeDir::Draw( const Matrix4f & mvp )
{
	if ( Path.GetSize() == 0 )
	{
		return;
	}
	Path[Path.GetSize()-1]->Swipe->Draw( mvp );
}

bool	SwipeDir::IsActive()
{
	if ( Path.GetSize() == 0 )
	{
		return false;
	}
	return Path[Path.GetSize()-1]->Swipe->State != SVS_CLOSED;

}

// Frees all the created panels and images (not the defaults)
SwipeDir::~SwipeDir()
{
	// TODO!
}


String		SwipeDir::NextFileInDirectory( const int step )
{
	if ( Path.GetSize() == 0 )
	{
		return "";
	}
	SwipeView * swipe = Path[Path.GetSize()-1]->Swipe;
	if ( !swipe )
	{
		return "";
	}
	// We may have to skip over many directories to get to
	// the next file.
	for ( int i = 0 ; i < swipe->Panels.GetSize() ; i++ )
	{
		swipe->SelectedPanel =
				( swipe->SelectedPanel + step + swipe->Panels.GetSize() )
				% swipe->Panels.GetSize();
		const String selected = (char *)swipe->Panels[swipe->SelectedPanel].Identifier;

		if ( !MatchesExtension( selected, "/" ) )
		{
			return selected;
		}
	}

	// Empty, or nothing but directories.
	return "";
}

void SwipeDir::PostEnterDirectory(SwipeDirLevel * dir, bool IsRoot ) {}

}	// namespace OVR
