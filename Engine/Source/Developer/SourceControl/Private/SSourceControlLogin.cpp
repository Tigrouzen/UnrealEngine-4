// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "SSourceControlLogin.h"
#include "SourceControlModule.h"
#include "SSourceControlPicker.h"
#include "MessageLog.h"

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlLogin"

class SSourceControlTitleBar : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SSourceControlTitleBar) {}

	/** A reference to the parent window */
	SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ParentWindowPtr = InArgs._ParentWindow;

		SBorder::Construct(SBorder::FArguments()
			.BorderImage(FEditorStyle::GetBrush("Window.Title.Active"))
			[
				SNew(SHorizontalBox)
				.Visibility( EVisibility::HitTestInvisible )
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( LOCTEXT("SourceControlLoginTitle", "Source Control Login") )
					.TextStyle(FEditorStyle::Get(), "Window.TitleText")
					.Visibility( EVisibility::HitTestInvisible )
				]
			]
		);
	}

	virtual EWindowZone::Type GetWindowZoneOverride() const OVERRIDE
	{
		return EWindowZone::TitleBar;
	}

private:
	/** The parent window of this widget */
	TWeakPtr<SWindow> ParentWindowPtr;
};

void SSourceControlLogin::Construct(const FArguments& InArgs)
{
	ParentWindowPtr = InArgs._ParentWindow;
	SourceControlLoginClosed = InArgs._OnSourceControlLoginClosed;

	ConnectionState = ELoginConnectionState::Disconnected;

	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();

#if WITH_UNREAL_DEVELOPER_TOOLS
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	TSharedRef<class IMessageLogListing> MessageLogListing = MessageLogModule.GetLogListing("SourceControl");
#endif

	ChildSlot
	[
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.BorderImage( FEditorStyle::GetBrush("ChildWindow.Background") )
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(SSourceControlTitleBar)
					.ParentWindow(InArgs._ParentWindow)
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f, 4.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SBox)
					.WidthOverride(500)
					[
						SNew(SSourceControlPicker)
						.IsEnabled( this, &SSourceControlLogin::AreControlsEnabled )
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SAssignNew(SettingsBorder, SBorder)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.Visibility(this, &SSourceControlLogin::GetSettingsVisibility)
					.IsEnabled(this, &SSourceControlLogin::AreControlsEnabled)
					.Padding(0.0f)
					[
						SourceControlModule.GetProvider().MakeSettingsWidget()
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.Visibility(this, &SSourceControlLogin::GetDisabledTextVisibility)
					.Padding(FMargin(0.0f, 12.0f))
					[
						SNew(STextBlock)
						.WrapTextAt(500.0f)
						.Text(LOCTEXT("SourceControlDisabledText", "Source control is currently disabled.\nTo enable, select a provider from the drop-down box above and enter your credentials.\nYou can re-enable source control by clicking on the icon in the top-right corner of the editor."))
					]
				]
			]
#if WITH_UNREAL_DEVELOPER_TOOLS
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SExpandableArea)
				.AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font")))
				.AreaTitle(LOCTEXT("LogTitle", "Source Control Log"))
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.IsEnabled( this, &SSourceControlLogin::AreControlsEnabled )
				.InitiallyCollapsed(true)
				.BodyContent()
				[
					SNew(SBox)
					.HeightOverride(250)
					.WidthOverride(400)
					[
						SNew(SBorder)
						[
							MessageLogModule.CreateLogListingWidget(MessageLogListing)
						]
					]
				]
			]
#endif
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SThrobber)
					.Visibility(this, &SSourceControlLogin::GetThrobberVisibility)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("AcceptSettings", "Accept Settings"))
						.OnClicked( this, &SSourceControlLogin::OnAcceptSettings )
						.IsEnabled( this, &SSourceControlLogin::IsAcceptSettingsEnabled )
					]
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("RunWithoutSourceControl", "Run Without Source Control"))
						.OnClicked( this, &SSourceControlLogin::OnDisableSourceControl )
						.IsEnabled( this, &SSourceControlLogin::AreControlsEnabled )
					]
				]
			]
		]
	];
}

void SSourceControlLogin::RefreshSettings()
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	SettingsBorder->SetContent(SourceControlModule.GetProvider().MakeSettingsWidget());
}

FReply SSourceControlLogin::OnAcceptSettings()
{
	ConnectionState = ELoginConnectionState::Connecting;

	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	if(!SourceControlModule.GetProvider().Login(FString(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SSourceControlLogin::SourceControlOperationComplete)))
	{
		DisplayConnectionError();
		ConnectionState = ELoginConnectionState::Disconnected;
	}

	return FReply::Handled();
}

FReply SSourceControlLogin::OnDisableSourceControl()
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	SourceControlModule.SetProvider("None");
	if(ParentWindowPtr.IsValid())
	{
		ParentWindowPtr.Pin()->RequestDestroyWindow();
	}
	SourceControlLoginClosed.ExecuteIfBound(false);
	return FReply::Handled();
}

void SSourceControlLogin::SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	if(InResult == ECommandResult::Succeeded)
	{
		ConnectionState = ELoginConnectionState::Connected;
		FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
		SourceControlModule.SaveSettings();
		DisplayConnectionSuccess();
		SourceControlLoginClosed.ExecuteIfBound(true);
		if(ParentWindowPtr.IsValid())
		{
			ParentWindowPtr.Pin()->RequestDestroyWindow();
		}
	}
	else
	{
		ConnectionState = ELoginConnectionState::Disconnected;
		DisplayConnectionError();
	}
}

void SSourceControlLogin::DisplayConnectionError() const
{
	FMessageLog SourceControlLog("SourceControl");
	SourceControlLog.Error( LOCTEXT("FailedToConnect", "Failed to connect to source control. Check your settings and connection then try again.") );
	SourceControlLog.Notify();
}

void SSourceControlLogin::DisplayConnectionSuccess() const
{
	FNotificationInfo Info( LOCTEXT("ConnectionSuccessful", "Connection to source control was successful!") );
	Info.bFireAndForget = true;
	Info.bUseSuccessFailIcons = true;
	Info.Image = FEditorStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
	FSlateNotificationManager::Get().AddNotification(Info);
}

EVisibility SSourceControlLogin::GetThrobberVisibility() const
{
	return ConnectionState == ELoginConnectionState::Connecting ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SSourceControlLogin::AreControlsEnabled() const
{
	return ConnectionState == ELoginConnectionState::Disconnected;
}

bool SSourceControlLogin::IsAcceptSettingsEnabled() const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	return AreControlsEnabled() && SourceControlModule.GetProvider().GetName() != "None";
}

EVisibility SSourceControlLogin::GetSettingsVisibility() const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	return SourceControlModule.GetProvider().GetName() == "None" ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SSourceControlLogin::GetDisabledTextVisibility() const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	return SourceControlModule.GetProvider().GetName() == "None" ? EVisibility::Visible : EVisibility::Collapsed;
}

void SSourceControlLogin::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// if we are modal then we need to tick the source control module
	TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin();
	if(ParentWindow.IsValid())
	{
		if(FSlateApplication::Get().GetActiveModalWindow() == ParentWindow)
		{
			FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
			SourceControlModule.Tick();
		}
	}
};

#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
