#pragma once

#include <optional>

// True if the running OS uses an API set schema version this code can parse (v6, Windows 10 and later).
bool IsApiSetSchemaSupported();

// Returns the host DLL implementing an API set contract (e.g. "api-ms-win-core-file-l1-2-0.dll" -> "kernelbase.dll")
// when imported by the module file name `importer` (e.g. "kernel32.dll"), applying the schema's per-importer exceptions;
// with no importer, returns the default host. The host is empty for contracts the schema lists without one.
// Returns nullopt if the contract isn't in the schema (the loader would fail to resolve it) or the schema isn't supported.
std::optional<std::wstring> ResolveApiSetHost(PCWSTR apiSetName, PCWSTR importer = nullptr);
