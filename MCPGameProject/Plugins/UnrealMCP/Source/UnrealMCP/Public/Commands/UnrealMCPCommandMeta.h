#pragma once

#include "CoreMinimal.h"

/**
 * Metadata for a single command parameter, used by the help system.
 */
struct FMCPParamMeta
{
	FString Name;
	FString Type;       // "string", "number", "bool", "object", "array", "any"
	bool bRequired;
	FString Description;
};

/**
 * Metadata for a single MCP command, used by the help system.
 * Each command handler class provides its own metadata via GetCommandMetadata().
 */
struct FMCPCommandMeta
{
	FString Name;
	FString Category;
	FString Description;
	TArray<FMCPParamMeta> Params;
};
