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

namespace CarriedObjectState
{
	const FName Home = FName(TEXT("Home"));
	const FName Held = FName(TEXT("Held"));
	const FName Dropped = FName(TEXT("Dropped"));
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

namespace FGameRuleCategories
{
	const FName TeamPlay = FName(TEXT("TeamPlay"));
	const FName Casual = FName(TEXT("Casual"));
	const FName Big = FName(TEXT("Big"));
	const FName Competitive = FName(TEXT("Competitive"));
	const FName Custom = FName(TEXT("Custom"));
}

namespace FQuickMatchTypeRulesetTag
{
	const FString CTF = TEXT("CaptureTheFlag");
	const FString DM = TEXT("Deathmatch");
}