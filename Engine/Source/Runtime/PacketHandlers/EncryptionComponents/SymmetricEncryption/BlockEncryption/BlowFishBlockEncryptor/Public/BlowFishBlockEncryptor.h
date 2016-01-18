// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"
#include "CryptoPP/5.6.2/include/blowfish.h"

/* BlowFish Block Encryptor Module Interface */
class FBlowFishBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};


/*
* BlowFish Block encryption
*/
class BLOWFISHBLOCKENCRYPTOR_API BlowFishBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<byte>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(byte* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(byte* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 8; }

private:
	/* Encryptors for AES */
	CryptoPP::Blowfish::Encryption Encryptor;
	CryptoPP::Blowfish::Decryption Decryptor;
};