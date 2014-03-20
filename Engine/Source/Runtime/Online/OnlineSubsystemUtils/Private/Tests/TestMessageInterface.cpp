// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "OnlineIdentityInterface.h"
#include "OnlineMessageInterface.h"
#include "ModuleManager.h"
#include "TestMessageInterface.h"


FTestMessageInterface::FTestMessageInterface(const FString& InSubsystem)
	: OnlineSub(NULL)
	, bEnumerateMessages(true)
	, bReadMessages(true)
	, bSendMessages(true)
	, bDeleteMessages(false)
{
	UE_LOG(LogOnline, Display, TEXT("FTestMessageInterface::FTestMessageInterface"));
	SubsystemName = InSubsystem;
}


FTestMessageInterface::~FTestMessageInterface()
{
	UE_LOG(LogOnline, Display, TEXT("FTestMessageInterface::~FTestMessageInterface"));
}

void FTestMessageInterface::Test(const TArray<FString>& InRecipients)
{
	UE_LOG(LogOnline, Display, TEXT("FTestMessageInterface::Test"));

	OnlineSub = IOnlineSubsystem::Get(SubsystemName.Len() ? FName(*SubsystemName, FNAME_Find) : NAME_None);
	if (OnlineSub != NULL &&
		OnlineSub->GetIdentityInterface().IsValid() &&
		OnlineSub->GetMessageInterface().IsValid())
	{
		// Add our delegate for the async call
		OnEnumerateMessagesCompleteDelegate = FOnEnumerateMessagesCompleteDelegate::CreateRaw(this, &FTestMessageInterface::OnEnumerateMessagesComplete);
		OnReadMessageCompleteDelegate = FOnReadMessageCompleteDelegate::CreateRaw(this, &FTestMessageInterface::OnReadMessageComplete);
		OnSendMessageCompleteDelegate = FOnSendMessageCompleteDelegate::CreateRaw(this, &FTestMessageInterface::OnSendMessageComplete);
		OnDeleteMessageCompleteDelegate = FOnDeleteMessageCompleteDelegate::CreateRaw(this, &FTestMessageInterface::OnDeleteMessageComplete);

		OnlineSub->GetMessageInterface()->AddOnEnumerateMessagesCompleteDelegate(0, OnEnumerateMessagesCompleteDelegate);
		OnlineSub->GetMessageInterface()->AddOnReadMessageCompleteDelegate(0, OnReadMessageCompleteDelegate);
		OnlineSub->GetMessageInterface()->AddOnSendMessageCompleteDelegate(0, OnSendMessageCompleteDelegate);
		OnlineSub->GetMessageInterface()->AddOnDeleteMessageCompleteDelegate(0, OnDeleteMessageCompleteDelegate);

		// list of users to send messages to
		for (int32 Idx=0; Idx < InRecipients.Num(); Idx++)
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineSub->GetIdentityInterface()->CreateUniquePlayerId(InRecipients[Idx]);
			if (UserId.IsValid())
			{
				Recipients.Add(UserId.ToSharedRef());
			}
		}

		// kick off next test
		StartNextTest();
	}
	else
	{
		UE_LOG(LogOnline, Warning,
			TEXT("Failed to get message interface for %s"), *SubsystemName);
		
		FinishTest();
	}
}

void FTestMessageInterface::StartNextTest()
{
	if (bEnumerateMessages)
	{
		OnlineSub->GetMessageInterface()->EnumerateMessages(0);
	}
	else if (bReadMessages && MessagesToRead.Num() > 0)
	{
		OnlineSub->GetMessageInterface()->ReadMessage(0, *MessagesToRead[0]);
	}
	else if (bSendMessages && Recipients.Num() > 0)
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			FOnlineMessagePayload TestPayload;

			TestPayload.SetAttribute(TEXT("INTValue"), FVariantData(512)); 
 			TestPayload.SetAttribute(TEXT("FLOATValue"), FVariantData(512.0f)); 
 			TestPayload.SetAttribute(TEXT("QWORDValue"), FVariantData((uint64)512)); 
 			TestPayload.SetAttribute(TEXT("DOUBLEValue"), FVariantData(512000.0)); 
 			TestPayload.SetAttribute(TEXT("STRINGValue"), FVariantData(TEXT("This Is A Test!"))); 

			TArray<uint8> TestData;
			TestData.Add((uint8)200);
 			TestPayload.SetAttribute(TEXT("BLOBValue"), FVariantData(TestData));

			OnlineSub->GetMessageInterface()->SendMessage(0, Recipients, TEXT("TestType"), TestPayload);
		}
		else
		{
			bSendMessages = false;
			StartNextTest();
		}
	}
	else if (bDeleteMessages && MessagesToDelete.Num() > 0)
	{
		OnlineSub->GetMessageInterface()->DeleteMessage(0, *MessagesToDelete[0]);
	}
	else
	{
		FinishTest();
	}
}

void FTestMessageInterface::FinishTest()
{
	if (OnlineSub != NULL &&
		OnlineSub->GetMessageInterface().IsValid())
	{
		// Clear delegates for the various async calls
		OnlineSub->GetMessageInterface()->ClearOnEnumerateMessagesCompleteDelegate(0, OnEnumerateMessagesCompleteDelegate);
		OnlineSub->GetMessageInterface()->ClearOnReadMessageCompleteDelegate(0, OnReadMessageCompleteDelegate);
		OnlineSub->GetMessageInterface()->ClearOnSendMessageCompleteDelegate(0, OnSendMessageCompleteDelegate);
		OnlineSub->GetMessageInterface()->ClearOnDeleteMessageCompleteDelegate(0, OnDeleteMessageCompleteDelegate);
	}
	delete this;
}

void FTestMessageInterface::OnEnumerateMessagesComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("EnumerateMessages() for player (%d) was success=%d"), LocalPlayer, bWasSuccessful);

	if (bWasSuccessful)
	{
		TArray< TSharedRef<FOnlineMessageHeader> > MessageHeaders;
		if (OnlineSub->GetMessageInterface()->GetMessageHeaders(LocalPlayer, MessageHeaders))
		{
			UE_LOG(LogOnline, Log,
				TEXT("GetMessageHeaders(%d) returned %d message headers"), LocalPlayer, MessageHeaders.Num());
			
			// Clear old entries
			MessagesToRead.Empty();
			MessagesToDelete.Empty();

			// Log each message header data out
			for (int32 Index = 0; Index < MessageHeaders.Num(); Index++)
			{
				const FOnlineMessageHeader& Header = *MessageHeaders[Index];
				UE_LOG(LogOnline, Log,
					TEXT("\t message id (%s)"), *Header.MessageId->ToDebugString());
				UE_LOG(LogOnline, Log,
					TEXT("\t\t from user id (%s)"), *Header.FromUserId->ToDebugString());
				UE_LOG(LogOnline, Log,
					TEXT("\t\t from name: %s"), *Header.FromName);
				UE_LOG(LogOnline, Log,
					TEXT("\t\t type (%s)"), *Header.Type);
				UE_LOG(LogOnline, Log,
					TEXT("\t\t time stamp (%s)"), *Header.TimeStamp);

				// Add to list of messages to download
				MessagesToRead.AddUnique(Header.MessageId);
				// Add to list of messages to delete
				MessagesToDelete.AddUnique(Header.MessageId);
			}
		}	
		else
		{
			UE_LOG(LogOnline, Log,
				TEXT("GetMessageHeaders(%d) failed"), LocalPlayer);
		}
	}
	// done with this part of the test
	bEnumerateMessages = false;
	// kick off next test
	StartNextTest();
}

void FTestMessageInterface::OnReadMessageComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueMessageId& MessageId, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("ReadMessage() for player (%d) was success=%d"), LocalPlayer, bWasSuccessful);

	// done with this part of the test if no more messages to download
	MessagesToRead.RemoveAt(0);
	if (MessagesToRead.Num() == 0)
	{
		bReadMessages = false;
	}
	// kick off next test
	StartNextTest();
}

void FTestMessageInterface::OnSendMessageComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("SendMessage() for player (%d) was success=%d"), LocalPlayer, bWasSuccessful);

	// done with this part of the test
	bSendMessages = false;
	bEnumerateMessages = true;
	// kick off next test
	StartNextTest();
}

void FTestMessageInterface::OnDeleteMessageComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueMessageId& MessageId, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("DeleteMessage() for player (%d) was success=%d"), LocalPlayer, bWasSuccessful);

	// done with this part of the test if no more messages to delete
	MessagesToDelete.RemoveAt(0);
	if (MessagesToDelete.Num() == 0)
	{
		bDeleteMessages = false;
	}
	// kick off next test
	StartNextTest();
}