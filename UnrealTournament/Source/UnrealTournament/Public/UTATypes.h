// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.generated.h"

// Const defines for Dialogs
const uint16 UTDIALOG_BUTTON_OK = 0x0001;			
const uint16 UTDIALOG_BUTTON_CANCEL = 0x0002;
const uint16 UTDIALOG_BUTTON_YES = 0x0004;
const uint16 UTDIALOG_BUTTON_NO = 0x0008;
const uint16 UTDIALOG_BUTTON_HELP = 0x0010;
const uint16 UTDIALOG_BUTTON_RECONNECT = 0x0020;
const uint16 UTDIALOG_BUTTON_EXIT = 0x0040;
const uint16 UTDIALOG_BUTTON_QUIT = 0x0080;
const uint16 UTDIALOG_BUTTON_VIEW = 0x0100;
const uint16 UTDIALOG_BUTTON_YESCLEAR = 0x0200;
const uint16 UTDIALOG_BUTTON_PLAY = 0x0400;
const uint16 UTDIALOG_BUTTON_LAN = 0x0800;

UENUM()
namespace EGameStage
{
	enum Type
	{
		Initializing,
		PreGame, 
		GameInProgress,
		GameOver,
		MAX,
	};
}

UENUM()
namespace ETextHorzPos
{
	enum Type
	{
		Left,
		Center, 
		Right,
		MAX,
	};
}

UENUM()
namespace ETextVertPos
{
	enum Type
	{
		Top,
		Center,
		Bottom,
		MAX,
	};
}

const FName NAME_Custom = FName(TEXT("Custom"));

namespace CarriedObjectState
{
	const FName Home = FName(TEXT("Home"));
	const FName Held = FName(TEXT("Held"));
	const FName Dropped = FName(TEXT("Dropped"));
}

namespace InventoryEventName
{
	const FName Landed = FName(TEXT("Landed"));
	const FName LandedWater = FName(TEXT("LandedWater"));
	const FName FiredWeapon = FName(TEXT("FiredWeapon"));
	const FName Jump = FName(TEXT("Jump"));
	const FName MultiJump = FName(TEXT("MultiJump"));
	const FName Dodge = FName(TEXT("Dodge"));
}

namespace StatusMessage
{
	const FName NeedBackup = FName(TEXT("NeedBackup"));
	const FName EnemyFCHere = FName(TEXT("EnemyFCHere"));
	const FName AreaSecure = FName(TEXT("AreaSecure"));
	const FName IGotFlag = FName(TEXT("IGotFlag"));
	const FName DefendFlag = FName(TEXT("DefendFlag"));
	const FName DefendFC = FName(TEXT("DefendFC"));
	const FName GetFlagBack = FName(TEXT("GetFlagBack"));
	const FName ImGoingIn = FName(TEXT("ImGoingIn"));
	const FName ImOnDefense = FName(TEXT("ImOnDefense"));
	const FName ImOnOffense = FName(TEXT("ImOnOffense"));
	const FName SpreadOut = FName(TEXT("SpreadOut"));
	const FName BaseUnderAttack = FName(TEXT("BaseUnderAttack"));
}

namespace HighlightNames
{
	const FName TopScorer = FName(TEXT("TopScorer"));
	const FName MostKills = FName(TEXT("MostKills"));
	const FName LeastDeaths = FName(TEXT("LeastDeaths"));
	const FName BestKD = FName(TEXT("BestKD"));
	const FName MostWeaponKills = FName(TEXT("MostWeaponKills"));
	const FName BestCombo = FName(TEXT("BestCombo"));
	const FName MostHeadShots = FName(TEXT("MostHeadShots"));
	const FName MostAirRockets = FName(TEXT("MostAirRockets"));

	const FName TopScorerRed = FName(TEXT("TopScorerRed"));
	const FName TopScorerBlue = FName(TEXT("TopScorerBlue"));
	const FName TopFlagCapturesRed = FName(TEXT("TopFlagCapturesRed"));
	const FName TopFlagCapturesBlue = FName(TEXT("TopFlagCapturesBlue"));
	const FName FlagCaptures = FName(TEXT("FlagCaptures"));
	const FName TopAssistsRed = FName(TEXT("TopAssistsRed"));
	const FName TopAssistsBlue = FName(TEXT("TopAssistsBlue"));
	const FName Assists = FName(TEXT("Assists"));
	const FName TopFlagReturnsRed = FName(TEXT("TopFlagReturnsRed"));
	const FName TopFlagReturnsBlue = FName(TEXT("TopFlagReturnsBlue"));
	const FName FlagReturns = FName(TEXT("FlagReturns"));
}

namespace ArmorTypeName
{
	const FName ShieldBelt = FName(TEXT("ShieldBelt"));
	const FName ThighPads = FName(TEXT("ThighPads"));
	const FName FlakVest = FName(TEXT("FlakVest"));
	const FName Helmet = FName(TEXT("Helmet"));
}

namespace ChatDestinations
{
	// You can chat with your friends from anywhere
	const FName Friends = FName(TEXT("CHAT_Friends"));

	// These are lobby chat types
	const FName Global = FName(TEXT("CHAT_Global"));
	const FName Match = FName(TEXT("CHAT_Match"));

	// these are general game chating
	const FName Lobby = FName(TEXT("CHAT_Lobby"));
	const FName Local = FName(TEXT("CHAT_Local"));
	const FName Team = FName(TEXT("CHAT_Team"));

	const FName System = FName(TEXT("CHAT_System"));
	const FName MOTD = FName(TEXT("CHAT_MOTD"));
}

// Our Dialog results delegate.  It passes in a reference to the dialog triggering it, as well as the button id 
DECLARE_DELEGATE_TwoParams(FDialogResultDelegate, TSharedPtr<SCompoundWidget>, uint16);

USTRUCT()
struct FTextureUVs
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TextureUVs")
	float U;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TextureUVs")
	float V;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TextureUVs")
	float UL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TextureUVs")
	float VL;

	FTextureUVs()
		: U(0.0f)
		, V(0.0f)
		, UL(0.0f)
		, VL(0.0f)
	{};

	FTextureUVs(float inU, float inV, float inUL, float inVL)
	{
		U = inU; V = inV; UL = inUL;  VL = inVL;
	}

};

USTRUCT(BlueprintType)
struct FHUDRenderObject
{
	GENERATED_USTRUCT_BODY()

	// Set to true to make this renderobject hidden
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bHidden;

	// The depth priority.  Higher means rendered later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float RenderPriority;

	// Where (in unscaled pixels) should this HUDObject be displayed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D Position;

	// How big (in unscaled pixels) is this HUDObject.  NOTE: the HUD object will be scaled to fit the size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D Size;

	// The Text Color to display this in.  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor RenderColor;

	// An override for the opacity of this object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float RenderOpacity;

	FHUDRenderObject()
	{
		RenderPriority = 0.0f;
		RenderColor = FLinearColor::White;
		RenderOpacity = 1.0f;
	};

public:
	virtual float GetWidth() { return Size.X; }
	virtual float GetHeight() { return Size.Y; }
};


USTRUCT(BlueprintType)
struct FHUDRenderObject_Texture : public FHUDRenderObject
{
	GENERATED_USTRUCT_BODY()

	// The texture to draw
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	UTexture* Atlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FTextureUVs UVs;

	// If true, this texture object will pickup the team color of the owner
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bUseTeamColors;

	// The team colors to select from.  If this array is empty, the base HUD team colors will be used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FLinearColor> TeamColorOverrides;

	// If true, this is a background element and should take the HUDWidgetBorderOpacity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bIsBorderElement;

	// If true, this is a background element and should take the HUDWidgetBorderOpacity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bIsSlateElement;


	// The offset to be applied to the position.  They are normalized to the width and height of the image being draw.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D RenderOffset;

	// The rotation angle to render with
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float Rotation;

	// The point at which within the image that the rotation will be around
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D RotPivot;

	FHUDRenderObject_Texture() : FHUDRenderObject()
	{
		Atlas = NULL;
		bUseTeamColors = false;
		bIsBorderElement = false;
		Rotation = 0.0f;
	}

public:
	virtual float GetWidth()
	{
		return (Size.X <= 0) ? UVs.UL : Size.X;
	}

	virtual float GetHeight()
	{
		return (Size.Y <= 0) ? UVs.VL : Size.Y;
	}

};

// This is a simple delegate that returns an FTEXT value for rendering things in HUD render widgets
DECLARE_DELEGATE_RetVal(FText, FUTGetTextDelegate)

USTRUCT(BlueprintType)
struct FHUDRenderObject_Text : public FHUDRenderObject
{
	GENERATED_USTRUCT_BODY()

	// If this delegate is set, then Text is ignore and this function is called each frame.
	FUTGetTextDelegate GetTextDelegate;

	// The text to be displayed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FText Text;

	// The font to render with
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	UFont* Font;

	// Additional scaling applied to the font.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float TextScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bDrawShadow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D ShadowDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor ShadowColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bDrawOutline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor OutlineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TEnumAsByte<ETextHorzPos::Type> HorzPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TEnumAsByte<ETextVertPos::Type> VertPosition;

	FHUDRenderObject_Text() : FHUDRenderObject()
	{
		Font = NULL;
		TextScale = 1.0f;
		bDrawShadow = false;
		ShadowColor = FLinearColor::White;
		bDrawOutline = false;
		OutlineColor = FLinearColor::Black;
		HorzPosition = ETextHorzPos::Left;
		VertPosition = ETextVertPos::Top;
	}

public:
	FVector2D GetSize()
	{
		if (Font)
		{
			FText TextToRender = Text;
			if (GetTextDelegate.IsBound())
			{
				TextToRender = GetTextDelegate.Execute();
			}

			int32 Width = 0;
			int32 Height = 0;
			Font->GetStringHeightAndWidth(TextToRender.ToString(), Height, Width);
			return FVector2D(Width * TextScale , Height * TextScale);
		}
	
		return FVector2D(0,0);
	}
};

DECLARE_DELEGATE(FGameOptionChangedDelegate);

// These are attribute tags that can be used to look up data in the MatchAttributesDatastore
namespace EMatchAttributeTags
{
	const FName GameMode = FName(TEXT("GameMode"));
	const FName GameName = FName(TEXT("GameName"));
	const FName Map = FName(TEXT("Map"));
	const FName Options = FName(TEXT("Options"));
	const FName Stats = FName(TEXT("Stats"));
	const FName Host = FName(TEXT("Host"));
	const FName PlayTime = FName(TEXT("PlayTime"));
	const FName RedScore = FName(TEXT("RedScore"));
	const FName BlueScore = FName(TEXT("BlueScore"));
	const FName PlayerCount = FName(TEXT("PlayerCount"));
}

namespace ELobbyMatchState
{
	const FName Dead = TEXT("Dead");
	const FName Initializing = TEXT("Initializing");
	const FName Setup = TEXT("Setup");
	const FName WaitingForPlayers = TEXT("WaitingForPlayers");
	const FName Launching = TEXT("Launching");
	const FName Aborting = TEXT("Aborting");
	const FName InProgress = TEXT("InProgress");
	const FName Completed = TEXT("Completed");
	const FName Recycling = TEXT("Recycling");
	const FName Returning = TEXT("Returning");
}

namespace QuickMatchTypes
{
	const FName Deathmatch = TEXT("QuickMatchDeathmatch");
	const FName CaptureTheFlag = TEXT("QuickMatchCaptureTheFlag");
}

class FSimpleListData
{
public: 
	FString DisplayText;
	FLinearColor DisplayColor;

	FSimpleListData(FString inDisplayText, FLinearColor inDisplayColor)
		: DisplayText(inDisplayText)
		, DisplayColor(inDisplayColor)
	{
	};

	static TSharedRef<FSimpleListData> Make( FString inDisplayText, FLinearColor inDisplayColor)
	{
		return MakeShareable( new FSimpleListData( inDisplayText, inDisplayColor ) );
	}
};

const FString HUBSessionIdKey = "HUBSessionId";

namespace FFriendsStatus
{
	const FName IsBot = FName(TEXT("IsBot"));
	const FName IsYou = FName(TEXT("IsYou"));
	const FName NotAFriend = FName(TEXT("NotAFriend"));
	const FName FriendRequestPending = FName(TEXT("FriendRequestPending"));
	const FName Friend = FName(TEXT("Friend"));
}

namespace FQuickMatchTypeRulesetTag
{
	const FString CTF = TEXT("CTF");
	const FString DM = TEXT("Deathmatch");
}

UENUM()
namespace ERedirectStatus
{
	enum Type
	{
		Pending,
		InProgress,
		Completed, 
		Failed,
		Cancelled,
		MAX,
	};
}

/*
	Describes a package that might be needed
*/
USTRUCT()
struct FPackageRedirectReference
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString PackageName;

	UPROPERTY()
	FString PackageURLProtocol;

	UPROPERTY()
	FString PackageURL;

	UPROPERTY()
	FString PackageChecksum;

	FPackageRedirectReference()
		: PackageName(TEXT("")), PackageURLProtocol(TEXT("")), PackageURL(TEXT("")), PackageChecksum(TEXT(""))
	{}

	FPackageRedirectReference(FString inPackageName, FString inPackageURLProtocol, FString inPackageURL, FString inPackageChecksum)
		: PackageName(inPackageName), PackageURLProtocol(inPackageURLProtocol), PackageURL(inPackageURL), PackageChecksum(inPackageChecksum)
	{}

	FPackageRedirectReference(FPackageRedirectReference* OtherReference)
	{
		PackageName = OtherReference->PackageName;
		PackageURLProtocol = OtherReference->PackageURLProtocol;
		PackageURL = OtherReference->PackageURL;
		PackageChecksum = OtherReference->PackageChecksum;
	}

	// Converts the redirect to a download URL
	FString ToString()
	{
		return PackageURLProtocol + TEXT("://") + PackageURL + TEXT(" ");
	}

};


/**
 *	Holds information about a map that can be set via config.  This will be used to build the FMapListInfo objects in various places but contains
 *  a cut down copy of the content to make life easier to manage.
 **/
USTRUCT()
struct FConfigMapInfo
{
	GENERATED_USTRUCT_BODY()

	// NOTE: this can be the long or short name for the map and will be validated when the maps are loaded
	UPROPERTY(Config)
	FString MapName;						

	// The Redirect for this map.
	UPROPERTY(Config)
	FPackageRedirectReference Redirect;		

	FConfigMapInfo()
	{
		MapName = TEXT("");
		Redirect.PackageName = TEXT("");
		Redirect.PackageURL = TEXT("");
		Redirect.PackageURLProtocol = TEXT("");
		Redirect.PackageChecksum = TEXT("");
	}

	FConfigMapInfo(const FString& inMapName)
	{
		MapName = inMapName;
		Redirect.PackageName = TEXT("");
		Redirect.PackageURL = TEXT("");
		Redirect.PackageURLProtocol = TEXT("");
		Redirect.PackageChecksum = TEXT("");
	}

	FConfigMapInfo(const FConfigMapInfo& ExistingRuleMapInfo)
	{
		MapName = ExistingRuleMapInfo.MapName;
		Redirect = ExistingRuleMapInfo.Redirect;
	}

	FConfigMapInfo(const FString& inMapName, const FString& inPackageName, const FString& inPackageURL, const FString& inPackageChecksum)
	{
		MapName = inMapName;
		Redirect.PackageName = inPackageName;
		Redirect.PackageURL = inPackageURL;
		Redirect.PackageURLProtocol = TEXT("http");
		Redirect.PackageChecksum = inPackageChecksum;
	}
};



UENUM()
namespace EGameDataType
{
	enum Type
	{
		GameMode,
		Map,
		Mutator, 
		MAX,
	};
}

USTRUCT()
struct FAllowedData
{
	GENERATED_USTRUCT_BODY()

	// What type of data is this.
	UPROPERTY()
	TEnumAsByte<EGameDataType::Type> DataType;

	// The package name of this content
	UPROPERTY()
	FString PackageName;

	FAllowedData()
		: DataType(EGameDataType::GameMode)
		, PackageName(TEXT(""))
	{}

	FAllowedData(EGameDataType::Type inDataType, const FString& inPackageName)
		: DataType(inDataType)
		, PackageName(inPackageName)
	{}

};

UENUM()
namespace EUnrealRoles
{
	enum Type
	{
		Gamer,
		Developer,
		Concepter,
		Contributor, 
		Marketplace,
		Prototyper,
		Ambassador,
		MAX,
	};
}

USTRUCT()
struct FFlagInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Title;

	UPROPERTY()
	int32 Id;

	FFlagInfo()
		: Title(FString())
		, Id(0)
	{
	}

	FFlagInfo(const FString inTitle, int32 inId)
		: Title(inTitle)
		, Id(inId)
	{
	}

	static TSharedRef<FFlagInfo> Make(const FFlagInfo& OtherFlag)
	{
		return MakeShareable( new FFlagInfo( OtherFlag.Title, OtherFlag.Id) );
	}

	static TSharedRef<FFlagInfo> Make(const FString inTitle, int32 inId)
	{
		return MakeShareable( new FFlagInfo( inTitle, inId) );
	}

};

static FName NAME_MapInfo_Title(TEXT("Title"));
static FName NAME_MapInfo_Author(TEXT("Author"));
static FName NAME_MapInfo_Description(TEXT("Description"));
static FName NAME_MapInfo_OptimalPlayerCount(TEXT("OptimalPlayerCount"));
static FName NAME_MapInfo_OptimalTeamPlayerCount(TEXT("OptimalTeamPlayerCount"));
static FName NAME_MapInfo_ScreenshotReference(TEXT("ScreenshotReference"));

// Called upon completion of a redirect transfer.  
DECLARE_MULTICAST_DELEGATE_ThreeParams(FContentDownloadComplete, class UUTGameViewportClient*, ERedirectStatus::Type, const FString&);

USTRUCT()
struct FServerInstanceData 
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString RuleSetIcon;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	UTexture2D* BadgeTexture;

#if !UE_SERVER
	FSlateDynamicImageBrush* SlateBadge;
#endif

	FServerInstanceData()
		: RuleSetIcon(TEXT(""))
		, Description(TEXT(""))
		, BadgeTexture(NULL)
	{
	}

	FServerInstanceData(FString inRuleSetIcon, FString inDescription)
		: RuleSetIcon(inRuleSetIcon)
		, Description(inDescription)
		, BadgeTexture(NULL)
	{
	}

	static TSharedRef<FServerInstanceData> Make(FServerInstanceData& inData)
	{
		return MakeShareable( new FServerInstanceData( inData.RuleSetIcon, inData.Description) );
	}

};

UENUM()
namespace EHudKillMsgStyle
{
	enum Type
	{
		KMS_Text,
		KMS_Icon,
		KMS_Both,
		KMS_None,
		MAX,
	};
}

namespace EQuickMatchResults
{
	const FName CantJoin = FName(TEXT("CantJoin"));
	const FName WaitingForStart = FName(TEXT("WaitingForStart"));
	const FName WaitingForStartNew = FName(TEXT("WaitingForStartNew"));
	const FName Join = FName(TEXT("Join"));
}

namespace EEpicDefaultRuleTags
{
	const FString Deathmatch = TEXT("DEATHMATCH");
	const FString BigDM = TEXT("BIGDM");
	const FString TDM = TEXT("TDM");
	const FString DUEL = TEXT("DUEL");
	const FString SHOWDOWN = TEXT("SHOWDOWN");
	const FString CTF = TEXT("CTF");
	const FString BIGCTF = TEXT("BIGCTF");
	const FString iDM = TEXT("iDM");
	const FString iTDM = TEXT("iTDM");
	const FString iCTF = TEXT("iCTF");
	const FString iCTFT = TEXT("iCTF+T");
}


USTRUCT(BlueprintType)
struct FCrosshairInfo
{
	GENERATED_USTRUCT_BODY()

		FCrosshairInfo()
	{
		//Global is used to describe the crosshair that is used when bCustomWeaponCrosshairs == false
		WeaponClassName = TEXT("Global");
		CrosshairClassName = TEXT("/Game/RestrictedAssets/UI/Crosshairs/BP_DefaultCrosshair.BP_DefaultCrosshair_C");
		Color = FLinearColor::White;
		Scale = 1.0f;
	}

	UPROPERTY(EditAnywhere, GlobalConfig, Category = CrosshairInfo)
	FString CrosshairClassName;

	UPROPERTY(EditAnywhere, GlobalConfig, Category = CrosshairInfo)
	FString WeaponClassName;

	UPROPERTY(EditAnywhere, GlobalConfig, Category = CrosshairInfo)
	float Scale;

	UPROPERTY(EditAnywhere, GlobalConfig, Category = CrosshairInfo)
	FLinearColor Color;

	bool operator==(const FCrosshairInfo& Other) const
	{
		return WeaponClassName == Other.WeaponClassName;
	}
};

namespace EPlayerListContentCommand
{
	const FName PlayerCard = FName(TEXT("PlayerCard"));
	const FName ChangeTeam = FName(TEXT("ChangeTeam"));
	const FName Spectate = FName(TEXT("Spectate"));
	const FName Kick = FName(TEXT("Kick"));
	const FName Ban = FName(TEXT("Ban"));
	const FName Invite = FName(TEXT("Invite"));
}


USTRUCT()
struct FMatchPlayerListStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	FString PlayerScore;

	FMatchPlayerListStruct()
	{
		PlayerName = TEXT("");
		PlayerScore = TEXT("");
	}

	FMatchPlayerListStruct(const FString& inPlayerName, const FString& inPlayerScore)
		: PlayerName(inPlayerName)
		, PlayerScore(inPlayerScore)
	{
	}

};


struct FMatchPlayerListCompare
{
	FORCEINLINE bool operator()( const FMatchPlayerListStruct A, const FMatchPlayerListStruct B ) const 
	{
		return A.PlayerName < B.PlayerName;
	}
};

