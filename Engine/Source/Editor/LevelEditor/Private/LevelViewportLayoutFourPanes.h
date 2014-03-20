// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __LevelViewportLayoutFourPanes_h__
#define __LevelViewportLayoutFourPanes_h__

#pragma once

#include "LevelViewportLayout.h"

class FLevelViewportLayoutFourPanes : public FLevelViewportLayout
{
public:
	/**
	 * Saves viewport layout information between editor sessions
	 */
	virtual void SaveLayoutString(const FString& LayoutString) const OVERRIDE;
protected:
	/**
	 * Creates the viewports and splitter for the four-pane layout              
	 */
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) OVERRIDE;

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1) = 0;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) OVERRIDE;


protected:
	/** The splitter widgets */
	TSharedPtr< class SSplitter > PrimarySplitterWidget;
	TSharedPtr< class SSplitter > SecondarySplitterWidget;
};


// FLevelViewportLayoutFourPanesLeft /////////////////////////////

class FLevelViewportLayoutFourPanesLeft : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const OVERRIDE { return LevelViewportConfigurationNames::FourPanesLeft; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1);
};


// FLevelViewportLayoutFourPanesRight /////////////////////////////

class FLevelViewportLayoutFourPanesRight : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const OVERRIDE { return LevelViewportConfigurationNames::FourPanesRight; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1);
};


// FLevelViewportLayoutFourPanesTop /////////////////////////////

class FLevelViewportLayoutFourPanesTop : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const OVERRIDE { return LevelViewportConfigurationNames::FourPanesTop; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1);
};


// FLevelViewportLayoutFourPanesBottom /////////////////////////////

class FLevelViewportLayoutFourPanesBottom : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const OVERRIDE { return LevelViewportConfigurationNames::FourPanesBottom; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1);
};

#endif
