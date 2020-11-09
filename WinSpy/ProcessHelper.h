#pragma once

struct ProcessHelper {
	static CString GetProcessImageName(DWORD pid, bool fullPath = false);
};

