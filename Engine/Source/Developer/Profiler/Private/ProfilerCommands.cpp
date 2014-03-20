// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "FProfilerCommands"

/*-----------------------------------------------------------------------------
	FProfilerCommands
-----------------------------------------------------------------------------*/

FProfilerCommands::FProfilerCommands() : TCommands<FProfilerCommands>
(
	TEXT( "ProfilerCommand" ), // Context name for fast lookup
	NSLOCTEXT("Contexts", "ProfilerCommand", "Profiler Command"), // Localized context name for displaying
	NAME_None, // Parent
	FEditorStyle::GetStyleSetName() // Icon Style Set
)
{}

/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FProfilerCommands::RegisterCommands()
{
	/*-----------------------------------------------------------------------------
		Global and custom commands.	
	-----------------------------------------------------------------------------*/

	UI_COMMAND( ToggleDataPreview, 	"Data Preview", "Toggles the data preview", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control, EKeys::R ) );
	UI_COMMAND( ToggleDataCapture, "Data Capture", "Toggles the data capture", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control, EKeys::C ) );
	UI_COMMAND( ToggleShowDataGraph, "Show Data Graph", "Toggles showing all data graphs", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( OpenEventGraph, "Open Event Graph", "Opens a new event graph", EUserInterfaceActionType::Button, FInputGesture() );

	/*-----------------------------------------------------------------------------
		Global commands.
	-----------------------------------------------------------------------------*/

	UI_COMMAND( Save, "Save", "Saves all collected data to file or files", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Control, EKeys::S ) );
	UI_COMMAND( StatsProfiler, "Statistics", "Enables the Stats Profiler", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control, EKeys::P ) );
	UI_COMMAND( MemoryProfiler, "Memory", "Enables the Memory Profiler", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control, EKeys::M ) );
	UI_COMMAND( FPSChart, "FPS Chart", "Shows the FPS Chart", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control, EKeys::H ) );

	UI_COMMAND( OpenSettings, "Settings", "Opens the settings for the profiler", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Control, EKeys::O ) );

	UI_COMMAND( ProfilerManager_Load, "Load", "Loads profiler data", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Control, EKeys::L ) );
	UI_COMMAND( ProfilerManager_ToggleLivePreview, "Live preview", "Toggles the real time live preview", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( DataGraph_ToggleViewMode, "Toggle graph view mode", "Toggles the data graph view mode between time based and index based", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( DataGraph_ToggleMultiMode, "Toggle graph multi mode", "Toggles the data graph multi mode between displaying area line graph and one line graph for each graph", EUserInterfaceActionType::Button, FInputGesture() );

	UI_COMMAND( DataGraph_ViewMode_SetTimeBased, "Time based", "Sets the data graph view mode to the time based", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( DataGraph_ViewMode_SetIndexBased, "Index based", "Sets the data graph view mode to the index based", EUserInterfaceActionType::RadioButton, FInputGesture() );

	UI_COMMAND( DataGraph_MultiMode_SetCombined, "Combined", "Set the data graph multi mode to the displaying area line graph", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( DataGraph_MultiMode_SetOneLinePerDataSource, "One line per data source", "Set the data graph multi mode to the displaying one line graph for each graph data source", EUserInterfaceActionType::RadioButton, FInputGesture() );

	UI_COMMAND( EventGraph_SelectAllFrames, "Select all frames", "Selects all frames in the data graph and displays them in the event graph", EUserInterfaceActionType::Button, FInputGesture() );
}
PRAGMA_ENABLE_OPTIMIZATION

//checkAtCompileTime(sizeof(FProfilerActionManager) == 0, FProfilerActionManager_CannotContainAnyVariablesAtThisMoment);

/*-----------------------------------------------------------------------------
	FProfilerMenuBuilder
-----------------------------------------------------------------------------*/

void FProfilerMenuBuilder::AddMenuEntry( FMenuBuilder& MenuBuilder, const TSharedPtr< FUICommandInfo >& FUICommandInfo, const FUIAction& UIAction )
{
	MenuBuilder.AddMenuEntry
	( 
		FUICommandInfo->GetLabel(),
		FUICommandInfo->GetDescription(),
		FUICommandInfo->GetIcon(),
		UIAction,
		NAME_None, 
		FUICommandInfo->GetUserInterfaceType()
	);
}

/*

// One session instance
if( SessionInstanceID.IsValid() )
{
}
// All session instances
else
{
}

}*/

/*-----------------------------------------------------------------------------
	ToggleDataPreview
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ToggleDataPreview_Global()
{
	This->CommandList->MapAction( This->GetCommands().ToggleDataPreview, ToggleDataPreview_Custom( FGuid() ) );
}

const FUIAction FProfilerActionManager::ToggleDataPreview_Custom( const FGuid SessionInstanceID ) const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataPreview_Execute, SessionInstanceID );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataPreview_CanExecute, SessionInstanceID );
	UIAction.IsCheckedDelegate = FIsActionChecked::CreateRaw( this, &FProfilerActionManager::ToggleDataPreview_IsChecked, SessionInstanceID );
	UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
	return UIAction;
}

void FProfilerActionManager::ToggleDataPreview_Execute( const FGuid SessionInstanceID )
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const FProfilerSessionRef* ProfilerSessionPtr = This->FindSessionInstance( SessionInstanceID );
		if( ProfilerSessionPtr )
		{
			(*ProfilerSessionPtr)->bDataPreviewing = !(*ProfilerSessionPtr)->bDataPreviewing;
			This->ProfilerClient->SetPreviewState( (*ProfilerSessionPtr)->bDataPreviewing, SessionInstanceID );
		}

		const bool bDataPreviewing = This->IsDataPreviewing();

		if( !bDataPreviewing )
		{
			This->bLivePreview = false;
		}
	}
	// All session instances
	else
	{
		const bool bDataPreviewing = !This->IsDataPreviewing();
		This->SetDataPreview( bDataPreviewing );

		if( !bDataPreviewing )
		{
			This->bLivePreview = false;
		}
	}
}

bool FProfilerActionManager::ToggleDataPreview_CanExecute( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const bool bIsSessionInstanceValid = This->IsSessionInstanceValid( SessionInstanceID ) && This->ProfilerType == EProfilerSessionTypes::Live;
		return bIsSessionInstanceValid;
	}
	// All session instances
	else
	{
		const bool bCanExecute = This->ActiveSession.IsValid() && This->ProfilerType == EProfilerSessionTypes::Live && This->GetProfilerInstancesNum() > 0;
		return bCanExecute;
	}
}

bool FProfilerActionManager::ToggleDataPreview_IsChecked( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const FProfilerSessionRef* ProfilerSessionPtr = This->FindSessionInstance( SessionInstanceID );
		if( ProfilerSessionPtr )
		{
			return (*ProfilerSessionPtr)->bDataPreviewing;
		}
		else
		{
			return false;
		}
	}
	// All session instances
	else
	{
		const bool bDataPreview = This->IsDataPreviewing();
		return bDataPreview;
	}
}

/*-----------------------------------------------------------------------------
	ProfilerManager_ToggleLivePreview
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ProfilerManager_ToggleLivePreview_Global()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_ToggleLivePreview_Execute );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_ToggleLivePreview_CanExecute );
	UIAction.IsCheckedDelegate = FIsActionChecked::CreateRaw( this, &FProfilerActionManager::ProfilerManager_ToggleLivePreview_IsChecked );
	UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();

	This->CommandList->MapAction( This->GetCommands().ProfilerManager_ToggleLivePreview, UIAction );
}

void FProfilerActionManager::ProfilerManager_ToggleLivePreview_Execute()
{
	This->bLivePreview = !This->bLivePreview;
}

bool FProfilerActionManager::ProfilerManager_ToggleLivePreview_CanExecute() const
{
	const bool bCanExecute = This->ActiveSession.IsValid() && This->ProfilerType == EProfilerSessionTypes::Live && This->GetNumDataPreviewingInstances() > 0;
	return bCanExecute;
}

bool FProfilerActionManager::ProfilerManager_ToggleLivePreview_IsChecked() const
{
	return This->bLivePreview;
}

/*-----------------------------------------------------------------------------
	ToggleShowDataGraph
-----------------------------------------------------------------------------*/

const FUIAction FProfilerActionManager::ToggleShowDataGraph_Custom( const FGuid SessionInstanceID ) const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleShowDataGraph_Execute, SessionInstanceID );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleShowDataGraph_CanExecute, SessionInstanceID );
	UIAction.IsCheckedDelegate = FIsActionChecked::CreateRaw( this, &FProfilerActionManager::ToggleShowDataGraph_IsChecked, SessionInstanceID );
	UIAction.IsActionVisibleDelegate = FIsActionButtonVisible::CreateRaw( this, &FProfilerActionManager::ToggleShowDataGraph_IsActionButtonVisible, SessionInstanceID );
	return UIAction;
}

void FProfilerActionManager::ToggleShowDataGraph_Execute( const FGuid SessionInstanceID )
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const bool bIsSessionInstanceTracked = This->IsSessionInstanceTracked( SessionInstanceID );

		if( bIsSessionInstanceTracked )
		{
			This->UntrackSessionInstance( SessionInstanceID );
		}
		else
		{
			This->TrackSessionInstance( SessionInstanceID );
		}
	}
}

bool FProfilerActionManager::ToggleShowDataGraph_CanExecute( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const bool bIsSessionInstanceValid = This->IsSessionInstanceValid( SessionInstanceID );
		return bIsSessionInstanceValid;
	}
	// All session instances
	else
	{
		return false;
	}
}

bool FProfilerActionManager::ToggleShowDataGraph_IsChecked( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const bool bIsSessionInstanceTracked = This->IsSessionInstanceTracked( SessionInstanceID );
		return bIsSessionInstanceTracked;
	}
	// All session instances
	else
	{
		return false;
	}
}

bool FProfilerActionManager::ToggleShowDataGraph_IsActionButtonVisible( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		return true;
	}
	// All session instances
	else
	{
		// Hiding all graphs for all session instances as it doesn't add any useful functionality
		return false;
	}
}

/*-----------------------------------------------------------------------------
	ProfilerManager_Load
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ProfilerManager_Load()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_Load_Execute );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_Load_CanExecute );
	UIAction.IsCheckedDelegate = FIsActionChecked();
	UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();

	This->CommandList->MapAction( This->GetCommands().ProfilerManager_Load, UIAction );
}

void FProfilerActionManager::ProfilerManager_Load_Execute()
{
	TArray<FString> OutFiles;
	const FString ProfilingDirectory = *FPaths::ConvertRelativePathToFull( *FPaths::ProfilingDir() );

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if( DesktopPlatform != NULL )
	{
		bOpened = DesktopPlatform->OpenFileDialog
		(
			NULL, 
			LOCTEXT("ProfilerManager_Load_Desc", "Open profiler capture file...").ToString(),
			ProfilingDirectory, 
			TEXT(""), 
			LOCTEXT("ProfilerManager_Load_FileFilter", "Capture Files (*.ue4stats)|*.ue4stats").ToString(), 
			FProfilerManager::GetSettings().bSingleInstanceMode ? EFileDialogFlags::None : EFileDialogFlags::Multiple, 
			OutFiles
		);
	}

	if( bOpened == true )
	{
		if( OutFiles.Num() > 0 )
		{
			for( int32 FileIndex = 0; FileIndex < OutFiles.Num(); FileIndex++ )
			{
				const FString ProfilerCaptureFilepath = OutFiles[FileIndex];
				This->LoadProfilerCapture( ProfilerCaptureFilepath, FileIndex == 0 ? false : true );
			}
		}
	}
}

bool FProfilerActionManager::ProfilerManager_Load_CanExecute() const
{
	const bool bIsConnectionActive = This->GetNumDataCapturingInstances() > 0 || This->GetNumDataPreviewingInstances() > 0 || This->bLivePreview;
	return !(bIsConnectionActive && This->ProfilerType == EProfilerSessionTypes::Live);
}

/*-----------------------------------------------------------------------------
	ToggleDataCapture
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ToggleDataCapture_Global()
{
	This->CommandList->MapAction( This->GetCommands().ToggleDataCapture, ToggleDataCapture_Custom( FGuid() ) );
}

const FUIAction FProfilerActionManager::ToggleDataCapture_Custom( const FGuid SessionInstanceID ) const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataCapture_Execute, SessionInstanceID );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataCapture_CanExecute, SessionInstanceID );
	UIAction.IsCheckedDelegate = FIsActionChecked::CreateRaw( this, &FProfilerActionManager::ToggleDataCapture_IsChecked, SessionInstanceID );
	UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
	return UIAction;
}

void FProfilerActionManager::ToggleDataCapture_Execute( const FGuid SessionInstanceID )
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const FProfilerSessionRef* ProfilerSessionPtr = This->FindSessionInstance( SessionInstanceID );
		if( ProfilerSessionPtr )
		{
			(*ProfilerSessionPtr)->bDataCapturing = !(*ProfilerSessionPtr)->bDataCapturing;
			This->ProfilerClient->SetCaptureState( (*ProfilerSessionPtr)->bDataCapturing, SessionInstanceID );
		}	
	}
	// All session instances
	else
	{
		const bool bDataCapturing = This->IsDataCapturing();
		This->SetDataCapture( !bDataCapturing );
	}

	// Assumes that when data capturing is off, we have captured stats files on the service side.
	const bool bDataCapturing = This->IsDataCapturing();
	if( !bDataCapturing )
	{
		EAppReturnType::Type Result = FPlatformMisc::MessageBoxExt
		( 
			EAppMsgType::YesNo, 
			*LOCTEXT("TransferServiceSideCaptureQuestion", "Would like to transfer the captured stats file(s) to this machine? This may take some time.").ToString(), 
			*LOCTEXT("Question", "Question").ToString() 
		);

		if( Result == EAppReturnType::Yes )
		{
			This->ProfilerClient->RequestLastCapturedFile();
		}
	}
}

bool FProfilerActionManager::ToggleDataCapture_CanExecute( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const bool bIsSessionInstanceValid = This->IsSessionInstanceValid( SessionInstanceID ) && This->ProfilerType == EProfilerSessionTypes::Live;
		return bIsSessionInstanceValid;
	}
	// All session instances
	else
	{
		const bool bCanExecute = This->ActiveSession.IsValid() && This->ProfilerType == EProfilerSessionTypes::Live && This->GetProfilerInstancesNum() > 0;
		return bCanExecute;
	}

	return false;
}

bool FProfilerActionManager::ToggleDataCapture_IsChecked( const FGuid SessionInstanceID ) const
{
	// One session instance
	if( SessionInstanceID.IsValid() )
	{
		const FProfilerSessionRef* ProfilerSessionPtr = This->FindSessionInstance( SessionInstanceID );
		if( ProfilerSessionPtr )
		{
			return (*ProfilerSessionPtr)->bDataCapturing;
		}
		else
		{
			return false;
		}
	}
	// All session instances
	else
	{
		const bool bDataCapturing = This->IsDataCapturing();
		return bDataCapturing;
	}

	return false;
}

/*-----------------------------------------------------------------------------
		OpenSettings
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_OpenSettings_Global()
{
	This->CommandList->MapAction( This->GetCommands().OpenSettings, OpenSettings_Custom() );
}

const FUIAction FProfilerActionManager::OpenSettings_Custom() const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::OpenSettings_Execute );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::OpenSettings_CanExecute );
	UIAction.IsCheckedDelegate = FIsActionChecked();
	UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
	return UIAction;
}

void FProfilerActionManager::OpenSettings_Execute()
{
	This->GetProfilerWindow()->OpenProfilerSettings();
}

bool FProfilerActionManager::OpenSettings_CanExecute() const
{
	return !This->Settings.IsEditing();
}

#undef LOCTEXT_NAMESPACE