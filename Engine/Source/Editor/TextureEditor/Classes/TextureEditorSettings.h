// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureEditorSettings.h: Declares the UTextureEditorSettings class.
=============================================================================*/

#pragma once


#include "TextureEditorSettings.generated.h"


/**
 * Enumerates background for the texture editor view port.
 */
UENUM()
enum ETextureEditorBackgrounds
{
	TextureEditorBackground_SolidColor UMETA(DisplayName="Solid Color"),
	TextureEditorBackground_Checkered UMETA(DisplayName="Checkered"),
	TextureEditorBackground_CheckeredFill UMETA(DisplayName="Checkered (Fill)")
};


/**
 * Implements the Editor's user settings.
 */
UCLASS(config=EditorUserSettings)
class TEXTUREEDITOR_API UTextureEditorSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * The type of background to draw in the texture editor view port.
	 */
	UPROPERTY(config)
	TEnumAsByte<ETextureEditorBackgrounds> Background;

	/**
	 * Background and foreground color used by Texture preview view ports.
	 */
	UPROPERTY(config, EditAnywhere, Category=Background)
	FColor BackgroundColor;

	/**
	 * The first color of the checkered background.
	 */
	UPROPERTY(config, EditAnywhere, Category=Background)
	FColor CheckerColorOne;

	/**
	 * The second color of the checkered background.
	 */
	UPROPERTY(config, EditAnywhere, Category=Background)
	FColor CheckerColorTwo;

	/**
	 * The size of the checkered background tiles.
	 */
	UPROPERTY(config, EditAnywhere, Category=Background)
	int32 CheckerSize;

public:

	/**
	 * Whether the texture should fill the view port.
	 */
	UPROPERTY(config)
	bool FillViewport;

	/**
	 * Color to use for the texture border, if enabled.
	 */
	UPROPERTY(config, EditAnywhere, Category=TextureBorder)
	FColor TextureBorderColor;

	/**
	 * If true, displays a border around the texture.
	 */
	UPROPERTY(config)
	bool TextureBorderEnabled;
};
