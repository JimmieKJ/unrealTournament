// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraConstantSet.h"

template<>
struct TStructOpsTypeTraits<FNiagaraConstantMap> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true
	};
};
IMPLEMENT_STRUCT(NiagaraConstantMap);
