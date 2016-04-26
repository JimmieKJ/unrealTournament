// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/TokenArchiveProxy.h"
#include "ParserHelper.h"

/* See UHTMakefile.h for overview how makefiles work. */
FTokenArchiveProxy::FTokenArchiveProxy(const FUHTMakefile& UHTMakefile, const FToken* Token)
	: FPropertyBaseArchiveProxy(UHTMakefile, Token)
{
	TokenType = Token->TokenType;
	TokenName = FNameArchiveProxy(UHTMakefile, Token->TokenName);
	StartPos = Token->StartPos;
	StartLine = Token->StartLine;
	FMemory::Memcpy(Identifier, Token->Identifier, sizeof(TCHAR) * NAME_SIZE);
	TokenPropertyIndex = UHTMakefile.GetPropertyIndex(Token->TokenProperty);

	switch (Token->Type)
	{
	case CPT_Byte:
		Byte = Token->Byte;
		break;
	case CPT_Int:
		Int = Token->Int;
		break;
	case CPT_Bool:
		NativeBool = Token->NativeBool;
		break;
	case CPT_Float:
		Float = Token->Float;
		break;
	case CPT_Double:
		Double = Token->Double;
		break;
	case CPT_Name:
		FMemory::Memcpy(NameBytes, Token->NameBytes, sizeof(FName));
		break;
	case CPT_String:
		FMemory::Memcpy(String, Token->String, sizeof(TCHAR) * MAX_STRING_CONST_SIZE);
		break;
	default:
		break;
	}
}

void FTokenArchiveProxy::AddReferencedNames(const FToken* Token, FUHTMakefile& UHTMakefile)
{
	FPropertyBaseArchiveProxy::AddReferencedNames(Token, UHTMakefile);
	UHTMakefile.AddName(Token->TokenName);
}

FToken* FTokenArchiveProxy::CreateToken() const
{
	FToken* Token = new FToken(Type);
	PostConstruct(Token);


	return Token;
}

void FTokenArchiveProxy::PostConstruct(FToken* Token) const
{
	FPropertyBaseArchiveProxy::PostConstruct(Token);
	Token->TokenType = TokenType;
	Token->StartPos = StartPos;
	Token->StartLine = StartLine;
	FMemory::Memcpy(Token->Identifier, Identifier, sizeof(TCHAR) * NAME_SIZE);

	switch (Type)
	{
	case CPT_Byte:
		Token->Byte = Byte;
		break;
	case CPT_Int:
		Token->Int = Int;
		break;
	case CPT_Bool:
		Token->NativeBool = NativeBool;
		break;
	case CPT_Float:
		Token->Float = Float;
		break;
	case CPT_Double:
		Token->Double = Double;
		break;
	case CPT_Name:
		FMemory::Memcpy(Token->NameBytes, NameBytes, sizeof(FName));
		break;
	case CPT_String:
		FMemory::Memcpy(Token->String, String, sizeof(TCHAR) * MAX_STRING_CONST_SIZE);
		break;
	default:
		break;
	}
}

void FTokenArchiveProxy::Resolve(FToken* Token, const FUHTMakefile& UHTMakefile) const
{
	FPropertyBaseArchiveProxy::Resolve(Token, UHTMakefile);
	Token->TokenName = TokenName.CreateName(UHTMakefile);
	Token->TokenProperty = UHTMakefile.GetPropertyByIndex(TokenPropertyIndex);
}

FArchive& operator<<(FArchive& Ar, FTokenArchiveProxy& TokenArchiveProxy)
{
	Ar << static_cast<FPropertyBaseArchiveProxy&>(TokenArchiveProxy);
	Ar << TokenArchiveProxy.TokenType;
	Ar << TokenArchiveProxy.TokenName;
	Ar << TokenArchiveProxy.StartPos;
	Ar << TokenArchiveProxy.StartLine;
	Ar.Serialize(TokenArchiveProxy.Identifier, sizeof(TokenArchiveProxy.Identifier));
	Ar << TokenArchiveProxy.TokenPropertyIndex;

	switch (TokenArchiveProxy.Type)
	{
	case CPT_Byte:
		Ar << TokenArchiveProxy.Byte;
		break;
	case CPT_Int:
		Ar << TokenArchiveProxy.Int;
		break;
	case CPT_Bool:
		Ar << TokenArchiveProxy.NativeBool;
		break;
	case CPT_Float:
		Ar << TokenArchiveProxy.Float;
		break;
	case CPT_Double:
		Ar << TokenArchiveProxy.Double;
		break;
	case CPT_Name:
		Ar.Serialize(TokenArchiveProxy.NameBytes, sizeof(TokenArchiveProxy.NameBytes));
		break;
	case CPT_String:
		Ar.Serialize(TokenArchiveProxy.String, sizeof(TokenArchiveProxy.String));
		break;
	default:
		break;
	}

	return Ar;
}
