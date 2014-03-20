// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "DefaultSourceControlProvider.h"
#include "ISourceControlState.h"
#include "ISourceControlModule.h"
#include "ISourceControlLabel.h"
#include "MessageLog.h"

#define LOCTEXT_NAMESPACE "DefaultSourceControlProvider"

void FDefaultSourceControlProvider::Init(bool bForceConnection)
{
	FMessageLog("SourceControl").Info(LOCTEXT("SourceControlDisabled", "Source control is disabled"));
}

void FDefaultSourceControlProvider::Close()
{

}

FString FDefaultSourceControlProvider::GetStatusText() const
{
	return LOCTEXT("SourceControlDisabled", "Source control is disabled").ToString();
}

bool FDefaultSourceControlProvider::IsAvailable() const
{
	return false;
}

bool FDefaultSourceControlProvider::IsEnabled() const
{
	return false;
}

const FName& FDefaultSourceControlProvider::GetName(void) const
{
	static FName ProviderName("None"); 
	return ProviderName; 
}

ECommandResult::Type FDefaultSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	return ECommandResult::Failed;
}

void FDefaultSourceControlProvider::RegisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{

}

void FDefaultSourceControlProvider::UnregisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{

}

ECommandResult::Type FDefaultSourceControlProvider::Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	return ECommandResult::Failed;
}

bool FDefaultSourceControlProvider::CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const
{
	return false;
}

void FDefaultSourceControlProvider::CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation )
{
}

bool FDefaultSourceControlProvider::UsesLocalReadOnlyState() const
{
	return false;
}

void FDefaultSourceControlProvider::Tick()
{

}

TArray< TSharedRef<ISourceControlLabel> > FDefaultSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	return TArray< TSharedRef<ISourceControlLabel> >();
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FDefaultSourceControlProvider::MakeSettingsWidget() const
{
	return SNullWidget::NullWidget;
}
#endif // SOURCE_CONTROL_WITH_SLATE

#undef LOCTEXT_NAMESPACE