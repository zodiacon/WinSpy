// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, PVOID lpReserved) {
	switch (reason) {
		case DLL_PROCESS_ATTACH:
			::DisableThreadLibraryCalls(hModule);
			break;
	}
	return TRUE;
}

