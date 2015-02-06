// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TaskBrowserPrivatePCH.h"
#include "STaskBrowser.h"
#include "STaskColumn.h"
#include "STaskComplete.h"
#include "STaskSettings.h"
#include "Public/TaskDatabase.h"
#include "Public/TaskDataManager.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include <winsock2.h>
	#include <iphlpapi.h>
	#include <wincrypt.h>
#include "HideWindowsPlatformTypes.h"

// Link with the Wintrust.lib file.
#pragma comment( lib, "Crypt32.lib" )
#pragma comment( lib, "Iphlpapi.lib" )
#endif

#if PLATFORM_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <CommonCrypto/CommonCryptor.h>
#endif
#include "STextComboBox.h"

#define LOCTEXT_NAMESPACE "STaskBrowser"
DEFINE_LOG_CATEGORY_STATIC( LogTaskBrowser, Log, All );


/**
 * Reads the mac address for the computer
 *
 * @param MacAddr the buffer that receives the mac address
 * @param MacAddrLen (in) the size of the dest buffer, (out) the size of the data that was written
 *
 * @return true if the address was read, false if it failed to get the address
 */
static bool GetMacAddress(uint8* MacAddr,uint32& MacAddrLen)
{
	TArray<uint8> MacAddrArray = FPlatformMisc::GetMacAddress();
	if (MacAddrLen < (uint32)MacAddrArray.Num() || MacAddrArray.Num() == 0)
	{
		return false;
	}
	MacAddrLen = MacAddrArray.Num();
	FMemory::Memcpy(MacAddr, MacAddrArray.GetData(), MacAddrLen);
	return true;
}

/**
 * Encrypts a buffer using the crypto API
 *
 * @param SrcBuffer the source data being encrypted
 * @param SrcLen the size of the buffer in bytes
 * @param DestBuffer (out) chunk of memory that is written to
 * @param DestLen (in) the size of the dest buffer, (out) the size of the encrypted data
 *
 * @return true if the encryption worked, false otherwise
 */
static bool EncryptBuffer(const uint8* SrcBuffer,const uint32 SrcLen,uint8* DestBuffer,uint32& DestLen)
{
	bool bEncryptedOk = false;

#if PLATFORM_MAC
	unsigned long long MacAddress = 0;
	uint32 AddressSize = sizeof(unsigned long long);
	GetMacAddress((uint8*)&MacAddress,AddressSize);
	unsigned long long Entropy = 5148284414757334885ull;
	Entropy ^= MacAddress;

	uint8 Key[kCCKeySizeAES128];
	check(kCCKeySizeAES128 == 2*sizeof(unsigned long long));
	FMemory::Memcpy(Key,&Entropy,sizeof(Entropy));
	FMemory::Memcpy(Key+sizeof(Entropy),&Entropy,sizeof(Entropy));

	size_t OutBufferSize = SrcLen + kCCBlockSizeAES128;
	uint8* OutBuffer = (uint8*)FMemory::Malloc(OutBufferSize);
	FMemory::Memset(OutBuffer,0,OutBufferSize);

	size_t BytesEncrypted = 0;
	CCCryptorStatus Status = CCCrypt( kCCEncrypt, kCCAlgorithmAES128,
		kCCOptionPKCS7Padding, Key, kCCKeySizeAES128, NULL, SrcBuffer,
		SrcLen, OutBuffer, OutBufferSize, &BytesEncrypted);
	if (Status == kCCSuccess)
	{
		DestLen = BytesEncrypted;
		FMemory::Memcpy(DestBuffer,OutBuffer,DestLen);
		bEncryptedOk = true;
	}
	else
	{
		UE_LOG(LogTaskBrowser, Log, TEXT("CCCrypt failed w/ 0x%08x"), Status);
	}
	FMemory::Free(OutBuffer);
#elif PLATFORM_LINUX
	printf("STaskBrowser.cpp: LINUX EncryptBuffer()\n");
#elif PLATFORM_WINDOWS
	DATA_BLOB SourceBlob, EntropyBlob, FinalBlob;
	// Set up the datablob to encrypt
	SourceBlob.cbData = SrcLen;
	SourceBlob.pbData = (uint8*)SrcBuffer;
	// Get the mac address for mixing into the entropy (ties the encryption to a location)
	ULONGLONG MacAddress = 0;
	uint32 AddressSize = sizeof(ULONGLONG);
	GetMacAddress((uint8*)&MacAddress,AddressSize);
	// Set up the entropy blob (changing this breaks all previous encrypted buffers!)
	ULONGLONG Entropy = 5148284414757334885ui64;
	Entropy ^= MacAddress;
	EntropyBlob.cbData = sizeof(ULONGLONG);
	EntropyBlob.pbData = (uint8*)&Entropy;
	// Zero the output data
	FMemory::Memzero(&FinalBlob,sizeof(DATA_BLOB));
	// Now encrypt the data
	if (CryptProtectData(&SourceBlob,
		NULL,
		&EntropyBlob,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&FinalBlob))
	{
		if (FinalBlob.cbData <= DestLen)
		{
			// Copy the final results
			DestLen = FinalBlob.cbData;
			FMemory::Memcpy(DestBuffer,FinalBlob.pbData,DestLen);
			bEncryptedOk = true;
		}
		// Free the encryption buffer
		LocalFree(FinalBlob.pbData);
	}
	else
	{
		uint32 Error = GetLastError();
		UE_LOG(LogTaskBrowser, Log, TEXT("CryptProtectData failed w/ 0x%08x"), Error);
	}
#else
	unimplemented();
#endif
	return bEncryptedOk;
}

/**
 * Decrypts a buffer using the crypto API
 *
 * @param SrcBuffer the source data being decrypted
 * @param SrcLen the size of the buffer in bytes
 * @param DestBuffer (out) chunk of memory that is written to
 * @param DestLen (in) the size of the dest buffer, (out) the size of the encrypted data
 *
 * @return true if the decryption worked, false otherwise
 */
static bool DecryptBuffer(const uint8* SrcBuffer,const uint32 SrcLen,uint8* DestBuffer,uint32& DestLen)
{
	bool bDecryptedOk = false;
#if PLATFORM_MAC
	unsigned long long MacAddress = 0;
	uint32 AddressSize = sizeof(unsigned long long);
	GetMacAddress((uint8*)&MacAddress,AddressSize);
	unsigned long long Entropy = 5148284414757334885ull;
	Entropy ^= MacAddress;

	uint8 Key[kCCKeySizeAES128];
	check(kCCKeySizeAES128 == 2*sizeof(unsigned long long));
	FMemory::Memcpy(Key,&Entropy,sizeof(Entropy));
	FMemory::Memcpy(Key+sizeof(Entropy),&Entropy,sizeof(Entropy));

	size_t OutBufferSize = SrcLen + kCCBlockSizeAES128;
	uint8* OutBuffer = (uint8*)FMemory::Malloc(OutBufferSize);
	FMemory::Memset(OutBuffer,0,OutBufferSize);

	size_t BytesDecrypted = 0;
	CCCryptorStatus Status = CCCrypt( kCCDecrypt, kCCAlgorithmAES128,
		kCCOptionPKCS7Padding, Key, kCCKeySizeAES128, NULL, SrcBuffer,
		SrcLen, OutBuffer, OutBufferSize, &BytesDecrypted);
	if (Status == kCCSuccess)
	{
		DestLen = BytesDecrypted;
		FMemory::Memcpy(DestBuffer,OutBuffer,DestLen);
		bDecryptedOk = true;
	}
	else
	{
		UE_LOG(LogTaskBrowser, Log, TEXT("CCCrypt failed w/ 0x%08x"), Status);
	}
	FMemory::Free(OutBuffer);
#elif PLATFORM_LINUX
		printf("STaskBrowser.cpp: LINUX DecryptBuffer()\n");
#elif PLATFORM_WINDOWS
	DATA_BLOB SourceBlob, EntropyBlob, FinalBlob;
	// Set up the datablob to encrypt
	SourceBlob.cbData = SrcLen;
	SourceBlob.pbData = (uint8*)SrcBuffer;
	// Get the mac address for mixing into the entropy (ties the encryption to a location)
	ULONGLONG MacAddress = 0;
	uint32 AddressSize = sizeof(ULONGLONG);
	GetMacAddress((uint8*)&MacAddress,AddressSize);
	// Set up the entropy blob
	ULONGLONG Entropy = 5148284414757334885ui64;
	Entropy ^= MacAddress;
	EntropyBlob.cbData = sizeof(ULONGLONG);
	EntropyBlob.pbData = (uint8*)&Entropy;
	// Zero the output data
	FMemory::Memzero(&FinalBlob,sizeof(DATA_BLOB));
	// Now encrypt the data
	if (CryptUnprotectData(&SourceBlob,
		NULL,
		&EntropyBlob,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&FinalBlob))
	{
		if (FinalBlob.cbData <= DestLen)
		{
			// Copy the final results
			DestLen = FinalBlob.cbData;
			FMemory::Memcpy(DestBuffer,FinalBlob.pbData,DestLen);
			bDecryptedOk = true;
		}
		// Free the decryption buffer
		LocalFree(FinalBlob.pbData);
	}
#else
	unimplemented();
#endif
	return bDecryptedOk;
}



//////////////////////////////////////////////////////////////////////////
// FTaskBrowserSettings
FTaskBrowserSettings::FTaskBrowserSettings()
	: ServerName(),
	ServerPort( 80 ),
	UserName(),
	Password(),
	ProjectName(),
	bAutoConnectAtStartup( false ),
	bUseSingleSignOn( true ),
	DBFilterName(),
	bFilterOnlyOpen( true ),
	bFilterAssignedToMe( false ),
	bFilterCreatedByMe( false ),
	bFilterCurrentMap( false ),
	TaskListSortColumn( static_cast< int32 >( EField::Priority ) ),
	bTaskListSortAscending( true )
{
	// Load default settings for Epic task database server here
	// NOTE: Licensees, you may want to replace the following to suit your own needs
	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultServerName" ), ServerName, GEditorIni );
	GConfig->GetInt( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultServerPort" ), ServerPort, GEditorIni );
	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultProjectName" ), ProjectName, GEditorIni );
	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultDBFilterName" ), DBFilterName, GEditorIni );
}

/** Loads settings from the configuration file */
void FTaskBrowserSettings::LoadSettings()
{
	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "ServerName" ), ServerName, GEditorUserSettingsIni );
		
	GConfig->GetInt( TEXT( "TaskBrowser" ), TEXT( "ServerPort" ), ServerPort, GEditorUserSettingsIni );

	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "UserName" ), UserName, GEditorUserSettingsIni );

	// Load encrypted password from disk
	FString EncryptedPasswordBlob = GConfig->GetStr( TEXT( "TaskBrowser" ), TEXT( "Password" ), GEditorUserSettingsIni );
	Password = TEXT( "" );
	const uint32 MaxEncryptedPasswordSize = 2048;
	uint8 EncryptedPasswordBuffer[ MaxEncryptedPasswordSize ];
	if( FString::ToBlob( EncryptedPasswordBlob, EncryptedPasswordBuffer, MaxEncryptedPasswordSize ) )
	{
		const uint32 MaxDecryptedPasswordSize = 2048;
		uint8 DecryptedPasswordBuffer[ MaxDecryptedPasswordSize ];
		const uint32 ExpectedEncryptedPasswordSize = EncryptedPasswordBlob.Len() / 3;
		uint32 DecryptedPasswordSize = MaxDecryptedPasswordSize;
		if( DecryptBuffer(
				EncryptedPasswordBuffer,
				ExpectedEncryptedPasswordSize,
				DecryptedPasswordBuffer,
				DecryptedPasswordSize ) )
		{
			FString DecryptedPassword = ( const TCHAR* )DecryptedPasswordBuffer;

			// Store password
			Password = DecryptedPassword;
		}
	}

	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "ProjectName" ), ProjectName, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "AutoConnectAtStartup" ), bAutoConnectAtStartup, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "UseSingleSignOn" ), bUseSingleSignOn, GEditorUserSettingsIni );

	GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "DBFilterName" ), DBFilterName, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterOnlyOpen" ), bFilterOnlyOpen, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterAssignedToMe" ), bFilterAssignedToMe, GEditorUserSettingsIni );
	
	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCreatedByMe" ), bFilterCreatedByMe, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCurrentMap" ), bFilterCurrentMap, GEditorUserSettingsIni );

	GConfig->GetInt( TEXT( "TaskBrowser" ), TEXT( "TaskListSortColumn" ), TaskListSortColumn, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "TaskListSortAscending" ), bTaskListSortAscending, GEditorUserSettingsIni );
}

/** Saves settings to the configuration file */
void FTaskBrowserSettings::SaveSettings()
{
	GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "ServerName" ), *ServerName, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT( "TaskBrowser" ), TEXT( "ServerPort" ), ServerPort, GEditorUserSettingsIni );
	GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "UserName" ), *UserName, GEditorUserSettingsIni );

	bool bHaveValidPassword = false;
	if( Password.Len() > 0 )
	{
		// Store the user's password encrypted on disk
		const uint32 MaxEncryptedPasswordSize = 2048;
		uint8 EncryptedPasswordBuffer[ MaxEncryptedPasswordSize ];
		uint32 EncryptedPasswordSize = MaxEncryptedPasswordSize;
		if( EncryptBuffer(
				( const uint8* )&Password[ 0 ],
				( Password.Len() + 1 ) * sizeof( TCHAR ),
				EncryptedPasswordBuffer,
				EncryptedPasswordSize ) )
		{
			FString EncryptedPasswordBlob = FString::FromBlob( EncryptedPasswordBuffer, EncryptedPasswordSize );
			GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "Password" ), *EncryptedPasswordBlob, GEditorUserSettingsIni );
			bHaveValidPassword = true;
		}
	}

	if( !bHaveValidPassword )
	{
		// Empty password
		GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "Password" ), TEXT( "" ), GEditorUserSettingsIni );
	}

	GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "ProjectName" ), *ProjectName, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "AutoConnectAtStartup" ), bAutoConnectAtStartup, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "UseSingleSignOn" ), bUseSingleSignOn, GEditorUserSettingsIni );

	GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "DBFilterName" ), *DBFilterName, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterOnlyOpen" ), bFilterOnlyOpen, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterAssignedToMe" ), bFilterAssignedToMe, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCreatedByMe" ), bFilterCreatedByMe, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCurrentMap" ), bFilterCurrentMap, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT( "TaskBrowser" ), TEXT( "TaskListSortColumn" ), TaskListSortColumn, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "TaskListSortAscending" ), bTaskListSortAscending, GEditorUserSettingsIni );
}

/** Returns true if the current connection settings are valid */
bool FTaskBrowserSettings::AreConnectionSettingsValid() const
{
	if( ServerName.Len() == 0 ||
		ServerPort == 0 ||
		UserName.Len() == 0 ||
		Password.Len() == 0 ||
		ProjectName.Len() == 0 )
	{
		return false;
	}

	return true;
}



//////////////////////////////////////////////////////////////////////////
// FTaskOverview
TMap< EField, FName > FTaskOverview::FieldNames;
FTaskOverview::FTaskOverview( const FTaskDatabaseEntry& InEntry )
	: FTaskDatabaseEntry()
{
	Number = InEntry.Number;
	Priority = InEntry.Priority;
	Summary = InEntry.Summary;
	Status = InEntry.Status;
	CreatedBy = InEntry.CreatedBy;
	AssignedTo = InEntry.AssignedTo;
}

FString FTaskOverview::GetFieldEntry( const FName InName ) const
{
	// Retrieve the key from the name specified
	PopulateFieldNames();
	const EField* FieldPtr = FieldNames.FindKey( InName );
	check( FieldPtr  );

	// Convert the field to it's equivalent property
	check( *FieldPtr  > EField::Invalid );
	check( *FieldPtr  < EField::NumColumnIDs );
	switch( *FieldPtr  )
	{
	case EField::Number:
		return FString::FromInt( Number );
	case EField::Priority:
		return Priority;
	case EField::Summary:
		return Summary;
	case EField::Status:
		return Status;
	case EField::CreatedBy:
		return CreatedBy;
	case EField::AssignedTo:
		return AssignedTo;
	}
	return TEXT("");
}

FName FTaskOverview::GetFieldName( const EField InField )
{
	// Retrieve the value from the field specified
	PopulateFieldNames();
	const FName* NamePtr = FieldNames.Find( InField );
	check( NamePtr );

	return *NamePtr;
}

void FTaskOverview::PopulateFieldNames()
{
	// Fill our map if it's empty
	if ( FieldNames.Num() == 0 )
	{
		FieldNames.Add( EField::Number,			FName(TEXT("Number")) );
		FieldNames.Add( EField::Priority,		FName(TEXT("Priority")) );
		FieldNames.Add( EField::Summary,		FName(TEXT("Summary")) );
		FieldNames.Add( EField::Status,			FName(TEXT("Status")) );
		FieldNames.Add( EField::CreatedBy,		FName(TEXT("CreatedBy")) );
		FieldNames.Add( EField::AssignedTo,		FName(TEXT("AssignedTo")) );
	}
}

//////////////////////////////////////////////////////////////////////////
// STaskOverview
class STaskTreeWidgetItem : public SMultiColumnTableRow< TSharedPtr<FTaskOverview> >
{
public:
	SLATE_BEGIN_ARGS(STaskTreeWidgetItem)
		: _WidgetInfoToVisualize()
		{}
		SLATE_ARGUMENT( TSharedPtr<FTaskOverview>, WidgetInfoToVisualize )
	SLATE_END_ARGS()

	/**
	 * Construct child widgets that comprise this widget.
	 *
	 * @param InArgs  Declaration from which to construct this widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		this->WidgetInfo = InArgs._WidgetInfoToVisualize;

		SMultiColumnTableRow< TSharedPtr<FTaskOverview> >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
	}

	/** @return Widget based on the column name */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		const FString Entry = WidgetInfo.Get()->GetFieldEntry( ColumnName );
		return
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew( STextBlock )
				.Text( FText::FromString(Entry) )
			];
	}

protected:
	/** The info about the widget that we are visualizing */
	TAttribute< TSharedPtr<FTaskOverview> > WidgetInfo;
};

//////////////////////////////////////////////////////////////////////////
// STaskBrowser

/**
 * Construct the widget
 *
 * @param InArgs		A declaration from which to construct the widget
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STaskBrowser::Construct(const FArguments& InArgs)
{
	// Create task data manager instance
	TaskDataManager = MakeShareable( new FTaskDataManager( this ) );

	// Load preferences
	FTaskBrowserSettings TBSettings;
	TBSettings.LoadSettings();

	// Propagate server connection settings to the task data manager
	FTaskDataManagerConnectionSettings ConnectionSettings;
	{
		ConnectionSettings.ServerName = TBSettings.ServerName;
		ConnectionSettings.ServerPort = TBSettings.ServerPort;
		ConnectionSettings.UserName = TBSettings.UserName;
		ConnectionSettings.Password = TBSettings.Password;
		ConnectionSettings.ProjectName = TBSettings.ProjectName;
	}
	TaskDataManager->SetConnectionSettings( ConnectionSettings );

	// Set column sorting rules
	EField eField = static_cast< EField >( TBSettings.TaskListSortColumn );
	TaskListSortColumn = ( ( eField > EField::Invalid && eField < EField::NumColumnIDs ) ? eField : EField::Priority );
	TaskListSortAscending = TBSettings.bTaskListSortAscending;
	TaskListNumSelectedAndOpen = -1;

	// Automatically connect to the server if we were asked to do that
	if( TBSettings.bAutoConnectAtStartup && TBSettings.AreConnectionSettingsValid() )
	{
		TaskDataManager->AttemptConnection();
	}


	// Standard paddings
	const FMargin BorderPadding( 6, 6 );
	const FMargin ButtonPadding( 8, 4 );
	const FMargin SmallPadding( 0, 0, 12, 0 );
	const FMargin XLargePadding( 0, 0, 48, 0 );

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding( BorderPadding )
		.FillHeight(0.45f)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( BorderPadding )
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SAssignNew(MarkComplete, SButton)
						.ContentPadding( ButtonPadding )
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.IsEnabled_Raw( this, &STaskBrowser::GetMarkCompleteEnabled )
						.OnClicked( this, &STaskBrowser::OnMarkCompleteClicked )
						[
							SNew(STextBlock) .Text( LOCTEXT("MarkComplete", "Mark Complete...") )
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( XLargePadding )
					[
						SAssignNew(RefreshView, SButton)
						.ContentPadding( ButtonPadding )
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked( this, &STaskBrowser::OnRefreshViewClicked )
						[
							SNew(STextBlock) .Text( LOCTEXT("RefreshView", "Refresh View") )
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock) .Text( LOCTEXT("DatabaseFilter", "Database filter") )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SAssignNew(DatabaseFilter, STextComboBox)
						.OptionsSource( &DatabaseOptions )
						.OnSelectionChanged( this, &STaskBrowser::OnDatabaseFilterSelect )
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( BorderPadding )
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SNew(STextBlock) .Text( LOCTEXT("Display", "Display") )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(OpenOnly, SCheckBox)
						.OnCheckStateChanged( this, &STaskBrowser::OnOpenOnlyChanged )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SNew(STextBlock) .Text( LOCTEXT("OpenOnly", "Open only") )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(AssignedToMe, SCheckBox)
						.OnCheckStateChanged( this, &STaskBrowser::OnAssignedToMeChanged )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SNew(STextBlock) .Text( LOCTEXT("AssignedToMe", "Assigned to me") )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(CreatedByMe, SCheckBox)
						.OnCheckStateChanged( this, &STaskBrowser::OnCreatedByMeChanged )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SNew(STextBlock) .Text( LOCTEXT("CreatedByMe", "Created by me") )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(CurrentMap, SCheckBox)
						.OnCheckStateChanged( this, &STaskBrowser::OnCurrentMapChanged )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( SmallPadding )
					[
						SNew(STextBlock) .Text( LOCTEXT("CurrentMap", "Current map") )
					]
				]
				+SVerticalBox::Slot()
				.Padding( BorderPadding )
				.FillHeight( 1.0f )
				[
					SNew(SBox)
					.Padding( BorderPadding )
					[
						SAssignNew(TaskList, SListView< TSharedPtr<FTaskOverview> >)
						// .SelectionMode(ESelectionMode::Single)
						.ItemHeight(24)
						.ListItemsSource( &TaskEntries )
						.OnGenerateRow( this, &STaskBrowser::OnTaskGenerateRow )
						.OnSelectionChanged( this, &STaskBrowser::OnTaskSelectionChanged )
						.OnMouseButtonDoubleClick( this, &STaskBrowser::OnTaskDoubleClicked )
						.HeaderRow
						(
							SAssignNew(TaskHeaders, SHeaderRow)	// @TODO as a workaround to columns not registering mouse clicks, use buttons instead
							+ SHeaderRow::Column(FTaskOverview::GetFieldName( EField::Number ))
							.FillWidth( 90.0f )
							[
								SNew(STaskColumn)
								.TaskBrowser( SharedThis( this ) )
								.Field( EField::Number )
							]
							+ SHeaderRow::Column(FTaskOverview::GetFieldName( EField::Priority ))
							.FillWidth( 150.0f )
							[
								SNew(STaskColumn)
								.TaskBrowser( SharedThis( this ) )
								.Field( EField::Priority )
							]
							+ SHeaderRow::Column(FTaskOverview::GetFieldName( EField::Summary ))
							.FillWidth( 360.0f )
							[
								SNew(STaskColumn)
								.TaskBrowser( SharedThis( this ) )
								.Field( EField::Summary )
							]
							+ SHeaderRow::Column(FTaskOverview::GetFieldName( EField::AssignedTo ))
							.FillWidth( 150.0f )
							[
								SNew(STaskColumn)
								.TaskBrowser( SharedThis( this ) )
								.Field( EField::AssignedTo )
							]
							+ SHeaderRow::Column(FTaskOverview::GetFieldName( EField::Status ))
							.FillWidth( 200.0f )
							[
								SNew(STaskColumn)
								.TaskBrowser( SharedThis( this ) )
								.Field( EField::Status )
							]
							+ SHeaderRow::Column(FTaskOverview::GetFieldName( EField::CreatedBy ))
							.FillWidth( 150.0f )
							[
								SNew(STaskColumn)
								.TaskBrowser( SharedThis( this ) )
								.Field( EField::CreatedBy )
							]
						)
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.Padding( BorderPadding )
		.FillHeight(0.55f)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( BorderPadding )
				[
					SAssignNew(Summary, SEditableTextBox) .IsReadOnly( true )
				]
				+SVerticalBox::Slot()
				.Padding( BorderPadding )
				.FillHeight(1.0f)
				[
					SAssignNew(Description, SEditableTextBox) .IsReadOnly( true )
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( BorderPadding )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( SmallPadding )
			[
				SAssignNew(Connect, SButton)
				.ContentPadding( ButtonPadding )
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.IsEnabled_Raw( this, &STaskBrowser::GetConnectEnabled )
				.OnClicked( this, &STaskBrowser::OnConnectClicked )
				[
					SNew(STextBlock) .Text_Raw( this, &STaskBrowser::GetConnectText )
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( SmallPadding )
			[
				SAssignNew(Settings, SButton)
				.ContentPadding( ButtonPadding )
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked( this, &STaskBrowser::OnSettingsClicked )
				[
					SNew(STextBlock) .Text( LOCTEXT("Settings", "Settings...") )
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( SmallPadding )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock) .Text_Raw( this, &STaskBrowser::GetStatusText )
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> STaskBrowser::GetContent()
{
	return SharedThis( this );
}

STaskBrowser::~STaskBrowser()
{
	// Cleanup task data manager
	TaskDataManager.Reset();
}

bool STaskBrowser::GetMarkCompleteEnabled() const
{
	// Update the 'Fix' button
	bool bEnableFixButton = false;

	// Must be connected to the server
	if( TaskDatabaseSystem::IsConnected() )
	{
		// Only enable the 'Mark as Fixed' button if at least one *OPEN* task is selected
		if( TaskListNumSelectedAndOpen > 0 )
		{
			bEnableFixButton = true;
		}
	}

	return bEnableFixButton;
}

FReply STaskBrowser::OnMarkCompleteClicked()
{
	const bool bOnlyOpenTasks = true;
	TArray< uint32 > TaskNumbersToFix;
	QuerySelectedTaskNumbers( TaskNumbersToFix, bOnlyOpenTasks );

	if( TaskNumbersToFix.Num() > 0 )
	{
		CompleteTaskDialog( TaskNumbersToFix );
	}
	
	return FReply::Handled();
}

FReply STaskBrowser::OnRefreshViewClicked()
{
	// Update everything using fresh data from the server
	TaskDataManager->ClearTaskDataAndInitiateRefresh();

	return FReply::Handled();
}

void STaskBrowser::OnDatabaseFilterSelect( TSharedPtr<FString> InFilter, ESelectInfo::Type SelectInfo )
{
	// Update the active filter!
	TaskDataManager->ChangeActiveFilter( *InFilter );

	// Save the new filter name in our preferences
	{
		FTaskBrowserSettings TBSettings;
		TBSettings.LoadSettings();
		TBSettings.DBFilterName = *InFilter;
		TBSettings.SaveSettings();
	}
}

void STaskBrowser::OnOpenOnlyChanged( const ECheckBoxState NewCheckedState )
{
	// Refresh the GUI.  It will apply the updated filters.
	RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildTaskList );

	// A filter was changed, so save updated settings to disk
	{
		FTaskBrowserSettings TBSettings;
		TBSettings.LoadSettings();
		TBSettings.bFilterOnlyOpen = NewCheckedState == ECheckBoxState::Checked;
		TBSettings.SaveSettings();
	}
}

void STaskBrowser::OnAssignedToMeChanged( const ECheckBoxState NewCheckedState )
{
	// Refresh the GUI.  It will apply the updated filters.
	RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildTaskList );

	// A filter was changed, so save updated settings to disk
	{
		FTaskBrowserSettings TBSettings;
		TBSettings.LoadSettings();
		TBSettings.bFilterAssignedToMe = NewCheckedState == ECheckBoxState::Checked;
		TBSettings.SaveSettings();
	}
}

void STaskBrowser::OnCreatedByMeChanged( const ECheckBoxState NewCheckedState )
{
	// Refresh the GUI.  It will apply the updated filters.
	RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildTaskList );

	// A filter was changed, so save updated settings to disk
	{
		FTaskBrowserSettings TBSettings;
		TBSettings.LoadSettings();
		TBSettings.bFilterCreatedByMe = NewCheckedState == ECheckBoxState::Checked;
		TBSettings.SaveSettings();
	}
}

void STaskBrowser::OnCurrentMapChanged( const ECheckBoxState NewCheckedState )
{
	// Refresh the GUI.  It will apply the updated filters.
	RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildTaskList );

	// A filter was changed, so save updated settings to disk
	{
		FTaskBrowserSettings TBSettings;
		TBSettings.LoadSettings();
		TBSettings.bFilterCurrentMap = NewCheckedState == ECheckBoxState::Checked;
		TBSettings.SaveSettings();
	}
}

TSharedRef<ITableRow> STaskBrowser::OnTaskGenerateRow( TSharedPtr<FTaskOverview> InTaskOverview, const TSharedRef<STableViewBase>& OwnerTable )
{
	return
		SNew(STaskTreeWidgetItem, OwnerTable)
		.WidgetInfoToVisualize(InTaskOverview);
}

void STaskBrowser::OnTaskSelectionChanged( TSharedPtr<FTaskOverview> InTaskOverview, ESelectInfo::Type /*SelectInfo*/ )
{
	// Make sure the task data manager is keep track of this task
	if ( InTaskOverview.IsValid() )
	{
		TaskDataManager->SetFocusedTaskNumber( InTaskOverview->Number );
	}
	else
	{
		TaskDataManager->SetFocusedTaskNumber( INDEX_NONE );
	}

	// Refresh the cached values for our Mark Complete button
	TaskListNumSelectedAndOpen = GetNumSelectedTasks( true );

	RefreshGUI( ETaskBrowserGUIRefreshOptions::UpdateTaskDescription );
}


int32 STaskBrowser::GetNumSelectedTasks( const bool bOnlyOpenTasks ) const
{
	// Just retrieve how maybe we have of that type
	TArray< uint32 > SelectedTaskNumbers;
	QuerySelectedTaskNumbers( SelectedTaskNumbers, bOnlyOpenTasks );
	const int32 iSelectedTaskNumbers = SelectedTaskNumbers.Num();
	return iSelectedTaskNumbers;
}

void STaskBrowser::OnTaskDoubleClicked( TSharedPtr<FTaskOverview> InTaskOverview )
{
	// Check to see if the bug status is "Open"	
	if( InTaskOverview.IsValid() && InTaskOverview->Status.StartsWith( TaskDataManager->GetOpenTaskStatusPrefix() ) )
	{
		// Add this task's number to our list of tasks to fix
		TArray< uint32 > TaskNumbersToFix;
		TaskNumbersToFix.Add( InTaskOverview->Number );

		CompleteTaskDialog( TaskNumbersToFix );
	}
}

FReply STaskBrowser::OnTaskColumnClicked( const EField InField )
{
	check( InField > EField::Invalid );
	check( InField < EField::NumColumnIDs )
	if( InField == TaskListSortColumn )
	{
		// Clicking on the same column will flip the sort order
		TaskListSortAscending = !TaskListSortAscending;
	}
	else
	{
		// Clicking on a new column will set that column as current and reset the sort order.
		TaskListSortColumn = InField;
		TaskListSortAscending = true;
	}

	// Save the sorting configuration
	FTaskBrowserSettings TBSettings;
	TBSettings.LoadSettings();
	TBSettings.TaskListSortColumn = static_cast< int32 >( TaskListSortColumn );
	TBSettings.bTaskListSortAscending = TaskListSortAscending;
	TBSettings.SaveSettings();

	// Refresh the GUI
	RefreshGUI( ETaskBrowserGUIRefreshOptions::SortTaskList );

	return FReply::Handled();
}

bool STaskBrowser::GetConnectEnabled() const
{
	switch( TaskDataManager->GetConnectionStatus() )
	{
	case ETaskDataManagerStatus::FailedToInit:
	case ETaskDataManagerStatus::Connecting:
	case ETaskDataManagerStatus::Disconnecting:
		return false;
	default:
		return true;
	}
}

FReply STaskBrowser::OnConnectClicked()
{
	if( TaskDataManager->GetConnectionStatus() != ETaskDataManagerStatus::FailedToInit )
	{
		if( TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::ReadyToConnect ||
			TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::ConnectionFailed )
		{
			// Did the user cancel the config dialog?
			bool bUserCancelled = false;

			// Load server settings from disk
			FTaskBrowserSettings TBSettings;
			TBSettings.LoadSettings();

			// Make sure that everything is valid, otherwise pop up a settings dialog
			if( !TBSettings.AreConnectionSettingsValid() )
			{
				// Give the user a chance to fix the settings
				SettingsDialog();
			}

			if( TBSettings.AreConnectionSettingsValid() )
			{
				// Connection failed, so allow the user to try again
				TaskDataManager->AttemptConnection();
			}
			else
			{
				// Only warn the user if the user did not press cancel
				if( !bUserCancelled )
				{
					// Warn the user that we'll need valid settings in order to connect to the server
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "TaskBrowser_Error_NeedValidConnectionSettings", "In order for the Task Browser to connect to the database server, you'll need to supply valid settings for the server name/port, user name and password, as well as the project (database) name to connect to.  If you're not sure what settings to use then please ask a system administrator, or check the similar settings in desktop task management application's configuration." ) );
				}
			}
		}
		else
		{
			// Queue a disconnect
			TaskDataManager->AttemptDisconnection();
		}
	}

	return FReply::Handled();
}

FText STaskBrowser::GetConnectText() const
{
	switch( TaskDataManager->GetConnectionStatus() )
	{
	case ETaskDataManagerStatus::FailedToInit:
	case ETaskDataManagerStatus::ReadyToConnect:
	case ETaskDataManagerStatus::ConnectionFailed:
	case ETaskDataManagerStatus::Disconnecting:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ConnectButtonLabel", "Connect" );
	default:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_DisconnectButtonLabel", "Disconnect" );
	}
}

FReply STaskBrowser::OnSettingsClicked()
{
	SettingsDialog();

	return FReply::Handled();
}

FText STaskBrowser::GetStatusText() const
{
	switch( TaskDataManager->GetGUIStatus() )
	{
	case ETaskDataManagerStatus::ReadyToConnect:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_ReadyToConnect", "Not connected.  Click the Connect button to login to the task database." );
	case ETaskDataManagerStatus::Connecting:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_Connecting", "Connecting to server and logging in..." );
	case ETaskDataManagerStatus::Connected:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_Connected", "Connected to the task database." );
	case ETaskDataManagerStatus::ConnectionFailed:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_ConnectionFailed", "Failed to connect to the task database.  Click the Connect button to try again." );
	case ETaskDataManagerStatus::Disconnecting:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_Disconnecting", "Disconnecting..." );
	case ETaskDataManagerStatus::QueryingFilters:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_QueryingFilters", "Retrieving filters..." );
	case ETaskDataManagerStatus::QueryingTasks:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_QueryingTasks", "Retrieving task list from server..." );
	case ETaskDataManagerStatus::QueryingTaskDetails:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_QueryingTaskDetails", "Retrieving task information..." );
	case ETaskDataManagerStatus::MarkingTaskComplete:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_MarkingTaskComplete", "Marking task as fixed..." );
	default:
		return NSLOCTEXT("UnrealEd", "TaskBrowser_ServerStatus_FailedToInit", "Failed to initialize.  No task database providers are available." );
	}
}

void STaskBrowser::CompleteTaskDialog( TArray< uint32 >& TaskNumbersToFix )
{
	// Create a new modal settings dialog
	const TArray< FString > &ResolutionValues = TaskDataManager->GetResolutionValues();
	FTaskResolutionData ResolutionData;
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title( LOCTEXT("TaskBrowserComplete", "Mark Task Complete") )
		.ClientSize( FVector2D(516, 273) )
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMinimize(false) .SupportsMaximize(false);

	WidgetWindow->SetContent
	(
		SNew(STaskComplete)
		.WidgetWindow(WidgetWindow)
		.ResolutionData( &ResolutionData )
		.ResolutionValues( &ResolutionValues )
	);
	GEditor->EditorAddModalWindow(WidgetWindow);

	if( ResolutionData.IsValid() )
	{
		// Queue these up to be marked as fixed!
		TaskDataManager->StartMarkingTasksComplete( TaskNumbersToFix, ResolutionData );
	}
}

/** Spawn our settings modal dialog */
void STaskBrowser::SettingsDialog( void )
{
	// Create a new modal settings dialog
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title( LOCTEXT("TaskBrowserSettings", "Task Browser Settings") )
		.ClientSize( FVector2D(376, 327) )
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMinimize(false) .SupportsMaximize(false);

	WidgetWindow->SetContent
	(
		SNew(STaskSettings)
		.WidgetWindow(WidgetWindow)
	);
	GEditor->EditorAddModalWindow(WidgetWindow);

	// Update the task data manager with any settings that may have changed
	FTaskBrowserSettings TBSettings;
	TBSettings.LoadSettings();
	FTaskDataManagerConnectionSettings NewConnectionSettings;
	{
		NewConnectionSettings.ServerName = TBSettings.ServerName;
		NewConnectionSettings.ServerPort = TBSettings.ServerPort;
		NewConnectionSettings.UserName = TBSettings.UserName;
		NewConnectionSettings.Password = TBSettings.Password;
		NewConnectionSettings.ProjectName = TBSettings.ProjectName;
	}
	TaskDataManager->SetConnectionSettings( NewConnectionSettings );
}

void STaskBrowser::QuerySelectedTaskNumbers( TArray< uint32 >& OutSelectedTaskNumbers, const bool bOnlyOpenTasks ) const
{
	OutSelectedTaskNumbers.Reset();
	
	// Create a list of task numbers that are currently selected in the task list
	TArray< TSharedPtr<FTaskOverview> > SelectedTasks = TaskList->GetSelectedItems();
	for ( int32 iSelectedTask = 0; iSelectedTask < SelectedTasks.Num(); iSelectedTask++ )
	{
		TSharedPtr<FTaskOverview> SelectedTask = SelectedTasks[ iSelectedTask ];

		// Make sure the task isn't already completed
		if( !bOnlyOpenTasks || SelectedTask->Status.StartsWith( TaskDataManager->GetOpenTaskStatusPrefix() ) )
		{
			// Add this task's number to our list of tasks
			OutSelectedTaskNumbers.Add( SelectedTask->Number );
		}
	}
}

bool STaskBrowser::TaskListItemSort( const TSharedPtr<FTaskOverview> TaskEntryA, const TSharedPtr<FTaskOverview> TaskEntryB ) const
{
	int SortResult = 0;
	check( TaskListSortColumn > EField::Invalid );
	check( TaskListSortColumn < EField::NumColumnIDs );
	switch( TaskListSortColumn )
	{
	case EField::Number:
		if( TaskEntryA->Number != TaskEntryB->Number )
		{
			SortResult = TaskEntryA->Number > TaskEntryB->Number ? 1 : -1;
		}
		break;
	case EField::Priority:
		SortResult = FCString::Stricmp( *TaskEntryA->Priority, *TaskEntryB->Priority );
		break;
	case EField::Summary:
		SortResult = FCString::Stricmp( *TaskEntryA->Summary, *TaskEntryB->Summary );
		break;
	case EField::AssignedTo:
		SortResult = FCString::Stricmp( *TaskEntryA->AssignedTo, *TaskEntryB->AssignedTo );
		break;
	case EField::Status:
		SortResult = FCString::Stricmp( *TaskEntryA->Status, *TaskEntryB->Status );
		break;
	case EField::CreatedBy:
		SortResult = FCString::Stricmp( *TaskEntryA->CreatedBy, *TaskEntryB->CreatedBy );
		break;
	}

	// If the items had the same value, then fallback to secondary sort criteria
	// @todo: Support custom secondary sorts (stack of sorted columns and ascend/descend state)
	if( SortResult == 0 )
	{
		if( TaskListSortColumn == EField::Number )
		{
			// Secondary sort by priority
			SortResult = TaskEntryA->Number > TaskEntryB->Number ? 1 : -1;
		}
		else
		{
			// Secondary sort by name
			SortResult = FCString::Stricmp( *TaskEntryA->Summary, *TaskEntryB->Summary );
		}
	}

	// Reverse the sort order if we were asked to do that
	if( !TaskListSortAscending )
	{
		SortResult = -SortResult;
	}

	return ( SortResult > 0 ? true : false );
}

void STaskBrowser::TaskListItemSort()
{
	// Early out if there's no elements in the list
	if ( TaskEntries.Num() == 0 )
	{
		return;
	}

	bool bChanged = false;	// Did the list change at all?
	bool bSwapped = true;	// Has the list changed this round?, keep sorting til it doesn't
	while( bSwapped )
	{
		bSwapped = false;
		for ( int32 CurTaskIndex = 0; CurTaskIndex < TaskEntries.Num() - 1; ++CurTaskIndex )
		{
			// Check to see if the elements need swapping
			TSharedPtr<FTaskOverview> TaskEntryA = TaskEntries[ CurTaskIndex ];
			TSharedPtr<FTaskOverview> TaskEntryB = TaskEntries[ CurTaskIndex+1 ];
			const bool bExchange = TaskListItemSort( TaskEntryA, TaskEntryB );

			// Do we need to swap the two elements around?
			if ( bExchange )
			{
				bChanged = true;
				bSwapped = true;
				TaskEntries[ CurTaskIndex ] = TaskEntryB;
				TaskEntries[ CurTaskIndex+1 ] = TaskEntryA;
				//FMemory::Memswap( TaskEntryA.Get(), TaskEntryB.Get(), sizeof( FTaskOverview ) );
			}
		}
	}

	if ( bChanged )
	{
		TaskList->RequestListRefresh();
	}
}

void STaskBrowser::RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options )
{
	if( Options & ETaskBrowserGUIRefreshOptions::RebuildFilterList )
	{	
		// Clear the contents of the list
		DatabaseOptions.Empty();

		// Copy all of the filters over to our own cached array
		TArray< FString > FilterNames = TaskDataManager->GetCachedFilterNames();
		for ( int32 iFilter = 0; iFilter < FilterNames.Num(); ++iFilter )
		{
			const FString& CurFilterName = FilterNames[ iFilter ];

			// Add this filter to our list
			DatabaseOptions.Add( MakeShareable( new FString( CurFilterName ) ) );
		}

		// Do we have an active filter already?
		int32 SelectedFilterIndex = INDEX_NONE;
		if( TaskDataManager->GetActiveFilterName().Len() > 0 )
		{
			verify( FilterNames.Find( TaskDataManager->GetActiveFilterName(), SelectedFilterIndex ) );
		}
		else
		{
			// No active filter yet, so we'll set one up now
			FTaskBrowserSettings TBSettings;
			TBSettings.LoadSettings();

			// Search for the default filter name
			if( !FilterNames.Find( TBSettings.DBFilterName, SelectedFilterIndex ) )
			{
				// Not found... hrm..
				if( FilterNames.Num() > 0 )
				{
					// Just use the first available filter
					SelectedFilterIndex = 0;
				}
			}

			if( SelectedFilterIndex != INDEX_NONE )
			{
				// Select the filter in the list
				DatabaseFilter->SetSelectedItem(DatabaseOptions[SelectedFilterIndex]);

				// Update task data manager
				TaskDataManager->ChangeActiveFilter( FilterNames[ SelectedFilterIndex ] );
			}
		}
	}


	if( Options & ETaskBrowserGUIRefreshOptions::RebuildTaskList )
	{
		// Grab a list of task numbers we already had selected
		TSet< int32 > SelectedTaskNumberSet;
		{
			const bool bOnlyOpenTasks = false;
			TArray< uint32 > SelectedTaskNumbers;
			QuerySelectedTaskNumbers( SelectedTaskNumbers, bOnlyOpenTasks );

			// Add everything to a set so that we can do quick tests later on
			for ( int32 CurTaskIndex = 0; CurTaskIndex < SelectedTaskNumbers.Num(); ++CurTaskIndex )
			{
				SelectedTaskNumberSet.Add( SelectedTaskNumbers[ CurTaskIndex ] );
			}
		}


		// Check to see which 'display filters' are enabled
		const bool bOnlyOpen = OpenOnly->IsChecked();
		const bool bAssignedToMe = AssignedToMe->IsChecked();
		const bool bCreatedByMe = CreatedByMe->IsChecked();
		const bool bCurrentMap = CurrentMap->IsChecked();

		// Grab the current loaded map name from the editor
		// @todo: Register for event to auto refresh task list when map loaded or new?
		FString CurrentMaNamePtr = GWorld->GetMapName();
		{
			const int32 MaxPrefixChars = 2;
			const int32 MaxSuffixChars = 3;

			if( CurrentMaNamePtr.Len() > MaxPrefixChars + 1 )
			{
				// Clean up any small prefixes or suffixes on the map name
				int32 FirstUnderscore = CurrentMaNamePtr.Find( TEXT( "_" ) );
				if( FirstUnderscore != INDEX_NONE )
				{
					const int32 NumPrefixChars = FirstUnderscore;
					if( NumPrefixChars <= MaxPrefixChars )
					{
						// Chop it!
						CurrentMaNamePtr = CurrentMaNamePtr.Mid( FirstUnderscore + 1 );
					}
				}
			}


			// Chop off multiple small suffixes
			bool bCheckForSuffix = true;
			while( bCheckForSuffix )
			{
				bCheckForSuffix = false;
				if( CurrentMaNamePtr.Len() > MaxSuffixChars + 1 )
				{
					// Clean up any small prefixes or suffixes on the map name
					int32 LastUnderscore = CurrentMaNamePtr.Find( TEXT( "_" ), ESearchCase::CaseSensitive, ESearchDir::FromEnd );
					if( LastUnderscore != INDEX_NONE )
					{
						const int32 NumSuffixChars = CurrentMaNamePtr.Len() - LastUnderscore;
						if( NumSuffixChars <= MaxSuffixChars )
						{
							// Chop it!
							CurrentMaNamePtr = CurrentMaNamePtr.Left( LastUnderscore );

							// Check for another suffix!
							bCheckForSuffix = true;
						}
					}
				}
			}
		}


		// Clear everything
		TaskEntries.Empty();
		TaskList->ClearSelection();

		// Grab the current user's real name
		FString UserRealName = TaskDataManager->GetUserRealName();
	
		// Copy all of the task entries to our own cached array
		const TArray< FTaskDatabaseEntry >& Tasks = TaskDataManager->GetCachedTaskArray();
		for ( int32 CurTaskIndex = 0; CurTaskIndex < Tasks.Num(); ++CurTaskIndex )
		{
			const FTaskDatabaseEntry& CurTask = Tasks[ CurTaskIndex ];

			if( ( !bOnlyOpen || CurTask.Status.StartsWith( TaskDataManager->GetOpenTaskStatusPrefix() ) ) &&
				( !bAssignedToMe || CurTask.AssignedTo.Contains( UserRealName) ) &&
				( !bCreatedByMe || CurTask.CreatedBy.Contains( UserRealName) ) &&
				( !bCurrentMap || CurTask.Summary.Contains( CurrentMaNamePtr) ) )
			{
				TSharedPtr<FTaskOverview> NewTask = MakeShareable( new FTaskOverview( CurTask ) );
				TaskEntries.Add( NewTask );
				
				// Try to keep the currently selected task selected
				if( SelectedTaskNumberSet.Contains( CurTask.Number ) )
				{
					TaskList->SetItemSelection( NewTask, true );
				}
			}
		}
		TaskList->RequestListRefresh();
	}


	if( Options & ( ETaskBrowserGUIRefreshOptions::ResetColumnSizes | ETaskBrowserGUIRefreshOptions::RebuildTaskList ) )
	{
		// Update task list column sizes
		TaskHeaders->ResetColumnWidths();
	}


	if( Options & ( ETaskBrowserGUIRefreshOptions::SortTaskList | ETaskBrowserGUIRefreshOptions::RebuildTaskList ) )
	{
		// Sort the task list
		TaskListItemSort();
	}


	if( Options & ETaskBrowserGUIRefreshOptions::UpdateTaskDescription )
	{
		FString TaskSummary;
		FString TaskDescription;

		const uint32 TaskNumber = TaskDataManager->GetFocusedTaskNumber();

		bool bHaveTask = false;
		if( TaskNumber != INDEX_NONE )
		{
			// Do we have any cached results to display?
			const FTaskDatabaseEntryDetails* TaskDetails = TaskDataManager->FindCachedTaskDetails( TaskNumber );
			if( TaskDetails != NULL )
			{
				// We have the task details!
				TaskSummary = TaskDetails->Summary;
				TaskDescription = TaskDetails->Description;

				// Append the task number to the name
				TaskSummary += FString::Printf( TEXT( " (# %i)" ), TaskNumber );

				bHaveTask = true;
			}
		}

		if( !bHaveTask )
		{
			// OK we don't have the task description yet, but it may be on it's way!  Let's check.
			if( TaskDataManager->GetGUIStatus() == ETaskDataManagerStatus::QueryingTaskDetails )
			{
				// Try to find the name of the task in our cached task entry array
				for ( int32 CurTaskIndex = 0; CurTaskIndex < TaskEntries.Num(); ++CurTaskIndex )
				{
					TSharedPtr<FTaskOverview> CurTask = TaskEntries[ CurTaskIndex ];
					if( CurTask->Number == TaskDataManager->GetFocusedTaskNumber() )
					{
						// Found it!
						TaskSummary = CurTask->Summary;

						// Append the task number to the name
						TaskSummary += FString::Printf( TEXT( " (# %i)" ), TaskNumber );

						break;
					}
				}

				// Let the user know we're downloading the task description now
				TaskDescription = NSLOCTEXT("UnrealEd", "TaskBrowser_WaitingForDescription", "\n(Downloading task details from server.  Please wait...)" ).ToString();
			}
		}

		// Update the task name text
		Summary->SetText( FText::FromString(TaskSummary) );

		// Update the task description
		Description->SetText( FText::FromString(TaskDescription) );
	}
}

void STaskBrowser::Callback_RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options )
{
	// Refresh the GUI
	RefreshGUI( Options );
}

#undef LOCTEXT_NAMESPACE
