// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Interaction.cpp: See .UC for for info
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/Console.h"
#include "Engine/LevelScriptActor.h"
#include "SlateBasics.h"
#include "DefaultValueHelper.h"
#include "ConsoleSettings.h"
#include "GameFramework/InputSettings.h"
#include "Stats/StatsData.h"

static const uint32 MAX_AUTOCOMPLETION_LINES = 20;

static FName NAME_Typing = FName(TEXT("Typing"));
static FName NAME_Open = FName(TEXT("Open"));

class FConsoleVariableAutoCompleteVisitor 
{
public:
	// @param Name must not be 0
	// @param CVar must not be 0
	static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CVar,TArray<struct FAutoCompleteCommand>& Sink)
	{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(CVar->TestFlags(ECVF_Cheat))
		{
			return;
		}
#endif // (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(CVar->TestFlags(ECVF_Unregistered))
		{
			return;
		}

		// can be optimized
		int32 NewIdx = Sink.AddZeroed(1);

		FAutoCompleteCommand& Cmd = Sink[NewIdx];
		
		Cmd.Command = Name;
	}
};

UConsole::UConsole(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
//	, HistoryBufferCur(0)
//	, AutoCompleteCursor(-1)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> BlackTexture;
		ConstructorHelpers::FObjectFinder<UTexture2D> WhiteSquareTexture;
		FConstructorStatics()
			: BlackTexture(TEXT("/Engine/EngineResources/Black"))
			, WhiteSquareTexture(TEXT("/Engine/EngineResources/WhiteSquareTexture"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	DefaultTexture_Black = ConstructorStatics.BlackTexture.Object;
	DefaultTexture_White = ConstructorStatics.WhiteSquareTexture.Object;
}

UConsole::~UConsole()
{
	// At shutdown, GLog may already be null
	if( GLog != NULL )
	{
		GLog->RemoveOutputDevice(this);
	}
}

void UConsole::PostInitProperties()
{
#if WITH_EDITOR
	// Re-load config properties when in editor to preserve command history
	// between PIE sessions. Can't use perobjectconfig because the console 
	// name changes each PIE session.
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		LoadConfig();
	}
#endif
	Super::PostInitProperties();
}

void UConsole::BuildRuntimeAutoCompleteList(bool bForce)
{
#if !UE_BUILD_SHIPPING
	if (!bForce)
	{
		// unless forced delay updating until needed
		bIsRuntimeAutoCompleteUpToDate = false;
		return;
	}

	// clear the existing tree
	//@todo - probably only need to rebuild the tree + partial command list on level load
	for (int32 Idx = 0; Idx < AutoCompleteTree.ChildNodes.Num(); Idx++)
	{
		FAutoCompleteNode *Node = AutoCompleteTree.ChildNodes[Idx];
		delete Node;
	}

	AutoCompleteTree.ChildNodes.Empty();

	const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();

	// copy the manual list first
	AutoCompleteList.Empty();
	AutoCompleteList.AddZeroed(ConsoleSettings->ManualAutoCompleteList.Num());
	for (int32 Idx = 0; Idx < ConsoleSettings->ManualAutoCompleteList.Num(); Idx++)
	{
		AutoCompleteList[Idx] = ConsoleSettings->ManualAutoCompleteList[Idx];
	}

	// console variables
	{
		IConsoleManager::Get().ForEachConsoleObject(
			FConsoleObjectVisitor::CreateStatic< TArray<struct FAutoCompleteCommand>& >(
				&FConsoleVariableAutoCompleteVisitor::OnConsoleVariable,
				AutoCompleteList ) );
	}

	// iterate through script exec functions and append to the list
	int32 ScriptExecCnt = 0;
	for (TObjectIterator<UFunction> It; It; ++It)
	{
		UFunction *Func = *It;

		// Determine whether or not this is a level script event that we can call (must be defined in the level script actor and not in parent, and has no return value)
		const UClass* FuncOuter = Cast<UClass>(Func->GetOuter());
		const bool bIsLevelScriptFunction = FuncOuter 
			&& (FuncOuter->IsChildOf(ALevelScriptActor::StaticClass()))
			&& (FuncOuter != ALevelScriptActor::StaticClass())
			&& (Func->ReturnValueOffset == MAX_uint16) 
			&& (Func->GetSuperFunction() == NULL);

		// exec functions that either have no parent, level script events, or are in the global state (filtering some unnecessary dupes)
		if ( (Func->HasAnyFunctionFlags(FUNC_Exec) && (Func->GetSuperFunction() == NULL || FuncOuter))
			|| bIsLevelScriptFunction)
		{
			FString FuncName = Func->GetName();
			if(FDefaultValueHelper::HasWhitespaces(FuncName))
			{
				FuncName = FString::Printf(TEXT("\"%s\""), *FuncName);
			}
			if( bIsLevelScriptFunction )
			{
				FuncName = FString(TEXT("ce ")) + FuncName;
			}

			int32 NewIdx = AutoCompleteList.AddZeroed(1);
			AutoCompleteList[NewIdx].Command = FuncName;
			// build a help string
			// append each property (and it's type) to the help string
			for (TFieldIterator<UProperty> PropIt(Func); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				UProperty *Prop = *PropIt;
				FuncName = FString::Printf(TEXT("%s %s[%s]"),*FuncName,*Prop->GetName(),*Prop->GetCPPType());
			}
			AutoCompleteList[NewIdx].Desc = FuncName;
			ScriptExecCnt++;
		}
	}

	// enumerate maps
	TArray<FString> Packages;
	for (int32 PathIdx = 0; PathIdx < ConsoleSettings->AutoCompleteMapPaths.Num(); ++PathIdx)
	{
		FPackageName::FindPackagesInDirectory(Packages, FString::Printf(TEXT("%s%s"), *FPaths::GameDir(), *ConsoleSettings->AutoCompleteMapPaths[PathIdx]));
	}
	
	// also include maps in this user's developer dir
	FPackageName::FindPackagesInDirectory(Packages, FPaths::GameUserDeveloperDir());

	for (int32 PackageIndex = 0; PackageIndex < Packages.Num(); PackageIndex++)
	{
		FString Pkg = Packages[PackageIndex];
		int32 ExtIdx = Pkg.Find(*FPackageName::GetMapPackageExtension(),ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		
		FString MapName;
		if (ExtIdx != INDEX_NONE && Pkg.Split(TEXT("/"),NULL,&MapName,ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			// try to peel off the extension
			FString TrimmedMapName;
			if (!MapName.Split(TEXT("."),&TrimmedMapName,NULL,ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				TrimmedMapName = MapName;
			}
			int32 NewIdx;
			// put _P maps at the front so that they match early, since those are generally the maps we want to actually open
			if (TrimmedMapName.EndsWith(TEXT("_P")))
			{
				NewIdx = 0;
				AutoCompleteList.InsertZeroed(0,3);
			}
			else
			{
				NewIdx = AutoCompleteList.AddZeroed(3);
			}
			AutoCompleteList[NewIdx].Command = FString::Printf(TEXT("open %s"),*TrimmedMapName);
			AutoCompleteList[NewIdx].Desc = FString::Printf(TEXT("open %s"),*TrimmedMapName);
			AutoCompleteList[NewIdx+1].Command = FString::Printf(TEXT("travel %s"),*TrimmedMapName);
			AutoCompleteList[NewIdx+1].Desc = FString::Printf(TEXT("travel %s"),*TrimmedMapName);
			AutoCompleteList[NewIdx+2].Command = FString::Printf(TEXT("servertravel %s"),*TrimmedMapName);
			AutoCompleteList[NewIdx+2].Desc = FString::Printf(TEXT("servertravel %s"),*TrimmedMapName);
			//MapNames.AddItem(Pkg);
		}
	}
	// misc commands
	{
		int32 NewIdx = AutoCompleteList.AddZeroed(1);
		AutoCompleteList[NewIdx].Command = FString(TEXT("open 127.0.0.1"));
		AutoCompleteList[NewIdx].Desc = FString(TEXT("open 127.0.0.1 (opens connection to localhost)"));
	}

#if STATS
	// stat commands
	{
		const TSet<FName>& StatGroupNames = FStatGroupGameThreadNotifier::Get().StatGroupNames;

		int32 NewIdx = AutoCompleteList.AddZeroed(StatGroupNames.Num());
		for (const FName& StatGroupName : StatGroupNames)
		{
			FString Command = FString(TEXT("Stat "));
			Command += StatGroupName.ToString().RightChop(sizeof("STATGROUP_") - 1);

			AutoCompleteList[NewIdx].Command = Command;
			AutoCompleteList[NewIdx].Desc = FString();
			NewIdx++;
		}
	}
#endif

	// Add all showflag commands.
	{
		struct FIterSink
		{
			FIterSink(TArray<FAutoCompleteCommand>& InAutoCompleteList)
				: AutoCompleteList(InAutoCompleteList)
			{
			}
			
			bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
			{
				// Get localized name.
				FText LocName;
				FEngineShowFlags::FindShowFlagDisplayName(InName, LocName);
				
				int32 NewIdx = AutoCompleteList.AddZeroed(1);
				AutoCompleteList[NewIdx].Command = TEXT("show ") + InName;
				AutoCompleteList[NewIdx].Desc = FString::Printf(TEXT("show %s (toggles the %s showflag)"),*InName, *LocName.ToString());
				
				return true;
			}
			
			TArray<FAutoCompleteCommand>& AutoCompleteList;
		};
		
		FIterSink Sink(AutoCompleteList);
		FEngineShowFlags::IterateAllFlags(Sink);
	}

	// build the magic tree!
	for (int32 ListIdx = 0; ListIdx < AutoCompleteList.Num(); ListIdx++)
	{
		FString Command = AutoCompleteList[ListIdx].Command.ToLower();
		FAutoCompleteNode *Node = &AutoCompleteTree;
		for (int32 Depth = 0; Depth < Command.Len(); Depth++)
		{
			int32 Char = Command[Depth];
			int32 FoundNodeIdx = INDEX_NONE;
			TArray<FAutoCompleteNode*> &NodeList = Node->ChildNodes;
			for (int32 NodeIdx = 0; NodeIdx < NodeList.Num(); NodeIdx++)
			{
				if (NodeList[NodeIdx]->IndexChar == Char)
				{
					FoundNodeIdx = NodeIdx;
					Node = NodeList[FoundNodeIdx];
					NodeList[FoundNodeIdx]->AutoCompleteListIndices.Add(ListIdx);
					break;
				}
			}
			if (FoundNodeIdx == INDEX_NONE)
			{
				FAutoCompleteNode *NewNode = new FAutoCompleteNode(Char);
				NewNode->AutoCompleteListIndices.Add(ListIdx);
				Node->ChildNodes.Add(NewNode);
				Node = NewNode;
			}
		}
	}
	bIsRuntimeAutoCompleteUpToDate = true;
	//PrintNode(&AutoCompleteTree);
#endif
}

void UConsole::UpdateCompleteIndices()
{
	if (!bIsRuntimeAutoCompleteUpToDate)
	{
		BuildRuntimeAutoCompleteList(true);
	}
	AutoCompleteIndex = 0;
	AutoCompleteCursor = -1;
	AutoComplete.Empty();
	FAutoCompleteNode *Node = &AutoCompleteTree;
	FString LowerTypedStr = TypedStr.ToLower();
	for (int32 Idx = 0; Idx < TypedStr.Len(); Idx++)
	{
		int32 Char = LowerTypedStr[Idx];
		bool bFoundMatch = false;
		int32 BranchCnt = 0;
		for (int32 CharIdx = 0; CharIdx < Node->ChildNodes.Num(); CharIdx++)
		{
			BranchCnt += Node->ChildNodes[CharIdx]->ChildNodes.Num();
			if (Node->ChildNodes[CharIdx]->IndexChar == Char)
			{
				bFoundMatch = true;
				Node = Node->ChildNodes[CharIdx];
				break;
			}
		}
		if (!bFoundMatch)
		{
			if (!bAutoCompleteLocked && BranchCnt > 0)
			{
				// we're off the grid!
				return;
			}
			else
			{
				if(Idx < TypedStr.Len())
				{
					// there is more text behind the auto completed text, we don't need auto completion
					return;
				}
				else
				{
					break;
				}
			}
		}
	}
	if (Node != &AutoCompleteTree)
	{
		const TArray<int32>& Leaf = Node->AutoCompleteListIndices;

		for(uint32 i = 0, Num = (uint32)Leaf.Num(); i < Num; ++i)
		{
			AutoComplete.Add(AutoCompleteList[Leaf[i]]);
		}
		AutoComplete.Sort();
	}
}

void UConsole::SetAutoCompleteFromHistory()
{
	AutoCompleteIndex = 0;
	AutoCompleteCursor = 0;
	AutoComplete.Empty();

	for(int32 i = HistoryBuffer.Num() - 1; i >= 0 ; --i)
	{
		FAutoCompleteCommand Cmd;

		Cmd.Command = HistoryBuffer[i]; 
		Cmd.Desc = FString::Printf(TEXT("> %s"), *Cmd.Command);

		AutoComplete.Add(Cmd);
	}
}

void UConsole::SetInputText( const FString& Text )
{
	TypedStr = Text;
}

void UConsole::SetCursorPos( int32 Position )
{
	TypedStrPos = Position;
}

void UConsole::ConsoleCommand(const FString& Command)
{
	// insert into history buffer
	{
		HistoryBuffer.Add(Command);

		NormalizeHistoryBuffer();
	}

	// Save the command history to the INI.
	SaveConfig();

	OutputText(FString::Printf(TEXT("\n>>> %s <<<"), *Command));

	UWorld *World = GetOuterUGameViewportClient()->GetWorld();
	if(ConsoleTargetPlayer != NULL)
	{
		// If there is a console target player, execute the command in the player's context.
		ConsoleTargetPlayer->PlayerController->ConsoleCommand(Command);
	}
	else if(World && World->GetPlayerControllerIterator())
	{
		// If there are any players, execute the command in the first player's context that has a non-null Player.
		for (auto PCIter = World->GetPlayerControllerIterator(); PCIter; ++PCIter)
		{
			APlayerController* PC = *PCIter;
			if (PC && PC->Player)
			{
				PC->ConsoleCommand(Command);
				break;
			}
		}
	}
	else
	{
		// Otherwise, execute the command in the context of the viewport.
		GetOuterUGameViewportClient()->ConsoleCommand(Command);
	}
}


void UConsole::ClearOutput()
{
	SBHead = 0;
	Scrollback.Empty();
}


void UConsole::OutputTextLine(const FString& Text)
{
	const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();

	// If we are full, delete the first line
	if (Scrollback.Num() > ConsoleSettings->MaxScrollbackSize)
	{
		Scrollback.RemoveAt(0, 1);
		SBHead = ConsoleSettings->MaxScrollbackSize - 1;
	}
	else
	{
		SBHead++;
	}

	// Add the line
	Scrollback.Add(Text);
}


void UConsole::OutputText(const FString& Text)
{
	FString RemainingText = Text;
	int32 StringLength = Text.Len();
	while(StringLength > 0)
	{
		// Find the number of characters in the next line of text.
		int32 LineLength = RemainingText.Find(TEXT("\n"), ESearchCase::CaseSensitive);
		if(LineLength == -1)
		{
			// There aren't any more newlines in the string, assume there's a newline at the end of the string.
			LineLength = StringLength;
		}

		// Output the line to the console.
		OutputTextLine(RemainingText.Left(LineLength));

		// Remove the line from the string.
		RemainingText = (RemainingText).Mid(LineLength + 1, MAX_int32);
		StringLength -= LineLength + 1;
	};
}


void UConsole::StartTyping(const FString& Text)
{
	static const FName NAME_Typing = FName(TEXT("Typing"));
	FakeGotoState(NAME_Typing);
	SetInputText(Text);
	SetCursorPos(Text.Len());
}


void UConsole::FlushPlayerInput()
{
	APlayerController* PC = nullptr;
	if (ConsoleTargetPlayer)
	{
		PC = ConsoleTargetPlayer->PlayerController;
	}
	else
	{
		UWorld* World = GetOuterUGameViewportClient()->GetWorld();
		if (ULocalPlayer* LocalPlayer = GEngine->GetFirstGamePlayer(World))
		{
			PC = LocalPlayer->PlayerController;
		}
	}

	if (PC && PC->PlayerInput)
	{
		PC->PlayerInput->FlushPressedKeys();
	}
}

bool UConsole::ProcessControlKey(FKey Key, EInputEvent Event)
{	
	if (Key == EKeys::LeftControl || Key == EKeys::RightControl)
	{
		if (Event == IE_Released)
		{
			bCtrl = false;
		}
		else if (Event == IE_Pressed)
		{
			bCtrl = true;
		}

		return true;
	}
	else if (bCtrl && Event == IE_Pressed)
	{
		if (Key == EKeys::V)
		{
			// paste
			FString ClipboardContent;
			FPlatformMisc::ClipboardPaste(ClipboardContent);
			AppendInputText(ClipboardContent);
			return true;
		}
		else if (Key == EKeys::C)
		{
			// copy
			FPlatformMisc::ClipboardCopy(*TypedStr);
			return true;
		}
		else if (Key == EKeys::X)
		{
			// cut
			if (!TypedStr.IsEmpty())
			{
				FPlatformMisc::ClipboardCopy(*TypedStr);
				SetInputText(TEXT(""));
				SetCursorPos(0);
			}
			return true;
		}
	}

	return false;
}


void UConsole::AppendInputText(const FString& Text)
{
	FString TextMod = Text;
	while (TextMod.Len() > 0)
	{
		int32 Character = **TextMod.Left(1);
		TextMod = TextMod.Mid(1);

		if (Character >= 0x20 && Character < 0x100)
		{
			TCHAR Temp[2];
			Temp[0] = Character;
			Temp[1] = 0;
			SetInputText(FString::Printf(TEXT("%s%s%s"), *TypedStr.Left(TypedStrPos), Temp, *TypedStr.Right(TypedStr.Len() - TypedStrPos)));
			SetCursorPos(TypedStrPos + 1);
		}
	};
	UpdateCompleteIndices();
}

bool UConsole::InputChar_Typing( int32 ControllerId, const FString& Unicode )
{
	if ( bCaptureKeyInput )
	{
		return true;
	}

	AppendInputText(Unicode);

	return true;
}

bool UConsole::InputKey_InputLine( int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	//`log(`location@`showvar(Key));

	if ( Event == IE_Pressed )
	{
		bCaptureKeyInput = false;
	}

	// cycle between console states
	bool bModifierDown = bCtrl;
	FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
	bModifierDown |= KeyState.IsAltDown() || KeyState.IsCommandDown() || KeyState.IsShiftDown() || KeyState.IsControlDown();
	if ( GetDefault<UInputSettings>()->ConsoleKeys.Contains(Key) && Event == IE_Pressed && !bModifierDown )
	{
		if (ConsoleState == NAME_Typing)
		{
			FakeGotoState(NAME_Open);
			bCaptureKeyInput = true;
		}
		else if (ConsoleState == NAME_Open)
		{
			FakeGotoState(NAME_None);
			bCaptureKeyInput = true;
		}
		else if (ConsoleState == NAME_None)
		{
			FakeGotoState(NAME_Typing);
			bCaptureKeyInput = true;
		}
		return true;
	}

	// if user input is open
	if(ConsoleState != NAME_None)
	{
		if (ProcessControlKey(Key, Event))
		{
			return true;
		}
		else if( bGamepad )
		{
			return false;
		}
		else if( Key == EKeys::Escape && Event == IE_Released )
		{
			if( !TypedStr.IsEmpty() )
			{
				SetInputText("");
				SetCursorPos(0);

				AutoCompleteIndex = 0;
				AutoCompleteCursor = -1;
				AutoComplete.Empty();
				bAutoCompleteLocked = false;

				return true;
			}
			else
			{
				FakeGotoState( NAME_None );
			}

			return true;
		}
		else if (Key == EKeys::Tab && Event == IE_Pressed)
		{
			if (!bAutoCompleteLocked)
			{
				SetInputLineFromAutoComplete();
			}

			return true;
		}
		else if( Key == EKeys::Enter && Event == IE_Released )
		{
			if( !TypedStr.IsEmpty() )
			{
				// Make a local copy of the string.
				FString Temp=TypedStr;

				SetInputText(TEXT(""));
				SetCursorPos(0);

				ConsoleCommand(Temp);

				//OutputText( Localize("Errors","Exec","Core") );

				OutputText( TEXT("") );

				if(ConsoleState == NAME_Typing)
				{
					// close after each command when in typing mode (single line)
					FakeGotoState(NAME_None);
				}

				UpdateCompleteIndices();
			}
			else
			{
				FakeGotoState(NAME_None);
			}

			return true;
		}
		else if( Event != IE_Pressed && Event != IE_Repeat )
		{
			if( !bGamepad )
			{
				return	Key != EKeys::LeftMouseButton
					&&	Key != EKeys::MiddleMouseButton
					&&	Key != EKeys::RightMouseButton;
			}
			return false;
		}
		else if( Key == EKeys::Up )
		{
			if (!bCtrl)
			{
				if(AutoComplete.Num())
				{
					if(AutoCompleteCursor + 1 < FMath::Min((int32)MAX_AUTOCOMPLETION_LINES, AutoComplete.Num()))
					{
						// move cursor within displayed region
						++AutoCompleteCursor;
					}
					else 
					{
						// can be negative
						int32 ScrollRegionSize = AutoComplete.Num() - (int32)MAX_AUTOCOMPLETION_LINES;

						// can we scroll?
						if(AutoCompleteIndex + 1 < ScrollRegionSize)
						{
							++AutoCompleteIndex;
						}
					}
					SetInputLineFromAutoComplete();
				}
				else
				{
					SetAutoCompleteFromHistory();
					SetInputLineFromAutoComplete();
				}
			}
			return true;
		}
		else if( Key == EKeys::Down )
		{
			if (!bCtrl && AutoComplete.Num() > 1)
			{
				if(AutoCompleteCursor > 0)
				{
					// move cursor within displayed region
					--AutoCompleteCursor;
				}
				else
				{
					// can we scroll?
					if(AutoCompleteIndex > 0)
					{
						--AutoCompleteIndex;
					}
					bAutoCompleteLocked = false;
				}
				SetInputLineFromAutoComplete();
			}
		}
		else if( Key==EKeys::BackSpace )
		{
			if( TypedStrPos>0 )
			{
				SetInputText(FString::Printf(TEXT("%s%s"), *TypedStr.Left(TypedStrPos-1), *TypedStr.Right( TypedStr.Len() - TypedStrPos)));
				SetCursorPos(TypedStrPos-1);
				// unlock auto-complete (@todo - track the lock position so we don't bother unlocking under bogus cases)
				bAutoCompleteLocked = false;
			}

			return true;
		}
		else if ( Key== EKeys::Delete )
		{
			if ( TypedStrPos < TypedStr.Len() )
			{
				SetInputText(FString::Printf(TEXT("%s%s"), *TypedStr.Left(TypedStrPos), *TypedStr.Right( TypedStr.Len() - TypedStrPos - 1)));
			}
			return true;
		}
		else if ( Key==EKeys::Left )
		{
			if (bCtrl)
			{
				// find the nearest '.' or ' '
				int32 NewPos = FMath::Max(TypedStr.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd, TypedStrPos),TypedStr.Find(TEXT(" "),ESearchCase::CaseSensitive, ESearchDir::FromEnd,TypedStrPos));
				SetCursorPos(FMath::Max(0,NewPos));
			}
			else
			{
				SetCursorPos(FMath::Max(0, TypedStrPos - 1));
			}
			return true;
		}
		else if ( Key==EKeys::Right )
		{
			if (bCtrl)
			{
				// find the nearest '.' or ' '
				int32 SpacePos = TypedStr.Find(TEXT(" "));
				int32 PeriodPos = TypedStr.Find(TEXT("."));
				// pick the closest valid index
				int32 NewPos = SpacePos < 0 ? PeriodPos : (PeriodPos < 0 ? SpacePos : FMath::Min(SpacePos,PeriodPos));
				// jump to end if nothing in between
				if (NewPos == INDEX_NONE)
				{
					NewPos = TypedStr.Len();
				}
				SetCursorPos(FMath::Min(TypedStr.Len(),FMath::Max(TypedStrPos,NewPos)));
			}
			else
			{
				SetCursorPos(FMath::Min(TypedStr.Len(), TypedStrPos + 1));
			}
			return true;
		}
		else if ( Key==EKeys::Home )
		{
			SetCursorPos(0);
			return true;
		}
		else if ( Key==EKeys::End )
		{
			SetCursorPos(TypedStr.Len());
			return true;
		}
		else
		{
			// auto complete is open and the user adds a key
			if (AutoComplete.Num() > 0 && !bAutoCompleteLocked && AutoCompleteCursor >= 0)
			{
				const FAutoCompleteCommand& Cmd = AutoComplete[AutoCompleteIndex + AutoCompleteCursor];

				TypedStr = Cmd.Command;
				SetCursorPos(TypedStr.Len());
				bAutoCompleteLocked = true;

				// no "return true" so that the following key will be added the auto completed string
			}
		}
	}

	return false;
}


namespace ConsoleDefs
{
	/** Colors */
	static const FColor BorderColor( 140, 140, 140 );
	static const FColor CursorColor( 255, 255, 255 );
	static const FColor InputTextColor( 220, 220, 220 );
	static const FColor AutocompleteBackgroundColor( 0, 0, 0 );
	static const FColor AutocompletePartialSuggestionColor( 120, 120, 120 );
	static const FColor AutocompleteSuggestionColor( 180, 180, 180 );

	/** Text that appears before the user's typed input string that acts as a visual cue for the editable area */
	static const FString LeadingInputText( TEXT( " > " ) );
}

void UConsole::SetInputLineFromAutoComplete()
{
	if (AutoComplete.Num() > 0)
	{
		const int32 Index = AutoCompleteIndex + (AutoCompleteCursor >= 0 ? AutoCompleteCursor : 0);
		const FAutoCompleteCommand& Cmd = AutoComplete[Index];

		TypedStr = Cmd.Command;
		SetCursorPos(TypedStr.Len());
		bAutoCompleteLocked = true;
	}
}

void UConsole::NormalizeHistoryBuffer()
{
	const uint32 Count = MAX_HISTORY_ENTRIES;

	check(Count > 0);

	if ((uint32)HistoryBuffer.Num() > Count)
	{
		uint32 ShrinkCount = (uint32)(HistoryBuffer.Num() - Count);
		HistoryBuffer.RemoveAt(0, ShrinkCount);
	}
}

void UConsole::PostRender_Console_Typing(UCanvas* Canvas)
{
	float xl, yl;

	// Blank out a space

	// use the smallest font
	UFont* Font = GEngine->GetSmallFont();
	// determine the position for the cursor
	FString OutStr = FString::Printf(TEXT("%s%s"), *ConsoleDefs::LeadingInputText, *TypedStr);
	Canvas->StrLen(Font, OutStr,xl,yl);

	float ClipX = Canvas->ClipX;
	float ClipY = Canvas->ClipY;
	float LeftPos = 0;

	if (GEngine->IsConsoleBuild())
	{
		ClipX	-= 64;
		ClipY	-= 32;
		LeftPos	 = 32;
	}

	if (GEngine->IsStereoscopic3D())
	{
		LeftPos = ClipX / 3;
		ClipX -= LeftPos;
		ClipY = ClipY * 0.60;
	}

	PostRender_InputLine(Canvas, FIntPoint(LeftPos, ClipY));
}

void UConsole::BeginState_Typing(FName PreviousStateName)
{
	if ( PreviousStateName == NAME_None )
	{
		FlushPlayerInput();
	}
	bCaptureKeyInput = true;
//	HistoryCur = HistoryTop;
}

void UConsole::EndState_Typing( FName NextStateName )
{
	bAutoCompleteLocked = false;
}


bool UConsole::InputChar_Open( int32 ControllerId, const FString& Unicode )
{
	if ( bCaptureKeyInput )
	{
		return true;
	}

	AppendInputText(Unicode);

	return true;
}


bool UConsole::InputKey_Open( int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	if ( Key==EKeys::PageUp || Key == EKeys::MouseScrollUp )
	{
		if (SBPos<Scrollback.Num()-1)
		{
			if (bCtrl)
				SBPos+=5;
			else
				SBPos++;

			if (SBPos>=Scrollback.Num())
				SBPos = Scrollback.Num()-1;
		}

		return true;
	}
	else if ( Key==EKeys::PageDown || Key==EKeys::MouseScrollDown )
	{
		if (SBPos>0)
		{
			if (bCtrl)
				SBPos-=5;
			else
				SBPos--;

			if (SBPos<0)
				SBPos = 0;
		}

		return true;
	}

	return false;
}

void UConsole::PostRender_Console_Open(UCanvas* Canvas)
{
	// the height of the buffer will be 75% of the height of the screen
	float Height = FMath::FloorToFloat(Canvas->ClipY * 0.75f);

	// shrink for TVs
	float ClipX = Canvas->ClipX;
	float TopPos = 0;
	float LeftPos = 0;

	if (GEngine->IsConsoleBuild())
	{
		ClipX	-= 80;
		TopPos	 = 30;
		LeftPos	 = 40;
	}
	if (GEngine->IsStereoscopic3D())
	{
		LeftPos = ClipX / 3;
		ClipX -= LeftPos;
		Height = Canvas->ClipY * 0.60;
	}

	UFont* Font = GEngine->GetSmallFont();

	// determine the height of the text
	float xl, yl;
	Canvas->StrLen(Font, TEXT("M"),xl,yl);

	// Background
	FCanvasTileItem ConsoleTile( FVector2D( LeftPos, 0.0f ), DefaultTexture_Black->Resource, FVector2D( ClipX, Height + TopPos - yl), FVector2D( 0.0f, 0.0f), FVector2D( 1.0f, 1.0f ), FLinearColor::White );
	Canvas->DrawItem( ConsoleTile );

	// figure out which element of the scrollback buffer to should appear first (at the top of the screen)
	int32 idx = SBHead - SBPos;
	
	float y = Height - yl;

	if(Scrollback.Num())
	{
		FCanvasTextItem ConsoleText( FVector2D( LeftPos,TopPos+Height-5-yl ), FText::FromString(TEXT("")), GEngine->GetSmallFont(), ConsoleDefs::InputTextColor );		

		// change the text color to white
		ConsoleText.SetColor( FLinearColor::White );

		// while we have enough room to draw another line and there are more lines to draw
		while (y > -yl && idx>=0)
		{
			float PenX;
			float PenY;
			float PenZ = 0.1f;
			PenX = LeftPos;
			PenY = TopPos+y;

			// adjust the location for any word wrapping due to long text lines
			if (idx < Scrollback.Num())
			{
				float ScrollLineXL, ScrollLineYL;
				Canvas->StrLen(Font, Scrollback[idx], ScrollLineXL, ScrollLineYL);
				if (ScrollLineYL > yl)
				{
					y -= (ScrollLineYL - yl);
					PenX = LeftPos;
					PenY = TopPos+y;				
				}

				ConsoleText.Text = FText::FromString( Scrollback[idx] );
				Canvas->DrawItem( ConsoleText, PenX, PenY );
			}
			idx--;
			y-=yl;
		}
	}

	PostRender_InputLine(Canvas, FIntPoint(LeftPos, TopPos + Height + 6));
}

void UConsole::BeginState_Open(FName PreviousStateName)
{
	bCaptureKeyInput = true;
//	HistoryCur = HistoryTop;

	SBPos = 0;
	bCtrl = false;

	if ( PreviousStateName == NAME_None )
	{
		FlushPlayerInput();
	}
}


bool UConsole::InputChar( int32 ControllerId, const FString& Unicode )
{
	if (ConsoleState == NAME_Typing)
	{
		return InputChar_Typing(ControllerId,Unicode);
	}
	if (ConsoleState == NAME_Open)
	{
		return InputChar_Open(ControllerId,Unicode);
	}
	return bCaptureKeyInput;
}

bool UConsole::InputKey( int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	bool bWasConsumed = InputKey_InputLine(ControllerId,Key,Event,AmountDepressed,bGamepad);

	if(!bWasConsumed)
	{
		if (ConsoleState == NAME_Typing)
		{
			// if the console is open we don't want any other one to consume the input
			return true;
		}
		if (ConsoleState == NAME_Open)
		{
			bWasConsumed = InputKey_Open(ControllerId,Key,Event,AmountDepressed,bGamepad);
			// if the console is open we don't want any other one to consume the input
			return true;
		}
	}

	return bWasConsumed;
}


void UConsole::PostRender_Console(UCanvas* Canvas)
{
	if (ConsoleState == NAME_Typing)
	{
		PostRender_Console_Typing(Canvas);
	}
	else if (ConsoleState == NAME_Open)
	{
		PostRender_Console_Open(Canvas);
	}
}

void UConsole::PostRender_InputLine(UCanvas* Canvas, FIntPoint UserInputLinePos)
{
	float xl, yl;

	// use the smallest font
	UFont* Font = GEngine->GetSmallFont();
	// determine the position for the cursor
	FString OutStr = FString::Printf(TEXT("%s%s"), *ConsoleDefs::LeadingInputText, *TypedStr);
	Canvas->StrLen(Font, OutStr,xl,yl);

	float ClipX = Canvas->ClipX;
	float ClipY = Canvas->ClipY;

	if (GEngine->IsConsoleBuild())
	{
		ClipX	-= 64;
		ClipY	-= 32;
	}

	if (GEngine->IsStereoscopic3D())
	{
		ClipX = Canvas->ClipX - UserInputLinePos.X;
		ClipY = ClipY * 0.60;
	}

	// Background
	FCanvasTileItem ConsoleTile( FVector2D( UserInputLinePos.X,UserInputLinePos.Y-6-yl ), DefaultTexture_Black->Resource, FVector2D( ClipX, yl+6 ), FVector2D( 0.0f, 0.0f), FVector2D( 1.0f, 1.0f ), FLinearColor::White );
	Canvas->DrawItem( ConsoleTile );

	// Separator line
	ConsoleTile.SetColor( ConsoleDefs::BorderColor );
	ConsoleTile.Texture = DefaultTexture_White->Resource;
	ConsoleTile.Size = FVector2D( ClipX, 2.0f );
	Canvas->DrawItem( ConsoleTile );

	// Currently typed string
	FText Str = FText::FromString( OutStr );
	FCanvasTextItem ConsoleText( FVector2D( UserInputLinePos.X,UserInputLinePos.Y-3-yl ), Str , GEngine->GetSmallFont(), ConsoleDefs::InputTextColor );		
	Canvas->DrawItem( ConsoleText );

	// draw the remaining text for matching auto-complete
	if (AutoComplete.Num() > 0)
	{
		int32 StartIdx = AutoCompleteIndex;
		if (StartIdx < 0)
		{
			StartIdx = FMath::Max(0, AutoComplete.Num() + StartIdx);
		}

		Canvas->StrLen(Font, *ConsoleDefs::LeadingInputText, xl, yl);
		
		float y = UserInputLinePos.Y - 6.0f - (yl * 2.0f);

		// Set the background color/texture of the auto-complete section
		ConsoleTile.SetColor( ConsoleDefs::AutocompleteBackgroundColor );
		ConsoleTile.Texture = DefaultTexture_White->Resource;

		// wasteful memory allocations but when typing in a console command this is fine
		TArray<FString> AutoCompleteElements;
		// to avoid memory many allocations
		AutoCompleteElements.Empty(MAX_AUTOCOMPLETION_LINES + 1);

		for (int32 MatchIdx = 0; MatchIdx < MAX_AUTOCOMPLETION_LINES && MatchIdx < AutoComplete.Num(); MatchIdx++)
		{
			const FAutoCompleteCommand &Cmd = AutoComplete[StartIdx + MatchIdx];
			OutStr = Cmd.Desc;

			if(OutStr.IsEmpty())
			{
				// no Description means we display the Command directly, without that the line would be empty (happens for ConsoleVariables and some ConsoleSettings->ManualAutoCompleteList)
				OutStr = Cmd.Command;
			}

			AutoCompleteElements.Add(OutStr);
		}

		// Display a message if there were more matches
		if (AutoComplete.Num() >= MAX_AUTOCOMPLETION_LINES)
		{
			OutStr = FString::Printf(TEXT("[%i more matches]"), (AutoComplete.Num() - MAX_AUTOCOMPLETION_LINES + 1));
			AutoCompleteElements.Add(OutStr);
		}

		// background rectangle behind auto completion
		{
			float MaxWidth = 0;
			float MaxHeight = 0;

			for(int32 i = 0, Num = AutoCompleteElements.Num(); i < Num; ++i)
			{
				const FString& AutoCompleteElement = AutoCompleteElements[i];

				float info_xl, info_yl;

				Canvas->StrLen(Font, AutoCompleteElement, info_xl, info_yl);

				MaxWidth = FMath::Max(MaxWidth, info_xl);
				MaxHeight += yl;
			}

			int32 Border = 4;

			// white border
			ConsoleTile.Size = FVector2D( MaxWidth + 2 * Border, MaxHeight + 2 * Border );
			ConsoleTile.SetColor( ConsoleDefs::BorderColor );
			Canvas->DrawItem( ConsoleTile, UserInputLinePos.X + xl - Border, y + yl - MaxHeight - Border );

			--Border;

			// dark inner part
			ConsoleTile.Size = FVector2D( MaxWidth + 2 * Border, MaxHeight + 2 * Border );
			ConsoleTile.SetColor( ConsoleDefs::AutocompleteBackgroundColor );
			Canvas->DrawItem( ConsoleTile, UserInputLinePos.X + xl - Border, y + yl - MaxHeight - Border );
		}

		// auto completion elements
		for(int32 i = 0, Num = AutoCompleteElements.Num(); i < Num; ++i)
		{
			const FString& AutoCompleteElement = AutoCompleteElements[i];

			if (i == AutoCompleteCursor									// cursor line is highlighted
				|| (Num >= MAX_AUTOCOMPLETION_LINES && i == Num - 1))	// e.g. [%i more matches]
			{
				ConsoleText.SetColor( ConsoleDefs::AutocompleteSuggestionColor );
			}
			else
			{
				ConsoleText.SetColor( ConsoleDefs::AutocompletePartialSuggestionColor );
			}
			ConsoleText.Text = FText::FromString(AutoCompleteElement);
			Canvas->DrawItem( ConsoleText, UserInputLinePos.X + xl, y );
			y -= yl;
		}
	}

	// determine the cursor position
	OutStr = FString::Printf(TEXT("%s%s"), *ConsoleDefs::LeadingInputText, *TypedStr.Left(TypedStrPos));
	Canvas->StrLen(Font, OutStr,xl,yl);

	// draw the cursor
	ConsoleText.SetColor( ConsoleDefs::CursorColor );
	OutStr = FString::Printf( TEXT("_") );
	ConsoleText.Text = FText::FromString( OutStr );
	Canvas->DrawItem( ConsoleText, UserInputLinePos.X + xl, UserInputLinePos.Y-1.0f-yl );

}

bool UConsole::ConsoleActive() const
{
	return (ConsoleState != NAME_None);
}

void UConsole::FakeGotoState(FName NextStateName)
{
	if (ConsoleState == NAME_Typing)
	{
		EndState_Typing(NextStateName);
	}
	if (NextStateName == NAME_Typing)
	{
		BeginState_Typing(ConsoleState);
		FSlateApplication::Get().ResetToDefaultPointerInputSettings();
	}
	else if (NextStateName == NAME_Open)
	{
		BeginState_Open(ConsoleState);
		FSlateApplication::Get().ResetToDefaultPointerInputSettings();
	}
	else if( NextStateName == NAME_None )
	{
		// We need to force the console state name change now otherwise inside the call 
		// to SetKeyboardFocus the console is still considered active
		ConsoleState = NAME_None;

		// Since the viewport may not be the current focus, we need to re-focus whatever the current focus is,
		// in order to ensure it gets a chance to reapply any custom input settings
		auto CurrentFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (CurrentFocus.IsValid())
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
			FSlateApplication::Get().SetKeyboardFocus(CurrentFocus);
		}
	}

	ConsoleState = NextStateName;
}

void UConsole::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// e.g. UE_LOG(LogConsoleResponse, Display, TEXT("Test"));
	static const FName LogConsoleResponse = FName("LogConsoleResponse");

	if (Category == LogConsoleResponse)
	{
		// log all LogConsoleResponse
		OutputText(V);
	}
	else
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("con.MinLogVerbosity"));

		if(CVar)
		{
			int MinVerbosity = CVar->GetValueOnAnyThread();

			if((int)Verbosity <= MinVerbosity)
			{
				// log all that is >= the specified verbosity
				OutputText(V);
			}
		}
	}
}
