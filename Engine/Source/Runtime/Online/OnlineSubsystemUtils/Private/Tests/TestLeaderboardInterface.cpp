// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "TestLeaderboardInterface.h"
#include "OnlineIdentityInterface.h"
#include "ModuleManager.h"

/**
 *	Example of a leaderboard write object
 */
class TestLeaderboardWrite : public FOnlineLeaderboardWrite
{
public:
	TestLeaderboardWrite()
	{
		// Default properties
		new (LeaderboardNames) FName(TEXT("TestLeaderboard"));
		RatedStat = "TestIntStat1";
		DisplayFormat = ELeaderboardFormat::Number;
		SortMethod = ELeaderboardSort::Descending;
		UpdateMethod = ELeaderboardUpdateMethod::KeepBest;
	}
};

/**
 *	Example of a leaderboard read object
 */
class TestLeaderboardRead : public FOnlineLeaderboardRead
{
public:
	TestLeaderboardRead()
	{
		// Default properties
		LeaderboardName = FName(TEXT("TestLeaderboard"));
		SortedColumn = "TestIntStat1";

		// Define default columns
		new (ColumnMetadata) FColumnMetaData("TestIntStat1", EOnlineKeyValuePairDataType::Int32);
		new (ColumnMetadata) FColumnMetaData("TestFloatStat1", EOnlineKeyValuePairDataType::Float);
	}
};

void FTestLeaderboardInterface::Test()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(FName(*Subsystem));
	check(OnlineSub); 

	if (OnlineSub->GetIdentityInterface().IsValid())
	{
		UserId = OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0);
	}

	// Cache interfaces
	Leaderboards = OnlineSub->GetLeaderboardsInterface();
	check(Leaderboards.IsValid());

	// Define delegates
	LeaderboardFlushDelegate = FOnLeaderboardFlushCompleteDelegate::CreateRaw(this, &FTestLeaderboardInterface::OnLeaderboardFlushComplete);
	LeaderboardReadCompleteDelegate = FOnLeaderboardReadCompleteDelegate::CreateRaw(this, &FTestLeaderboardInterface::OnLeaderboardReadComplete);
}

void FTestLeaderboardInterface::WriteLeaderboards()
{
	TestLeaderboardWrite WriteObject;
	
	// Set some data
	WriteObject.SetIntStat("TestIntStat1", 50);
	WriteObject.SetFloatStat("TestFloatStat1", 99.0f);

	// Write it to the buffers
	Leaderboards->WriteLeaderboards(TEXT("TEST"), *UserId, WriteObject);
	TestPhase++;
}

void FTestLeaderboardInterface::OnLeaderboardFlushComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnLeaderboardFlushComplete Session: %s bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	Leaderboards->ClearOnLeaderboardFlushCompleteDelegate(LeaderboardFlushDelegate);
	TestPhase++;
}

void FTestLeaderboardInterface::FlushLeaderboards()
{
	Leaderboards->AddOnLeaderboardFlushCompleteDelegate(LeaderboardFlushDelegate);
	Leaderboards->FlushLeaderboards(TEXT("TEST"));
}

void FTestLeaderboardInterface::OnLeaderboardReadComplete(bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnLeaderboardReadComplete bWasSuccessful: %d"), bWasSuccessful);
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	Leaderboards->ClearOnLeaderboardReadCompleteDelegate(LeaderboardReadCompleteDelegate);
	TestPhase++;
}

void FTestLeaderboardInterface::ReadLeaderboards()
{
	ReadObject = MakeShareable(new TestLeaderboardRead());
	FOnlineLeaderboardReadRef ReadObjectRef = ReadObject.ToSharedRef();

	Leaderboards->AddOnLeaderboardReadCompleteDelegate(LeaderboardReadCompleteDelegate);
	Leaderboards->ReadLeaderboardsForFriends(0, ReadObjectRef);
}

bool FTestLeaderboardInterface::Tick( float DeltaTime )
{
	if (TestPhase != LastTestPhase)
	{
		LastTestPhase = TestPhase;
		if (!bOverallSuccess)
		{
			UE_LOG(LogOnline, Log, TEXT("Testing failed in phase %d"), LastTestPhase);
			TestPhase = 3;
		}

		switch(TestPhase)
		{
		case 0:
			WriteLeaderboards();
			break;
		case 1:
			FlushLeaderboards();
			break;
		case 2:
			ReadLeaderboards();
			break;
		case 3:
			UE_LOG(LogOnline, Log, TEXT("TESTING COMPLETE Success:%s!"), bOverallSuccess ? TEXT("true") : TEXT("false"));
			delete this;
			return false;
		}
	}
	return true;
}
