#pragma once

struct MessageDecoder final abstract {
	static CString Decode(UINT msg, WPARAM wp, LPARAM lp);
	static CString MouseKey(int key);
	static CString SysCommandToString(DWORD cmd);
	static CString SizeParamToString(DWORD type);
};