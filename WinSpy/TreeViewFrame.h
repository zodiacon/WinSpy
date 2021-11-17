#pragma once

#include "FrameWindowHelper.h"

struct CTreeViewFrame : 
	CFrameWindowHelper<CTreeViewFrame>,
	CAutoUpdateUI<CTreeViewFrame>,
	CFrameWindowImpl<CTreeViewFrame, CWindow, CControlWinTraits> {
	using BaseFrame = CFrameWindowImpl<CTreeViewFrame, CWindow, CControlWinTraits>;

protected:
	BEGIN_MSG_MAP(CTreeViewFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CAutoUpdateUI<CTreeViewFrame>)
		CHAIN_MSG_MAP(BaseFrame)
		FORWARD_NOTIFICATIONS()
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		m_hWndClient = m_Tree.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);

		return 0;
	}

	CTreeViewCtrlEx m_Tree;
};
