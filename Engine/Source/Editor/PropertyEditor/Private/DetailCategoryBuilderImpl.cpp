// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "DetailCategoryBuilderImpl.h"

#include "AssetSelection.h"

#include "PropertyHandleImpl.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"

#include "SPropertyEditor.h"
#include "SPropertyEditorBool.h"
#include "DetailCategoryGroupNode.h"
#include "DetailItemNode.h"
#include "DetailAdvancedDropdownNode.h"
#include "SResetToDefaultPropertyEditor.h"
#include "DetailPropertyRow.h"
#include "DetailGroup.h"
#include "DetailCustomBuilderRow.h"

namespace DetailLayoutConstants
{
	// Padding for each layout row
	const FMargin RowPadding( 10.0f, 2.0f );
}

void FDetailLayout::AddCustomLayout( const FDetailLayoutCustomization& Layout, bool bAdvanced )
{
	AddLayoutInternal( Layout, bAdvanced ? CustomAdvancedLayouts : CustomSimpleLayouts );
}

void FDetailLayout::AddDefaultLayout( const FDetailLayoutCustomization& Layout, bool bAdvanced )
{
	AddLayoutInternal( Layout, bAdvanced ? DefaultAdvancedLayouts : DefaultSimpleLayouts );
}

void FDetailLayout::AddLayoutInternal( const FDetailLayoutCustomization& Layout, FCustomizationList& ListToUse )
{
	ListToUse.Add( Layout );
}

FDetailLayoutCustomization::FDetailLayoutCustomization()
	: PropertyRow( NULL )
	, WidgetDecl( NULL )
	, CustomBuilderRow( NULL )
{

}

TSharedPtr<FPropertyNode> FDetailLayoutCustomization::GetPropertyNode() const 
{
	return PropertyRow.IsValid() ? PropertyRow->GetPropertyNode() : NULL;
}

FDetailWidgetRow FDetailLayoutCustomization::GetWidgetRow() const
{
	if( HasCustomWidget() )
	{
		return *WidgetDecl;
	}
	else if( HasCustomBuilder() )
	{
		return CustomBuilderRow->GetWidgetRow();
	}
	else if( HasPropertyNode() )
	{
		return PropertyRow->GetWidgetRow();
	}
	else
	{
		return DetailGroup->GetWidgetRow();
	}
}


FDetailCategoryImpl::FDetailCategoryImpl( FName InCategoryName, TSharedRef<FDetailLayoutBuilderImpl> InDetailLayout )
	: DetailLayoutBuilder( InDetailLayout )
	, CategoryName( InCategoryName )
	, HeaderContentWidget( NULL )
	, SortOrder( 0 )
	, bRestoreExpansionState( true )
	, bShouldBeInitiallyCollapsed( false )
	, bUserShowAdvanced( false )
	, bForceAdvanced( false )
	, bHasFilterStrings( false )
	, bHasVisibleDetails( true )
{
	const UClass* BaseClass = InDetailLayout->GetDetailsView().GetBaseClass();
	// Use the base class name if there is one otherwise this is a generic category not specific to a class
	FName BaseClassName = BaseClass ? BaseClass->GetFName() : FName("Generic");

	CategoryPathName = BaseClassName.ToString() + TEXT(".") + CategoryName.ToString();

	GConfig->GetBool( TEXT("DetailCategoriesAdvanced"), *CategoryPathName, bUserShowAdvanced, GEditorUserSettingsIni );

}

FDetailCategoryImpl::~FDetailCategoryImpl()
{
}


FDetailWidgetRow& FDetailCategoryImpl::AddCustomRow( const FString& FilterString, bool bForAdvanced )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.WidgetDecl = MakeShareable( new FDetailWidgetRow );
	NewCustomization.WidgetDecl->FilterString( FilterString );

	AddCustomLayout( NewCustomization, bForAdvanced );

	return *NewCustomization.WidgetDecl;
}


void FDetailCategoryImpl::AddCustomBuilder( TSharedRef<IDetailCustomNodeBuilder> InCustomBuilder, bool bForAdvanced )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.CustomBuilderRow = MakeShareable( new FDetailCustomBuilderRow( InCustomBuilder ) );

	AddCustomLayout( NewCustomization, bForAdvanced );
}

IDetailGroup& FDetailCategoryImpl::AddGroup( FName GroupName, const FString& LocalizedDisplayName, bool bForAdvanced )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.DetailGroup = MakeShareable( new FDetailGroup( GroupName, AsShared(), LocalizedDisplayName ) );

	AddCustomLayout( NewCustomization, bForAdvanced );

	return *NewCustomization.DetailGroup;
}

void FDetailCategoryImpl::GetDefaultProperties( TArray<TSharedRef<IPropertyHandle> >& OutDefaultProperties, bool bSimpleProperties, bool bAdvancedProperties )
{
	FDetailLayoutBuilderImpl& DetailLayoutBuilderRef = GetParentLayoutImpl();
	for( int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex )
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		if( bSimpleProperties )
		{
			const FCustomizationList& CustomizationList = Layout.GetDefaultSimpleLayouts();

			for( int32 CustomizationIndex = 0; CustomizationIndex < CustomizationList.Num(); ++CustomizationIndex )
			{
				if( CustomizationList[CustomizationIndex].HasPropertyNode() )
				{
					const TSharedPtr<FPropertyNode>& Node = CustomizationList[CustomizationIndex].GetPropertyNode();

					TSharedRef<IPropertyHandle> PropertyHandle = DetailLayoutBuilderRef.GetPropertyHandle( Node );

					if( PropertyHandle->IsValidHandle() )
					{
						OutDefaultProperties.Add( PropertyHandle );
					}
				}
			}
		}

		if( bAdvancedProperties )
		{
			const FCustomizationList& CustomizationList = Layout.GetDefaultAdvancedLayouts();

			for( int32 CustomizationIndex = 0; CustomizationIndex < CustomizationList.Num(); ++CustomizationIndex )
			{
				if( CustomizationList[CustomizationIndex].HasPropertyNode() )
				{
					const TSharedPtr<FPropertyNode>& Node = CustomizationList[CustomizationIndex].GetPropertyNode();

					OutDefaultProperties.Add( DetailLayoutBuilderRef.GetPropertyHandle( Node ) );
				}
			}
		}
	}
}


IDetailCategoryBuilder& FDetailCategoryImpl::InitiallyCollapsed( bool bInShouldBeInitiallyCollapsed )
{
	this->bShouldBeInitiallyCollapsed = bInShouldBeInitiallyCollapsed;
	return *this;
}

IDetailCategoryBuilder& FDetailCategoryImpl::OnExpansionChanged( FOnBooleanValueChanged InOnExpansionChanged )
{
	this->OnExpansionChangedDelegate = InOnExpansionChanged;
	return *this;
}

IDetailCategoryBuilder& FDetailCategoryImpl::RestoreExpansionState( bool bRestore )
{
	this->bRestoreExpansionState = bRestore;
	return *this;
}

IDetailCategoryBuilder& FDetailCategoryImpl::HeaderContent( TSharedRef<SWidget> InHeaderContent )
{
	this->HeaderContentWidget = InHeaderContent;
	return *this;
}

IDetailPropertyRow& FDetailCategoryImpl::AddProperty( FName PropertyPath, UClass* ClassOutermost, FName InstanceName, EPropertyLocation::Type Location )
{
	FDetailLayoutCustomization NewCustomization;
	TSharedPtr<FPropertyNode> PropertyNode = GetParentLayoutImpl().GetPropertyNode( PropertyPath, ClassOutermost, InstanceName );
	if( PropertyNode.IsValid() )
	{
		GetParentLayoutImpl().SetCustomProperty( PropertyNode );
	}
	
	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( PropertyNode, AsShared() ) );

	bool bForAdvanced = false;
	
	if( Location == EPropertyLocation::Default )
	{
		// Get the default location of this property
		bForAdvanced = IsAdvancedLayout( NewCustomization );
	}
	else if( Location == EPropertyLocation::Advanced )
	{
		// Force advanced
		bForAdvanced = true;
	}

	AddCustomLayout( NewCustomization, bForAdvanced);

	return *NewCustomization.PropertyRow;
}

IDetailPropertyRow& FDetailCategoryImpl::AddProperty( TSharedPtr<IPropertyHandle> PropertyHandle,  EPropertyLocation::Type Location)
{
	FDetailLayoutCustomization NewCustomization;
	TSharedPtr<FPropertyNode> PropertyNode = GetParentLayoutImpl().GetPropertyNode( PropertyHandle );

	if( PropertyNode.IsValid() )
	{
		GetParentLayoutImpl().SetCustomProperty( PropertyNode );
	}

	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( PropertyNode, AsShared() ) );

	bool bForAdvanced = false;
	

	if( Location == EPropertyLocation::Default )
	{
		// Get the default location of this property
		bForAdvanced = IsAdvancedLayout( NewCustomization );
	}
	else if( Location == EPropertyLocation::Advanced )
	{
		// Force advanced
		bForAdvanced = true;
	}


	AddCustomLayout( NewCustomization, bForAdvanced );

	return *NewCustomization.PropertyRow;
}

IDetailPropertyRow* FDetailCategoryImpl::AddExternalProperty( const TArray<UObject*>& Objects, FName PropertyName, EPropertyLocation::Type Location )
{
	TSharedRef<FObjectPropertyNode> RootPropertyNode( new FObjectPropertyNode );

	for( int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex )
	{
		RootPropertyNode->AddObject( Objects[ObjectIndex] );
	}

	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = NULL;
	InitParams.Property = NULL;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	// we'll generate the children
	InitParams.bAllowChildren = false;
	InitParams.bForceHiddenPropertyVisibility = false;
	InitParams.bCreateCategoryNodes = false;

	RootPropertyNode->InitNode( InitParams );

	TSharedPtr<FPropertyNode> PropertyNode = RootPropertyNode->GenerateSingleChild( PropertyName );

	if( PropertyNode.IsValid() )
	{
		PropertyNode->RebuildChildren();

		FDetailLayoutCustomization NewCustomization;

		NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( PropertyNode, AsShared(), RootPropertyNode ) );

		bool bForAdvanced = false;
		if( Location == EPropertyLocation::Advanced )
		{
			// Force advanced
			bForAdvanced = true;
		}

		DetailLayoutBuilder.Pin()->AddExternalRootPropertyNode( RootPropertyNode );

		AddCustomLayout( NewCustomization, bForAdvanced );

		return NewCustomization.PropertyRow.Get();
	}

	return NULL;
}

void FDetailCategoryImpl::AddPropertyNode( TSharedRef<FPropertyNode> PropertyNode, FName InstanceName )
{
	FDetailLayoutCustomization NewCustomization;

	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( PropertyNode, AsShared() ) );
	AddDefaultLayout( NewCustomization, IsAdvancedLayout( NewCustomization ), InstanceName );
}

bool FDetailCategoryImpl::IsAdvancedLayout( const FDetailLayoutCustomization& LayoutInfo )
{
	bool bAdvanced = false;
	if( LayoutInfo.PropertyRow.IsValid() && LayoutInfo.GetPropertyNode().IsValid() && LayoutInfo.GetPropertyNode()->HasNodeFlags( EPropertyNodeFlags::IsAdvanced )  )
	{
		bAdvanced = true;
	}

	return bAdvanced;
}

void FDetailCategoryImpl::AddCustomLayout( const FDetailLayoutCustomization& LayoutInfo, bool bForAdvanced )
{
	GetLayoutForInstance( GetParentLayoutImpl().GetCurrentCustomizationVariableName() ).AddCustomLayout( LayoutInfo, bForAdvanced );
}

void FDetailCategoryImpl::AddDefaultLayout( const FDetailLayoutCustomization& LayoutInfo, bool bForAdvanced, FName InstanceName )
{
	GetLayoutForInstance( InstanceName ).AddDefaultLayout( LayoutInfo, bForAdvanced );
}

FDetailLayout& FDetailCategoryImpl::GetLayoutForInstance( FName InstanceName )
{
	return LayoutMap.FindOrAdd( InstanceName );
}

void FDetailCategoryImpl::OnAdvancedDropdownClicked()
{
	bUserShowAdvanced = !bUserShowAdvanced;

	GConfig->SetBool( TEXT("DetailCategoriesAdvanced"), *CategoryPathName, bUserShowAdvanced, GEditorUserSettingsIni );

	const bool bRefilterCategory = true;
	RefreshTree( bRefilterCategory );
}

bool FDetailCategoryImpl::ShouldShowAdvanced() const
{
	return bUserShowAdvanced || bForceAdvanced;
}

bool FDetailCategoryImpl::IsAdvancedDropdownEnabled() const
{
	return !bForceAdvanced;
}

void FDetailCategoryImpl::RequestItemExpanded( TSharedRef<IDetailTreeNode> TreeNode, bool bShouldBeExpanded )
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if( DetailLayoutBuilderPtr.IsValid() )
	{
		GetDetailsView().RequestItemExpanded( TreeNode, bShouldBeExpanded );
	}
}

void FDetailCategoryImpl::RefreshTree( bool bRefilterCategory )
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if( DetailLayoutBuilderPtr.IsValid() )
	{
		if( bRefilterCategory )
		{
			FilterNode( DetailLayoutBuilderPtr->GetCurrentFilter() );
		}

		GetDetailsView().RefreshTree();
	}
}

void FDetailCategoryImpl::AddTickableNode( IDetailTreeNode& TickableNode )
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if( DetailLayoutBuilderPtr.IsValid() )
	{
		DetailLayoutBuilderPtr->AddTickableNode( TickableNode );
	}
}

void FDetailCategoryImpl::RemoveTickableNode( IDetailTreeNode& TickableNode )
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if( DetailLayoutBuilderPtr.IsValid() )
	{
		DetailLayoutBuilderPtr->RemoveTickableNode( TickableNode );
	}
}

void FDetailCategoryImpl::SaveExpansionState( IDetailTreeNode& InTreeNode )
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if( DetailLayoutBuilderPtr.IsValid() )
	{
		bool bIsExpanded = InTreeNode.ShouldBeExpanded();

		FString Key = CategoryPathName;
		Key += TEXT(".");
		Key += InTreeNode.GetNodeName().ToString();

		DetailLayoutBuilderPtr->SaveExpansionState( Key, bIsExpanded );
	}
}

bool FDetailCategoryImpl::GetSavedExpansionState( IDetailTreeNode& InTreeNode ) const
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if( DetailLayoutBuilderPtr.IsValid() )
	{
		FString Key = CategoryPathName;
		Key += TEXT(".");
		Key += InTreeNode.GetNodeName().ToString();

		return DetailLayoutBuilderPtr->GetSavedExpansionState( Key );
	}

	return false;
}

bool FDetailCategoryImpl::ContainsOnlyAdvanced() const
{
	return SimpleChildNodes.Num() == 0 && AdvancedChildNodes.Num() > 0;
}

void FDetailCategoryImpl::SetDisplayName( FName InCategoryName, const FString& LocalizedNameOverride )
{
	if( !LocalizedNameOverride.IsEmpty() )
	{
		DisplayName = LocalizedNameOverride;
	}
	else
	{
		const UClass* BaseClass = GetParentLayoutImpl().GetDetailsView().GetBaseClass();
		// Use the base class name if there is one otherwise this is a generic category not specific to a class
		FName BaseClassName = BaseClass ? BaseClass->GetFName() : FName("Generic");

		FString CategoryStr = InCategoryName != NAME_None ? InCategoryName.ToString() : BaseClassName.ToString();
		FString SourceCategoryStr = EngineUtils::SanitizeDisplayName( CategoryStr, false );

		bool FoundText = false;
		FText DisplayNameText;
		if ( InCategoryName != NAME_None )
		{
			FoundText = FText::FindText( TEXT("DetailCategory.CategoryName"), InCategoryName.ToString(), /*OUT*/DisplayNameText );
		}
		else
		{
			FoundText = FText::FindText( TEXT("DetailCategory.ClassName"), BaseClassName.ToString(), /*OUT*/DisplayNameText );
		}

		if ( FoundText )
		{
			DisplayName = DisplayNameText.ToString();
		}
		else
		{
			DisplayName = SourceCategoryStr;
		}
	}
}


TSharedRef<ITableRow> FDetailCategoryImpl::GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities ) 
{
	return
		SNew( SDetailCategoryTableRow, AsShared(), OwnerTable )
		.DisplayName( GetDisplayName() )
		.HeaderContent( HeaderContentWidget );
}


void FDetailCategoryImpl::OnItemExpansionChanged( bool bIsExpanded )
{
	if( bRestoreExpansionState )
	{
		// Save the collapsed state of this section
		GConfig->SetBool( TEXT("DetailCategories"), *CategoryPathName, bIsExpanded, GEditorUserSettingsIni );
	}

	OnExpansionChangedDelegate.ExecuteIfBound( bIsExpanded );
}

bool FDetailCategoryImpl::ShouldBeExpanded() const
{
	if( bHasFilterStrings )
	{
		return true;
	}
	else if( bRestoreExpansionState )
	{
		bool bShouldBeExpanded = true;
		// Save the collapsed state of this section
		GConfig->GetBool( TEXT("DetailCategories"), *CategoryPathName, bShouldBeExpanded, GEditorUserSettingsIni );
		return bShouldBeExpanded;
	}
	else
	{
		return !bShouldBeInitiallyCollapsed;
	}
}

ENodeVisibility::Type FDetailCategoryImpl::GetVisibility() const
{
	return bHasVisibleDetails ? ENodeVisibility::Visible : ENodeVisibility::ForcedHidden;
}

bool IsCustomProperty( const TSharedPtr<FPropertyNode>& PropertyNode )
{
	// The property node is custom if it has a custom layout or if its a struct and any of its children have a custom layout
	bool bIsCustom = !PropertyNode.IsValid() || PropertyNode->HasNodeFlags( EPropertyNodeFlags::IsCustomized ) != 0;

	return bIsCustom;
}


void FDetailCategoryImpl::GenerateNodesFromCustomizations( const FCustomizationList& InCustomizationList, bool bDefaultLayouts, FDetailNodeList& OutNodeList, bool &bOutLastItemHasMultipleColumns )
{
	bOutLastItemHasMultipleColumns = false;
	for( int32 CustomizationIndex = 0; CustomizationIndex < InCustomizationList.Num(); ++CustomizationIndex )
	{
		const FDetailLayoutCustomization& Customization = InCustomizationList[CustomizationIndex];
		// When building default layouts cull default properties which have been customized
		if( Customization.IsValidCustomization() && ( !bDefaultLayouts || !IsCustomProperty( Customization.GetPropertyNode() ) ) )
		{
			bool bParentEnabled = true;
			TSharedRef<FDetailItemNode> NewNode = MakeShareable( new FDetailItemNode( Customization, AsShared(), bParentEnabled ) );
			NewNode->Initialize();

			if( CustomizationIndex == InCustomizationList.Num()-1 )
			{
				bOutLastItemHasMultipleColumns = NewNode->HasMultiColumnWidget();
			}

			OutNodeList.Add( NewNode );
		}
	}
}


bool FDetailCategoryImpl::GenerateChildrenForSingleLayout( const FName RequiredGroupName, bool bDefaultLayout, bool bNeedsGroup, const FCustomizationList& LayoutList, FDetailNodeList& OutChildren, bool& bOutLastItemHasMultipleColumns )
{
	bool bGeneratedAnyChildren = false;
	if( LayoutList.Num() > 0 )
	{
		FDetailNodeList GeneratedChildren;
		GenerateNodesFromCustomizations( LayoutList, bDefaultLayout, GeneratedChildren, bOutLastItemHasMultipleColumns );

		if( GeneratedChildren.Num() > 0 )
		{
			bGeneratedAnyChildren = true;
			if( bNeedsGroup )
			{
				TSharedRef<IDetailTreeNode> GroupNode = MakeShareable( new FDetailCategoryGroupNode( GeneratedChildren, RequiredGroupName, *this ) );
				OutChildren.Add( GroupNode );
			}
			else
			{
				OutChildren.Append( GeneratedChildren );
			}
		}
	}

	return bGeneratedAnyChildren;
}

void FDetailCategoryImpl::GenerateChildrenForLayouts()
{
	bool bHasAdvancedLayouts = false;
	bool bGeneratedAnyChildren = false;

	bool bLastItemHasMultipleColumns = false;
	// @todo Details each loop can be a function
	for( int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex )
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		const FName RequiredGroupName = Layout.GetInstanceName();

		bHasAdvancedLayouts |= Layout.HasAdvancedLayouts();

		const bool bDefaultLayout = false;
		const bool bShouldShowGroup = LayoutMap.ShouldShowGroup( RequiredGroupName );
		bGeneratedAnyChildren |= GenerateChildrenForSingleLayout( RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetCustomSimpleLayouts(), SimpleChildNodes, bLastItemHasMultipleColumns );
	}

	for( int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex )
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		const FName RequiredGroupName = Layout.GetInstanceName();
	
		const bool bDefaultLayout = true;
		const bool bShouldShowGroup = LayoutMap.ShouldShowGroup( RequiredGroupName );
		bGeneratedAnyChildren |= GenerateChildrenForSingleLayout( RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetDefaultSimpleLayouts(), SimpleChildNodes, bLastItemHasMultipleColumns );
	}

	TAttribute<bool> ShowAdvanced( this, &FDetailCategoryImpl::ShouldShowAdvanced );
	TAttribute<bool> IsEnabled( this, &FDetailCategoryImpl::IsAdvancedDropdownEnabled );
	if( bHasAdvancedLayouts )
	{
		for( int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex )
		{
			const FDetailLayout& Layout = LayoutMap[LayoutIndex];

			const FName RequiredGroupName = Layout.GetInstanceName();

			const bool bDefaultLayout = false;
			const bool bShouldShowGroup = LayoutMap.ShouldShowGroup( RequiredGroupName );
			bGeneratedAnyChildren |= GenerateChildrenForSingleLayout( RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetCustomAdvancedLayouts(), AdvancedChildNodes, bLastItemHasMultipleColumns );
		}

		for( int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex )
		{
			const FDetailLayout& Layout = LayoutMap[LayoutIndex];

			const FName RequiredGroupName = Layout.GetInstanceName();

			const bool bDefaultLayout = true;
			const bool bShouldShowGroup = LayoutMap.ShouldShowGroup( RequiredGroupName );
			bGeneratedAnyChildren |= GenerateChildrenForSingleLayout( RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetDefaultAdvancedLayouts(), AdvancedChildNodes, bLastItemHasMultipleColumns );
		}

		
	}

	// Generate nodes for advanced dropdowns
	{
		if( AdvancedChildNodes.Num() > 0 )
		{
			AdvancedDropdownNodeTop =  MakeShareable( new FAdvancedDropdownNode( *this, true )  ); 
		}
		
		const bool bShowSplitter = bLastItemHasMultipleColumns;
		// if we are forcing advanced mode disable the dropdown
		const bool bIsEnabled = !bForceAdvanced;
		AdvancedDropdownNodeBottom = MakeShareable( new FAdvancedDropdownNode( *this, ShowAdvanced, IsEnabled, AdvancedChildNodes.Num() > 0, SimpleChildNodes.Num() == 0, bShowSplitter ) ); 
	}
}


void FDetailCategoryImpl::GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren )
{
	for( int32 ChildIndex = 0; ChildIndex < SimpleChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<IDetailTreeNode>& Child = SimpleChildNodes[ChildIndex];
		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			if( Child->ShouldShowOnlyChildren() )
			{
				Child->GetChildren( OutChildren );
			}
			else
			{
				OutChildren.Add( Child );
			}
		}
	}

	if( ShouldShowAdvanced() )
	{
		if( AdvancedDropdownNodeTop.IsValid() )
		{
			OutChildren.Add( AdvancedDropdownNodeTop.ToSharedRef() );
		}

		for( int32 ChildIndex = 0; ChildIndex < AdvancedChildNodes.Num(); ++ChildIndex )
		{
			TSharedRef<IDetailTreeNode>& Child = AdvancedChildNodes[ChildIndex];
		
			if( Child->GetVisibility() == ENodeVisibility::Visible )
			{
				if( Child->ShouldShowOnlyChildren() )
				{
					Child->GetChildren( OutChildren );
				}
				else
				{
					OutChildren.Add( Child );
				}
			}
		}
	}

	if( AdvancedDropdownNodeBottom.IsValid() )
	{
		OutChildren.Add( AdvancedDropdownNodeBottom.ToSharedRef() );
	}
}

void FDetailCategoryImpl::FilterNode( const FDetailFilter& InFilter )
{
	bHasFilterStrings = InFilter.FilterStrings.Num() > 0;
	bForceAdvanced = bHasFilterStrings || InFilter.bShowAllAdvanced == true;

	bHasVisibleDetails = false;

	for( int32 ChildIndex = 0; ChildIndex < SimpleChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<IDetailTreeNode>& Child = SimpleChildNodes[ChildIndex];
		Child->FilterNode( InFilter );

		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			bHasVisibleDetails = true;
			RequestItemExpanded( Child, Child->ShouldBeExpanded() );
		}
	}

	for( int32 ChildIndex = 0; ChildIndex < AdvancedChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<IDetailTreeNode>& Child = AdvancedChildNodes[ChildIndex];
		Child->FilterNode( InFilter );

		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			bHasVisibleDetails = true;
			RequestItemExpanded( Child, Child->ShouldBeExpanded() );
		}

	}
}

void FDetailCategoryImpl::GenerateLayout()
{
	// Reset all children
	SimpleChildNodes.Empty();
	AdvancedChildNodes.Empty();
	AdvancedDropdownNodeTop.Reset();
	AdvancedDropdownNodeBottom.Reset();

	GenerateChildrenForLayouts();

	bHasVisibleDetails = SimpleChildNodes.Num() + AdvancedChildNodes.Num() > 0;
}

