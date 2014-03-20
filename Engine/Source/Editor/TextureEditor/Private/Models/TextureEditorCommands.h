// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureEditorCommands.h: Declares the FTextureEditorCommands class.
=============================================================================*/

#pragma once


/**
 * Holds the UI commands for the TextureEditorToolkit widget.
 */
class FTextureEditorCommands
	: public TCommands<FTextureEditorCommands>
{
public:

	/**
	 * Default constructor.
	 */
	FTextureEditorCommands( ) 
		: TCommands<FTextureEditorCommands>("TextureEditor", NSLOCTEXT("Contexts", "TextureEditor", "Texture Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{ }

public:

	/** Initialize commands */
	virtual void RegisterCommands() OVERRIDE;
	
public:

	/** Toggles the red channel */
	TSharedPtr<FUICommandInfo> RedChannel;
	
	/** Toggles the green channel */
	TSharedPtr<FUICommandInfo> GreenChannel;
	
	/** Toggles the blue channel */
	TSharedPtr<FUICommandInfo> BlueChannel;
	
	/** Toggles the alpha channel */
	TSharedPtr<FUICommandInfo> AlphaChannel;

	/** Toggles color saturation */
	TSharedPtr<FUICommandInfo> Saturation;

	/** If enabled, the texture will be scaled to fit the viewport */
	TSharedPtr<FUICommandInfo> FillViewport;

	/** Sets the checkered background pattern */
	TSharedPtr<FUICommandInfo> CheckeredBackground;

	/** Sets the checkered background pattern (filling the view port) */
	TSharedPtr<FUICommandInfo> CheckeredBackgroundFill;

	/** Sets the solid color background */
	TSharedPtr<FUICommandInfo> SolidBackground;

	/** If enabled, a border is drawn around the texture */
	TSharedPtr<FUICommandInfo> TextureBorder;

	/** Compress the texture */
	TSharedPtr<FUICommandInfo> CompressNow;

	/** Reimports the texture */
	TSharedPtr<FUICommandInfo> Reimport;

	/** Open the texture editor settings. */
	TSharedPtr< FUICommandInfo > Settings;
};
