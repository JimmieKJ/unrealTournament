#include "NiagaraPrivate.h"
#include "NiagaraCommon.h"

uint32 GetNiagaraDataElementCount(ENiagaraDataType Type)
{
	static int32 Counts[] = { 1, 4, 16 };
	return Counts[(uint8)Type];
}

uint32 GetNiagaraBytesPerElement(ENiagaraDataType DataType, ENiagaraType Type)
{
	static int32 Sizes[] = { 1, 2, 4, 4 };
	return Sizes[(uint8)Type] * GetNiagaraDataElementCount(DataType);
}


