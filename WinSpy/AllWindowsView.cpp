#include "pch.h"
#include "resource.h"
#include "AllWindowsView.h"

void CAllWindowsView::OnActivate(bool activate) {
	// what changed while away (style changes, for one, raise no events)
	if (activate)
		m_List.Refresh();
}

void CAllWindowsView::OnWindowEvent(DWORD event, HWND hWnd) {
	// top-level windows only (a destroyed window can't be asked for its parent anymore)
	bool relevant = event == EVENT_OBJECT_DESTROY ? m_List.IsListed(hWnd) : ::GetAncestor(hWnd, GA_PARENT) == ::GetDesktopWindow();
	if (relevant && !m_RefreshPending) {
		// once for a burst of events
		m_RefreshPending = true;
		SetTimer(1, 300, nullptr);
	}
}

LRESULT CAllWindowsView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_List.UpdateListAll();
	WindowEvents::Subscribe(this);
	return 0;
}

LRESULT CAllWindowsView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	WindowEvents::Unsubscribe(this);
	bHandled = FALSE;
	return 0;
}

LRESULT CAllWindowsView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1) {
		KillTimer(1);
		m_RefreshPending = false;
		m_List.Refresh();
	}
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
