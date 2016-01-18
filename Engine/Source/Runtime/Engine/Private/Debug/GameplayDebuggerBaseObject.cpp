// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CoreMisc.h"
#include "Regex.h"
#include "Debug/GameplayDebuggerBaseObject.h"
#include "AssetRegistryModule.h"

DEFINE_LOG_CATEGORY(LogGameplayDebugger);

UGameplayDebuggerBaseObject::UGameplayDebuggerBaseObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UGameplayDebuggerBaseObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
#if ENABLED_GAMEPLAY_DEBUGGER
	DOREPLIFETIME(UGameplayDebuggerBaseObject, GenericShapeElements);
	DOREPLIFETIME(UGameplayDebuggerBaseObject, ReplicatedBlob);
#endif
}

int32 UGameplayDebuggerBaseObject::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}
	check(GetOuter() != NULL);
	return GetOuter()->GetFunctionCallspace(Function, Parameters, Stack);
#endif //ENABLED_GAMEPLAY_DEBUGGER

	return FunctionCallspace::Local;
}

bool UGameplayDebuggerBaseObject::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	check(!HasAnyFlags(RF_ClassDefaultObject));
	check(GetOuter() != NULL);

	AActor* Owner = CastChecked<AActor>(GetOuter());
	UNetDriver* NetDriver = Owner->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parameters, OutParms, Stack, this);
		return true;
	}
#endif
	return false;
}

void UGameplayDebuggerBaseObject::OnRep_GenericShapeElements()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	MarkRenderStateDirty();
#endif
}

void UGameplayDebuggerBaseObject::OnRep_ReplicatedBlob()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (LocalReplicationData.VersionNum != ReplicatedBlob.VersionNum || LocalReplicationData.RepDataOffset != ReplicatedBlob.RepDataOffset)
	{
		LocalReplicationData.VersionNum = ReplicatedBlob.VersionNum;
		LocalReplicationData.RepDataOffset = ReplicatedBlob.RepDataOffset;
		LocalReplicationData.RepDataSize = ReplicatedBlob.RepDataSize;
		LocalReplicationData.RepData.Reset();
		if (ReplicatedBlob.RepDataOffset != 0)
		{
			ServerRequestPacket(0);
			return;
		}
	}

	const int32 StartIdx = LocalReplicationData.RepData.Num();
	LocalReplicationData.RepData.Append(ReplicatedBlob.RepData);
	LocalReplicationData.RepDataOffset += ReplicatedBlob.RepData.Num();

	const int32 HeaderSize = sizeof(FReplicationDataHeader);
	if (LocalReplicationData.RepDataSize == LocalReplicationData.RepData.Num() && LocalReplicationData.RepData.Num() > HeaderSize)
	{
		TArray<uint8> UncompressedBuffer;
		FReplicationDataHeader DataHeader;

		uint8* SourceBuffer = (uint8*)LocalReplicationData.RepData.GetData();
		FMemory::Memcpy(&DataHeader, SourceBuffer, sizeof(FReplicationDataHeader));
		SourceBuffer += sizeof(FReplicationDataHeader);

		UncompressedBuffer.AddZeroed(DataHeader.UncompressedSize);
		if (DataHeader.bUseCompression)
		{
			const int32 CompressedSize = LocalReplicationData.RepData.Num() - HeaderSize;
			FCompression::UncompressMemory((ECompressionFlags)(COMPRESS_ZLIB),
				(void*)UncompressedBuffer.GetData(), DataHeader.UncompressedSize, SourceBuffer, CompressedSize);
		}
		else
		{
			FMemory::Memcpy(UncompressedBuffer.GetData(), SourceBuffer, DataHeader.UncompressedSize);
		}
		LocalReplicationData.RepData = UncompressedBuffer;
		LocalReplicationData.RepDataSize = UncompressedBuffer.Num();

		OnDataReplicationComplited(LocalReplicationData.RepData);
	}
	else
	{
		ServerRequestPacket(LocalReplicationData.RepDataOffset);
	}

#endif // ENABLED_GAMEPLAY_DEBUGGER
}

bool UGameplayDebuggerBaseObject::CollectDataToReplicateOnServer_Validate(APlayerController* MyPC, AActor *SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#else
	return false;
#endif
}

void UGameplayDebuggerBaseObject::CollectDataToReplicateOnServer_Implementation(APlayerController* MyPC, AActor *SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	CollectDataToReplicate(MyPC, SelectedActor);
#endif
}

bool UGameplayDebuggerBaseObject::ServerRequestPacket_Validate(int32 Offset)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#else
	return false;
#endif
}

void UGameplayDebuggerBaseObject::ServerRequestPacket_Implementation(int32 Offset)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (LocalReplicationData.RepData.IsValidIndex(Offset) == false)
	{
		return;
	}
	LocalReplicationData.RepDataOffset = Offset;

	int32 MaxPacketSizeAsFunctionArgument = MAX_PACKET_SIZE;
	int32 NumToSend = LocalReplicationData.RepData.Num() - LocalReplicationData.RepDataOffset;
	int32 PacketSize = FMath::Min(MaxPacketSizeAsFunctionArgument, NumToSend);

	TArray<uint8> PacketData;
	PacketData.AddZeroed(PacketSize);
	const uint8*SrcBuffer = LocalReplicationData.RepData.GetData() + LocalReplicationData.RepDataOffset;
	uint8 *DestBuffer = PacketData.GetData();
	FMemory::Memcpy(DestBuffer, SrcBuffer, PacketSize);

	ReplicatedBlob = FGameplayDebuggerReplicatedBlob(PacketData, LocalReplicationData.VersionNum, LocalReplicationData.RepDataOffset, LocalReplicationData.RepDataSize);
#endif
}

void UGameplayDebuggerBaseObject::RequestDataReplication(const FString& DataName, const TArray<uint8>& UncompressedBuffer, EGameplayDebuggerReplicate Replicate)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	LocalReplicationData.VersionNum++;
	LocalReplicationData.RepDataOffset = 0;
	ENetMode NetMode = GetWorld()->GetNetMode();
	if (NetMode != NM_Standalone)
	{
		const double Timer2 = FPlatformTime::Seconds();

		FReplicationDataHeader DataHeader;
		DataHeader.bUseCompression = Replicate == EGameplayDebuggerReplicate::WithCompressed;
		DataHeader.UncompressedSize = UncompressedBuffer.Num();

		const int32 HeaderSize = sizeof(FReplicationDataHeader);
		LocalReplicationData.RepData.Init(0, HeaderSize + FMath::TruncToInt(1.1f * UncompressedBuffer.Num()));

		const int32 UncompressedSize = UncompressedBuffer.Num();
		int32 CompressedSize = LocalReplicationData.RepData.Num() - HeaderSize;
		uint8* DestBuffer = LocalReplicationData.RepData.GetData();
		FMemory::Memcpy(DestBuffer, &DataHeader, HeaderSize);
		DestBuffer += HeaderSize;

		if (Replicate == EGameplayDebuggerReplicate::WithCompressed)
		{
			FCompression::CompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				(void*)DestBuffer, CompressedSize, (void*)UncompressedBuffer.GetData(), UncompressedSize);

			LocalReplicationData.RepData.SetNum(CompressedSize + HeaderSize, false);
		}
		else
		{
			FMemory::Memcpy(DestBuffer, (void*)UncompressedBuffer.GetData(), UncompressedSize);
			LocalReplicationData.RepData.SetNum(UncompressedSize + HeaderSize, false);
		}

		LocalReplicationData.RepDataSize = LocalReplicationData.RepData.Num();
		ServerRequestPacket(/*int32 Offset*/0);

#if UE_BUILD_DEBUG
		const double Timer3 = FPlatformTime::Seconds();
		UE_LOG(LogGameplayDebugger, Verbose, TEXT("Preparing '%s' data to replicate: %.1fkB took %.3fms (collect: %.3fms + %s compress %d%%: %.3fms)"),
			*DataName,
			LocalReplicationData.RepData.Num() / 1024.0f, 
			1000.0f * (Timer3 - DataPreparationStartTime),
			1000.0f * (Timer2 - DataPreparationStartTime),
			Replicate == EGameplayDebuggerReplicate::WithCompressed ? TEXT("") : TEXT("no"),
			FMath::TruncToInt(100.0f * LocalReplicationData.RepData.Num() / UncompressedBuffer.Num()),
			1000.0f * (Timer3 - Timer2)
		);
#endif
	}
	else
	{
		LocalReplicationData.RepData = UncompressedBuffer;
		LocalReplicationData.RepDataSize = LocalReplicationData.RepData.Num();
		OnDataReplicationComplited(LocalReplicationData.RepData);
	}
#endif
}

void UGameplayDebuggerBaseObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	DataPreparationStartTime = FPlatformTime::Seconds();
#endif
}

void UGameplayDebuggerBaseObject::DrawCollectedData(APlayerController* MyPC, AActor* SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	float VerticalLabelOffset = 0.f;
	for (const auto& Shape : GenericShapeElements)
	{
		if (Shape.GetType() == EGameplayDebuggerShapeElement::String)
		{
			const FVector& Loc = Shape.Points[0];
			const FVector ScreenLoc = DefaultContext.Canvas->Project(Loc);

			PrintString(DefaultContext, Shape.GetFColor(), Shape.Description, ScreenLoc.X, ScreenLoc.Y + VerticalLabelOffset);
			VerticalLabelOffset += 17;
		}
		else if (Shape.GetType() == EGameplayDebuggerShapeElement::Segment)
		{
			DrawDebugLine(GetWorld(), Shape.Points[0], Shape.Points[1], Shape.GetFColor());
		}
		else if (Shape.GetType() == EGameplayDebuggerShapeElement::SinglePoint)
		{
			DrawDebugSphere(GetWorld(), Shape.Points[0], Shape.ThicknesOrRadius, 16, Shape.GetFColor());
		}
		else if (Shape.GetType() == EGameplayDebuggerShapeElement::Cylinder)
		{
			DrawDebugCylinder(GetWorld(), Shape.Points[0], Shape.Points[1], Shape.ThicknesOrRadius, 16, Shape.GetFColor());
		}
	}
#endif
}

void UGameplayDebuggerBaseObject::BindKeyboardImput(class UInputComponent*& InputComponent)
{

}

void UGameplayDebuggerBaseObject::PostLoad()
{
	Super::PostLoad();

#if ENABLED_GAMEPLAY_DEBUGGER
	if (HasAnyFlags(RF_ClassDefaultObject)
#	if WITH_HOT_RELOAD
		&& !GIsHotReload
#	endif // WITH_HOT_RELOAD
		)
	{
		UGameplayDebuggerBaseObject::GetOnClassRegisterEvent().Broadcast(this);
	}
#endif
}

void UGameplayDebuggerBaseObject::BeginDestroy()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (HasAnyFlags(RF_ClassDefaultObject)
#	if WITH_HOT_RELOAD
		&& !GIsHotReload
#	endif // WITH_HOT_RELOAD
		)
	{
		UGameplayDebuggerBaseObject::GetOnClassUnregisterEvent().Broadcast(this);
	}
#endif
	Super::BeginDestroy();
}

UGameplayDebuggerBaseObject::FOnClassRegisterChangeEvent& UGameplayDebuggerBaseObject::GetOnClassRegisterEvent()
{
	static UGameplayDebuggerBaseObject::FOnClassRegisterChangeEvent OnClassRegisterEvent;
	return OnClassRegisterEvent;
}

UGameplayDebuggerBaseObject::FOnClassRegisterChangeEvent& UGameplayDebuggerBaseObject::GetOnClassUnregisterEvent()
{
	static UGameplayDebuggerBaseObject::FOnClassRegisterChangeEvent OnClassUnregisterEvent;
	return OnClassUnregisterEvent;
}


//----------------------------------------------------------------------//
// FDebugShapeElement
//----------------------------------------------------------------------//

FArchive& operator<<(FArchive& Ar, FGameplayDebuggerShapeElement& ShapeElement)
{
	Ar << ShapeElement.Description;
	Ar << ShapeElement.Points;
	Ar << ShapeElement.TransformationMatrix;
	Ar << ShapeElement.Type;
	Ar << ShapeElement.Color;
	Ar << ShapeElement.ThicknesOrRadius;

	return Ar;
}

FGameplayDebuggerShapeElement::FGameplayDebuggerShapeElement(EGameplayDebuggerShapeElement InType)
	: TransformationMatrix(FMatrix::Identity)
	, Type(InType)
	, Color(0xff)
	, ThicknesOrRadius(0)
{
	// Empty
}

FGameplayDebuggerShapeElement::FGameplayDebuggerShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness)
	: TransformationMatrix(FMatrix::Identity)
	, Type(EGameplayDebuggerShapeElement::Invalid)
	, ThicknesOrRadius(InThickness)
{
	if (InDescription.IsEmpty() == false)
	{
		Description = InDescription;
	}
	SetColor(InColor);
}

void FGameplayDebuggerShapeElement::SetColor(const FColor& InColor)
{
	Color = ((InColor.DWColor() >> 30) << 6) | (((InColor.DWColor() & 0x00ff0000) >> 22) << 4) | (((InColor.DWColor() & 0x0000ff00) >> 14) << 2) | ((InColor.DWColor() & 0x000000ff) >> 6);
}

EGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::GetType() const
{
	return Type;
}

void FGameplayDebuggerShapeElement::SetType(EGameplayDebuggerShapeElement InType)
{
	Type = InType;
}

FColor FGameplayDebuggerShapeElement::GetFColor() const
{
	FColor RetColor(((Color & 0xc0) << 24) | ((Color & 0x30) << 18) | ((Color & 0x0c) << 12) | ((Color & 0x03) << 6));
	RetColor.A = (RetColor.A * 255) / 192; // convert alpha to 0-255 range
	return RetColor;
}

//////////////////////////////////////////////////////////////////////////
UGameplayDebuggerHelper::FPrintContext UGameplayDebuggerHelper::OverHeadContext;
UGameplayDebuggerHelper::FPrintContext UGameplayDebuggerHelper::DefaultContext;

UGameplayDebuggerHelper::UGameplayDebuggerHelper(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UGameplayDebuggerHelper::PrintDebugString(const FString& InString)
{
	PrintString(GetDefaultContext(), InString);
}

void UGameplayDebuggerHelper::PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FString& InString)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	class FParserHelper
	{
		enum TokenType
		{
			OpenTag,
			CloseTag,
			NewLine,
			EndOfString,
			RegularChar,
			Tab
		};

		enum Tag
		{
			DefinedColor,
			OtherColor,
			ErrorTag,
		};

		uint8 ReadToken()
		{
			uint8 OutToken = RegularChar;
			TCHAR ch = Index < DataString.Len() ? DataString[Index] : '\0';
			switch (ch)
			{
			case '\0':
				OutToken = EndOfString;
				break;
			case '{':
				OutToken = OpenTag;
				Index++;
				break;
			case '}':
				OutToken = CloseTag;
				Index++;
				break;
			case '\n':
				OutToken = NewLine;
				Index++;
				break;
			case '\t':
				OutToken = Tab;
				Index++;
				break;
			default:
				OutToken = RegularChar;
				break;
			}
			return OutToken;
		}

		uint32 ParseTag(FString& OutData)
		{
			FString TagString;
			int32 OryginalIndex = Index;
			uint8 token = ReadToken();
			while (token != EndOfString && token != CloseTag)
			{
				if (token == RegularChar)
				{
					TagString.AppendChar(DataString[Index++]);
				}
				token = ReadToken();
			}

			int OutTag = ErrorTag;

			if (token != CloseTag)
			{
				Index = OryginalIndex;
				OutData = FString::Printf(TEXT("{%s"), *TagString);
				OutData.AppendChar(DataString[Index - 1]);
				return OutTag;
			}

			if (GColorList.IsValidColorName(*TagString.ToLower()))
			{
				OutTag = DefinedColor;
				OutData = TagString;
			}
			else
			{
				FColor Color;
				if (Color.InitFromString(TagString))
				{
					OutTag = OtherColor;
					OutData = TagString;
				}
				else
				{
					OutTag = ErrorTag;
					OutData = FString::Printf(TEXT("{%s"), *TagString);
					OutData.AppendChar(DataString[Index - 1]);
				}
			}
			//Index++;
			return OutTag;
		}

		struct StringNode
		{
			FString String;
			FColor Color;
			bool bNewLine;
			StringNode() : Color(FColor::White), bNewLine(false) {}
		};
		int32 Index;
		FString DataString;
	public:
		TArray<StringNode> Strings;

		void ParseString(const FString& StringToParse)
		{
			Index = 0;
			DataString = StringToParse;
			Strings.Add(StringNode());
			if (Index >= DataString.Len())
				return;

			uint8 Token = ReadToken();
			while (Token != EndOfString)
			{
				switch (Token)
				{
				case RegularChar:
					Strings[Strings.Num() - 1].String.AppendChar(DataString[Index++]);
					break;
				case NewLine:
					Strings.Add(StringNode());
					Strings[Strings.Num() - 1].bNewLine = true;
					Strings[Strings.Num() - 1].Color = Strings[Strings.Num() - 2].Color;
					break;
				case EndOfString:
					break;
				case Tab:
				{
					const FString TabString(TEXT("     "));
					Strings[Strings.Num() - 1].String.Append(TabString);
					static bool sbTest = false;
					if (sbTest)
					{
						Index++;
					}
					break;
				}
				case OpenTag:
				{
					FString OutData;
					switch (ParseTag(OutData))
					{
					case DefinedColor:
					{
						int32 i = Strings.Add(StringNode());
						Strings[i].Color = GColorList.GetFColorByName(*OutData.ToLower());
					}
						break;
					case OtherColor:
					{
						FColor NewColor;
						if (NewColor.InitFromString(OutData))
						{
							int32 i = Strings.Add(StringNode());
							Strings[i].Color = NewColor;
							break;
						}
					}
					default:
						Strings[Strings.Num() - 1].String += OutData;
						break;
					}
				}
					break;
				}
				Token = ReadToken();
			}
		}
	};

	FParserHelper Helper;
	Helper.ParseString(InString);

	float YMovement = 0;
	float XL = 0.f, YL = 0.f;
	CalulateStringSize(Context, Context.Font.Get(), TEXT("X"), XL, YL);

	for (int32 Index = 0; Index < Helper.Strings.Num(); ++Index)
	{
		if (Index > 0 && Helper.Strings[Index].bNewLine)
		{
			if (Context.Canvas != NULL && YMovement + Context.CursorY > Context.Canvas->ClipY)
			{
				Context.DefaultX += Context.Canvas->ClipX / 2;
				Context.CursorX = Context.DefaultX;
				Context.CursorY = Context.DefaultY;
				YMovement = 0;
			}
			YMovement += YL;
			Context.CursorX = Context.DefaultX;
		}
		const FString Str = Helper.Strings[Index].String;

		if (Str.Len() > 0 && Context.Canvas != NULL)
		{
			float SizeX, SizeY;
			CalulateStringSize(Context, Context.Font.Get(), Str, SizeX, SizeY);

			FCanvasTextItem TextItem(FVector2D::ZeroVector, FText::FromString(Str), Context.Font.Get(), FLinearColor::White);
			if (Context.FontRenderInfo.bEnableShadow)
			{
				TextItem.EnableShadow(FColor::Black, FVector2D(1, 1));
			}

			TextItem.SetColor(Helper.Strings[Index].Color);
			DrawItem(Context, TextItem, Context.CursorX, YMovement + Context.CursorY);
			Context.CursorX += SizeX;
		}
	}

	Context.CursorY += YMovement;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggerHelper::PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FColor& InColor, const FString& InString)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	PrintString(Context, FString::Printf(TEXT("{%s}%s"), *InColor.ToString(), *InString));
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggerHelper::PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FString& InString, float X, float Y)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const float OldX = Context.CursorX, OldY = Context.CursorY;
	const float OldDX = Context.DefaultX, OldDY = Context.DefaultY;

	Context.DefaultX = Context.CursorX = X;
	Context.DefaultY = Context.CursorY = Y;
	PrintString(Context, InString);

	Context.CursorX = OldX;
	Context.CursorY = OldY;
	Context.DefaultX = OldDX;
	Context.DefaultY = OldDY;
#endif
}

void UGameplayDebuggerHelper::PrintString(UGameplayDebuggerHelper::FPrintContext& Context, const FColor& InColor, const FString& InString, float X, float Y)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const float OldX = Context.CursorX, OldY = Context.CursorY;
	const float OldDX = Context.DefaultX, OldDY = Context.DefaultY;

	Context.DefaultX = Context.CursorX = X;
	Context.DefaultY = Context.CursorY = Y;
	PrintString(Context, InColor, InString);

	Context.CursorX = OldX;
	Context.CursorY = OldY;
	Context.DefaultX = OldDX;
	Context.DefaultY = OldDY;
#endif
}

void UGameplayDebuggerHelper::CalulateStringSize(const UGameplayDebuggerHelper::FPrintContext& Context, UFont* Font, const FString& InString, float& OutX, float& OutY)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	FString String = InString;
	const FRegexPattern ElementRegexPattern(TEXT("\\{.*?\\}"));
	FRegexMatcher ElementRegexMatcher(ElementRegexPattern, String);

	while (ElementRegexMatcher.FindNext())
	{
		int32 AttributeListBegin = ElementRegexMatcher.GetCaptureGroupBeginning(0);
		int32 AttributeListEnd = ElementRegexMatcher.GetCaptureGroupEnding(0);
		String.RemoveAt(AttributeListBegin, AttributeListEnd - AttributeListBegin);
		ElementRegexMatcher = FRegexMatcher(ElementRegexPattern, String);
	}

	OutX = OutY = 0;
	if (Context.Canvas != NULL)
	{
		TArray<FString> Lines;
		String.ParseIntoArrayLines(Lines);
		for (FString Line : Lines)
		{
			float Width, Height;
			Context.Canvas->StrLen(Font != NULL ? Font : Context.Font.Get(), Line, Width, Height);
			OutX = FMath::Max(OutX, Width);
			OutY += Height;
		}
	}
#endif
}

void UGameplayDebuggerHelper::CalulateTextSize(const UGameplayDebuggerHelper::FPrintContext& Context, UFont* Font, const FText& InText, float& OutX, float& OutY)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	CalulateStringSize(Context, Font, InText.ToString(), OutX, OutY);
#endif
}

FVector UGameplayDebuggerHelper::ProjectLocation(const UGameplayDebuggerHelper::FPrintContext& Context, const FVector& Location)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (Context.Canvas != NULL)
	{
		return Context.Canvas->Project(Location);
	}
#endif
	return FVector();
}

void UGameplayDebuggerHelper::DrawItem(const UGameplayDebuggerHelper::FPrintContext& Context, class FCanvasItem& Item, float X, float Y)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (Context.Canvas.IsValid())
	{
		Context.Canvas->DrawItem(Item, FVector2D(X, Y));
	}
#endif
}

void UGameplayDebuggerHelper::DrawIcon(const UGameplayDebuggerHelper::FPrintContext& Context, const FColor& InColor, const FCanvasIcon& Icon, float X, float Y, float Scale)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (Context.Canvas.IsValid())
	{
		Context.Canvas->SetDrawColor(InColor);
		Context.Canvas->DrawIcon(Icon, X, Y, Scale);
	}
#endif
}

FGameplayDebuggerShapeElement UGameplayDebuggerHelper::MakeString(const FString& Description, const FVector& Location)
{
	FGameplayDebuggerShapeElement StringElement(Description, FColor::White, 0);
	StringElement.SetType(EGameplayDebuggerShapeElement::String);
	StringElement.Points.Add(Location);

	return StringElement;
}

FGameplayDebuggerShapeElement UGameplayDebuggerHelper::MakeLine(const FVector& Start, const FVector& End, FColor Color, float Thickness)
{
	FGameplayDebuggerShapeElement LineElement(EGameplayDebuggerShapeElement::Segment);
	LineElement.Points.Add(Start);
	LineElement.Points.Add(End);
	LineElement.SetColor(Color);
	LineElement.ThicknesOrRadius = Thickness;

	return LineElement;
}

FGameplayDebuggerShapeElement UGameplayDebuggerHelper::MakePoint(const FVector& Location, FColor Color, float Radius)
{
	FGameplayDebuggerShapeElement PointElement(EGameplayDebuggerShapeElement::SinglePoint);
	PointElement.Points.Add(Location);
	PointElement.ThicknesOrRadius = Radius;
	PointElement.SetColor(Color);

	return PointElement;
}

FGameplayDebuggerShapeElement UGameplayDebuggerHelper::MakeCylinder(FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color)
{
	FGameplayDebuggerShapeElement ShapeElement(EGameplayDebuggerShapeElement::Cylinder);
	ShapeElement.Points.Add(Start);
	ShapeElement.Points.Add(End);
	ShapeElement.ThicknesOrRadius = Radius;
	ShapeElement.SetColor(Color);

	return ShapeElement;
}

