// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma  once 

	extern "C" { 
		 bool UE_SendAndRecievePayLoad( char *URL, char* indata, int insize, char** outdata, int* outsize );
		 bool UE_DoesSaveGameExist(const char* Name, const int UserIndex);
		 bool UE_SaveGame(const char* Name, const int UserIndex, const char* indata, int insize);
		 bool UE_LoadGame(const char* Name, const int UserIndex, char **outdata, int* outsize);
		 int  UE_MessageBox( int MsgType, const char* Text, const char* Caption);
		 int  UE_GetCurrentCultureName(const char* OutName, int outsize);
		 void UE_MakeHTTPDataRequest(void *ctx, const char* url, const char* verb, const char* payload, int payloadsize, const char* headers, int freeBuffer, void(*onload)(void*, void*, unsigned), void(*onerror)(void*, int, const char*), void(*onprogress)(void*, int, int));
	}

