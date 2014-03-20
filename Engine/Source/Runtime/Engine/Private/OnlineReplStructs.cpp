// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineReplStructs.cpp: Unreal networking serialization helpers
=============================================================================*/

#include "EnginePrivate.h"
#include "Online.h"

FArchive& operator<<( FArchive& Ar, FUniqueNetIdRepl& UniqueNetId)
{
	int32 Size = UniqueNetId.IsValid() ? UniqueNetId->GetSize() : 0;
	Ar << Size;

	if (Size > 0)
	{
		if (Ar.IsSaving())
		{
			check(UniqueNetId.IsValid());
			FString Contents = UniqueNetId->ToString();
			Ar << Contents;
		}
		else if (Ar.IsLoading())
		{
			FString Contents;
			Ar << Contents;	// that takes care about possible overflow

			IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface();
			if (IdentityInt.IsValid())
			{
				TSharedPtr<FUniqueNetId> UniqueNetIdPtr = IdentityInt->CreateUniquePlayerId(Contents);
				UniqueNetId.SetUniqueNetId(UniqueNetIdPtr);
			}
		}
	}

	return Ar;
}

bool FUniqueNetIdRepl::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << *this;
	bOutSuccess = true;
	return true;
}

bool FUniqueNetIdRepl::Serialize(FArchive& Ar)
{
	Ar << *this;
	return true;
}

bool FUniqueNetIdRepl::ExportTextItem(FString& ValueStr, FUniqueNetIdRepl const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	ValueStr += UniqueNetId.IsValid() ? UniqueNetId->ToString() : TEXT("INVALID");
	return true;
}

void TestUniqueIdRepl()
{
	bool bSuccess = true;

	IOnlineIdentityPtr IdentityPtr = Online::GetIdentityInterface();
	if (IdentityPtr.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = IdentityPtr->GetUniquePlayerId(0);

		FUniqueNetIdRepl EmptyIdIn;
		if (EmptyIdIn.IsValid())
		{
			UE_LOG(LogNet, Warning, TEXT("EmptyId is valid."), *EmptyIdIn->ToString());
			bSuccess = false;
		}

		FUniqueNetIdRepl ValidIdIn(UserId);
		if (!ValidIdIn.IsValid() || UserId != ValidIdIn.GetUniqueNetId())
		{
			UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *UserId->ToString(), *ValidIdIn->ToString());
			bSuccess = false;
		}

		if (bSuccess)
		{
			TArray<uint8> Buffer;
			for (int32 i=0; i<2; i++)
			{
				Buffer.Empty();
				FMemoryWriter TestWriteUniqueId(Buffer);

				if (i == 0)
				{
					// Normal serialize
					TestWriteUniqueId << EmptyIdIn;
					TestWriteUniqueId << ValidIdIn;
				}
				else
				{
					// Net serialize
					bool bSuccess = false;
					EmptyIdIn.NetSerialize(TestWriteUniqueId, NULL, bSuccess);
					ValidIdIn.NetSerialize(TestWriteUniqueId, NULL, bSuccess);
				}

				FMemoryReader TestReadUniqueId(Buffer);

				FUniqueNetIdRepl EmptyIdOut;
				TestReadUniqueId << EmptyIdOut;
				if (EmptyIdOut.GetUniqueNetId().IsValid())
				{
					UE_LOG(LogNet, Warning, TEXT("EmptyId %s should have been invalid"), *EmptyIdOut->ToString());
					bSuccess = false;
				}

				FUniqueNetIdRepl ValidIdOut;
				TestReadUniqueId << ValidIdOut;
				if (*UserId != *ValidIdOut.GetUniqueNetId())
				{
					UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *ValidIdIn->ToString(), *ValidIdOut->ToString());
					bSuccess = false;
				}
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogNet, Warning, TEXT("TestUniqueIdRepl test failure!"));
	}
}




