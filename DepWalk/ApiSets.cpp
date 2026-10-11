#include "pch.h"
#include "ApiSets.h"
#include <winternl.h>

namespace {
	//
	// API set schema version 6 (Windows 10 and later), as mapped by the loader into every process
	//
	struct API_SET_NAMESPACE {
		ULONG Version;
		ULONG Size;
		ULONG Flags;
		ULONG Count;
		ULONG EntryOffset;
		ULONG HashOffset;
		ULONG HashFactor;
	};

	struct API_SET_NAMESPACE_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG HashedLength;
		ULONG ValueOffset;
		ULONG ValueCount;
	};

	struct API_SET_VALUE_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG ValueOffset;
		ULONG ValueLength;
	};

	// PEB::ApiSetMap is undocumented; winternl.h exposes it as the first of the Reserved9 slots
	static_assert(offsetof(PEB, Reserved9) == (sizeof(void*) == 8 ? 0x68 : 0x38));

	void ToLower(std::wstring& s) {
		for (auto& c : s)
			c = towlower(c);
	}

	API_SET_NAMESPACE const* GetSchema() {
		auto ns = (API_SET_NAMESPACE const*)NtCurrentTeb()->ProcessEnvironmentBlock->Reserved9[0];
		return ns && ns->Version == 6 ? ns : nullptr;
	}

	struct ApiSetHosts {
		std::wstring DefaultHost;
		std::vector<std::pair<std::wstring, std::wstring>> Exceptions;	// importing module -> host
	};

	std::unordered_map<std::wstring, ApiSetHosts> BuildHostMap() {
		std::unordered_map<std::wstring, ApiSetHosts> hosts;
		auto ns = GetSchema();
		if (ns == nullptr)
			return hosts;

		auto base = (BYTE const*)ns;
		auto entries = (API_SET_NAMESPACE_ENTRY const*)(base + ns->EntryOffset);
		hosts.reserve(ns->Count);
		for (ULONG i = 0; i < ns->Count; i++) {
			auto& entry = entries[i];
			// the hashed part of the name excludes the last "-N" version component, which the loader ignores
			std::wstring name((PCWSTR)(base + entry.NameOffset), entry.HashedLength / sizeof(WCHAR));
			ToLower(name);

			ApiSetHosts entryHosts;
			auto values = (API_SET_VALUE_ENTRY const*)(base + entry.ValueOffset);
			for (ULONG j = 0; j < entry.ValueCount; j++) {
				auto& value = values[j];
				std::wstring host((PCWSTR)(base + value.ValueOffset), value.ValueLength / sizeof(WCHAR));
				// the default host has no importing module name; others are per-importer exceptions
				if (value.NameLength == 0)
					entryHosts.DefaultHost = std::move(host);
				else
					entryHosts.Exceptions.emplace_back(std::wstring((PCWSTR)(base + value.NameOffset), value.NameLength / sizeof(WCHAR)), std::move(host));
			}
			hosts.emplace(std::move(name), std::move(entryHosts));
		}
		return hosts;
	}
}

bool IsApiSetSchemaSupported() {
	return GetSchema() != nullptr;
}

std::optional<std::wstring> ResolveApiSetHost(PCWSTR apiSetName, PCWSTR importer) {
	static auto const hosts = BuildHostMap();

	std::wstring key(apiSetName);
	ToLower(key);
	if (key.ends_with(L".dll"))
		key.resize(key.size() - 4);
	if (auto pos = key.rfind(L'-'); pos != std::wstring::npos)
		key.resize(pos);

	auto it = hosts.find(key);
	if (it == hosts.end())
		return std::nullopt;

	if (importer) {
		for (auto& [name, host] : it->second.Exceptions)
			if (_wcsicmp(name.c_str(), importer) == 0)
				return host;
	}
	return it->second.DefaultHost;
}
