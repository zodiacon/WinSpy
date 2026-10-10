#pragma once

enum class ExtraDataType : DWORD;

struct MessageDecoder final {
	static CString Decode(UINT msg, WPARAM wp, LPARAM lp);
	// what a sent message's pointer parameter pointed to (copied by the hook)
	static CString DecodeExtra(ExtraDataType type, BYTE const* data, DWORD size);
	static CString MouseKey(int key);
	static CString SysCommandToString(DWORD cmd);
	static CString SizeParamToString(DWORD type);
	static CString HitTestToString(int hit);
	static CString ScrollRequestToString(int request, bool horizontal);
	static CString KeyDataToString(LPARAM lp);
};