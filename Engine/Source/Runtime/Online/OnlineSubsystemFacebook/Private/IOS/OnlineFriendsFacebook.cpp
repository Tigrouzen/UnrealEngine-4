// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineSharingFacebook.h"
#include "OnlineFriendsFacebook.h"

// FOnlineFriendFacebook

TSharedRef<FUniqueNetId> FOnlineFriendFacebook::GetUserId() const
{
	return UserId;
}

FString FOnlineFriendFacebook::GetRealName() const
{
	FString Result;
	GetAccountData(TEXT("name"), Result);
	return Result;
}

FString FOnlineFriendFacebook::GetDisplayName() const
{
	FString Result;
	GetAccountData(TEXT("username"), Result);
	return Result;
}

bool FOnlineFriendFacebook::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return GetAccountData(AttrName, OutAttrValue);
}

EInviteStatus::Type FOnlineFriendFacebook::GetInviteStatus() const
{
	return EInviteStatus::Accepted;
}

const FOnlineUserPresence& FOnlineFriendFacebook::GetPresence() const
{
	return Presence;
}

// FOnlineFriendsFacebook

FOnlineFriendsFacebook::FOnlineFriendsFacebook(FOnlineSubsystemFacebook* InSubsystem) 
{
	// Get our handle to the identity interface
	IdentityInterface = InSubsystem->GetIdentityInterface();
	check( IdentityInterface.IsValid() );
	SharingInterface = InSubsystem->GetSharingInterface();
	check( SharingInterface.IsValid() );
	GConfig->GetArray(TEXT("OnlineSubsystemFacebook.OnlineFriendsFacebook"), TEXT("FriendsFields"), FriendsFields, GEngineIni);	
	
	// always required fields
	FriendsFields.AddUnique(TEXT("name"));
	FriendsFields.AddUnique(TEXT("username"));
}


FOnlineFriendsFacebook::FOnlineFriendsFacebook()
{
}


FOnlineFriendsFacebook::~FOnlineFriendsFacebook()
{
	IdentityInterface = NULL;
	SharingInterface = NULL;
}

bool FOnlineFriendsFacebook::ReadFriendsList(int32 LocalUserNum, const FString& ListName)
{
	bool bRequestTriggered = false;
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::ReadFriendsList()"));

	// To read friends, we need to be logged in and have authorization.
	if( IdentityInterface->GetLoginStatus( LocalUserNum ) == ELoginStatus::LoggedIn )
	{
		bRequestTriggered = true;

		RequestFriendsReadPermissionsDelegate = FOnRequestNewReadPermissionsCompleteDelegate::CreateRaw(this, &FOnlineFriendsFacebook::OnReadFriendsPermissionsUpdated);
		SharingInterface->AddOnRequestNewReadPermissionsCompleteDelegate(LocalUserNum, RequestFriendsReadPermissionsDelegate);
		SharingInterface->RequestNewReadPermissions(LocalUserNum, EOnlineSharingReadCategory::Friends);
	}
	else
	{
		UE_LOG(LogOnline, Verbose, TEXT("Cannot read friends if we are not logged into facebook."));

		// Cannot read friends if we are not logged into facebook.
		TriggerOnReadFriendsListCompleteDelegates( LocalUserNum , false, ListName, TEXT("not logged in.") );
	}

	return bRequestTriggered;
}

bool FOnlineFriendsFacebook::DeleteFriendsList(int32 LocalUserNum, const FString& ListName)
{
	TriggerOnDeleteFriendsListCompleteDelegates(LocalUserNum, false, ListName, FString(TEXT("DeleteFriendsList() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnSendInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("SendInvite() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnAcceptInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("AcceptInvite() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("RejectInvite() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("DeleteFriend() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::GetFriendsList()"));

	for (int32 Idx=0; Idx < CachedFriends.Num(); Idx++)
	{
		OutFriends.Add(CachedFriends[Idx]);
	}

	return true;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsFacebook::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<FOnlineFriend> Result;

	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::GetFriend()"));

	for (int32 Idx=0; Idx < CachedFriends.Num(); Idx++)
	{
		if (*(CachedFriends[Idx]->GetUserId()) == FriendId)
		{
			Result = CachedFriends[Idx];
			break;
		}
	}

	return Result;
}

bool FOnlineFriendsFacebook::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::IsFriend()"));

	TSharedPtr<FOnlineFriend> Friend = GetFriend(LocalUserNum,FriendId,ListName);
	if (Friend.IsValid() &&
		Friend->GetInviteStatus() == EInviteStatus::Accepted)
	{
		return true;
	}
	return false;
}

void FOnlineFriendsFacebook::OnReadFriendsPermissionsUpdated(int32 LocalUserNum, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::OnReadPermissionsUpdated() - %d"), bWasSuccessful);
	SharingInterface->ClearOnRequestNewReadPermissionsCompleteDelegate(LocalUserNum, RequestFriendsReadPermissionsDelegate);

	if( bWasSuccessful )
	{
		ReadFriendsUsingGraphPath(LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default));
	}
	else
	{
		// Permissions werent applied so we cannot read friends.
		TriggerOnReadFriendsListCompleteDelegates(LocalUserNum, false, EFriendsLists::ToString(EFriendsLists::Default), TEXT("no read permissions"));
	}
}

void FOnlineFriendsFacebook::ReadFriendsUsingGraphPath(int32 LocalUserNum, const FString& ListName)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::ReadFriendsUsingGraphPath()"));

	dispatch_async(dispatch_get_main_queue(),^ 
		{
			// We need to determine what the graph path we are querying is.
			NSMutableString* GraphPath = [[NSMutableString alloc] init];
			[GraphPath appendString:@"me/friends"];

			// Optional list of fields to query for each friend
			if(FriendsFields.Num() > 0)
			{
				[GraphPath appendString:@"?fields="];

				FString FieldsStr;
				for (int32 Idx=0; Idx < FriendsFields.Num(); Idx++)
				{
					FieldsStr += FriendsFields[Idx];
					if (Idx < (FriendsFields.Num()-1))
					{
						FieldsStr += TEXT(",");
					}
				}

				[GraphPath appendString:[NSString stringWithCString:TCHAR_TO_ANSI(*FieldsStr) encoding:NSASCIIStringEncoding]];
			}

			UE_LOG(LogOnline, Verbose, TEXT("GraphPath=%s"), ANSI_TO_TCHAR([GraphPath cStringUsingEncoding:NSASCIIStringEncoding]));

			[FBRequestConnection startWithGraphPath:GraphPath parameters:nil HTTPMethod:@"GET"
				completionHandler:^(FBRequestConnection *connection, id result, NSError *error)
				{
					bool bSuccess = error == nil && result;
					UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::startWithGraphPath() - %d"), bSuccess);
						
					if(bSuccess)
					{
						NSArray* friends = [[NSArray alloc] initWithArray:[result objectForKey:@"data"]];

						UE_LOG(LogOnline, Verbose, TEXT("Found %i friends"), [friends count]);

						// Clear our previosly cached friends before we repopulate the cache.
						CachedFriends.Empty();

						for( int32 FriendIdx = 0; FriendIdx < [friends count]; FriendIdx++ )
						{	
							NSDictionary<FBGraphUser>* user = friends[ FriendIdx ];		

							// Add new friend entry to list
							TSharedRef<FOnlineFriendFacebook> FriendEntry(
								new FOnlineFriendFacebook(ANSI_TO_TCHAR([[user objectForKey:@"id"] cStringUsingEncoding:NSASCIIStringEncoding]))
								);
							FriendEntry->AccountData.Add(
								TEXT("id"), ANSI_TO_TCHAR([[user objectForKey:@"id"] cStringUsingEncoding:NSASCIIStringEncoding])
								);
							FriendEntry->AccountData.Add(
								TEXT("name"), ANSI_TO_TCHAR([[user objectForKey:@"name"] cStringUsingEncoding:NSASCIIStringEncoding])
								);
							FriendEntry->AccountData.Add(
								TEXT("username"), ANSI_TO_TCHAR([[user objectForKey:@"username"] cStringUsingEncoding:NSASCIIStringEncoding])
								);
							CachedFriends.Add(FriendEntry);

							UE_LOG(LogOnline, Verbose, TEXT("GCFriend - Id:%s - NickName:%s - RealName:%s"), 
								*FriendEntry->GetUserId()->ToString(), *FriendEntry->GetDisplayName(), *FriendEntry->GetRealName() );	
						}
					}

					// Did this operation complete? Let whoever is listening know.
					TriggerOnReadFriendsListCompleteDelegates( LocalUserNum, bSuccess, ListName, FString() );
				}
			];
		}
	);
}

