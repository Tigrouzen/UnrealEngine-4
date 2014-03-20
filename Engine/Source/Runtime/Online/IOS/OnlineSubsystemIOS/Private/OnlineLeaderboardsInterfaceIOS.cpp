// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineSubsystemIOSPrivatePCH.h"

// GameCenter includes
#include <GameKit/GKLeaderboard.h>
#include <GameKit/GKScore.h>


FOnlineLeaderboardsIOS::FOnlineLeaderboardsIOS(FOnlineSubsystemIOS* InSubsystem)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::FOnlineLeaderboardsIOS()"));

	// Cache a reference to the OSS Identity and Friends interfaces, we will need these when we are performing leaderboard actions
	IdentityInterface = (FOnlineIdentityIOS*)InSubsystem->GetIdentityInterface().Get();
	FriendsInterface = (FOnlineFriendsIOS*)InSubsystem->GetFriendsInterface().Get();
}


bool FOnlineLeaderboardsIOS::ReadLeaderboards(const TArray< TSharedRef<FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& InReadObject)
{
	auto ReadObject = InReadObject;

	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::ReadLeaderboards() %p"), &ReadObject);
	bool bTriggeredReadRequest = false;

	ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
	ReadObject->Rows.Empty();
	
	if ((IdentityInterface->GetLocalGameCenterUser() != NULL) && IdentityInterface->GetLocalGameCenterUser().isAuthenticated)
	{
		ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

		// Populate a list of id's for our friends which we want to look up.
		NSMutableArray* FriendIds = [NSMutableArray arrayWithCapacity: (Players.Num() + 1)];
		
		// Add the local player to the list of ids to look up.
		FriendIds[0] = [NSString stringWithCString:TCHAR_TO_ANSI(*IdentityInterface->GetUniquePlayerId(0)->ToString())  encoding:NSASCIIStringEncoding];
		
		for (int32 FriendIdx = 0; FriendIdx < Players.Num(); FriendIdx++)
		{
			FriendIds[FriendIdx + 1] = [NSString stringWithCString:TCHAR_TO_ANSI(*Players[FriendIdx]->ToString())  encoding:NSASCIIStringEncoding];
		}

		// Kick off a game center read request for the list of users
		GKLeaderboard* LeaderboardRequest = [[GKLeaderboard alloc] initWithPlayerIDs:FriendIds];
		if (LeaderboardRequest != nil)
		{
			const FString LeaderboardName = ReadObject->LeaderboardName.ToString();
			NSString* Category = [NSString stringWithCString:TCHAR_TO_ANSI( *LeaderboardName )  encoding:NSASCIIStringEncoding];
			UE_LOG(LogOnline, Display, TEXT("Attempting to read leaderboard: %s"), *LeaderboardName);

			LeaderboardRequest.playerScope = GKLeaderboardPlayerScopeGlobal;
			LeaderboardRequest.timeScope = GKLeaderboardTimeScopeToday;
			LeaderboardRequest.category = Category;
			LeaderboardRequest.range = NSMakeRange(1,10);

			bTriggeredReadRequest = true;
			dispatch_async(dispatch_get_main_queue(), ^
			{
				[LeaderboardRequest loadScoresWithCompletionHandler: ^(NSArray *scores, NSError *Error) 
					{
						bool bWasSuccessful = (Error == nil) && [scores count] > 0;

						if (bWasSuccessful)
						{
							UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::loadScoresWithCompletionHandler() - %s"), (bWasSuccessful ? TEXT("Success!") : TEXT("Failed!, no scores retrieved")));
							for (GKScore* score in scores)
							{
								UE_LOG(LogOnline, Display, TEXT("----------------------------------------------------------------"));
								UE_LOG(LogOnline, Display, TEXT("PlayerId: %s"), ANSI_TO_TCHAR([score.playerID cStringUsingEncoding : NSASCIIStringEncoding]));
								UE_LOG(LogOnline, Display, TEXT("Value: %d"), score.value);
								UE_LOG(LogOnline, Display, TEXT("----------------------------------------------------------------"));

								const FString PlayerIDString(ANSI_TO_TCHAR([score.playerID cStringUsingEncoding : NSASCIIStringEncoding]));
								TSharedRef<FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(PlayerIDString));

								FOnlineStatsRow* UserRow = ReadObject.Get().FindPlayerRecord(UserId.Get());
								if (UserRow == NULL)
								{
									UserRow = new (ReadObject->Rows) FOnlineStatsRow(PlayerIDString, UserId);
								}

								for (int32 StatIdx = 0; StatIdx < ReadObject->ColumnMetadata.Num(); StatIdx++)
								{
									const FColumnMetaData& ColumnMeta = ReadObject->ColumnMetadata[StatIdx];

									FVariantData* LastColumn = NULL;
									switch (ColumnMeta.DataType)
									{
									case EOnlineKeyValuePairDataType::Int32:
										{
											int32 Value = score.value;
											LastColumn = UserRow->Columns.Add(ColumnMeta.ColumnName, FVariantData(Value));
											bWasSuccessful = true;
											break;
										}

									default:
										{
											UE_LOG_ONLINE(Warning, TEXT("Unsupported key value pair during retrieval from GameCenter %s"), ColumnMeta.ColumnName);
											break;
										}
									}
								}
							}
						}
						else if (Error)
						{
							// if we have failed to read the leaderboard then report this
							NSDictionary *userInfo = [Error userInfo];
							NSString *errstr = [[userInfo objectForKey : NSUnderlyingErrorKey] localizedDescription];
							UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::loadScoresWithCompletionHandler() - Failed to read leaderboard with error: [%s]"), ANSI_TO_TCHAR([errstr cStringUsingEncoding : NSASCIIStringEncoding]));
							UE_LOG(LogOnline, Warning, TEXT("You should check that the leaderboard name matches that of one in ITunesConnect"));
						}
						
						// Report back to the game thread whether this succeeded.
						[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
						{
							ReadObject->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
							TriggerOnLeaderboardReadCompleteDelegates(bWasSuccessful);
							return true;
						}];
					}
				];
			});
		}
	}
	
	// If we have failed to kick off a read request, we should still tell whoever is listening.
	if (!bTriggeredReadRequest)
	{
		UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::loadScoresWithCompletionHandler() - Failed!"));
		TriggerOnLeaderboardReadCompleteDelegates(false);
	}

	return bTriggeredReadRequest;
}


bool FOnlineLeaderboardsIOS::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::ReadLeaderboardsForFriends()"));
	if( IdentityInterface->GetLocalGameCenterUser() != NULL && IdentityInterface->GetLocalGameCenterUser().isAuthenticated )
	{
		// Gather the friends from the local players game center friends list and perform a read request for these
		TArray< TSharedRef<FOnlineFriend> > Friends;
		FriendsInterface->GetFriendsList( 0, EFriendsLists::ToString(EFriendsLists::Default), Friends );

		TArray< TSharedRef< FUniqueNetId > > FriendIds;
		for( int32 Idx = 0; Idx < Friends.Num(); Idx++ )
		{
			FriendIds.Add( Friends[ Idx ]->GetUserId() );
		}
		ReadLeaderboards( FriendIds, ReadObject );
	}
	
	return true;
}


void FOnlineLeaderboardsIOS::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::FreeStats()"));
	// not implemented for gc leaderboards
}


bool FOnlineLeaderboardsIOS::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::WriteLeaderboards()"));
	bool bWroteAnyLeaderboard = false;

	// Make sure we have storage space for scores
	if (UnreportedScores == nil)
	{
		UnreportedScores = [NSMutableArray arrayWithCapacity : WriteObject.Properties.Num()];
	}

	//@TODO: Note: The array of leaderboard names is ignored, because they offer no data.
	// Instead the stat names are used as the leaderboard names for iOS for now.  This whole API needs rework!

	// Queue up the leaderboard stat writes
	for (FStatPropertyArray::TConstIterator It(WriteObject.Properties); It; ++It)
	{
		// Access the stat and the value.
		const FVariantData& Stat = It.Value();

		const FString LeaderboardName(It.Key().ToString());
		NSString* Category = [NSString stringWithCString:TCHAR_TO_ANSI(*LeaderboardName) encoding:NSASCIIStringEncoding];

		// Create a leaderboard score object which should be posted to the [Category] leaderboard.
		GKScore* Score = [[GKScore alloc] initWithCategory:Category];
		
		Score.context = 0;

		bool bIsValidScore = false;

		// Setup the score with the value we are writing from the variant type
		switch (Stat.GetType())
		{
			case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value;
				Stat.GetValue(Value);
				Score.value = Value;
					
				bIsValidScore = true;
				break;
			}

			default:
			{
				UE_LOG(LogOnline, Warning, TEXT("FOnlineLeaderboardsIOS::WriteLeaderboards(Leaderboard: %s) Invalid data type (only Int32 is currently supported)"), *LeaderboardName);
				break;
			}
		}

		if (bIsValidScore)
		{
			int32 Value = Score.value;
			UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::WriteLeaderboards() Queued score %d on leaderboard %s"), Value, *LeaderboardName);

			[UnreportedScores addObject : Score];
			bWroteAnyLeaderboard = true;
		}
	}
	
	// Return whether any stat was cached.
	return bWroteAnyLeaderboard;
}


bool FOnlineLeaderboardsIOS::FlushLeaderboards(const FName& SessionName)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::FlushLeaderboards()"));
	bool bBeganFlushingScores = false;

	if ((IdentityInterface->GetLocalGameCenterUser() != NULL) && IdentityInterface->GetLocalGameCenterUser().isAuthenticated)
	{
		const int32 UnreportedScoreCount = UnreportedScores.count;
		bBeganFlushingScores = UnreportedScoreCount > 0;

		if (bBeganFlushingScores)
		{
			NSMutableArray* ArrayCopy = UnreportedScores;
			UnreportedScores = nil;
			dispatch_async(dispatch_get_main_queue(), ^
			{
				[GKScore reportScores : ArrayCopy withCompletionHandler : ^ (NSError *error)
				{
					// Tell whoever was listening that we have written (or failed to write) to the leaderboard
					bool bSucceeded = error == NULL;
					if (bSucceeded)
					{
						UE_LOG(LogOnline, Display, TEXT("Flushed %d scores to Game Center"), UnreportedScoreCount);
					}
					else
					{
						UE_LOG(LogOnline, Display, TEXT("Error while flushing scores (code %d)"), [error code]);
					}

					// Report back to the game thread whether this succeeded.
					[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
					{
						TriggerOnLeaderboardFlushCompleteDelegates(SessionName, bSucceeded);
						return true;
					}];
				}
				];
			});
		}
	}

	// If we didn't begin writing to the leaderboard we should still notify whoever was listening.
	if (!bBeganFlushingScores)
	{
		TriggerOnLeaderboardFlushCompleteDelegates(SessionName, false);
		UE_LOG(LogOnline, Display, TEXT("Failed to flush scores to leaderboard"));
	}

	
	return bBeganFlushingScores;
}


bool FOnlineLeaderboardsIOS::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineLeaderboardsIOS::WriteOnlinePlayerRatings()"));
	// not implemented for gc leaderboards
	
	return false;
}