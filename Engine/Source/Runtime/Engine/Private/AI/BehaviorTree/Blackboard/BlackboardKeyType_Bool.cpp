// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UBlackboardKeyType_Bool::UBlackboardKeyType_Bool(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	ValueSize = sizeof(uint8);
}

bool UBlackboardKeyType_Bool::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<uint8>(RawData) != 0;
}

bool UBlackboardKeyType_Bool::SetValue(uint8* RawData, bool bValue)
{
	return SetValueInMemory<uint8>(RawData, bValue);
}

FString UBlackboardKeyType_Bool::DescribeValue(const uint8* RawData) const
{
	return GetValue(RawData) ? TEXT("true") : TEXT("false");
}

int32 UBlackboardKeyType_Bool::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return GetValueFromMemory<uint8>(MemoryBlockA) == GetValueFromMemory<uint8>(MemoryBlockB) ? UBlackboardKeyType::Equal : UBlackboardKeyType::NotEqual;
}
