// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/Online/OnlineSubsystem/Public/Interfaces/OnlineLeaderboardInterface.h"
#include "LeaderboardQueryCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLeaderboardQueryResult, int32, LeaderboardValue);

UCLASS(MinimalAPI)
class ULeaderboardQueryCallbackProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called when there is a successful leaderboard query
	UPROPERTY(BlueprintAssignable)
	FLeaderboardQueryResult OnSuccess;

	// Called when there is an unsuccessful leaderboard query
	UPROPERTY(BlueprintAssignable)
	FLeaderboardQueryResult OnFailure;

	// Called to perform the query internally
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static ULeaderboardQueryCallbackProxy* CreateProxyObjectForIntQuery(class APlayerController* PlayerController, FName StatName);

public:
	// Begin UObject interface
	virtual void BeginDestroy() OVERRIDE;
	// End UObject interface

private:
	/** Called by the leaderboard system when the read is finished */
	void OnStatsRead_Delayed();
	void OnStatsRead(bool bWasSuccessful);

	/** Unregisters our delegate from the leaderboard system */
	void RemoveDelegate();

	/** Triggers the query for a specifed user; the ReadObject must already be set up */
	void TriggerQuery(class APlayerController* PlayerController, FName InStatName, EOnlineKeyValuePairDataType::Type StatType);

private:
	/** Delegate called when a leaderboard has been successfully read */
	FOnLeaderboardReadCompleteDelegate LeaderboardReadCompleteDelegate;

	/** The leaderboard read request */
	TSharedPtr<class FOnlineLeaderboardRead, ESPMode::ThreadSafe> ReadObject;

	/** Did we fail immediately? */
	bool bFailedToEvenSubmit;

	/** Name of the stat being queried */
	FName StatName;

	// Pointer to the world, needed to delay the results slightly
	TWeakObjectPtr<UWorld> WorldPtr;

	// Did the read succeed?
	bool bSavedWasSuccessful;
	int32 SavedValue;
};
