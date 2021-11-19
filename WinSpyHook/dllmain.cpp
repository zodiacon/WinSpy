// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

HINSTANCE g_hInstDll;

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, PVOID lpReserved) {
	switch (reason) {
		case DLL_PROCESS_ATTACH:
			g_hInstDll = hModule;
			::DisableThreadLibraryCalls(hModule);
			break;
	}
	return TRUE;
}

