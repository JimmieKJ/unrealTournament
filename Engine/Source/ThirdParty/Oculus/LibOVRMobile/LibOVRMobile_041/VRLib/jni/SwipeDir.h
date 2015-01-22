/************************************************************************************

Filename    :   SwipeDir.h
Content     :   Directory browsing with SwipeViews
Created     :   July 25, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


************************************************************************************/

#ifndef OVR_SwipeDir_h
#define OVR_SwipeDir_h

#include "SwipeView.h"
#include "MessageQueue.h"
#include "Kernel/OVR_Array.h"
#include "App.h"

namespace OVR
{

class SearchPaths;
class SwipeDir;

class SwipeDirLevel
{
public:
	SwipeDirLevel() : Root( NULL ), Swipe( NULL ) {};

	SwipeDir *	Root;
	String		FullPath;
	SwipeView * Swipe;
};

class SwipeDir
{
public:
	// Starts a background thread to load thumbnails
	SwipeDir( const char *	relativeRootDir,	// Should include a trailing slash
			App & 			app,		// For playing sounds, gaze cursor, font, and fontSurface
			unsigned 		defaultFileTexture,
			unsigned 		defaultDirectoryTexture,
			float			panelWidth,		// in radians
			float			panelHeight,
			float			panelGap,
			int				panelRows );

	// Frees all the created panels and images (not the defaults)
	virtual ~SwipeDir();

	// Returns the full path to the selected file, or empty string.
	String		Frame( App * app, const VrFrame & vrFrame, const Matrix4f & view, SearchPaths const & searchPaths );

	// Swiping when the view is inactive can cycle through files in
	// the directory.  Step can be 1 or -1.
	String		NextFileInDirectory( const int step );

	// Handle key events
	bool 		OnKeyEvent(const int keyCode, const KeyState::eKeyEventType eventType);

	// Called for each stereo eye
	void		Draw( const Matrix4f & mvp );

	// Returns to the state it was last closed from.
	void		Activate( SearchPaths const & searchPaths );

	// Back out one directory level
	void		BackDirectory();

	// True if opening or active, false of closing or closed.
	bool		IsActive();

	// True if currently at root directory
	bool		AtRoot() const;

	//================================================================================
	// Subclass interface

	// Decide if a file should be included
	// Names that match the thumbnail patter will have already been excluded.
	virtual bool ShouldAddFile( const char * filename ) = 0;

	// Called on a background thread
	//
	// Create the thumbnail image for the file, which will
	// be saved out as a _thumb.jpg.
	//
	// The returned memory buffer will be free()'d after writing the thumbnail.
	//
	// Return NULL if the thumbnail couldn't be created.
	virtual unsigned char * CreateThumbnail( const char * filename, int & width, int & height ) = 0;

	// Adds thumbnail extension to a file to find/create its thumbnail
	virtual String	ThumbName( const String s ) = 0;

	// Adds thumbnail extension to a directory to find/create its thumbnail
	virtual String	DirectoryThumbName( const String s ) = 0;

	// Optional function called at end of EnterDirectory
	virtual void PostEnterDirectory( SwipeDirLevel * dir, bool IsRoot );
	//================================================================================

private:
	void	EnterDirectory( SearchPaths const & searchPaths, const char * dirName );
	static void * ThumbnailThread( void * v );

	String				RelativeRootDir;
	App &		 		ParentApp;				// For playing sounds, gaze cursor, font, and fontSurface
	unsigned 			DefaultFileTexture;
	unsigned 			DefaultDirectoryTexture;
	float				PanelWidth;			// in radians
	float				PanelHeight;
	float				PanelGap;
	int					PanelRows;

	// Checked at Frame() time for commands from the thumbnail/create thread
	MessageQueue		TextureCommands;

	// Create / load thumbnails by background thread
	MessageQueue		BackgroundCommands;

	// Path[0] is the root directory
	// Path[Path.GetSize()-1] is the current directory
	// Back pops up one directory level.
	Array<SwipeDirLevel *>	Path;

	// All directories, some of which may not be in the current back chain
	Array<SwipeDirLevel *>	Directories;

};

}

#endif	// OVR_SwipeDir_h
