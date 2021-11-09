// View.cpp : implementation of the CWindowsView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "FormatHelper.h"
#include "WindowsView.h"
#include "ProcessHelper.h"
#include "WindowHelper.h"
#include "SortHelper.h"

BOOL CWindowsView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}


void CWindowsView::OnActivate(bool activate) {
	if (activate) {
		UpdateUI();
		SetTimer(1, 2000, nullptr);
	}
	else {
		KillTimer(1);
	}
}

void CWindowsView::OnFinalMessage(HWND /*hWnd*/) {
	delete this;
}

void CWindowsView::UpdateUI() {
	auto& ui = GetFrame()->GetUIUpdate();
	if (::GetFocus() == m_Tree) {
		ui.UISetCheck(ID_VIEW_HIDDENWINDOWS, m_ShowHiddenWindows);
		ui.UISetCheck(ID_VIEW_EMPTYTITLEWINDOWS, m_ShowNoTitleWindows);
		ui.UIEnable(ID_WINDOW_PROPERTIES, m_SelectedHwnd != nullptr);
		//ui.UISetCheck(ID_VIEW_CHILDWINDOWS, m_ShowChildWindows);
	}
	else {
		m_WindowsView.UpdateUI(ui);
	}
}

void CWindowsView::Refresh() {
	if (!m_SelectedHwnd.IsWindow()) {
		m_Tree.DeleteItem(m_Selected);
		return;
	}
	m_WindowsView.UpdateList(m_SelectedHwnd);
}

void CWindowsView::InitTree() {
	m_TotalVisibleWindows = m_TotalWindows = m_TopLevelWindows = 0;
	m_DesktopNode = nullptr;
	HWND hDesktop = ::GetDesktopWindow();
	if (!hDesktop)
		return;

	CWaitCursor wait;

	m_Tree.LockWindowUpdate(TRUE);
	m_Deleting = true;
	m_Tree.DeleteAllItems();
	m_WindowMap.clear();
	m_Deleting = false;

	m_DesktopNode = AddNode(hDesktop, TVI_ROOT);

	::EnumWindows([](auto hWnd, auto lp) -> BOOL {
		auto pThis = (CWindowsView*)lp;
		pThis->AddNode(hWnd, pThis->m_DesktopNode);
		return TRUE;
		}, reinterpret_cast<LPARAM>(this));
	m_DesktopNode.Expand(TVE_EXPAND);
	m_Tree.LockWindowUpdate(FALSE);
	m_DesktopNode.Select();
	m_DesktopNode.EnsureVisible();
	m_Tree.SetScrollPos(SB_HORZ, 0);
}

void CWindowsView::AddChildWindows(HTREEITEM hParent) {
	auto hWnd = (HWND)m_Tree.GetItemData(hParent);
	ATLASSERT(hWnd);

	m_hCurrentNode = hParent;
	::EnumChildWindows(hWnd, [](auto hChild, auto p) -> BOOL {
		auto pThis = (CWindowsView*)p;
		return pThis->AddChildNode(hChild);
		}, reinterpret_cast<LPARAM>(this));
}

CTreeItem CWindowsView::AddNode(HWND hWnd, HTREEITEM hParent) {
	CString text, name;
	CWindow win(hWnd);
	m_TotalWindows++;
	if (win.IsWindowVisible())
		m_TotalVisibleWindows++;


	if (m_DesktopNode) {
		if (::GetAncestor(hWnd, GA_PARENT) == (HWND)m_DesktopNode.GetData())
			m_TopLevelWindows++;

		if (!m_ShowHiddenWindows && !win.IsWindowVisible())
			return nullptr;
		win.GetWindowText(name);
		if (!m_ShowNoTitleWindows && name.IsEmpty())
			return nullptr;
	}

	if (name.GetLength() > 64)
		name = name.Left(64) + L"...";
	if (!name.IsEmpty())
		name = L"[" + name + L"]";
	WCHAR className[64] = { 0 };
	::GetClassName(hWnd, className, _countof(className));
	text.Format(L"0x%zX (%s) %s", (DWORD_PTR)hWnd, className, (PCWSTR)name);

	HICON hIcon{ nullptr };
	int image = 0;
	if ((win.GetStyle() & WS_CHILD) == 0) {
		auto& icons = WindowHelper::GetIconMap();
		if (auto it = icons.find(hWnd); it == icons.end()) {
			hIcon = WindowHelper::GetWindowOrProcessIcon(hWnd);
			if (hIcon) {
				icons.insert({ hWnd, image = WindowHelper::GetImageList().AddIcon(hIcon) });
			}
		}
		else {
			image = it->second;
		}
	}

	auto node = m_Tree.InsertItem(text, image, image, hParent, TVI_LAST);
	node.SetData((DWORD_PTR)hWnd);
	m_WindowMap.insert({ hWnd, node });

	if (!win.IsWindowVisible())
		node.SetState(TVIS_CUT, TVIS_CUT);

	if (m_DesktopNode && win.GetWindow(GW_CHILD)) {
		// add a "plus" button
		node.AddTail(L"*", 0);
	}
	return node;
}

BOOL CWindowsView::AddChildNode(HWND hChild) {
	if (::GetAncestor(hChild, GA_PARENT) == (HWND)m_Tree.GetItemData(m_hCurrentNode)) {
		AddNode(hChild, m_hCurrentNode);
	}
	return TRUE;
}

LRESULT CWindowsView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Splitter.SetSplitterExtendedStyle(SPLIT_FLATBAR | SPLIT_PROPORTIONAL);
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_TREE);
	m_WindowsView.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_Tree.SetImageList(WindowHelper::GetImageList(), TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_WindowsView);
	UpdateLayout();
	m_Splitter.SetSplitterPosPct(35);

	InitTree();
	m_WindowMap.reserve(256);

	SetTimer(1, 2000, nullptr);

	return 0;
}

LRESULT CWindowsView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1)
		Refresh();
	else if (id == 3) {
		KillTimer(3);
		m_Selected = m_Tree.GetSelectedItem();
		NodeSelected();
	}
	return 0;
}

void CWindowsView::NodeSelected() {
	m_SelectedHwnd.Detach();
	auto hWnd = (HWND)m_Selected.GetData();
	if (!::IsWindow(hWnd))	// window is probably destroyed
		m_Selected.Delete();
	else {
		m_SelectedHwnd.Attach(hWnd);
		m_WindowsView.UpdateList(hWnd);
	}
	UpdateUI();
}

LRESULT CWindowsView::OnNodeExpanding(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action == TVE_EXPAND) {
		auto hItem = tv->itemNew.hItem;

		auto child = m_Tree.GetChildItem(hItem);
		if (child.GetData() == 0) {
			child.Delete();
			AddChildWindows(hItem);
		}
	}
	return 0;
}

LRESULT CWindowsView::OnNodeDeleted(int, LPNMHDR hdr, BOOL&) {
	if (!m_Deleting) {
		auto tv = (NMTREEVIEW*)hdr;
		m_WindowMap.erase((HWND)tv->itemOld.lParam);
	}
	return 0;
}

LRESULT CWindowsView::OnNodeSelected(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action == TVC_BYKEYBOARD) {
		// short delay before update in case the user moves quickly through the tree
		SetTimer(3, 250, nullptr);
	}
	else {
		m_Selected.m_hTreeItem = tv->itemNew.hItem;
		m_Selected.m_pTreeView = &m_Tree;
		NodeSelected();
	}

	return 0;
}

LRESULT CWindowsView::OnWindowShow(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	m_SelectedHwnd.ShowWindow(SW_SHOW);

	return 0;
}

LRESULT CWindowsView::OnWindowHide(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	m_SelectedHwnd.ShowWindow(SW_HIDE);
	return 0;
}

LRESULT CWindowsView::OnWindowMinimize(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	m_SelectedHwnd.ShowWindow(SW_MINIMIZE);
	return 0;
}

LRESULT CWindowsView::OnWindowMaximize(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	m_SelectedHwnd.ShowWindow(SW_MAXIMIZE);

	return 0;
}

LRESULT CWindowsView::OnWindowRestore(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	m_SelectedHwnd.ShowWindow(SW_RESTORE);
	return 0;
}

LRESULT CWindowsView::OnWindowBringToFront(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	m_SelectedHwnd.BringWindowToTop();
	return 0;
}

LRESULT CWindowsView::OnToggleHiddenWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowHiddenWindows = !m_ShowHiddenWindows;
	UpdateUI();
	InitTree();
	return 0;
}

LRESULT CWindowsView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	InitTree();
	return 0;
}

LRESULT CWindowsView::OnToggleEmptyTitleWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowNoTitleWindows = !m_ShowNoTitleWindows;
	UpdateUI();
	InitTree();
	return 0;
}

LRESULT CWindowsView::OnToggleChildWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowChildWindows = !m_ShowChildWindows;
	UpdateUI();
	InitTree();
	return 0;
}

LRESULT CWindowsView::OnTreeNodeRightClick(HTREEITEM hItem, CPoint const& pt) {
	ATLASSERT(m_Selected);
	if (!m_Selected)
		return 0;

	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);

	return GetFrame()->ShowContextMenu(menu.GetSubMenu(0), pt);
}

LRESULT CWindowsView::OnWindowFlash(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	WindowHelper::Flash(m_SelectedHwnd);

	return 0;
}

LRESULT CWindowsView::OnWindowProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& handled) {
	if (::GetFocus() != m_Tree) {
		handled = FALSE;
		return 0;
	}

	ATLASSERT(m_SelectedHwnd);
	WindowHelper::ShowWindowProperties(m_SelectedHwnd);

	return 0;
}

LRESULT CWindowsView::OnTreeNodeDoubleClick(HTREEITEM hItem, CPoint const& pt) {
	WindowHelper::ShowWindowProperties((HWND)m_Tree.GetItemData(hItem));
	return 1;
}

LRESULT CWindowsView::OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Tree.SetFocus();
	return 0;
}


