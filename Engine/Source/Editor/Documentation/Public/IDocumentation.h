// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IDocumentationModule.h"
#include "IDocumentationPage.h"

/** Invoked when someone clicks on a hyperlink. */
DECLARE_DELEGATE_OneParam( FOnNavigate, const FString& )

class FDocumentationStyle
{
public:
	FDocumentationStyle()
		: ContentStyleName(TEXT("Documentation.Content"))
		, BoldContentStyleName(TEXT("Documentation.BoldContent"))
		, NumberedContentStyleName(TEXT("Documentation.NumberedContent"))
		, Header1StyleName(TEXT("Documentation.Header1"))
		, Header2StyleName(TEXT("Documentation.Header2"))
		, HyperlinkButtonStyleName(TEXT("Documentation.Hyperlink.Button"))
		, HyperlinkTextStyleName(TEXT("Documentation.Hyperlink.Text"))
		, SeparatorStyleName(TEXT("Documentation.Separator"))
	{
	}

	/** Set the content style for this documentation */
	FDocumentationStyle& ContentStyle(const FName& InName) 
	{
		ContentStyleName = InName;
		return *this;
	}

	/** Set the bold content style for this documentation */
	FDocumentationStyle& BoldContentStyle(const FName& InName) 
	{
		BoldContentStyleName = InName;
		return *this;
	}

	/** Set the numbered content style for this documentation */
	FDocumentationStyle& NumberedContentStyle(const FName& InName) 
	{
		NumberedContentStyleName = InName;
		return *this;
	}

	/** Set the header 1 style for this documentation */
	FDocumentationStyle& Header1Style(const FName& InName) 
	{
		Header1StyleName = InName;
		return *this;
	}

	/** Set the header 2 style for this documentation */
	FDocumentationStyle& Header2Style(const FName& InName) 
	{
		Header2StyleName = InName;
		return *this;
	}

	/** Set the hyperlink button style for this documentation */
	FDocumentationStyle& HyperlinkButtonStyle(const FName& InName) 
	{
		HyperlinkButtonStyleName = InName;
		return *this;
	}

	/** Set the hyperlink text style for this documentation */
	FDocumentationStyle& HyperlinkTextStyle(const FName& InName) 
	{
		HyperlinkTextStyleName = InName;
		return *this;
	}

	/** Set the separator style for this documentation */
	FDocumentationStyle& SeparatorStyle(const FName& InName) 
	{
		SeparatorStyleName = InName;
		return *this;
	}

	/** Content text style */
	FName ContentStyleName;

	/** Bold content text style */
	FName BoldContentStyleName;

	/** Numbered content text style */
	FName NumberedContentStyleName;

	/** Header1 text style */
	FName Header1StyleName;

	/** Header2 text style */
	FName Header2StyleName;

	/** Hyperlink button style */
	FName HyperlinkButtonStyleName;

	/** Hyperlink text style */
	FName HyperlinkTextStyleName;

	/** Separator style name */
	FName SeparatorStyleName;
};

class FParserConfiguration
{
public:
	static TSharedRef< FParserConfiguration > Create()
	{
		return MakeShareable( new FParserConfiguration() );
	}

public:
	~FParserConfiguration() {}

	FOnNavigate OnNavigate;

private:
	FParserConfiguration() {}
};

class IDocumentation
{
public:

	static inline TSharedRef< IDocumentation > Get()
	{
		IDocumentationModule& Module = FModuleManager::LoadModuleChecked< IDocumentationModule >( "Documentation" );
		return Module.GetDocumentation();
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "Documentation" );
	}

public:

	virtual bool OpenHome() const = 0;

	virtual bool OpenAPIHome() const = 0;

	virtual bool Open( const FString& Link ) const = 0;

	virtual TSharedRef< class SWidget > CreateAnchor( const FString& Link, const FString& PreviewLink = FString(), const FString& PreviewExcerptName = FString() ) const = 0;

	virtual TSharedRef< class IDocumentationPage > GetPage( const FString& Link, const TSharedPtr< FParserConfiguration >& Config, const FDocumentationStyle& Style = FDocumentationStyle() ) = 0;

	virtual bool PageExists(const FString& Link) const = 0;

	virtual TSharedRef< class SToolTip > CreateToolTip( const TAttribute<FText>& Text, const TSharedPtr<SWidget>& OverrideContent, const FString& Link, const FString& ExcerptName ) const = 0;
};
