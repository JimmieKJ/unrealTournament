/************************************************************************************

Filename    :   FolderBrowser.h
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_FolderBrowser_h )
#define OVR_FolderBrowser_h

#include "VRMenu.h"
#include "MessageQueue.h"
#include "MetaDataManager.h"
#include "ScrollManager.h"

namespace OVR {

class OvrFolderBrowserRootComponent;
class OvrFolderSwipeComponent;
class OvrFolderBrowserSwipeComponent;
class OvrDefaultComponent;
class OvrPanel_OnUp;

//==============================================================
// OvrFolderBrowser
class OvrFolderBrowser : public VRMenu
{
public:
	struct PanelView
	{
		PanelView() 
			: Id( -1 )
		{}

		menuHandle_t			Handle;				// Handle to the panel		
		int						Id;					// Unique id for thumbnail loading
		Vector2f				Size;				// Thumbnail texture size
	};

	struct FolderView
	{
		FolderView( const String & name, const String & tag ) 
			: CategoryTag( tag )
			, LocalizedName( name )
			, MaxRotation( 0.0f ) 
		{}
		const String			CategoryTag;
		const String			LocalizedName;		// Store for rebuild of title
		menuHandle_t			Handle;				// Handle to main root - parent to both Title and Panels
		menuHandle_t			TitleRootHandle;	// Handle to the folder title root
		menuHandle_t			TitleHandle;		// Handle to the folder title
		menuHandle_t			SwipeHandle;		// Handle to root for panels
		menuHandle_t			ScrollBarHandle;	// Handle to the scrollbar object
		menuHandle_t			WrapIndicatorHandle;
		float					MaxRotation;		// Used by SwipeComponent 
		Array<PanelView>		Panels;
	};

	static char const *			MENU_NAME;
    static  VRMenuId_t			ID_CENTER_ROOT;

	virtual void						OneTimeInit( const OvrMetaData & metaData );
    // Builds the menu view using the passed in model. Builds only what's marked dirty - can be called multiple times.
	virtual void						BuildDirtyMenu( OvrMetaData & metaData );
	// Swiping when the menu is inactive can cycle through files in
	// the directory.  Step can be 1 or -1.
	virtual	const OvrMetaDatum *		NextFileInDirectory( const int step );
	// Called if touch up is activated without a focused item
	// User returns true if consumed
	virtual bool						OnTouchUpNoFocused()							{ return false; }

	FolderView *						GetFolderView( const String & categoryTag );
	FolderView *						GetFolderView( int index );
	MessageQueue &						GetTextureCommands()							{ return TextureCommands;  }
	void								SetPanelTextSpacingScale( const float scale )	{ PanelTextSpacingScale = scale; }
	void								SetFolderTitleSpacingScale( const float scale ) { FolderTitleSpacingScale = scale; }
	void								SetScrollBarSpacingScale( const float scale )	{ ScrollBarSpacingScale = scale; }
	void								SetScrollBarRadiusScale( const float scale )	{ ScrollBarRadiusScale = scale; }
	void								SetAllowPanelTouchUp( const bool allow )		{ AllowPanelTouchUp = allow; }

	enum RootDirection
	{
		MOVE_ROOT_NONE,
		MOVE_ROOT_UP,
		MOVE_ROOT_DOWN
	};
	// Attempts to scroll - returns false if not possible due to being at boundary or currently scrolling
	bool								ScrollRoot( const RootDirection direction, bool hideScrollHint = false );

	enum CategoryDirection
	{
		MOVE_PANELS_NONE,
		MOVE_PANELS_LEFT,
		MOVE_PANELS_RIGHT
	};
	bool								RotateCategory( const CategoryDirection direction );
	void								SetCategoryRotation( const int folderIndex, const int panelIndex );

	void 								TouchDown();
	void 								TouchUp();
	void 								TouchRelative( Vector3f touchPos );

	// Accessors
	const FolderView *					GetFolderView( int index ) const;
	int									GetNumFolders() const					{ return Folders.GetSizeI(); }
	int									GetCircumferencePanelSlots() const		{ return CircumferencePanelSlots; }
	float								GetRadius() const						{ return Radius; }
	float 								GetPanelHeight() const					{ return PanelHeight; }
	float 								GetPanelWidth() const					{ return PanelWidth; }
	// The index for the folder that's at the center - considered the actively selected folder
	int									GetActiveFolderIndex() const;
	// Returns the number of panels shown per folder - or Swipe Component
	int									GetNumSwipePanels() const				{ return NumSwipePanels; }
	unsigned							GetThumbWidth() const					{ return ThumbWidth; }
	unsigned							GetThumbHeight() const					{ return ThumbHeight;  }
	bool								HasNoMedia() const						{ return NoMedia; }
	bool								GazingAtMenu() const;
	void								SetWrapIndicatorVisible( FolderView & folder, const bool visible );

	OvrFolderSwipeComponent * 			GetSwipeComponentForActiveFolder();

	void								SetScrollHintVisible( const bool visible );
	void								SetActiveFolder( int folderIdx );

	eScrollDirectionLockType			GetControllerDirectionLock()				{ return ControllerDirectionLock; }
	eScrollDirectionLockType			GetTouchDirectionLock()						{ return TouchDirectionLocked; }
	bool								ApplyThumbAntialiasing( unsigned char * inOutBuffer, int width, int height ) const;

protected:
	OvrFolderBrowser( App * app,
				OvrMetaData & metaData,
				float panelWidth,
				float panelHeight,
				float radius,
				unsigned numSwipePanels,
				unsigned thumbWidth, 
				unsigned thumbHeight );

    virtual ~OvrFolderBrowser();

	//================================================================================
	// Subclass protected interface

	// Called when building a panel
	virtual const char *		GetPanelTitle( const OvrMetaDatum & panelData ) = 0;

	// Called when a panel is activated
	virtual void				OnPanelActivated( const OvrMetaDatum * panelData ) = 0;

	// Called on a background thread
	// The returned memory buffer will be free()'d after writing the thumbnail.
	// Return NULL if the thumbnail couldn't be created.
	virtual unsigned char *		CreateAndCacheThumbnail( const char * soureFile, const char * cacheDestinationFile, int & outWidth, int & outHeight ) = 0;

	// Called on a background thread to load thumbnail
	virtual	unsigned char *		LoadThumbnail( const char * filename, int & width, int & height ) = 0;

	// Adds thumbnail extension to a file to find/create its thumbnail
	virtual String				ThumbName( const String & s ) = 0;

	// Media not found - have subclass set the title, image and caption to display
	virtual void				OnMediaNotFound( App * app, String & title, String & imageFile, String & message ) = 0;

	// Optional interface
	//
	// Request external thumbnail - called on main thread
	virtual unsigned char *		RetrieveRemoteThumbnail( const char * url, const char * cacheDestinationFile, int & outWidth, int & outHeight ) { return NULL; }

	// If we fail to load one type of thumbnail, try an alternative
	virtual String				AlternateThumbName( const String & s ) { return String(); }

	// Called on opening menu
	virtual void				OnBrowserOpen() {}
	
	//================================================================================
	
	// OnEnterMenuRootAdjust is set to be performed the 
	// next time the menu is opened to adjust for a potentially deleted or added category
	void						SetRootAdjust( const RootDirection dir )	{ OnEnterMenuRootAdjust = dir;  }
	RootDirection				GetRootAdjust() const						{ return OnEnterMenuRootAdjust; }

	// Rebuilds a folder using passed in data
	void						RebuildFolder( OvrMetaData & metaData, const int folderIndex, const Array< const OvrMetaDatum * > & data );

protected:
	App *						AppPtr;

private:
	static void *		ThumbnailThread( void * v );
	pthread_t			ThumbnailThreadId;
	void				LoadThumbnailToTexture( const char * thumbnailCommand );

	friend class OvrPanel_OnUp;
	void				OnPanelUp( const OvrMetaDatum * data );

    virtual void        Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                        BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );
	virtual void		Open_Impl( App * app, OvrGazeCursor & gazeCursor );

	void				BuildFolder( OvrMetaData::Category & category, FolderView * const folder, const OvrMetaData & metaData, VRMenuId_t foldersRootId, const int folderIndex );
	void				LoadFolderPanels( const OvrMetaData & metaData, const OvrMetaData::Category & category, const int folderIndex, FolderView & folder,
							 Array< VRMenuObjectParms const * > & outParms );
	void				AddPanelToFolder( const OvrMetaDatum * panoData, const int folderIndex, FolderView & folder, Array< VRMenuObjectParms const * >& outParms );
	void				DisplaceFolder( int index, const Vector3f & direction, float distance, bool startOffSelf = false );
	void				UpdateFolderTitle( const FolderView * folder  );
	float				CalcFolderMaxRotation( const FolderView * folder ) const;

	// Members
	VRMenuId_t			FoldersRootId;
	float				PanelWidth;
	float				PanelHeight;
	int					ThumbWidth;
	int					ThumbHeight;
	float				PanelTextSpacingScale;		// Panel text placed with vertical position of -panelHeight * PanelTextSpacingScale
	float				FolderTitleSpacingScale;	// Folder title placed with vertical position of PanelHeight * FolderTitleSpacingScale
	float				ScrollBarSpacingScale;		// Scroll bar placed with vertical position of PanelHeight * ScrollBarSpacingScale
	float				ScrollBarRadiusScale;		// Scroll bar placed with horizontal position of FWD * Radius * ScrollBarRadiusScale 

	int					CircumferencePanelSlots;
	unsigned			NumSwipePanels;
	float				Radius;
	float				VisiblePanelsArcAngle;
	bool				SwipeHeldDown;
	float				DebounceTime;
	bool				NoMedia;
	bool				AllowPanelTouchUp;

	Array< FolderView * >	Folders;

	menuHandle_t 		ScrollSuggestionRootHandle;

	RootDirection		OnEnterMenuRootAdjust;
	
	// Checked at Frame() time for commands from the thumbnail/create thread
	MessageQueue		TextureCommands;

	// Create / load thumbnails by background thread
	struct OvrCreateThumbCmd
	{
		String SourceImagePath;
		String ThumbDestination;
		String LoadCmd;
	};
	Array< OvrCreateThumbCmd > ThumbCreateAndLoadCommands;
	MessageQueue		BackgroundCommands;
	Array< String >		ThumbSearchPaths;
	String				AppCachePath;

	// Keep a reference to Panel texture used for AA alpha when creating thumbnails
	static unsigned char *		ThumbPanelBG;
	
	// Default panel textures (base and highlight) - loaded once 
	GLuint				DefaultPanelTextureIds[ 2 ];

	// Restricted Scrolling
	static const float 						CONTROLER_COOL_DOWN = 0.2f; // Controller goes to rest very frequently so cool down helps
	eScrollDirectionLockType				ControllerDirectionLock;
	float									LastControllerInputTimeStamp;

	static const float 						SCROLL_DIRECTION_DECIDING_DISTANCE = 10.0f;
	bool									IsTouchDownPosistionTracked;
	Vector3f 								TouchDownPosistion; // First event in touch relative is considered as touch down position
	eScrollDirectionLockType				TouchDirectionLocked;

	int										MediaCount; // Used to determine if no media was loaded
};

} // namespace OVR

#endif // OVR_GlobalMenu_h
