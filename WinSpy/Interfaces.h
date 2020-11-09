#pragma once

struct IMainFrame {
	virtual CUpdateUIBase& GetUIUpdate() = 0;
	virtual UINT ShowContextMenu(HMENU hMenu, const POINT& pt, DWORD flags = 0) = 0;
};
