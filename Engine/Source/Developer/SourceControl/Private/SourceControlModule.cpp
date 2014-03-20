// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "SourceControlModule.h"
#include "SourceControlSettings.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Developer/MessageLog/Public/MessageLogModule.h"
#include "SSourceControlLogin.h"
#include "SSourceControlStatus.h"

#if WITH_EDITOR
	#include "Runtime/Engine/Public/EngineAnalytics.h"
	#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#endif

DEFINE_LOG_CATEGORY(LogSourceControl);

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SourceControl"

namespace SourceControlConstants
{
	/** The maximum number of file/directory status requests we should dispatch in a tick */
	const int32 MaxStatusDispatchesPerTick = 64;

	/** The interval at which we refresh a file's state */
	const FTimespan StateRefreshInterval = FTimespan::FromMinutes(5.0);
}

FSourceControlModule::FSourceControlModule()
	: CurrentSourceControlProvider(NULL)
	, bTemporarilyDisabled(false)
{
}

void FSourceControlModule::StartupModule()
{
	// load our settings
	SourceControlSettings.LoadSettings();

	// Register to check for source control features
	IModularFeatures::Get().OnModularFeatureRegistered().AddRaw(this, &FSourceControlModule::HandleModularFeatureRegistered);

	// bind default provider to editor
	IModularFeatures::Get().RegisterModularFeature( "SourceControl", &DefaultSourceControlProvider );

#if WITH_UNREAL_DEVELOPER_TOOLS
	// create a message log for source control to use
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("SourceControl", LOCTEXT("SourceControlLogLabel", "Source Control"));
#endif
}

void FSourceControlModule::ShutdownModule()
{
	// close the current provider
	GetProvider().Close();

#if WITH_UNREAL_DEVELOPER_TOOLS
	// unregister message log
	if(FModuleManager::Get().IsModuleLoaded("MessageLog"))
	{
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("SourceControl");
	}
#endif

	// unbind default provider from editor
	IModularFeatures::Get().UnregisterModularFeature( "SourceControl", &DefaultSourceControlProvider );

	// we don't care about modular features any more
	IModularFeatures::Get().OnModularFeatureRegistered().RemoveAll(this);
}

void FSourceControlModule::SaveSettings()
{
	SourceControlSettings.SaveSettings();
}

void FSourceControlModule::ShowLoginDialog(const FSourceControlLoginClosed& InOnSourceControlLoginClosed, ELoginWindowMode::Type InLoginWindowMode, EOnLoginWindowStartup::Type InOnLoginWindowStartup)
{
	// Get Active Provider Name
	ActiveProviderName = GetProvider().GetName().ToString();

	// if we are showing a modal version of the dialog & a modeless version already exists, we must destroy the modeless dialog first
	if(InLoginWindowMode == ELoginWindowMode::Modal && SourceControlLoginPtr.IsValid())
	{
		// unhook the delegate so it doesn't fire in this case
		SourceControlLoginWindowPtr->SetOnWindowClosed(FOnWindowClosed());
		SourceControlLoginWindowPtr->RequestDestroyWindow();
		SourceControlLoginWindowPtr = NULL;
		SourceControlLoginPtr = NULL;
	}

	if(SourceControlLoginWindowPtr.IsValid())
	{
		SourceControlLoginWindowPtr->BringToFront();
	}
	else
	{
		// set provider to 'none'.
		// When we open the window, we turn off the fact that source control is available, this solves issues that are present with
		// being a three state modeless system (Accepted settings, disabled, and not yet decided).
		if(InOnLoginWindowStartup == EOnLoginWindowStartup::ResetProviderToNone)
		{
			SetProvider("None");
		}

		// temporarily disable access to source control features
		bTemporarilyDisabled = true;

		// Create the window
		SourceControlLoginWindowPtr = SNew(SWindow)
			.Title( LOCTEXT("SourceControlLoginTitle", "Source Control Login") )
			.SupportsMaximize(false) 
			.SupportsMinimize(false)
			.CreateTitleBar(false)
			.SizingRule( ESizingRule::Autosized )
			.AutoCenter( EAutoCenter::PrimaryWorkArea );

		// Set the closed callback
		SourceControlLoginWindowPtr->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FSourceControlModule::OnSourceControlDialogClosed));

		// Setup the content for the created login window.
		SourceControlLoginWindowPtr->SetContent(
			SAssignNew(SourceControlLoginPtr, SSourceControlLogin)
			.ParentWindow(SourceControlLoginWindowPtr)
			.OnSourceControlLoginClosed(InOnSourceControlLoginClosed)
			);

		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		if(RootWindow.IsValid())
		{
			if(InLoginWindowMode == ELoginWindowMode::Modal)
			{
				FSlateApplication::Get().AddModalWindow(SourceControlLoginWindowPtr.ToSharedRef(), RootWindow);
			}
			else
			{
				FSlateApplication::Get().AddWindowAsNativeChild(SourceControlLoginWindowPtr.ToSharedRef(), RootWindow.ToSharedRef());
			}
		}
		else
		{
			FSlateApplication::Get().AddWindow(SourceControlLoginWindowPtr.ToSharedRef());
		}
	}
}

TSharedPtr<class SWidget> FSourceControlModule::CreateStatusWidget() const
{
	return SNew(SSourceControlStatus);
}

void FSourceControlModule::OnSourceControlDialogClosed(const TSharedRef<SWindow>& InWindow)
{
	SourceControlLoginWindowPtr = NULL;
	SourceControlLoginPtr = NULL;
	bTemporarilyDisabled = false;

#if WITH_EDITOR
	FString NewProvider = CurrentSourceControlProvider->GetName().ToString();
	if( FEngineAnalytics::IsAvailable() && !ActiveProviderName.Equals( NewProvider, ESearchCase::IgnoreCase ))
	{
		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.Usage.SourceControl" ), TEXT( "Provider" ), NewProvider );
		ActiveProviderName = NewProvider;
	}
#endif
}

void FSourceControlModule::RefreshSourceControlProviders() const
{
	TArray<FName> ModuleNames;
	FModuleManager::Get().FindModules(TEXT("*SourceControl"), ModuleNames);
	ModuleNames.Remove(FName(TEXT("SourceControl")));

	for(int32 ModuleIndex = 0; ModuleIndex < ModuleNames.Num(); ModuleIndex++)
	{
		FModuleManager::Get().LoadModule(ModuleNames[ModuleIndex]);
	}
}

void FSourceControlModule::InitializeSourceControlProviders()
{
	// Look for valid SourceControl modules - they will register themselves as editor features
	RefreshSourceControlProviders();

	int32 SourceControlCount = IModularFeatures::Get().GetModularFeatureImplementationCount("SourceControl");
	if( SourceControlCount > 0 )
	{
		FString PreferredSourceControlProvider = SourceControlSettings.GetProvider();
		TArray<ISourceControlProvider*> Providers = IModularFeatures::Get().GetModularFeatureImplementations<ISourceControlProvider>("SourceControl");
		for(auto It(Providers.CreateIterator()); It; It++)
		{ 
			ISourceControlProvider* Provider = *It;
			if(PreferredSourceControlProvider == Provider->GetName().ToString())
			{
				CurrentSourceControlProvider = Provider;
				break;
			}
		}

		// no provider found of this name, default to the first one
		if( CurrentSourceControlProvider == NULL )
		{
			CurrentSourceControlProvider = &DefaultSourceControlProvider;
		}
	}

	check(CurrentSourceControlProvider);

	CurrentSourceControlProvider->Init(false);	// Don't force a connection here, as its synchronous. Let the user establish a connection.
}

void FSourceControlModule::Tick()
{	
	ISourceControlProvider& Provider = GetProvider();

	// tick the provider, so any operation results can be read back
	Provider.Tick();
	
	// don't allow background status updates when temporarily disabled for login
	if(!bTemporarilyDisabled)
	{
		// check for any pending dispatches
		if(PendingStatusUpdateFiles.Num() > 0)
		{
			// grab a batch of files
			TArray<FString> FilesToDispatch;
			for(auto Iter(PendingStatusUpdateFiles.CreateConstIterator()); Iter; Iter++)
			{
				if(FilesToDispatch.Num() >= SourceControlConstants::MaxStatusDispatchesPerTick)
				{
					break;
				}
				FilesToDispatch.Add(*Iter);
			}

			if(FilesToDispatch.Num() > 0)
			{
				// remove the files we are dispatching so we don't try again
				PendingStatusUpdateFiles.RemoveAt(0, FilesToDispatch.Num());

				// dispatch update
				Provider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), FilesToDispatch, EConcurrency::Asynchronous);
			}
		}
	}
}

void FSourceControlModule::QueueStatusUpdate(const TArray<UPackage*>& InPackages)
{
	if(IsEnabled())
	{
		for(auto It(InPackages.CreateConstIterator()); It; It++)
		{
			QueueStatusUpdate(*It);
		}
	}
}

void FSourceControlModule::QueueStatusUpdate(const TArray<FString>& InFilenames)
{
	if(IsEnabled())
	{
		for(auto It(InFilenames.CreateConstIterator()); It; It++)
		{
			QueueStatusUpdate(*It);
		}
	}
}

void FSourceControlModule::QueueStatusUpdate(UPackage* InPackage)
{
	if(IsEnabled())
	{
		QueueStatusUpdate(SourceControlHelpers::PackageFilename(InPackage));
	}
}

void FSourceControlModule::QueueStatusUpdate(const FString& InFilename)
{
	if(IsEnabled())
	{
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = GetProvider().GetState(InFilename, EStateCacheUsage::Use);
		FTimespan TimeSinceLastUpdate = FDateTime::Now() - SourceControlState->GetTimeStamp();
		if(TimeSinceLastUpdate > SourceControlConstants::StateRefreshInterval)
		{
			PendingStatusUpdateFiles.AddUnique(InFilename);
		}
	}
}

bool FSourceControlModule::IsEnabled() const
{
	return !bTemporarilyDisabled && GetProvider().IsEnabled();
}

ISourceControlProvider& FSourceControlModule::GetProvider() const
{
	return *CurrentSourceControlProvider;
}

void FSourceControlModule::SetProvider( const FName& InName )
{
	TArray<ISourceControlProvider*> Providers = IModularFeatures::Get().GetModularFeatureImplementations<ISourceControlProvider>("SourceControl");
	for(auto It(Providers.CreateIterator()); It; It++)
	{
		ISourceControlProvider* Provider = *It;
		if(InName == Provider->GetName())
		{
			SetCurrentSourceControlProvider(*Provider);
			return;
		}
	}

	UE_LOG(LogSourceControl, Fatal, TEXT("Tried to set unknown source control provider: %s"), *InName.ToString());
}

void FSourceControlModule::ClearCurrentSourceControlProvider()
{
	if( CurrentSourceControlProvider != NULL )
	{
		CurrentSourceControlProvider->Close();
		CurrentSourceControlProvider = &DefaultSourceControlProvider;
	}
}

int32 FSourceControlModule::GetNumSourceControlProviders()
{
	return IModularFeatures::Get().GetModularFeatureImplementationCount("SourceControl");
}

void FSourceControlModule::SetCurrentSourceControlProvider(int32 ProviderIndex)
{
	TArray<ISourceControlProvider*> Providers = IModularFeatures::Get().GetModularFeatureImplementations<ISourceControlProvider>("SourceControl");
	check(Providers.IsValidIndex(ProviderIndex));
	SetCurrentSourceControlProvider(*Providers[ProviderIndex]);
}

void FSourceControlModule::SetCurrentSourceControlProvider(ISourceControlProvider& InProvider)
{
	// see if we are switching or not
	if(&InProvider == CurrentSourceControlProvider)
	{
		return;
	}

	ClearCurrentSourceControlProvider();

	CurrentSourceControlProvider = &InProvider;
	CurrentSourceControlProvider->Init(false);	// Don't force a connection here, as its synchronous. Let the user establish a connection.

	SourceControlSettings.SetProvider(CurrentSourceControlProvider->GetName().ToString());

	SaveSettings();
}

FName FSourceControlModule::GetSourceControlProviderName(int32 ProviderIndex)
{
	TArray<ISourceControlProvider*> Providers = IModularFeatures::Get().GetModularFeatureImplementations<ISourceControlProvider>("SourceControl");
	check(Providers.IsValidIndex(ProviderIndex));
	return Providers[ProviderIndex]->GetName();
}

TSharedPtr<class SSourceControlLogin> FSourceControlModule::GetLoginWidget() const
{
	return SourceControlLoginPtr;
}

void FSourceControlModule::HandleModularFeatureRegistered(const FName& Type)
{
	if(Type == TEXT("SourceControl"))
	{
		InitializeSourceControlProviders();
	}
}

bool FSourceControlModule::GetUseGlobalSettings() const
{
	return SourceControlSettings.GetUseGlobalSettings();
}

void FSourceControlModule::SetUseGlobalSettings(bool bIsUseGlobalSettings)
{
	SourceControlSettings.SetUseGlobalSettings(bIsUseGlobalSettings);
	
	// force the user to re-log in
	ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
}	

IMPLEMENT_MODULE( FSourceControlModule, SourceControl );

#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
