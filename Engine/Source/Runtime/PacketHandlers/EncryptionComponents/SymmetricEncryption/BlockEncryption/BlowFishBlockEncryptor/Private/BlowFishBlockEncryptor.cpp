// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlowFishBlockEncryptor.h"

IMPLEMENT_MODULE(FBlowFishBlockEncryptorModuleInterface, BlowFishBlockEncryptor);

// MODULE INTERFACE
BlockEncryptor* FBlowFishBlockEncryptorModuleInterface::CreateBlockEncryptorInstance()
{
	return new BlowFishBlockEncryptor;
}

// BLOWFISH
void BlowFishBlockEncryptor::Initialize(TArray<byte>* InKey)
{
	Key = InKey;

	if (Key->Num() <= 4 || Key->Num() >= 54 || Key->Num() % 8 != 0)
	{
		LowLevelFatalError(TEXT("Incorrect key size. %i size chosen"), Key->Num());
	}

	Encryptor = CryptoPP::Blowfish::Encryption(Key->GetData(), Key->Num());
	Decryptor = CryptoPP::Blowfish::Decryption(Key->GetData(), Key->Num());

	FixedBlockSize = 8;
}

void BlowFishBlockEncryptor::EncryptBlock(byte* Block)
{
	byte* Output = new byte[FixedBlockSize];
	Encryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("BlowFish Encrypted"));
}

void BlowFishBlockEncryptor::DecryptBlock(byte* Block)
{
	byte* Output = new byte[FixedBlockSize];
	Decryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("BlowFish Decrypted"));
}