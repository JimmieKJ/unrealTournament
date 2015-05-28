// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.generated.h"

INPUTCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogInput, Log, All);

USTRUCT(BlueprintType)
struct INPUTCORE_API FKey
{
	GENERATED_USTRUCT_BODY()

	FKey()
	{
	}

	FKey(const FName InName)
		: KeyName(InName)
	{
	}

	FKey(const TCHAR* InName)
		: KeyName(FName(InName))
	{
	}

	FKey(const ANSICHAR* InName)
		: KeyName(FName(InName))
	{
	}

	bool IsValid() const;
	bool IsModifierKey() const;
	bool IsGamepadKey() const;
	bool IsMouseButton() const;
	bool IsFloatAxis() const;
	bool IsVectorAxis() const;
	bool IsBindableInBlueprints() const;
	FText GetDisplayName() const;
	FString ToString() const;
	FName GetFName() const;
	FName GetMenuCategory() const;

	bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar);
	bool ExportTextItem(FString& ValueStr, FKey const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);
	void PostSerialize(const FArchive& Ar);

	friend bool operator==(const FKey& KeyA, const FKey& KeyB) { return KeyA.KeyName == KeyB.KeyName; }
	friend bool operator!=(const FKey& KeyA, const FKey& KeyB) { return KeyA.KeyName != KeyB.KeyName; }
	friend bool operator<(const FKey& KeyA, const FKey& KeyB) { return KeyA.KeyName < KeyB.KeyName; }
	friend uint32 GetTypeHash(const FKey& Key) { return GetTypeHash(Key.KeyName); }

	friend struct EKeys;

private:

	UPROPERTY()
	FName KeyName;
	
	mutable class TSharedPtr<struct FKeyDetails> KeyDetails;

	void ConditionalLookupKeyDetails() const;

};

template<>
struct TStructOpsTypeTraits<FKey> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithPostSerialize = true,
		WithCopy = true,		// Necessary so that TSharedPtr<FKeyDetails> Data is copied around
	};
};

DECLARE_DELEGATE_RetVal_OneParam(FText, FGetKeyDisplayNameSignature, const FKey);

struct INPUTCORE_API FKeyDetails
{
	enum EKeyFlags
	{
		GamepadKey				= 0x01,
		MouseButton				= 0x02,
		ModifierKey				= 0x04,
		NotBlueprintBindableKey	= 0x08,
		FloatAxis				= 0x10,
		VectorAxis				= 0x20,

		NoFlags                 = 0,
	};

	FKeyDetails(const FKey InKey, const TAttribute<FText>& InDisplayName, const uint8 InKeyFlags = 0, const FName InMenuCategory = NAME_None);

	bool IsModifierKey() const { return bIsModifierKey != 0; }
	bool IsGamepadKey() const { return bIsGamepadKey != 0; }
	bool IsMouseButton() const { return bIsMouseButton != 0; }
	bool IsFloatAxis() const { return AxisType == EInputAxisType::Float; }
	bool IsVectorAxis() const { return AxisType == EInputAxisType::Vector; }
	bool IsBindableInBlueprints() const { return bIsBindableInBlueprints != 0; }
	FName GetMenuCategory() const { return MenuCategory; }
	FText GetDisplayName() const;
	const FKey& GetKey() const { return Key; }

private:

	enum class EInputAxisType : uint8
	{
		None,
		Float,
		Vector
	};

	FKey  Key;
	
	TAttribute<FText> DisplayName;

	FName MenuCategory;

	int32 bIsModifierKey:1;
	int32 bIsGamepadKey:1;
	int32 bIsMouseButton:1;
	int32 bIsBindableInBlueprints:1;
	EInputAxisType AxisType;

};

UENUM(BlueprintType)
namespace ETouchIndex
{
	enum Type
	{
		Touch1,
		Touch2,
		Touch3,
		Touch4,
		Touch5,
		Touch6,
		Touch7,
		Touch8,
		Touch9,
		Touch10 // The number of entries in ETouchIndex must match the number of touch keys defined in EKeys and NUM_TOUCH_KEYS above
	};
}

UENUM()
namespace EConsoleForGamepadLabels
{
	enum Type
	{
		None,
		XBoxOne,
		PS4
	};
}
struct INPUTCORE_API EKeys
{
	static const FKey MouseX;
	static const FKey MouseY;
	static const FKey MouseScrollUp;
	static const FKey MouseScrollDown;

	static const FKey LeftMouseButton;
	static const FKey RightMouseButton;
	static const FKey MiddleMouseButton;
	static const FKey ThumbMouseButton;
	static const FKey ThumbMouseButton2;

	static const FKey BackSpace;
	static const FKey Tab;
	static const FKey Enter;
	static const FKey Pause;

	static const FKey CapsLock;
	static const FKey Escape;
	static const FKey SpaceBar;
	static const FKey PageUp;
	static const FKey PageDown;
	static const FKey End;
	static const FKey Home;

	static const FKey Left;
	static const FKey Up;
	static const FKey Right;
	static const FKey Down;

	static const FKey Insert;
	static const FKey Delete;

	static const FKey Zero;
	static const FKey One;
	static const FKey Two;
	static const FKey Three;
	static const FKey Four;
	static const FKey Five;
	static const FKey Six;
	static const FKey Seven;
	static const FKey Eight;
	static const FKey Nine;

	static const FKey A;
	static const FKey B;
	static const FKey C;
	static const FKey D;
	static const FKey E;
	static const FKey F;
	static const FKey G;
	static const FKey H;
	static const FKey I;
	static const FKey J;
	static const FKey K;
	static const FKey L;
	static const FKey M;
	static const FKey N;
	static const FKey O;
	static const FKey P;
	static const FKey Q;
	static const FKey R;
	static const FKey S;
	static const FKey T;
	static const FKey U;
	static const FKey V;
	static const FKey W;
	static const FKey X;
	static const FKey Y;
	static const FKey Z;

	static const FKey NumPadZero;
	static const FKey NumPadOne;
	static const FKey NumPadTwo;
	static const FKey NumPadThree;
	static const FKey NumPadFour;
	static const FKey NumPadFive;
	static const FKey NumPadSix;
	static const FKey NumPadSeven;
	static const FKey NumPadEight;
	static const FKey NumPadNine;

	static const FKey Multiply;
	static const FKey Add;
	static const FKey Subtract;
	static const FKey Decimal;
	static const FKey Divide;

	static const FKey F1;
	static const FKey F2;
	static const FKey F3;
	static const FKey F4;
	static const FKey F5;
	static const FKey F6;
	static const FKey F7;
	static const FKey F8;
	static const FKey F9;
	static const FKey F10;
	static const FKey F11;
	static const FKey F12;

	static const FKey NumLock;

	static const FKey ScrollLock;

	static const FKey LeftShift;
	static const FKey RightShift;
	static const FKey LeftControl;
	static const FKey RightControl;
	static const FKey LeftAlt;
	static const FKey RightAlt;
	static const FKey LeftCommand;
	static const FKey RightCommand;

	static const FKey Semicolon;
	static const FKey Equals;
	static const FKey Comma;
	static const FKey Underscore;
	static const FKey Hyphen;
	static const FKey Period;
	static const FKey Slash;
	static const FKey Tilde;
	static const FKey LeftBracket;
	static const FKey Backslash;
	static const FKey RightBracket;
	static const FKey Apostrophe;

	static const FKey Ampersand;
	static const FKey Asterix;
	static const FKey Caret;
	static const FKey Colon;
	static const FKey Dollar;
	static const FKey Exclamation;
	static const FKey LeftParantheses;
	static const FKey RightParantheses;
	static const FKey Quote;

	static const FKey A_AccentGrave;
	static const FKey E_AccentGrave;
	static const FKey E_AccentAigu;
	static const FKey C_Cedille;

	// Platform Keys
	// These keys platform specific versions of keys that go by different names.
	// The delete key is a good example, on Windows Delete is the virtual key for Delete.
	// On Macs, the Delete key is the virtual key for BackSpace.
	static const FKey Platform_Delete;

	// Gameplay Keys
	static const FKey Gamepad_LeftX;
	static const FKey Gamepad_LeftY;
	static const FKey Gamepad_RightX;
	static const FKey Gamepad_RightY;
	static const FKey Gamepad_LeftTriggerAxis;
	static const FKey Gamepad_RightTriggerAxis;

	static const FKey Gamepad_LeftThumbstick;
	static const FKey Gamepad_RightThumbstick;
	static const FKey Gamepad_Special_Left;
	static const FKey Gamepad_Special_Right;
	static const FKey Gamepad_FaceButton_Bottom;
	static const FKey Gamepad_FaceButton_Right;
	static const FKey Gamepad_FaceButton_Left;
	static const FKey Gamepad_FaceButton_Top;
	static const FKey Gamepad_LeftShoulder;
	static const FKey Gamepad_RightShoulder;
	static const FKey Gamepad_LeftTrigger;
	static const FKey Gamepad_RightTrigger;
	static const FKey Gamepad_DPad_Up;
	static const FKey Gamepad_DPad_Down;
	static const FKey Gamepad_DPad_Right;
	static const FKey Gamepad_DPad_Left;

	// Virtual key codes used for input axis button press/release emulation
	static const FKey Gamepad_LeftStick_Up;
	static const FKey Gamepad_LeftStick_Down;
	static const FKey Gamepad_LeftStick_Right;
	static const FKey Gamepad_LeftStick_Left;

	static const FKey Gamepad_RightStick_Up;
	static const FKey Gamepad_RightStick_Down;
	static const FKey Gamepad_RightStick_Right;
	static const FKey Gamepad_RightStick_Left;

	// static const FKey Vector axes (FVector; not float)
	static const FKey Tilt;
	static const FKey RotationRate;
	static const FKey Gravity;
	static const FKey Acceleration;

	// Gestures
	static const FKey Gesture_Pinch;
	static const FKey Gesture_Flick;

	// PS4-specific
	static const FKey PS4_Special;

	// Steam Controller Specific
	static const FKey Steam_Touch_0;
	static const FKey Steam_Touch_1;
	static const FKey Steam_Touch_2;
	static const FKey Steam_Touch_3;
	static const FKey Steam_Back_Left;
	static const FKey Steam_Back_Right;

	// Xbox One global speech commands
	static const FKey Global_Menu;
	static const FKey Global_View;
	static const FKey Global_Pause;
	static const FKey Global_Play;
	static const FKey Global_Back;

	// Android-specific
	static const FKey Android_Back;
	static const FKey Android_Volume_Up;
	static const FKey Android_Volume_Down;
	static const FKey Android_Menu;

	static const FKey Invalid;

	static const int32 NUM_TOUCH_KEYS = 11;
	static const FKey TouchKeys[NUM_TOUCH_KEYS];

	static EConsoleForGamepadLabels::Type ConsoleForGamepadLabels;

	static const FName NAME_KeyboardCategory;
	static const FName NAME_GamepadCategory;
	static const FName NAME_MouseCategory;

	static void Initialize();
	static void AddKey(const FKeyDetails& KeyDetails);
	static void GetAllKeys(TArray<FKey>& OutKeys);
	static TSharedPtr<FKeyDetails> GetKeyDetails(const FKey Key);

	// These exist for backwards compatibility reasons only
	static bool IsModifierKey(FKey Key) { return Key.IsModifierKey(); }
	static bool IsGamepadKey(FKey Key) { return Key.IsGamepadKey(); }
	static bool IsAxis(FKey Key) { return Key.IsFloatAxis(); }
	static bool IsBindableInBlueprints(const FKey Key) { return Key.IsBindableInBlueprints(); }
	static void SetConsoleForGamepadLabels(const EConsoleForGamepadLabels::Type Console) { ConsoleForGamepadLabels = Console; }

	// Function that provides remapping for some gamepad keys in display windows
	static FText GetGamepadDisplayName(const FKey Key);

	static void AddMenuCategoryDisplayInfo(const FName CategoryName, const FText DisplayName, const FName PaletteIcon);
	static FText GetMenuCategoryDisplayName(const FName CategoryName);
	static FName GetMenuCategoryPaletteIcon(const FName CategoryName);
private:

	struct FCategoryDisplayInfo
	{
		FText DisplayName;
		FName PaletteIcon;
	};

	static TMap<FKey, TSharedPtr<FKeyDetails> > InputKeys;
	static TMap<FName, FCategoryDisplayInfo> MenuCategoryDisplayInfo;
	static bool bInitialized;

};

// various states of touch inputs
UENUM()
namespace ETouchType
{
	enum Type
	{
		Began,
		Moved,
		Stationary,
		Ended,

		NumTypes
	};
}


struct INPUTCORE_API FInputKeyManager
{
public:
	static FInputKeyManager& Get();

	void GetCodesFromKey(const FKey Key, const uint16*& KeyCode, const uint16*& CharCode) const;

	/**
	 * Retrieves the key mapped to the specified character code.
	 * @param KeyCode	The key code to get the name for.
	 */
	FKey GetKeyFromCodes( const uint16 KeyCode, const uint16 CharCode ) const;
	void InitKeyMappings();
private:
	FInputKeyManager()
	{
		InitKeyMappings();
	}

	static TSharedPtr< FInputKeyManager > Instance;
	TMap<uint16, FKey> KeyMapVirtualToEnum;
	TMap<uint16, FKey> KeyMapCharToEnum;
};

UCLASS(abstract)
class UInputCoreTypes : public UObject
{
	GENERATED_BODY()

};