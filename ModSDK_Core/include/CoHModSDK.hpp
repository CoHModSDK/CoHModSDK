/**
 *  CoHModSDK - The lightweight modding SDK for Company of Heroes
 *  Copyright (c) 2026 Tosox
 *
 *  This project is licensed under the Creative Commons
 *  Attribution-NonCommercial-NoDerivatives 4.0 International License
 *  (CC BY-NC-ND 4.0) with additional permissions.
 *
 *  Independent mods using this project only through its public interfaces
 *  are not required to use CC BY-NC-ND 4.0.
 *
 *  See the repository root LICENSE file for the full license text and
 *  additional permissions.
 */

#pragma once

#include <Windows.h>

extern "C" {
	/**
	 * @brief Called once when the SDK loads the mod DLL.
	 *
	 * Perform any early setup required for the mod here (e.g., install hooks, patch memory).
	 */
	__declspec(dllexport) void OnSDKLoad();

	/**
	 * @brief Called when the game is starting (after all mods have been loaded).
	 *
	 * Use this to initialize features that require the game to be fully running.
	 */
	__declspec(dllexport) void OnGameStart();

	/**
	 * @brief Called when the game is shutting down.
	 *
	 * Use this to clean up any hooks, memory patches, or resources before unloading.
	 */
	__declspec(dllexport) void OnGameShutdown();

	/**
	 * @brief Returns the display name of the mod.
	 *
	 * @return const char* - Name of the mod.
	 */
	__declspec(dllexport) const char* GetModName();

	/**
	 * @brief Returns the version string of the mod.
	 *
	 * @return const char* - Version of the mod.
	 */
	__declspec(dllexport) const char* GetModVersion();

	/**
	 * @brief Returns the author name(s) of the mod.
	 *
	 * @return const char* - Author or team name.
	 */
	__declspec(dllexport) const char* GetModAuthor();
}

namespace ModSDK {
	namespace Memory {
		/**
		 * @brief Returns a handle to the module that contains the original game code.
		 *
		 * @return HMODULE - Handle to `WW2Mod.original.dll` with fallback to `WW2Mod.dll`.
		 */
		HMODULE GetGameModuleHandle();

		/**
		 * @brief Scans a module for a byte pattern signature.
		 *
		 * Use `GetGameModuleHandle()` when you want to scan the original game module.
		 *
		 * @param moduleHandle Handle to the module to scan.
		 * @param signature Pattern string (e.g., "48 8B ?? ?? ?? ?? ?? 48 8B").
		 * @param reportError Whether to show an error if the pattern is not found.
		 * @return std::uintptr_t Address where the pattern was found or 0 if not found.
		 */
		std::uintptr_t FindPattern(HMODULE moduleHandle, const char* signature, bool reportError = true);

		/**
		 * @brief Patches memory by copying bytes to a destination address.
		 *
		 * Automatically changes memory protection to allow writing.
		 *
		 * @param destination Target address to patch.
		 * @param source Bytes to write.
		 * @param size Number of bytes to copy.
		 */
		void PatchMemory(void* destination, const void* source, std::size_t size);
	}

	namespace Hooks {
		/**
		 * @brief Creates a hook from a target function to a detour function.
		 *
		 * @param targetFunction Pointer to the function to hook.
		 * @param detourFunction Pointer to the custom function (your detour).
		 * @param originalFunction Out parameter; will store the pointer to call original later.
		 * @return true if the hook was created successfully, false otherwise.
		 */
		bool CreateHook(void* targetFunction, void* detourFunction, void** originalFunction);

		/**
		 * @brief Enables an individual installed hook.
		 *
		 * @param targetFunction Pointer to the function where a hook was created.
		 * @return true if successfully enabled, false otherwise.
		 */
		bool EnableHook(void* targetFunction);

		/**
		 * @brief Enables all hooks created by the SDK.
		 *
		 * @return true if successful, false otherwise.
		 */
		bool EnableAllHooks();

		/**
		 * @brief Disables an individual hook.
		 *
		 * @param targetFunction Pointer to the hooked function.
		 * @return true if successfully disabled, false otherwise.
		 */
		bool DisableHook(void* targetFunction);

		/**
		 * @brief Disables all active hooks created by the SDK.
		 *
		 * @return true if successful, false otherwise.
		 */
		bool DisableAllHooks();
	}
}
