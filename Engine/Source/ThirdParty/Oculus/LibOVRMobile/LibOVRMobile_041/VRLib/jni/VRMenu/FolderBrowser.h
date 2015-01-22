/************************************************************************************

Filename    :   FolderBrowser.h
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_FolderBrowser_h )
#define OVR_FolderBrowser_h

#include "VRMenu.h"
#include "MessageQueue.h"
#include "Kernel/OVR_StringHash.h"

namespace OVR {

class SearchPaths;
class OvrFolderBrowserRootComponent;
class OvrFolderSwipeComponent;
class OvrFolderBrowserSwipeComponent;
class OvrDefaultComponent;
class JSON;
//==============================================================
// OvrMetaData
struct OvrMetaDatum
{
	OvrMetaDatum( int i ) : JsonIndex( i ) {}
	const int JsonIndex;		//Index into JSON array
	Array< String > Tags;
	String Url;
	String Title;
	String Author;
};

enum TagAction
{
	TAG_ADDED,
	TAG_REMOVED,
	TAG_ERROR
};

struct OvrMetaDataFileExtensions
{
	Array< String > GoodExtensions;
	Array< String > BadExtensions;
};

class OvrMetaData
{
public:
	static const char * CATEGORIES;
	static const char * DATA;
	static const char * TITLE;
	static const char * URL;
	static const char * FAVORITES_TAG;
	static const char * NAME;
	static const char * TAGS;
	static const char * CATEGORY;
	static const char * URL_INNER;
	static const char * TITLE_INNER;
	static const char * AUTHOR_INNER;

	struct Category
	{
		String CategoryName;
		Array< int > DatumIndicies;
	};

	OvrMetaData() {}
	~OvrMetaData() {}

	// Init meta data from contents on disk
	void InitFromDirectory( const char * relativePath, const SearchPaths & paths, const OvrMetaDataFileExtensions & fileExtensions );

	// Directory contents are read in and we reconcile against meta data if it exists
	void InitFromDirectoryMergeMeta( const char * relativePath, const SearchPaths & paths, const OvrMetaDataFileExtensions & fileExtensions, const char * metaFile, const char * packageName );

	// Retitle Categories
	void RenameCategory( const char * currentName, const char * newName );

	// Adds or removes tag and returns action taken
	TagAction ToggleTag( OvrMetaDatum * data, const String & tag );

	void						AddCategory( const String & name );

	const Array< Category > 	GetCategories() const 					{ return Categories; }
	const Category & 			GetCategory( const int index ) const 	{ return Categories.At( index ); }
	const OvrMetaDatum &		GetMetaDatum( const int index ) const;
	bool 						GetMetaData( const Category & category, Array< const OvrMetaDatum * > & outMetaData ) const;

private:
	JSON *						MetaDataToJson();
	void						WriteMetaFile( const char * metaFile ) const;
	Category * 					GetCategory( const String & categoryName );
	bool 						ShouldAddFile( const char * filename, const OvrMetaDataFileExtensions & fileExtensions );
	void						ExtractMetaData( JSON * dataFile, const SearchPaths & paths, Array< OvrMetaDatum > & outMetaData ) const;
	void						RegenerateCategoryIndices();
	void						ReconcileData( Array< String > & storedCategories, Array< OvrMetaDatum > & storedMetaData );

	String 						FilePath;
	Array< Category >			Categories;
	Array< OvrMetaDatum >		MetaData;
	StringHash< int >			UrlToIndex;
};


//==============================================================
// OvrFolderBrowser
class OvrFolderBrowser : public VRMenu
{
public:
	struct Panel
	{
		Panel() 
		: Data( NULL )
		, Id( -1 )
		{}

		OvrMetaDatum *		 	Data;				// Datum in OvrMetaData - payload
		menuHandle_t			Handle;				// Handle to the panel
		int						Id;					// Unique id for thumbnail loading
		Vector2f				Size;				// Thumbnail texture size
	};

	struct Folder
	{
		Folder( const String & name ) : Name( name ), MaxRotation( 0.0f ) {}
		const String	Name;						// Store for rebuild of title
		menuHandle_t	Handle;						// Handle to main root - parent to both Title and Panels
		menuHandle_t	TitleRootHandle;			// Handle to the folder title root
		menuHandle_t	TitleHandle;				// Handle to the folder title
		menuHandle_t	SwipeHandle;				// Handle to root for panels
		float			MaxRotation;				// Used by SwipeComponent 
		Array<Panel>	Panels;
	};

	static char const *	MENU_NAME;
    static  VRMenuId_t  ID_CENTER_ROOT;

	// Swiping when the menu is inactive can cycle through files in
	// the directory.  Step can be 1 or -1.
	virtual	OvrMetaDatum *	NextFileInDirectory( const int step );

	void				BuildMenu();

	// Returns pointer to active panel's data or NULL if none selected
	OvrMetaDatum	*	QueryActivePanel();
	
	Folder &			GetFolder( int index );
	void				GetFolderData( const int folderIndex, Array< OvrMetaDatum * >& data );
	void				SetPanelTextSpacingScale( const float scale )	{ PanelTextSpacingScale = scale; }
	void				SetFolderTitleSpacingScale( const float scale ) { FolderTitleSpacingScale = scale; }
	void				SetAllowPanelTouchUp( const bool allow )		{ AllowPanelTouchUp = allow; }

	enum RootDirection
	{
		MOVE_ROOT_NONE,
		MOVE_ROOT_UP,
		MOVE_ROOT_DOWN
	};
	// Attempts to scroll - returns false if not possible due to being at boundary or currently scrolling
	bool				ScrollRoot( const RootDirection direction );

	enum CategoryDirection
	{
		MOVE_PANELS_NONE,
		MOVE_PANELS_LEFT,
		MOVE_PANELS_RIGHT
	};
	bool				RotateCategory( const CategoryDirection direction );
	void				SetCategoryRotation( const int folderIndex, const int panelIndex );

	// Accessors
	const Folder &		GetFolder( int index ) const;
	int					GetNumFolders() const					{ return Folders.GetSizeI(); }
	int					GetCircumferencePanelSlots() const		{ return CircumferencePanelSlots; }
	float				GetRadius() const						{ return Radius; }
	float 				GetPanelHeight() const					{ return PanelHeight; }
	float 				GetPanelWidth() const					{ return PanelWidth; }
	// The index for the folder that's at the center - considered the actively selected folder
	int					GetActiveFolderIndex() const;
	// Returns the number of panels shown per folder - or Swipe Component
	int					GetNumSwipePanels() const				{ return NumSwipePanels; }
	unsigned			GetThumbWidth() const					{ return ThumbWidth; }
	unsigned			GetThumbHeight() const					{ return ThumbHeight;  }
	bool				HasNoMedia() const						{ return NoMedia; }
	bool				GazingAtMenu() const;

protected:
	OvrFolderBrowser( App * app,
				OvrVRMenuMgr & menuMgr,
				BitmapFont const & font,
				SearchPaths & paths,
				OvrMetaData & metaData,
				unsigned	defaultFileTexture,
				unsigned	defaultDirectoryTexture,
				float panelWidth = 512.0f,
				float panelHeight = 512.0f,
				float radius = 3.0f,
				unsigned numSwipePanels = 5,
				unsigned thumbWidth = 256, 
				unsigned thumbHeight = 160 );

    virtual ~OvrFolderBrowser();

	//================================================================================
	// Subclass interface

	// Called on a background thread
	// The returned memory buffer will be free()'d after writing the thumbnail.
	// Return NULL if the thumbnail couldn't be created.
	virtual unsigned char * CreateThumbnail( const char * filename, int & width, int & height ) = 0;

	// Called on a background thread to load thumbnail
	virtual	unsigned char * LoadThumbnail( const char * filename, int & width, int & height ) = 0;

	// Adds thumbnail extension to a file to find/create its thumbnail
	virtual String	ThumbName( const String & s ) = 0;

	// Optional interface
	//
	// If we fail to load one type of thumbnail, try an alternative
	virtual String	AlternateThumbName( const String & s ) { return String(); }

	// Called on opening menu
	virtual void OnBrowserOpen() {}

	// Media not found - have subclass set the title, image and caption to display
	virtual void OnMediaNotFound( String & title, String & imageFile, String & message ) = 0;

	//================================================================================
	
	// OnEnterMenuRootAdjust is set to be performed the 
	// next time the menu is opened to adjust for a potentially deleted or added category
	void				SetRootAdjust( const RootDirection dir )	{ OnEnterMenuRootAdjust = dir;  }
	RootDirection		GetRootAdjust() const						{ return OnEnterMenuRootAdjust; }

	SearchPaths const &	GetSearchPaths() const { return Paths; }

	// Rebuilds a folder using passed in data
	void				RebuildFolder( const int folderIndex, const Array< OvrMetaDatum * > & data );

private:

	static void *		ThumbnailThread( void * v );
	void				LoadThumbnailToTexture( const char * thumbnailCommand );
	virtual void		OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event );
    virtual void        Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                        BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );
	virtual void		Open_Impl( App * app, OvrGazeCursor & gazeCursor );

	void				LoadCategory( const int folderIndex );
	void				LoadFolderPanels( const OvrMetaData::Category & category, const int folderIndex, Folder & folder,
							 Array< VRMenuObjectParms const * > & outParms );
	void				AddPanelToFolder( OvrMetaDatum * const panoData, const int folderIndex, Folder & folder, Array< VRMenuObjectParms const * >& outParms );
	void				DisplaceFolder( int index, const Vector3f & direction, float distance, bool startOffSelf = false );
	void				UpdateFolderTitle( const int folderIndex );
	void				SetScrollHintVisible( const bool visible );
	float				CalcFolderMaxRotation( const int folderIndex ) const;
	unsigned char *		LoadThumbAndApplyAA( const String & fileName, int & width, int & height );

	// Globals
	App *				AppPtr;
	OvrVRMenuMgr &		MenuMgr;
	const BitmapFont &	Font;
	SearchPaths	&		Paths;
	OvrMetaData & 		MetaData;

	// Members
	OvrMetaDatum *		CurrentPanelData; // queried each frame and set to NULL when queried
		
	unsigned 			DefaultFileTexture;
	unsigned 			DefaultDirectoryTexture;
	float				PanelWidth;
	float				PanelHeight;
	unsigned			ThumbWidth;
	unsigned			ThumbHeight;
	float				PanelTextSpacingScale;		// Panel text placed with vertical position of -panelHeight * PanelTextSpacingScale
	float				FolderTitleSpacingScale;	// Folder title placed with vertical position of PanelHeight * FolderTitleSpacingScale

	int					CircumferencePanelSlots;
	unsigned			NumSwipePanels;
	float				Radius;
	float				VisiblePanelsArcAngle;
	bool				SwipeHeldDown;
	float				DebounceTime;
	bool				NoMedia;
	bool				AllowPanelTouchUp;

	Array<Folder>		Folders;

	menuHandle_t		ScrollHintHandle;				// Handle to "swipe to browse" hint
	bool				ScrollHintShown;

	RootDirection		OnEnterMenuRootAdjust;
	
	// Checked at Frame() time for commands from the thumbnail/create thread
	MessageQueue		TextureCommands;

	// Create / load thumbnails by background thread
	MessageQueue		BackgroundCommands;

	// Keep a reference to Panel texture used for AA alpha when creating thumbnails
	static unsigned char *		ThumbPanelBG;
};

} // namespace OVR

#endif // OVR_GlobalMenu_h
