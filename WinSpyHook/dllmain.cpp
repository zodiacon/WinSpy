// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, PVOID lpReserved) {
	switch (reason) {
		case DLL_PROCESS_ATTACH:
			::DisableThreadLibraryCalls(hModule);
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

