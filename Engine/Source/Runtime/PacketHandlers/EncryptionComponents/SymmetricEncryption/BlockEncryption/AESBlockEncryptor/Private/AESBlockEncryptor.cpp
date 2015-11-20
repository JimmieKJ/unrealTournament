// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AESBlockEncryptor.h"

IMPLEMENT_MODULE(FAESBlockEncryptorModuleInterface, AESBlockEncryptor);

// MODULE INTERFACE
BlockEncryptor* FAESBlockEncryptorModuleInterface::CreateBlockEncryptorInstance()
{
	return new AESBlockEncryptor;
}

// AES
void AESBlockEncryptor::Initialize(TArray<byte>* InKey)
{
	Key = InKey;

	if (Key->Num() != 16 && Key->Num() != 32)
	{
		LowLevelFatalError(TEXT("Incorrect key size. %i size chosen"), Key->Num());
	}

	Encryptor = CryptoPP::AES::Encryption(Key->GetData(), Key->Num());
	Decryptor = CryptoPP::AES::Decryption(Key->GetData(), Key->Num());

	FixedBlockSize = 16;
}

void AESBlockEncryptor::EncryptBlock(byte* Block)
{
	byte* Output = new byte[FixedBlockSize];
	Encryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("AES Encrypted"));
}

void AESBlockEncryptor::DecryptBlock(byte* Block)
{
	byte* Output = new byte[FixedBlockSize];
	Decryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("AES Decrypted"));
}
