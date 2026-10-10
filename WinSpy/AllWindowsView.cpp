#include "pch.h"
#include "resource.h"
#include "AllWindowsView.h"

void CAllWindowsView::OnActivate(bool activate) {
	if (activate) {
		m_List.Refresh();
		SetTimer(1, 2000, nullptr);
	}
	else {
		KillTimer(1);
	}
}

LRESULT CAllWindowsView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_List.UpdateListAll();
	return 0;
}

LRESULT CAllWindowsView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1)
		m_List.Refresh();
	return 0;
}

LRESULT CAllWindowsView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.SetFocus();
	return 0;
}

LRESULT CAllWindowsView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	m_List.Refresh();
	return 0;
}
