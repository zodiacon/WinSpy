#pragma once

#include "ViewBase.h"
#include "WindowsListView.h"

//
// View > Windows List: all the top-level windows in one sortable list
//
class CAllWindowsView : public CViewBase<CAllWindowsView> {
public:
	CAllWindowsView(IMainFrame* frame) : CViewBase(frame), m_List(frame) {}

	void OnActivate(bool activate);

protected:
	BEGIN_MSG_MAP(CAllWindowsView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		CHAIN_MSG_MAP(CViewBase<CAllWindowsView>)
		CHAIN_MSG_MAP_ALT_MEMBER(m_List, 1)
	END_MSG_MAP()

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CWindowsListView m_List;
};
