// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserPCH.h"
#include "FrontendFilters.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

/** A class for check boxes in the filter list. If you double click a filter checkbox, you will enable it and disable all others */
class SFilterCheckBox : public SCheckBox
{
public:
	void SetOnFilterDoubleClicked( const FOnClicked& NewFilterDoubleClicked )
	{
		OnFilterDoubleClicked = NewFilterDoubleClicked;
	}

	void SetOnFilterMiddleButtonClicked( const FOnClicked& NewFilterMiddleButtonClicked )
	{
		OnFilterMiddleButtonClicked = NewFilterMiddleButtonClicked;
	}

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) OVERRIDE
	{
		if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OnFilterDoubleClicked.IsBound() )
		{
			return OnFilterDoubleClicked.Execute();
		}
		else
		{
			return SCheckBox::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
		}
	}

	virtual FReply OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) OVERRIDE
	{
		if( InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && OnFilterMiddleButtonClicked.IsBound() )
		{
			return OnFilterMiddleButtonClicked.Execute();
		}
		else
		{
			return SCheckBox::OnMouseButtonUp(InMyGeometry, InMouseEvent);
		}
	}

private:
	FOnClicked OnFilterDoubleClicked;
	FOnClicked OnFilterMiddleButtonClicked;
};

/**
 * A single filter in the filter list. Can be removed by clicking the remove button on it.
 */
class SFilter : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnRequestRemove, const TSharedRef<SFilter>& /*FilterToRemove*/ );
	DECLARE_DELEGATE_OneParam( FOnRequestEnableOnly, const TSharedRef<SFilter>& /*FilterToEnable*/ );
	DECLARE_DELEGATE( FOnRequestDisableAll );
	DECLARE_DELEGATE( FOnRequestRemoveAll );

	SLATE_BEGIN_ARGS( SFilter ){}

		/** The asset type actions that are associated with this filter */
		SLATE_ARGUMENT( TWeakPtr<IAssetTypeActions>, AssetTypeActions )

		/** If this is an front end filter, this is the filter object */
		SLATE_ARGUMENT( TSharedPtr<FFrontendFilter>, FrontendFilter )

		/** Invoked when the filter toggled */
		SLATE_EVENT( SFilterList::FOnFilterChanged, OnFilterChanged )

		/** Invoked when a request to remove this filter originated from within this filter */
		SLATE_EVENT( FOnRequestRemove, OnRequestRemove )

		/** Invoked when a request to enable only this filter originated from within this filter */
		SLATE_EVENT( FOnRequestEnableOnly, OnRequestEnableOnly )

		/** Invoked when a request to disable all filters originated from within this filter */
		SLATE_EVENT( FOnRequestDisableAll, OnRequestDisableAll )

		/** Invoked when a request to remove all filters originated from within this filter */
		SLATE_EVENT( FOnRequestRemoveAll, OnRequestRemoveAll )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		bEnabled = false;
		OnFilterChanged = InArgs._OnFilterChanged;
		AssetTypeActions = InArgs._AssetTypeActions;
		OnRequestRemove = InArgs._OnRequestRemove;
		OnRequestEnableOnly = InArgs._OnRequestEnableOnly;
		OnRequestDisableAll = InArgs._OnRequestDisableAll;
		OnRequestRemoveAll = InArgs._OnRequestRemoveAll;
		FrontendFilter = InArgs._FrontendFilter;

		// Get the tooltip and color of the type represented by this filter
		FText FilterToolTip;
		FilterColor = FLinearColor::White;
		if ( InArgs._AssetTypeActions.IsValid() )
		{
			TSharedPtr<IAssetTypeActions> TypeActions = InArgs._AssetTypeActions.Pin();
			FilterColor = FLinearColor( TypeActions->GetTypeColor() );

			// No tooltip for asset type filters
		}
		else if ( InArgs._FrontendFilter.IsValid() )
		{
			FilterColor = FrontendFilter->GetColor();
			FilterToolTip = FrontendFilter->GetToolTipText();
		}

		ChildSlot
		[
			SNew(SBorder)
			.Padding(0)
			.BorderBackgroundColor( FLinearColor(0.2f, 0.2f, 0.2f, 0.2f) )
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.FilterButtonBorder"))
			[
				SAssignNew( ToggleButtonPtr, SFilterCheckBox )
				.Style(FEditorStyle::Get(), "ContentBrowser.FilterButton")
				.ToolTipText(FilterToolTip)
				.Padding(this, &SFilter::GetFilterNamePadding)
				.IsChecked(this, &SFilter::IsChecked)
				.OnCheckStateChanged(this, &SFilter::FilterToggled)
				.OnGetMenuContent(this, &SFilter::GetRightClickMenuContent)
				.ForegroundColor(this, &SFilter::GetFilterForegroundColor)
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &SFilter::GetFilterNameColorAndOpacity)
					.Font(FEditorStyle::GetFontStyle("ContentBrowser.FilterNameFont"))
					.ShadowOffset(FVector2D(1.f, 1.f))
					.Text( GetFilterName() )
				]
			]
		];

		ToggleButtonPtr->SetOnFilterDoubleClicked( FOnClicked::CreateSP(this, &SFilter::FilterDoubleClicked) );
		ToggleButtonPtr->SetOnFilterMiddleButtonClicked( FOnClicked::CreateSP(this, &SFilter::FilterMiddleButtonClicked) );
	}

	/** Sets whether or not this filter is applied to the combined filter */
	void SetEnabled(bool InEnabled)
	{
		if ( InEnabled != bEnabled)
		{
			bEnabled = InEnabled;
			OnFilterChanged.ExecuteIfBound();
		}
	}

	/** Returns true if this filter contributes to the combined filter */
	bool IsEnabled() const
	{
		return bEnabled;
	}

	/** Returns this widgets contribution to the combined filter */
	FARFilter GetBackendFilter() const
	{
		FARFilter Filter;

		if ( AssetTypeActions.IsValid() )
		{
			// Add the supported class for this type to a filter
			Filter.ClassNames.Add(AssetTypeActions.Pin()->GetSupportedClass()->GetFName());
			Filter.bRecursiveClasses = true;
		}

		return Filter;
	}

	/** If this is an front end filter, this is the filter object */
	const TSharedPtr<FFrontendFilter>& GetFrontendFilter() const
	{
		return FrontendFilter;
	}

	/** Gets the asset type actions associated with this filter */
	const TWeakPtr<IAssetTypeActions>& GetAssetTypeActions() const
	{
		return AssetTypeActions;
	}

private:
	/** Handler for when the filter checkbox is clicked */
	void FilterToggled(ESlateCheckBoxState::Type NewState)
	{
		bEnabled = NewState == ESlateCheckBoxState::Checked;
		OnFilterChanged.ExecuteIfBound();
	}

	/** Handler for when the filter checkbox is double clicked */
	FReply FilterDoubleClicked()
	{
		// Disable all other filters and enable this one.
		OnRequestDisableAll.ExecuteIfBound();
		bEnabled = true;
		OnFilterChanged.ExecuteIfBound();

		return FReply::Handled();
	}

	/** Handler for when the filter checkbox is middle button clicked */
	FReply FilterMiddleButtonClicked()
	{
		RemoveFilter();
		return FReply::Handled();
	}

	/** Handler to create a right click menu */
	TSharedRef<SWidget> GetRightClickMenuContent()
	{
		FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);

		MenuBuilder.BeginSection("FilterOptions", LOCTEXT("FilterContextHeading", "Filter Options"));
		{
			MenuBuilder.AddMenuEntry(
				FText::Format( LOCTEXT("RemoveFilter", "Remove: {0}"), GetFilterName() ),
				LOCTEXT("RemoveFilterTooltip", "Remove this filter from the list. It can be added again in the filters menu."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP(this, &SFilter::RemoveFilter) )
				);

			MenuBuilder.AddMenuEntry(
				FText::Format( LOCTEXT("EnableOnlyThisFilter", "Enable this only: {0}"), GetFilterName() ),
				LOCTEXT("EnableOnlyThisFilterTooltip", "Enable only this filter from the list."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP(this, &SFilter::EnableOnly) )
				);

		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("FilterBulkOptions", LOCTEXT("BulkFilterContextHeading", "Bulk Filter Options"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DisableAllFilters", "Disable All Filters"),
				LOCTEXT("DisableAllFiltersTooltip", "Disables all active filters."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP(this, &SFilter::DisableAllFilters) )
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RemoveAllFilters", "Remove All Filters"),
				LOCTEXT("RemoveAllFiltersTooltip", "Removes all filters from the list."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP(this, &SFilter::RemoveAllFilters) )
				);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	/** Removes this filter from the filter list */
	void RemoveFilter()
	{
		TSharedRef<SFilter> Self = SharedThis(this);
		OnRequestRemove.ExecuteIfBound( Self );
	}

	/** Enables only this filter from the filter list */
	void EnableOnly()
	{
		TSharedRef<SFilter> Self = SharedThis(this);
		OnRequestEnableOnly.ExecuteIfBound( Self );
	}

	/** Disables all active filters in the list */
	void DisableAllFilters()
	{
		OnRequestDisableAll.ExecuteIfBound();
	}

	/** Removes all filters in the list */
	void RemoveAllFilters()
	{
		OnRequestRemoveAll.ExecuteIfBound();
	}

	/** Handler to determine the "checked" state of the filter checkbox */
	ESlateCheckBoxState::Type IsChecked() const
	{
		return bEnabled ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	/** Handler to determine the color of the checkbox when it is checked */
	FSlateColor GetFilterForegroundColor() const
	{
		return IsChecked() ? FilterColor : FLinearColor::White;
	}

	/** Handler to determine the padding of the checkbox text when it is pressed */
	FMargin GetFilterNamePadding() const
	{
		return ToggleButtonPtr->IsPressed() ? FMargin(3,2,4,0) : FMargin(3,1,4,1);
	}

	/** Handler to determine the color of the checkbox text when it is hovered */
	FSlateColor GetFilterNameColorAndOpacity() const
	{
		const float DimFactor = 0.75f;
		return IsHovered() ? FLinearColor(DimFactor, DimFactor, DimFactor, 1.0f) : FLinearColor::White;
	}

	/** Returns the display name for this filter */
	FText GetFilterName() const
	{
		FText FilterName;
		if ( AssetTypeActions.IsValid() )
		{
			TSharedPtr<IAssetTypeActions> TypeActions = AssetTypeActions.Pin();
			FilterName = TypeActions->GetName();
		}
		else if ( FrontendFilter.IsValid() )
		{
			FilterName = FrontendFilter->GetDisplayName();
		}

		if ( FilterName.IsEmpty() )
		{
			FilterName = LOCTEXT("UnknownFilter", "???");
		}

		return FilterName;
	}

private:
	/** Invoked when the filter toggled */
	SFilterList::FOnFilterChanged OnFilterChanged;

	/** Invoked when a request to remove this filter originated from within this filter */
	FOnRequestRemove OnRequestRemove;

	/** Invoked when a request to enable only this filter originated from within this filter */
	FOnRequestEnableOnly OnRequestEnableOnly;

	/** Invoked when a request to disable all filters originated from within this filter */
	FOnRequestDisableAll OnRequestDisableAll;

	/** Invoked when a request to remove all filters originated from within this filter */
	FOnRequestDisableAll OnRequestRemoveAll;

	/** true when this filter should be applied to the search */
	bool bEnabled;

	/** The asset type actions that are associated with this filter */
	TWeakPtr<IAssetTypeActions> AssetTypeActions;

	/** If this is an front end filter, this is the filter object */
	TSharedPtr<FFrontendFilter> FrontendFilter;

	/** The button to toggle the filter on or off */
	TSharedPtr<SFilterCheckBox> ToggleButtonPtr;

	/** The color of the checkbox for this filter */
	FLinearColor FilterColor;
};


/////////////////////
// SFilterList
/////////////////////


void SFilterList::Construct( const FArguments& InArgs )
{
	OnGetContextMenu = InArgs._OnGetContextMenu;
	OnFilterChanged = InArgs._OnFilterChanged;
	FrontendFilters = InArgs._FrontendFilters;

	// Add all frontend filters here
	AllFrontendFilters.Add( MakeShareable(new FFrontendFilter_CheckedOut) );
	AllFrontendFilters.Add( MakeShareable(new FFrontendFilter_Modified) );
	AllFrontendFilters.Add( MakeShareable(new FFrontendFilter_ShowOtherDevelopers) );
	AllFrontendFilters.Add( MakeShareable(new FFrontendFilter_ReplicatedBlueprint) );
	AllFrontendFilters.Add( MakeShareable(new FFrontendFilter_ShowRedirectors) );
	AllFrontendFilters.Add( MakeShareable(new FFrontendFilter_InUseByLoadedLevels) );

	// Auto add all inverse filters
	for ( auto FilterIt = AllFrontendFilters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		if ( (*FilterIt)->IsInverseFilter() )
		{
			SetFrontendFilterActive(*FilterIt, true);
		}
	}

	ChildSlot
	[
		SAssignNew(FilterBox, SWrapBox)
		.UseAllottedWidth(true)
	];
}

FReply SFilterList::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		if ( OnGetContextMenu.IsBound() )
		{
			FReply Reply = FReply::Handled().ReleaseMouseCapture();

			// Get the context menu content. If NULL, don't open a menu.
			TSharedPtr<SWidget> MenuContent = OnGetContextMenu.Execute();

			if ( MenuContent.IsValid() )
			{
				FVector2D SummonLocation = MouseEvent.GetScreenSpacePosition();
				FSlateApplication::Get().PushMenu( AsShared(), MenuContent.ToSharedRef(), SummonLocation, FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu ) );
			}

			return Reply;
		}
	}

	return FReply::Unhandled();
}

bool SFilterList::HasAnyFilters() const
{
	return Filters.Num() > 0;
}

FARFilter SFilterList::GetCombinedBackendFilter() const
{
	FARFilter CombinedFilter;

	// Add all selected filters
	for (int32 FilterIdx = 0; FilterIdx < Filters.Num(); ++FilterIdx)
	{
		if ( Filters[FilterIdx]->IsEnabled() )
		{
			CombinedFilter.Append(Filters[FilterIdx]->GetBackendFilter());
		}
	}

	if ( CombinedFilter.bRecursiveClasses )
	{
		// Add exclusions for AssetTypeActions NOT in the filter.
		// This will prevent assets from showing up that are both derived from an asset in the filter set and derived from an asset not in the filter set
		// Get the list of all asset type actions
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		TArray< TWeakPtr<IAssetTypeActions> > AssetTypeActionsList;
		AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);
		for ( auto TypeActionsIt = AssetTypeActionsList.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt )
		{
			const TWeakPtr<IAssetTypeActions>& WeakTypeActions = *TypeActionsIt;
			if ( WeakTypeActions.IsValid() )
			{
				const TSharedPtr<IAssetTypeActions> TypeActions = WeakTypeActions.Pin();
				if ( TypeActions->CanFilter() )
				{
					const UClass* TypeClass = TypeActions->GetSupportedClass();
					if ( !CombinedFilter.ClassNames.Contains(TypeClass->GetFName()) )
					{
						CombinedFilter.RecursiveClassesExclusionSet.Add(TypeClass->GetFName());
					}
				}
			}
		}
	}

	// HACK: A blueprint can be shown as Blueprint or as BlueprintGeneratedClass, but we don't want to distinguish them while filtering.
	// This should be removed, once all blueprints are shown as BlueprintGeneratedClass.
	if(CombinedFilter.ClassNames.Contains(FName(TEXT("Blueprint"))))
	{
		CombinedFilter.ClassNames.AddUnique(FName(TEXT("BlueprintGeneratedClass")));
	}

	return CombinedFilter;
}

TSharedRef<SWidget> SFilterList::ExternalMakeAddFilterMenu()
{
	return MakeAddFilterMenu();
}

void SFilterList::DisableAllFilters()
{
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		(*FilterIt)->SetEnabled(false);
	}
}

void SFilterList::RemoveAllFilters()
{
	if ( HasAnyFilters() )
	{
		bool bBackendFilterChanged = false;

		for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
		{
			const TSharedRef<SFilter>& FilterToRemove = *FilterIt;

			if ( FilterToRemove->GetAssetTypeActions().IsValid() )
			{
				bBackendFilterChanged = true;
			}
			else if (FilterToRemove->GetFrontendFilter().IsValid() )
			{
				// Update the frontend filters collection
				const TSharedRef<FFrontendFilter>& FrontendFilter = FilterToRemove->GetFrontendFilter().ToSharedRef();
				if ( FrontendFilter->IsInverseFilter() )
				{
					SetFrontendFilterActive(FrontendFilter, true);
				}
				else
				{
					SetFrontendFilterActive(FrontendFilter, false);
				}
			}
		}

		FilterBox->ClearChildren();
		Filters.Empty();

		if ( bBackendFilterChanged )
		{
			// Notify that a backend filter changed
			OnFilterChanged.ExecuteIfBound();
		}
	}
}

void SFilterList::DisableFiltersThatHideAssets(const TArray<FAssetData>& AssetDataList)
{
	if ( HasAnyFilters() )
	{
		// Determine if we should disable backend filters. If any asset fails the combined backend filter, disable them all.
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FARFilter CombinedBackendFilter = GetCombinedBackendFilter();
		bool bDisableAllBackendFilters = false;
		TArray<FAssetData> LocalAssetDataList = AssetDataList;
		AssetRegistryModule.Get().RunAssetsThroughFilter(LocalAssetDataList, CombinedBackendFilter);
		if ( LocalAssetDataList.Num() != AssetDataList.Num() )
		{
			bDisableAllBackendFilters = true;
		}

		// Iterate over all enabled filters and disable any frontend filters that would hide any of the supplied assets
		// and disable all backend filters if it was determined that the combined backend filter hides any of the assets
		for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
		{
			const TSharedRef<SFilter>& Filter = *FilterIt;
			if ( Filter->IsEnabled() )
			{
				const TSharedPtr<FFrontendFilter>& FrontendFilter = Filter->GetFrontendFilter();
				if ( FrontendFilter.IsValid() )
				{
					for ( auto AssetIt = AssetDataList.CreateConstIterator(); AssetIt; ++AssetIt )
					{
						if ( !FrontendFilter->PassesFilter( *AssetIt ) )
						{
							// This is a frontend filter and at least one asset did not pass.
							Filter->SetEnabled(false);
						}
					}
				}

				if ( bDisableAllBackendFilters )
				{
					FARFilter BackendFilter = Filter->GetBackendFilter();
					if ( !BackendFilter.IsEmpty() )
					{
						Filter->SetEnabled(false);
					}
				}
			}
		}
	}
}

void SFilterList::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	FString ActiveTypeFilterString;
	FString EnabledTypeFilterString;
	FString ActiveFrontendFilterString;
	FString EnabledFrontendFilterString;
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		const TSharedRef<SFilter>& Filter = *FilterIt;

		if ( Filter->GetAssetTypeActions().IsValid() )
		{
			if ( ActiveTypeFilterString.Len() > 0 )
			{
				ActiveTypeFilterString += TEXT(",");
			}

			const FString FilterName = Filter->GetAssetTypeActions().Pin()->GetSupportedClass()->GetName();
			ActiveTypeFilterString += FilterName;

			if ( Filter->IsEnabled() )
			{
				if ( EnabledTypeFilterString.Len() > 0 )
				{
					EnabledTypeFilterString += TEXT(",");
				}

				EnabledTypeFilterString += FilterName;
			}
		}
		else if ( Filter->GetFrontendFilter().IsValid() )
		{
			if ( ActiveFrontendFilterString.Len() > 0 )
			{
				ActiveFrontendFilterString += TEXT(",");
			}

			const FString FilterName = Filter->GetFrontendFilter()->GetName();
			ActiveFrontendFilterString += FilterName;

			if ( Filter->IsEnabled() )
			{
				if ( EnabledFrontendFilterString.Len() > 0 )
				{
					EnabledFrontendFilterString += TEXT(",");
				}

				EnabledFrontendFilterString += FilterName;
			}
		}
	}

	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".ActiveTypeFilters")), *ActiveTypeFilterString, IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".EnabledTypeFilters")), *EnabledTypeFilterString, IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".ActiveFrontendFilters")), *ActiveFrontendFilterString, IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".EnabledFrontendFilters")), *EnabledFrontendFilterString, IniFilename);
}

void SFilterList::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	{
		// Add all the type filters that were found in the ActiveTypeFilters
		FString ActiveTypeFilterString;
		FString EnabledTypeFilterString;
		GConfig->GetString(*IniSection, *(SettingsString + TEXT(".ActiveTypeFilters")), ActiveTypeFilterString, IniFilename);
		GConfig->GetString(*IniSection, *(SettingsString + TEXT(".EnabledTypeFilters")), EnabledTypeFilterString, IniFilename);

		// Parse comma delimited strings into arrays
		TArray<FString> TypeFilterNames;
		TArray<FString> EnabledTypeFilterNames;
		ActiveTypeFilterString.ParseIntoArray(&TypeFilterNames, TEXT(","), /*bCullEmpty=*/true);
		EnabledTypeFilterString.ParseIntoArray(&EnabledTypeFilterNames, TEXT(","), /*bCullEmpty=*/true);

		// Get the list of all asset type actions
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		TArray< TWeakPtr<IAssetTypeActions> > AssetTypeActionsList;
		AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);

		// For each TypeActions, add any that were active and enable any that were previously enabled
		for ( auto TypeActionsIt = AssetTypeActionsList.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt )
		{
			const TWeakPtr<IAssetTypeActions>& TypeActions = *TypeActionsIt;
			if ( TypeActions.IsValid() && TypeActions.Pin()->CanFilter() && !IsAssetTypeActionsInUse(TypeActions) )
			{
				const FString& ClassName = TypeActions.Pin()->GetSupportedClass()->GetName();
				if ( TypeFilterNames.Contains(ClassName) )
				{
					TSharedRef<SFilter> NewFilter = AddFilter(TypeActions);

					if ( EnabledTypeFilterNames.Contains(ClassName) )
					{
						NewFilter->SetEnabled(true);
					}
				}
			}
		}
	}

	{
		// Add all the frontend filters that were found in the ActiveFrontendFilters
		FString ActiveFrontendFilterString;	
		FString EnabledFrontendFilterString;
		GConfig->GetString(*IniSection, *(SettingsString + TEXT(".ActiveFrontendFilters")), ActiveFrontendFilterString, IniFilename);
		GConfig->GetString(*IniSection, *(SettingsString + TEXT(".EnabledFrontendFilters")), EnabledFrontendFilterString, IniFilename);

		// Parse comma delimited strings into arrays
		TArray<FString> FrontendFilterNames;
		TArray<FString> EnabledFrontendFilterNames;
		ActiveFrontendFilterString.ParseIntoArray(&FrontendFilterNames, TEXT(","), /*bCullEmpty=*/true);
		EnabledFrontendFilterString.ParseIntoArray(&EnabledFrontendFilterNames, TEXT(","), /*bCullEmpty=*/true);

		// For each FrontendFilter, add any that were active and enable any that were previously enabled
		for ( auto FrontendFilterIt = AllFrontendFilters.CreateConstIterator(); FrontendFilterIt; ++FrontendFilterIt )
		{
			const TSharedRef<FFrontendFilter>& FrontendFilter = *FrontendFilterIt;
			if ( !IsFrontendFilterInUse(FrontendFilter) )
			{
				const FString& FilterName = FrontendFilter->GetName();
				if ( FrontendFilterNames.Contains(FilterName) )
				{
					TSharedRef<SFilter> NewFilter = AddFilter(FrontendFilter);

					if ( EnabledFrontendFilterNames.Contains(FilterName) )
					{
						NewFilter->SetEnabled(true);
					}
				}
			}
		}
	}
}

void SFilterList::SetFrontendFilterActive(const TSharedRef<FFrontendFilter>& Filter, bool bActive)
{
	Filter->ActiveStateChanged(bActive);

	if ( bActive )
	{
		FrontendFilters->Add(Filter);
	}
	else
	{
		FrontendFilters->Remove(Filter);
	}
}

TSharedRef<SFilter> SFilterList::AddFilter(const TWeakPtr<IAssetTypeActions>& AssetTypeActions)
{
	TSharedRef<SFilter> NewFilter =
		SNew(SFilter)
		.AssetTypeActions(AssetTypeActions)
		.OnFilterChanged(OnFilterChanged)
		.OnRequestRemove(this, &SFilterList::RemoveFilter)
		.OnRequestEnableOnly(this, &SFilterList::EnableOnlyThisFilter)
		.OnRequestDisableAll(this, &SFilterList::DisableAllFilters)
		.OnRequestRemoveAll(this, &SFilterList::RemoveAllFilters);

	AddFilter( NewFilter );

	return NewFilter;
}

TSharedRef<SFilter> SFilterList::AddFilter(const TSharedRef<FFrontendFilter>& FrontendFilter)
{
	TSharedRef<SFilter> NewFilter =
		SNew(SFilter)
		.FrontendFilter(FrontendFilter)
		.OnFilterChanged( this, &SFilterList::FrontendFilterChanged, FrontendFilter )
		.OnRequestRemove(this, &SFilterList::RemoveFilter)
		.OnRequestDisableAll(this, &SFilterList::DisableAllFilters)
		.OnRequestRemoveAll(this, &SFilterList::RemoveAllFilters);

	AddFilter( NewFilter );

	return NewFilter;
}

void SFilterList::AddFilter(const TSharedRef<SFilter>& FilterToAdd)
{
	Filters.Add(FilterToAdd);

	FilterBox->AddSlot()
	.Padding(3, 3)
	[
		FilterToAdd
	];
}

void SFilterList::RemoveFilter(const TWeakPtr<IAssetTypeActions>& AssetTypeActions)
{
	TSharedPtr<SFilter> FilterToRemove;
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		const TWeakPtr<IAssetTypeActions>& Actions = (*FilterIt)->GetAssetTypeActions();
		if ( Actions.IsValid() && Actions == AssetTypeActions)
		{
			FilterToRemove = *FilterIt;
			break;
		}
	}

	if ( FilterToRemove.IsValid() )
	{
		RemoveFilter(FilterToRemove.ToSharedRef());
	}
}

void SFilterList::EnableOnlyThisFilter(const TSharedRef<SFilter>& FilterToEnable)
{
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		bool bEnable = *FilterIt == FilterToEnable;
		(*FilterIt)->SetEnabled(bEnable);
	}
}

void SFilterList::RemoveFilter(const TSharedRef<FFrontendFilter>& FrontendFilter)
{
	TSharedPtr<SFilter> FilterToRemove;
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		TSharedPtr<FFrontendFilter> Filter = (*FilterIt)->GetFrontendFilter();
		if ( Filter.IsValid() && Filter == FrontendFilter )
		{
			FilterToRemove = *FilterIt;
			break;
		}
	}

	if ( FilterToRemove.IsValid() )
	{
		RemoveFilter(FilterToRemove.ToSharedRef());
	}
}

void SFilterList::RemoveFilter(const TSharedRef<SFilter>& FilterToRemove)
{
	FilterBox->RemoveSlot(FilterToRemove);
	Filters.Remove(FilterToRemove);

	if ( FilterToRemove->GetAssetTypeActions().IsValid() )
	{
		// Notify that a backend filter changed
		OnFilterChanged.ExecuteIfBound();
	}
	else if (FilterToRemove->GetFrontendFilter().IsValid() )
	{
		// Update the frontend filters collection
		const TSharedRef<FFrontendFilter>& FrontendFilter = FilterToRemove->GetFrontendFilter().ToSharedRef();
		if ( FrontendFilter->IsInverseFilter() )
		{
			SetFrontendFilterActive(FrontendFilter, true);
		}
		else
		{
			SetFrontendFilterActive(FrontendFilter, false);
		}
	}
}

void SFilterList::FrontendFilterChanged(TSharedRef<FFrontendFilter> FrontendFilter)
{
	TSharedPtr<SFilter> FilterToUpdate;
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt )
	{
		TSharedPtr<FFrontendFilter> Filter = (*FilterIt)->GetFrontendFilter();
		if ( Filter.IsValid() && Filter == FrontendFilter )
		{
			FilterToUpdate = *FilterIt;
			break;
		}
	}

	if ( FilterToUpdate.IsValid() )
	{
		bool bShouldAdd = FilterToUpdate->IsEnabled();

		// Inverse filters are only added when they are disabled
		if ( FrontendFilter->IsInverseFilter() )
		{
			bShouldAdd = !bShouldAdd;
		}

		if ( bShouldAdd )
		{
			SetFrontendFilterActive(FrontendFilter, true);
		}
		else
		{
			SetFrontendFilterActive(FrontendFilter, false);
		}
	}
}

void SFilterList::CreateFiltersMenuCategory(FMenuBuilder& MenuBuilder, const TArray<TWeakPtr<IAssetTypeActions>> AssetTypeActionsList) const
{
	for (int32 ClassIdx = 0; ClassIdx < AssetTypeActionsList.Num(); ++ClassIdx)
	{
		const TWeakPtr<IAssetTypeActions>& WeakTypeActions = AssetTypeActionsList[ClassIdx];
		if ( WeakTypeActions.IsValid() )
		{
			TSharedPtr<IAssetTypeActions> TypeActions = WeakTypeActions.Pin();
			if ( TypeActions.IsValid() )
			{
				const FText& LabelText = TypeActions->GetName();
				MenuBuilder.AddMenuEntry(
					LabelText,
					FText::Format( LOCTEXT("FilterByTooltipPrefix", "Filter by {0}"), LabelText ),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP( this, &SFilterList::FilterByTypeClicked, WeakTypeActions ),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP(this, &SFilterList::IsAssetTypeActionsInUse, WeakTypeActions ) ),
					NAME_None,
					EUserInterfaceActionType::ToggleButton
					);
			}
		}
	}
}

void SFilterList::CreateOtherFiltersMenuCategory(FMenuBuilder& MenuBuilder) const
{	
	for ( auto FrontendFilterIt = AllFrontendFilters.CreateConstIterator(); FrontendFilterIt; ++FrontendFilterIt )
	{
		const TSharedRef<FFrontendFilter>& FrontendFilter = *FrontendFilterIt;

		MenuBuilder.AddMenuEntry(
			FrontendFilter->GetDisplayName(),
			FrontendFilter->GetToolTipText(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), FrontendFilter->GetIconName()),
			FUIAction(
				FExecuteAction::CreateSP( this, &SFilterList::FrontendFilterClicked, FrontendFilter ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SFilterList::IsFrontendFilterInUse, FrontendFilter ) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
}

TSharedRef<SWidget> SFilterList::MakeAddFilterMenu()
{
	// A local struct to describe a category in the filter menu
	struct FCategoryMenu
	{
		FText Name;
		FText Tooltip;
		TArray<TWeakPtr<IAssetTypeActions>> Assets;

		FCategoryMenu(const FText& InName, const FText& InTooltip)
			: Name(InName)
			, Tooltip(InTooltip)
			, Assets()
		{}
	};

	// Create a map of Categories to Menus
	TMap<EAssetTypeCategories::Type, FCategoryMenu> CategoryToMenuMap;
	CategoryToMenuMap.Add(EAssetTypeCategories::Basic, FCategoryMenu( LOCTEXT("BasicFilter", "Basic"), LOCTEXT("BasicFilterTooltip", "Filter by basic assets.") ) );
	CategoryToMenuMap.Add(EAssetTypeCategories::Animation, FCategoryMenu( LOCTEXT("AnimationFilter", "Animation"), LOCTEXT("AnimationFilterTooltip", "Filter by animation assets.") ) );
	CategoryToMenuMap.Add(EAssetTypeCategories::MaterialsAndTextures, FCategoryMenu( LOCTEXT("MaterialFilter", "Materials & Textures"), LOCTEXT("MaterialFilterTooltip", "Filter by material and texture assets.") ) );
	CategoryToMenuMap.Add(EAssetTypeCategories::Sounds, FCategoryMenu( LOCTEXT("SoundFilter", "Sounds"), LOCTEXT("SoundFilterTooltip", "Filter by sound assets.") ) );
	CategoryToMenuMap.Add(EAssetTypeCategories::Physics, FCategoryMenu( LOCTEXT("PhysicsFilter", "Physics"), LOCTEXT("PhysicsFilterTooltip", "Filter by physics assets.") ) );
	CategoryToMenuMap.Add(EAssetTypeCategories::Misc, FCategoryMenu( LOCTEXT("MiscFilter", "Miscellaneous"), LOCTEXT("MiscFilterTooltip", "Filter by miscellaneous assets.") ) );

	// Load the asset tools module to get access to the browser type maps
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<TWeakPtr<IAssetTypeActions>> AssetTypeActionsList;
	AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);

	// Sort the list
	struct FCompareIAssetTypeActions
	{
		FORCEINLINE bool operator()( const TWeakPtr<IAssetTypeActions>& A, const TWeakPtr<IAssetTypeActions>& B ) const
		{
			return A.Pin()->GetName().CompareTo( B.Pin()->GetName() ) == -1;
		}
	};
	AssetTypeActionsList.Sort( FCompareIAssetTypeActions() );

	// For every asset type, move it into all the catgories it should appear in
	for (int32 ClassIdx = 0; ClassIdx < AssetTypeActionsList.Num(); ++ClassIdx)
	{
		const TWeakPtr<IAssetTypeActions>& WeakTypeActions = AssetTypeActionsList[ClassIdx];
		if ( WeakTypeActions.IsValid() )
		{
			TSharedPtr<IAssetTypeActions> TypeActions = WeakTypeActions.Pin();
			if ( ensure(TypeActions.IsValid()) && TypeActions->CanFilter() )
			{
				for ( auto MenuIt = CategoryToMenuMap.CreateIterator(); MenuIt; ++MenuIt )
				{
					if ( TypeActions->GetCategories() & MenuIt.Key() )
					{
						// This is a valid asset type which can be filtered, add it to the correct category
						FCategoryMenu& Menu = MenuIt.Value();
						Menu.Assets.Add( WeakTypeActions );
					}
				}
			}
		}
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);

	MenuBuilder.BeginSection( "ContentBrowserResetFilters" );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "FilterListResetFilters", "Reset Filters" ),
			LOCTEXT( "FilterListResetToolTip", "Resets current filter selection" ),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SFilterList::OnResetFilters ) )
			);
	}
	MenuBuilder.EndSection(); //ContentBrowserResetFilters

	// First add the basic category, this appears as standard entries in the list (Note: intentionally not using FindChecked here as removing it from the map later would cause the ref to be garbage)
	FCategoryMenu* BasicCategory = CategoryToMenuMap.Find( EAssetTypeCategories::Basic );
	check( BasicCategory );

	MenuBuilder.BeginSection("ContentBrowserFilterBasicAsset", LOCTEXT("BasicAssetsMenuHeading", "Basic Assets") );
	{
		// Add a menu entry which toggles all other categories
		MenuBuilder.AddMenuEntry(
			BasicCategory->Name,
			BasicCategory->Tooltip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SFilterList::FilterByTypeCategoryClicked, EAssetTypeCategories::Basic ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SFilterList::IsAssetTypeCategoryInUse, EAssetTypeCategories::Basic ) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);

		// Now populate with all the basic assets
		SFilterList::CreateFiltersMenuCategory( MenuBuilder, BasicCategory->Assets);
	}
	MenuBuilder.EndSection(); //ContentBrowserFilterBasicAsset

	// Remove the basic category from the map now, as this is treated differently and is no longer needed.
	BasicCategory = NULL;
	CategoryToMenuMap.Remove( EAssetTypeCategories::Basic );

	MenuBuilder.BeginSection("ContentBrowserFilterAdvancedAsset", LOCTEXT("AdvancedAssetsMenuHeading", "Other Assets") );
	{
		// For all the remaining categories, add them as submenus
		for ( auto MenuIt = CategoryToMenuMap.CreateIterator(); MenuIt; ++MenuIt )
		{
			FCategoryMenu& Menu = MenuIt.Value();

			MenuBuilder.AddSubMenu(
				Menu.Name,
				Menu.Tooltip,
				FNewMenuDelegate::CreateSP(this, &SFilterList::CreateFiltersMenuCategory, Menu.Assets),
				FUIAction(
					FExecuteAction::CreateSP( this, &SFilterList::FilterByTypeCategoryClicked, MenuIt.Key() ),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SFilterList::IsAssetTypeCategoryInUse, MenuIt.Key() ) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
				);
		}
	
		// Now add the other filter which aren't assets
		const FText LabelText = LOCTEXT("FrontendFiltersCategory", "Other Filters");
		MenuBuilder.AddSubMenu(
			LabelText,
			LOCTEXT("FrontendFiltersCategoryTooltip", "Filter assets by all filters in this category."),
			FNewMenuDelegate::CreateSP(this, &SFilterList::CreateOtherFiltersMenuCategory),
			FUIAction(
				FExecuteAction::CreateSP( this, &SFilterList::FrontendFilterCategoryClicked ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SFilterList::IsFrontendFilterCategoryInUse ) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
	MenuBuilder.EndSection(); //ContentBrowserFilterAdvancedAsset

	MenuBuilder.BeginSection("ContentBrowserFilterMiscAsset", LOCTEXT("MiscAssetsMenuHeading", "Misc Options") );
	MenuBuilder.EndSection(); //ContentBrowserFilterMiscAsset

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	return 
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.MaxHeight(DisplaySize.Y * 0.5)
		[
			MenuBuilder.MakeWidget()
		];
}

void SFilterList::FilterByTypeClicked(TWeakPtr<IAssetTypeActions> AssetTypeActions)
{
	if ( AssetTypeActions.IsValid() )
	{
		if ( IsAssetTypeActionsInUse(AssetTypeActions) )
		{
			RemoveFilter(AssetTypeActions);
		}
		else
		{
			TSharedRef<SFilter> NewFilter = AddFilter(AssetTypeActions);
			NewFilter->SetEnabled(true);
		}
	}
}

bool SFilterList::IsAssetTypeActionsInUse(TWeakPtr<IAssetTypeActions> AssetTypeActions) const
{
	if ( !AssetTypeActions.IsValid() )
	{
		return false;
	}

	TSharedPtr<IAssetTypeActions> TypeActions = AssetTypeActions.Pin();
	if ( !TypeActions.IsValid() )
	{
		return false;
	}

	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt)
	{
		TWeakPtr<IAssetTypeActions> FilterActions = (*FilterIt)->GetAssetTypeActions();
		if ( FilterActions.IsValid() && FilterActions.Pin() == TypeActions )
		{
			return true;
		}
	}

	return false;
}

void SFilterList::FilterByTypeCategoryClicked(EAssetTypeCategories::Type Category)
{
	TArray<TWeakPtr<IAssetTypeActions>> TypeActionsList;
	GetTypeActionsForCategory(Category, TypeActionsList);

	bool bFullCategoryInUse = IsAssetTypeCategoryInUse(Category);
	for ( auto TypeIt = TypeActionsList.CreateConstIterator(); TypeIt; ++TypeIt )
	{
		auto AssetTypeActions = (*TypeIt);
		if ( AssetTypeActions.IsValid() )
		{
			if ( bFullCategoryInUse )
			{
				RemoveFilter(AssetTypeActions);
			}
			else if ( !IsAssetTypeActionsInUse(AssetTypeActions) )
			{
				TSharedRef<SFilter> NewFilter = AddFilter(AssetTypeActions);
				NewFilter->SetEnabled(true);
			}
		}
	}
}

bool SFilterList::IsAssetTypeCategoryInUse(EAssetTypeCategories::Type Category) const
{
	TArray<TWeakPtr<IAssetTypeActions>> TypeActionsList;
	GetTypeActionsForCategory(Category, TypeActionsList);

	for ( auto TypeIt = TypeActionsList.CreateConstIterator(); TypeIt; ++TypeIt )
	{
		auto AssetTypeActions = (*TypeIt);
		if ( AssetTypeActions.IsValid() )
		{
			if ( !IsAssetTypeActionsInUse(AssetTypeActions) )
			{
				return false;
			}
		}
	}

	return true;
}

void SFilterList::GetTypeActionsForCategory(EAssetTypeCategories::Type Category, TArray< TWeakPtr<IAssetTypeActions> >& TypeActions) const
{
	// Load the asset tools module
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<TWeakPtr<IAssetTypeActions>> AssetTypeActionsList;
	AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);

	// Find all asset type actions that match the category
	for (int32 ClassIdx = 0; ClassIdx < AssetTypeActionsList.Num(); ++ClassIdx)
	{
		const TWeakPtr<IAssetTypeActions>& WeakTypeActions = AssetTypeActionsList[ClassIdx];
		TSharedPtr<IAssetTypeActions> AssetTypeActions = WeakTypeActions.Pin();

		if ( ensure(AssetTypeActions.IsValid()) && AssetTypeActions->CanFilter() && AssetTypeActions->GetCategories() & Category)
		{
			TypeActions.Add(WeakTypeActions);
		}
	}
}

void SFilterList::FrontendFilterClicked(TSharedRef<FFrontendFilter> FrontendFilter)
{
	if ( IsFrontendFilterInUse(FrontendFilter) )
	{
		RemoveFilter(FrontendFilter);
	}
	else
	{
		TSharedRef<SFilter> NewFilter = AddFilter(FrontendFilter);
		NewFilter->SetEnabled(true);
	}
}

bool SFilterList::IsFrontendFilterInUse(TSharedRef<FFrontendFilter> FrontendFilter) const
{
	for ( auto FilterIt = Filters.CreateConstIterator(); FilterIt; ++FilterIt)
	{
		TSharedPtr<FFrontendFilter> Filter = (*FilterIt)->GetFrontendFilter();
		if ( Filter.IsValid() && Filter == FrontendFilter )
		{
			return true;
		}
	}

	return false;
}

void SFilterList::FrontendFilterCategoryClicked()
{
	bool bFullCategoryInUse = IsFrontendFilterCategoryInUse();
	for ( auto FrontendFilterIt = AllFrontendFilters.CreateConstIterator(); FrontendFilterIt; ++FrontendFilterIt )
	{
		const TSharedRef<FFrontendFilter>& FrontendFilter = *FrontendFilterIt;

		if ( bFullCategoryInUse )
		{
			RemoveFilter( FrontendFilter );
		}
		else if ( !IsFrontendFilterInUse( FrontendFilter ) )
		{
			TSharedRef<SFilter> NewFilter = AddFilter( FrontendFilter );
			NewFilter->SetEnabled(true);
		}
	}
}

bool SFilterList::IsFrontendFilterCategoryInUse() const
{
	for ( auto FrontendFilterIt = AllFrontendFilters.CreateConstIterator(); FrontendFilterIt; ++FrontendFilterIt )
	{
		const TSharedRef<FFrontendFilter>& FrontendFilter = *FrontendFilterIt;

		if ( !IsFrontendFilterInUse( FrontendFilter ) )
		{
			return false;
		}
	}

	return true;
}

void SFilterList::OnResetFilters()
{
	RemoveAllFilters();
}

#undef LOCTEXT_NAMESPACE
