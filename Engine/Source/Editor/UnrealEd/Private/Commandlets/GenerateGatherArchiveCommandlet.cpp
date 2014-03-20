// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ISourceControlModule.h"
#include "Json.h"
#include "Internationalization/InternationalizationArchive.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "InternationalizationArchiveJsonSerializer.h"
#include "InternationalizationManifestJsonSerializer.h"


DEFINE_LOG_CATEGORY_STATIC(LogGenerateArchiveCommandlet, Log, All);

/**
 *	UGenerateGatherArchiveCommandlet
 */
UGenerateGatherArchiveCommandlet::UGenerateGatherArchiveCommandlet(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

int32 UGenerateGatherArchiveCommandlet::Main( const FString& Params )
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateArchiveCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateArchiveCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Get source path.
	FString SourcePath;
	if( !( GetConfigString( *SectionName, TEXT("SourcePath"), SourcePath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateArchiveCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Get manifest name.
	FString ManifestName;
	if( !GetConfigString( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) )
	{
		UE_LOG( LogGenerateArchiveCommandlet, Error, TEXT("No manifest name specified.") );
		return -1;
	}

	// Get source culture.
	FString SourceCulture;
	if( GetConfigString( *SectionName, TEXT("SourceCulture"), SourceCulture, GatherTextConfigPath ) )
	{
		if( FInternationalization::GetCulture( SourceCulture ).IsValid() )
		{
			UE_LOG(LogGenerateArchiveCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(SourceCulture) );
		}
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	GetConfigArray(*SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath);
	
	if( CulturesToGenerate.Num() == 0 )
	{
		UE_LOG(LogGenerateArchiveCommandlet, Error, TEXT("No cultures specified for generation."));
		return -1;
	}

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( FInternationalization::GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogGenerateArchiveCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]) );
		}
	}

	// Get destination path.
	FString DestinationPath;
	if( !GetConfigString( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) )
	{
		UE_LOG( LogGenerateArchiveCommandlet, Error, TEXT("No destination path specified.") );
		return -1;
	}

	// Get archive name.
	FString ArchiveName;
	if( !( GetConfigString(* SectionName, TEXT("ArchiveName"), ArchiveName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateArchiveCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get bPurgeOldEmptyEntries option.
	bool ShouldPurgeOldEmptyEntries;
	if ( !GetConfigBool( *SectionName, TEXT("bPurgeOldEmptyEntries"), ShouldPurgeOldEmptyEntries, GatherTextConfigPath) )
	{
		ShouldPurgeOldEmptyEntries = false;
	}

	FString ManifestFilePath = DestinationPath / ManifestName;
	TSharedPtr<FJsonObject> ManifestJsonObject = ReadJSONTextFile( ManifestFilePath );

	if( !ManifestJsonObject.IsValid() )
	{
		UE_LOG(LogGenerateArchiveCommandlet, Error, TEXT("Could not read manifest file %s."), *ManifestFilePath);
		return -1;
	}

	FInternationalizationManifestJsonSerializer ManifestSerializer;
	TSharedRef< FInternationalizationManifest > InternationalizationManifest = MakeShareable( new FInternationalizationManifest );

	ManifestSerializer.DeserializeManifest( ManifestJsonObject.ToSharedRef(), InternationalizationManifest );

	for(int32 Culture = 0; Culture < CulturesToGenerate.Num(); Culture++)
	{
		TSharedRef< FInternationalizationArchive > InternationalizationArchive = MakeShareable( new FInternationalizationArchive );
		BuildArchiveFromManifest( InternationalizationManifest, InternationalizationArchive, SourceCulture, CulturesToGenerate[Culture] );

		const FString CulturePath = DestinationPath / CulturesToGenerate[Culture];
		FInternationalizationArchiveJsonSerializer ArchiveSerializer;
		TSharedRef< FInternationalizationArchive > OutputInternationalizationArchive = MakeShareable( new FInternationalizationArchive );

		// Read in any existing archive for this culture.
		FString ExistingArchiveFileName = CulturePath / ArchiveName;
		TSharedPtr< FJsonObject > ExistingArchiveJsonObject = NULL;
			
		if( FPaths::FileExists(ExistingArchiveFileName) )
		{
			ExistingArchiveJsonObject = ReadJSONTextFile( ExistingArchiveFileName );
			
			// Some of the existing archives were saved out with an "Unnamed" namespace for the root instead of the empty string.  We try to fix that here.
			if( ExistingArchiveJsonObject->HasField( FInternationalizationArchiveJsonSerializer::TAG_NAMESPACE ) )
			{
				FString RootNamespace = ExistingArchiveJsonObject->GetStringField( FInternationalizationArchiveJsonSerializer::TAG_NAMESPACE );
				if( RootNamespace == TEXT("Unnamed") )
				{
					ExistingArchiveJsonObject->RemoveField( FInternationalizationArchiveJsonSerializer::TAG_NAMESPACE );
					ExistingArchiveJsonObject->SetStringField( FInternationalizationArchiveJsonSerializer::TAG_NAMESPACE, TEXT("") );
				}
			}

			struct Local
			{
				// Purges this JSONObject of an entries with no translated text and purges empty namespaces.
				// Returns true if the object was modified, false if not.
				static bool PurgeNamespaceOfEmptyEntries(const TSharedPtr<FJsonObject>& JSONObject)
				{
					bool ModifiedChildrenArray = false;
					if( JSONObject->HasField( FInternationalizationArchiveJsonSerializer::TAG_CHILDREN ) )
					{
						TArray<TSharedPtr<FJsonValue>> ChildrenArray = JSONObject->GetArrayField(FInternationalizationArchiveJsonSerializer::TAG_CHILDREN);
						for( int32 ChildIndex = ChildrenArray.Num() - 1; ChildIndex >= 0; --ChildIndex )
						{
							TSharedPtr<FJsonObject> Child = ChildrenArray[ ChildIndex ]->AsObject();
							TSharedPtr<FJsonObject> TranslationObject = Child->GetObjectField(FInternationalizationArchiveJsonSerializer::TAG_TRANSLATION);

							const FString& TranslatedText = TranslationObject->GetStringField(FInternationalizationArchiveJsonSerializer::TAG_TRANSLATION_TEXT);

							if(TranslatedText.IsEmpty())
							{
								ChildrenArray.RemoveAt( ChildIndex );
								Child = NULL;
								ModifiedChildrenArray = true;
							}
						}
						if(ModifiedChildrenArray)
						{
							JSONObject->RemoveField(FInternationalizationArchiveJsonSerializer::TAG_CHILDREN);
							if(ChildrenArray.Num())
							{
								JSONObject->SetArrayField(FInternationalizationArchiveJsonSerializer::TAG_CHILDREN, ChildrenArray);
							}
						}
					}

					bool ModifiedSubnamespaceArray = false;
					if( JSONObject->HasField( FInternationalizationArchiveJsonSerializer::TAG_SUBNAMESPACES ) )
					{
						TArray<TSharedPtr<FJsonValue>> SubnamespaceArray = JSONObject->GetArrayField(FInternationalizationArchiveJsonSerializer::TAG_SUBNAMESPACES);

						for( int32 Index = SubnamespaceArray.Num() - 1; Index >= 0; --Index )
						{
							TSharedPtr<FJsonObject> Subnamespace = SubnamespaceArray[ Index ]->AsObject();
							ModifiedSubnamespaceArray = PurgeNamespaceOfEmptyEntries(Subnamespace);

							bool HasChildren = Subnamespace->HasField( FInternationalizationArchiveJsonSerializer::TAG_CHILDREN );
							bool HasSubnamespaces = Subnamespace->HasField( FInternationalizationArchiveJsonSerializer::TAG_SUBNAMESPACES );

							if(!HasChildren && !HasSubnamespaces)
							{
								SubnamespaceArray.RemoveAt( Index );
								Subnamespace = NULL;
								ModifiedSubnamespaceArray = true;
							}
						}
						if(ModifiedSubnamespaceArray)
						{
							JSONObject->RemoveField(FInternationalizationArchiveJsonSerializer::TAG_SUBNAMESPACES);
							if(SubnamespaceArray.Num())
							{
								JSONObject->SetArrayField(FInternationalizationArchiveJsonSerializer::TAG_SUBNAMESPACES, SubnamespaceArray);
							}
						}
					}

					return ModifiedChildrenArray || ModifiedSubnamespaceArray;
				}
			};

			if(ShouldPurgeOldEmptyEntries)
			{
				// Remove entries lacking translations from pre-existing archive.
				// If they are absent in the source manifest, we save on not translating non-existent text.
				// If they are present in the source manifest, then the newly generated entries will contain the empty text again.
				Local::PurgeNamespaceOfEmptyEntries(ExistingArchiveJsonObject);
			}

			ArchiveSerializer.DeserializeArchive( ExistingArchiveJsonObject.ToSharedRef(), OutputInternationalizationArchive );
		}

		// Combine the generated gather archive with the contents of the archive structure we will write out.
		AppendArchiveData( InternationalizationArchive, OutputInternationalizationArchive );

		TSharedRef< FJsonObject > OutputArchiveJsonObj = MakeShareable( new FJsonObject );
		ArchiveSerializer.SerializeArchive( OutputInternationalizationArchive, OutputArchiveJsonObj );

		if( !WriteArchiveToFile( OutputArchiveJsonObj, DestinationPath, *CulturesToGenerate[Culture], *ArchiveName ) )
		{
			UE_LOG( LogGenerateArchiveCommandlet, Error,TEXT("Failed to write archive to %s."), *DestinationPath );				
			return -1;
		}
	}
	
	return 0;
}

bool UGenerateGatherArchiveCommandlet::WriteArchiveToFile(TSharedRef< FJsonObject > ManifestObject, const FString& OutputDirectoryPath, const TCHAR* Culture, const FString& FileName)
{
	FString FullOutPath = FPaths::ConvertRelativePathToFull(OutputDirectoryPath) / Culture / FileName;

	if( !WriteJSONToTextFile(ManifestObject, FullOutPath, SourceControlInfo ) )
	{
		return false;
	}

	UE_LOG(LogGenerateArchiveCommandlet, Log, TEXT("Writing archive to %s."), *OutputDirectoryPath);

	return true;
}


void UGenerateGatherArchiveCommandlet::BuildArchiveFromManifest( TSharedRef< const FInternationalizationManifest > InManifest, TSharedRef< FInternationalizationArchive > Archive, const FString& SourceCulture, const FString& TargetCulture )
{
	for(TManifestEntryByContextIdContainer::TConstIterator It( InManifest->GetEntriesByContextIdIterator() ); It; ++It)
	{
		const TSharedRef<FManifestEntry> UnstructuredManifestEntry = It.Value();
		const FString& Namespace = UnstructuredManifestEntry->Namespace;
		const FLocItem& Source = UnstructuredManifestEntry->Source; 

		for( auto ContextIter = UnstructuredManifestEntry->Contexts.CreateConstIterator(); ContextIter; ++ContextIter )
		{
			const FContext& Context = *ContextIter;

			// We only add the non-optional entries
			if( !Context.bIsOptional )
			{
				FLocItem Translation = Source;

				if( SourceCulture != TargetCulture)
				{
					// We want to process the translation before adding it to the archive
					ConditionTranslation( Translation );
				}

				// We also condition the source object
				FLocItem ConditionedSource = Source;
				ConditionSource( ConditionedSource );

				Archive->AddEntry(Namespace, ConditionedSource, Translation, Context.KeyMetadataObj, Context.bIsOptional );
					
			}
		}
	}
}

void UGenerateGatherArchiveCommandlet::AppendArchiveData( TSharedRef< const FInternationalizationArchive > InArchiveToAppend, TSharedRef< FInternationalizationArchive > ArchiveCombined )
{
	for(TArchiveEntryContainer::TConstIterator It( InArchiveToAppend->GetEntryIterator() ); It; ++It)
	{
		const TSharedRef<FArchiveEntry> EntryToAppend = It.Value();
		ArchiveCombined->AddEntry( EntryToAppend );
	}
}

void ConditionTranslationMetadata( TSharedRef<FLocMetadataValue> MetadataValue )
{
	switch( MetadataValue->Type )
	{
	case ELocMetadataType::String:
		{
			TSharedPtr<FLocMetadataValue> MetadataValuePtr = MetadataValue;
			TSharedPtr<FLocMetadataValueString> MetadataString = StaticCastSharedPtr<FLocMetadataValueString>( MetadataValuePtr );
			MetadataString->SetString( TEXT("") );
		}
		break;

	case ELocMetadataType::Array:
		{
			TArray< TSharedPtr< FLocMetadataValue > > MetadataArray = MetadataValue->AsArray();
			for( auto ArrayIter = MetadataArray.CreateIterator(); ArrayIter; ++ArrayIter )
			{
				TSharedPtr<FLocMetadataValue>& Item = *ArrayIter;
				if( Item.IsValid() )
				{
					ConditionTranslationMetadata( Item.ToSharedRef() );
				}
			}
		}
		break;

	case ELocMetadataType::Object:
		{
			TSharedPtr< FLocMetadataObject > MetadataObject = MetadataValue->AsObject();
			for( auto ValueIter = MetadataObject->Values.CreateConstIterator(); ValueIter; ++ValueIter )
			{
				const FString Name = (*ValueIter).Key;
				TSharedPtr< FLocMetadataValue > Value = (*ValueIter).Value;
				if( Value.IsValid() )
				{
					if( Value->Type == ELocMetadataType::String )
					{
						MetadataObject->SetStringField( Name, TEXT("") );
					}
					else
					{
						ConditionTranslationMetadata( Value.ToSharedRef() );
					}
				}
			}
		}

	default:
		break;
	}
}

void UGenerateGatherArchiveCommandlet::ConditionTranslation( FLocItem& LocItem )
{
	// We clear out the translation text because this should only be entered by translators
	LocItem.Text = TEXT("");

	// The translation might have metadata so we want to clear all the values of any string metadata
	if( LocItem.MetadataObj.IsValid() )
	{
		ConditionTranslationMetadata( MakeShareable( new FLocMetadataValueObject( LocItem.MetadataObj ) ) );
	}
	
}

void ConditionSourceMetadata( TSharedRef<FLocMetadataValue> MetadataValue )
{
	switch( MetadataValue->Type )
	{
	case ELocMetadataType::Object:
		{
			TSharedPtr< FLocMetadataObject > MetadataObject = MetadataValue->AsObject();

			TArray< FString > NamesToBeReplaced;

			// We go through all the source metadata entries and look for any that have names prefixed with a '*'.  If we
			//  find any, we will replace them with a String type that contains an empty value
			for( auto Iter = MetadataObject->Values.CreateConstIterator(); Iter; ++Iter )
			{
				const FString& Name = (*Iter).Key;
				TSharedPtr< FLocMetadataValue > Value = (*Iter).Value;

				if( Name.StartsWith( FLocMetadataObject::COMPARISON_MODIFIER_PREFIX ) )
				{
					NamesToBeReplaced.Add(Name);
				}
				else
				{
					ConditionSourceMetadata( Value.ToSharedRef() );
				}
			}

			for( auto Iter = NamesToBeReplaced.CreateConstIterator(); Iter; ++Iter )
			{
				MetadataObject->RemoveField( *Iter );
				MetadataObject->SetStringField( *Iter, TEXT("") );
			}
		}

	default:
		break;
	}
}

void UGenerateGatherArchiveCommandlet::ConditionSource( FLocItem& LocItem )
{
	if( LocItem.MetadataObj.IsValid() )
	{
		if( LocItem.MetadataObj.IsValid() )
		{
			ConditionSourceMetadata( MakeShareable( new FLocMetadataValueObject( LocItem.MetadataObj ) ) );
		}
	}
}
