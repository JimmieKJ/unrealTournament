// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME 1
#else
	#define USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME 0
#endif

