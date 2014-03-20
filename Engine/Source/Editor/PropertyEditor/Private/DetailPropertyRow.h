// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDetailPropertyRow : public IDetailPropertyRow, public IStructCustomizationUtils, public TSharedFromThis<FDetailPropertyRow>
{
public:
	FDetailPropertyRow( TSharedPtr<FPropertyNode> InPropertyNode, TSharedRef<FDetailCategoryImpl> InParentCategory, TSharedPtr<FObjectPropertyNode> InExternalRootNode = NULL);

	/** IDetailPropertyRow interface */
	virtual IDetailPropertyRow& DisplayName( const FString& InDisplayName ) OVERRIDE;
	virtual IDetailPropertyRow& ToolTip( const FString& InToolTip ) OVERRIDE;
	virtual IDetailPropertyRow& ShowPropertyButtons( bool bInShowPropertyButtons ) OVERRIDE;
	virtual IDetailPropertyRow& EditCondition( TAttribute<bool> EditConditionValue, FOnBooleanValueChanged OnEditConditionValueChanged ) OVERRIDE;
	virtual IDetailPropertyRow& IsEnabled( TAttribute<bool> InIsEnabled ) OVERRIDE;
	virtual IDetailPropertyRow& Visibility( TAttribute<EVisibility> Visibility ) OVERRIDE;
	virtual IDetailPropertyRow& OverrideResetToDefault( TAttribute<bool> IsResetToDefaultVisible, FSimpleDelegate OnResetToDefaultClicked ) OVERRIDE;
	virtual FDetailWidgetRow& CustomWidget( bool bShowChildren = false ) OVERRIDE;
	virtual void GetDefaultWidgets( TSharedPtr<SWidget>& OutNameWidget, TSharedPtr<SWidget>& OutValueWidget ) OVERRIDE;
	virtual void GetDefaultWidgets( TSharedPtr<SWidget>& OutNameWidget, TSharedPtr<SWidget>& OutValueWidget, FDetailWidgetRow& Row ) OVERRIDE;

	/** IStructCustomizationUtils interface */
	virtual TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const OVERRIDE;

	/** @return true if this row has widgets with columns */
	bool HasColumns() const;

	/** @return true if this row shows only children and is not visible itself */
	bool ShowOnlyChildren() const;

	/** @return true if this row should be ticked */
	bool RequiresTick() const;

	/**
	 * Called when the owner node is initialized
	 *
	 * @param InParentCategory	The category that this property is in
	 * @param InIsParentEnabled	Whether or not our parent is enabled
	 */
	void OnItemNodeInitialized( TSharedRef<FDetailCategoryImpl> InParentCategory, const TAttribute<bool>& InIsParentEnabled );

	/** @return The widget row that should be displayed for this property row */
	FDetailWidgetRow GetWidgetRow();

	/**
	 * @return The property node for this row
	 */
	TSharedPtr<FPropertyNode> GetPropertyNode() { return PropertyNode; }

	/**
	 * Called when children of this row should be generated
	 *
	 * @param OutChildren	The list of children created
	 */
	void OnGenerateChildren( FDetailNodeList& OutChildren );

	/** 
	 * @return The visibility of this property
	 */
	EVisibility GetPropertyVisibility() const { return PropertyVisibility.Get(); }
private:
	/**
	 * Makes a name widget for the tree
	 */
	void MakeNameWidget( FDetailWidgetRow& Row, const TSharedPtr<FDetailWidgetRow> InCustomRow ) const;

	/**
	 * Makes the value widget for the tree
	 */
	void MakeValueWidget( FDetailWidgetRow& Row, const TSharedPtr<FDetailWidgetRow> InCustomRow, bool bAddWidgetDecoration = true ) const;

	/**
	 * @return true if this row has an edit condition
	 */
	bool HasEditCondition() const;

	/**
	 * @return Whether or not this row is enabled
	 */
	bool GetEnabledState() const;

	/**
	 * Generates children for the property node
	 *
	 * @param RootPropertyNode	The property node to generate children for (May be a child of the property node being stored)
	 * @param OutChildren	The list of children created 
	 */
	void GenerateChildrenForPropertyNode( TSharedPtr<FPropertyNode>& RootPropertyNode, FDetailNodeList& OutChildren );

	/**
	 * Makes a property editor from the property node on this row
	 *
	 * @param PropertyUtilities	Utilities for the property editor
	 */
	TSharedRef<FPropertyEditor> MakePropertyEditor( const TSharedRef<IPropertyUtilities>& PropertyUtilities );
private:
	/** User driven enabled state */
	TAttribute<bool> CustomIsEnabledAttrib;
	/** Whether or not our parent is enabled */
	TAttribute<bool> IsParentEnabled;
	/** Visibility of the property */
	TAttribute<EVisibility> PropertyVisibility;
	/** If the property on this row is a customized struct, this is the interface to that customization */
	TSharedPtr<IStructCustomization> CustomStructInterface;
	/** Builder for children of a customized struct */
	TSharedPtr<class FCustomChildrenBuilder> StructLayoutBuilder;
	/** The property handle for this row */
	TSharedPtr<IPropertyHandle> PropertyHandle;
	/** The property node for this row */
	TSharedPtr<FPropertyNode> PropertyNode;
	/** The property editor for this row */
	TSharedPtr<FPropertyEditor> PropertyEditor;
	/** Custom widgets to use for this row instead of the default ones */
	TSharedPtr<FDetailWidgetRow> CustomPropertyWidget;
	/** User customized edit condition */
	TSharedPtr<struct FCustomEditCondition> CustomEditCondition;
	/** User customized reset to default */
	TSharedPtr<struct FCustomResetToDefault> CustomResetToDefault;
	/** The category this row resides in */
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
	/** Root of the property node if this node comes from an external tree */
	TSharedPtr<FObjectPropertyNode> ExternalRootNode;
	/** Whether or not to show standard property buttons */
	bool bShowPropertyButtons;
	/** True to show custom property children */
	bool bShowCustomPropertyChildren;
};
