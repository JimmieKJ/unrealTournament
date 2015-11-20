// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"
#include "CryptoPP/5.6.2/include/twofish.h"

/* TwoFish Block Encryptor Module Interface */
class FTwoFishBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};

/*
* TwoFish Block encryption
*/
class TWOFISHBLOCKENCRYPTOR_API TwoFishBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<byte>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(byte* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(byte* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 16; }

private:
	/* Encryptors for AES */
	CryptoPP::Twofish::Encryption Encryptor;
	CryptoPP::Twofish::Decryption Decryptor;
};