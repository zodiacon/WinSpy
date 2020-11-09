// View.cpp : implementation of the CWindowsView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "FormatHelper.h"
#include "WindowsView.h"
#include "ProcessHelper.h"
#include "WindowHelper.h"

BOOL CWindowsView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

CString CWindowsView::GetColumnText(HWND, int row, int col) const {
	const auto& item = m_Items[row];
	if (col == 0)
		return item.Property;
	if (col == 2)
		return GetDetails(item);

	CString text;
	switch (item.Type) {
		case DataItemType::Handle:
			text.Format(L"0x%zX", m_SelectedHwnd.m_hWnd);
			break;

		case DataItemType::Style:
			text.Format(L"0x%08X", m_SelectedHwnd.GetStyle());
			break;

		case DataItemType::ProcessId:
		{
			DWORD pid;
			::GetWindowThreadProcessId(m_SelectedHwnd, &pid);
			text.Format(L"%u (0x%X)", pid, pid);
			break;
		}

		case DataItemType::ThreadId:
		{
			auto tid = ::GetWindowThreadProcessId(m_SelectedHwnd, nullptr);
			text.Format(L"%u (0x%X)", tid, tid);
			break;
		}

		case DataItemType::Rectangle:
			return WindowHelper::WindowRectToString(m_SelectedHwnd);

		case DataItemType::ExtendedStyle:
			text.Format(L"0x%08X", m_SelectedHwnd.GetExStyle());
			break;

		case DataItemType::ClassName:
			::GetClassName(m_SelectedHwnd, text.GetBufferSetLength(64), 64);
			text.FreeExtra();
			break;

		case DataItemType::Text:
			m_SelectedHwnd.GetWindowText(text);
			break;

		case DataItemType::WindowProc:
			text.Format(L"0x%zX", m_SelectedHwnd.GetWindowLongPtr(GWLP_WNDPROC));
			break;

		case DataItemType::UserData:
			text.Format(L"0x%zX", m_SelectedHwnd.GetWindowLongPtr(GWLP_USERDATA));
			break;

		case DataItemType::ID:
			text.Format(L"0x%zX", m_SelectedHwnd.GetWindowLongPtr(GWLP_ID));
			break;

		case DataItemType::ParentWindow: return FormatHelper::FormatHWndOrNone(::GetAncestor(m_SelectedHwnd, GA_PARENT));
		case DataItemType::NextWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_HWNDNEXT));
		case DataItemType::PrevWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_HWNDPREV));
		case DataItemType::OwnerWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_OWNER));
		case DataItemType::FirstChildWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_CHILD));

	}
	return text;
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
	ui.UISetCheck(ID_VIEW_HIDDENWINDOWS, m_ShowHiddenWindows);
	ui.UISetCheck(ID_VIEW_EMPTYTITLEWINDOWS, m_ShowNoTitleWindows);
	//ui.UISetCheck(ID_VIEW_CHILDWINDOWS, m_ShowChildWindows);
}

void CWindowsView::Refresh() {
	if (!m_SelectedHwnd.IsWindow()) {
		m_Tree.DeleteItem(m_Selected);
		return;
	}
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetCountPerPage() + m_List.GetTopIndex());
}

void CWindowsView::InitTree() {
	m_DesktopNode = nullptr;
	HWND hDesktop = ::GetDesktopWindow();
	if (!hDesktop)
		return;
	CWaitCursor wait;

	m_Tree.LockWindowUpdate(TRUE);
	m_Tree.DeleteAllItems();
	m_Images.RemoveAll();
	m_Images.AddIcon(AtlLoadIconImage(IDI_WINDOW, 0, 16, 16));
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
	if (m_DesktopNode) {
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
	if ((win.GetStyle() & WS_CHILD) == 0) {
		hIcon = WindowHelper::GetWindowOrProcessIcon(hWnd);
	}
	int image = 0;
	if (hIcon)
		image = m_Images.AddIcon(hIcon);
	auto node = m_Tree.InsertItem(text, image, image, hParent, TVI_LAST);
	node.SetData((DWORD_PTR)hWnd);
	if (!win.IsWindowVisible())
		node.SetState(TVIS_CUT, TVIS_CUT);

	if (m_DesktopNode && win.GetWindow(GW_CHILD)) {
		// add a "plus" button
		node.AddTail(L"*", 0);
	}
	return node;
}

BOOL CWindowsView::AddChildNode(HWND hChild) {
	if (::GetAncestor(hChild, GA_PARENT) == (HWND)m_Tree.GetItemData(m_hCurrentNode))
		AddNode(hChild, m_hCurrentNode);

	return TRUE;
}

void CWindowsView::UpdateList() {
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
}

CString CWindowsView::GetDetails(const WindowDataItem& item) const {
	CString text;
	switch (item.Type) {
		case DataItemType::ProcessId:
		{
			DWORD pid = 0;
			::GetWindowThreadProcessId(m_SelectedHwnd, &pid);
			return ProcessHelper::GetProcessImageName(pid);
		}

		case DataItemType::Text:
			text.Format(L"Length: %d", m_SelectedHwnd.GetWindowTextLength());
			break;

			return WindowHelper::WindowStyleToString(m_SelectedHwnd);
		case DataItemType::Style: return WindowHelper::WindowStyleToString(m_SelectedHwnd);
		case DataItemType::ExtendedStyle: return WindowHelper::WindowExtendedStyleToString(m_SelectedHwnd);
		case DataItemType::Rectangle:
			if (m_SelectedHwnd.IsZoomed())
				return L"Maximized";
			if (m_SelectedHwnd.IsIconic())
				return L"Minimized";
			break;
	}
	return text;
}

LRESULT CWindowsView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Splitter.SetSplitterExtendedStyle(SPLIT_FLATBAR | SPLIT_PROPORTIONAL);
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 0, IDC_TREE);
	m_List.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL, 0);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_Images.Create(16, 16, ILC_COLOR32, 32, 32);
	m_Tree.SetImageList(m_Images, TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	UpdateLayout();
	m_Splitter.SetSplitterPosPct(35);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Property", LVCFMT_LEFT, 150);
	cm->AddColumn(L"Value", LVCFMT_LEFT, 220);
	cm->AddColumn(L"Details", LVCFMT_LEFT, 420);
	cm->UpdateColumns();

	m_Items = std::vector<WindowDataItem>{
		{ L"Handle", DataItemType::Handle },
		{ L"Class Name", DataItemType::ClassName },
		{ L"Text", DataItemType::Text },
		{ L"Style", DataItemType::Style },
		{ L"Extended Style", DataItemType::ExtendedStyle },
		{ L"Process Id", DataItemType::ProcessId },
		{ L"Thread Id", DataItemType::ThreadId },
		{ L"Rectangle", DataItemType::Rectangle },
		{ L"Parent Window", DataItemType::ParentWindow },
		{ L"Owner Window", DataItemType::OwnerWindow },
		{ L"Next Window", DataItemType::NextWindow },
		{ L"Previous Window", DataItemType::PrevWindow },
		{ L"First Child Window", DataItemType::FirstChildWindow },
		{ L"Window Procedure", DataItemType::WindowProc },
		{ L"User Data", DataItemType::UserData },
		{ L"ID", DataItemType::ID },
	};

	InitTree();

	SetTimer(1, 2000, nullptr);

	return 0;
}

LRESULT CWindowsView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1)
		Refresh();
	return 0;
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

LRESULT CWindowsView::OnNodeSelected(int, LPNMHDR, BOOL&) {
	m_Selected = m_Tree.GetSelectedItem();
	m_SelectedHwnd.Detach();
	auto hWnd = (HWND)m_Selected.GetData();
	if (!::IsWindow(hWnd))	// window is probably destroyed
		m_Selected.Delete();
	else {
		m_SelectedHwnd.Attach(hWnd);
		UpdateList();
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

LRESULT CWindowsView::OnTreeNodeRightClick(int, LPNMHDR, BOOL&) {
	ATLASSERT(m_Selected);
	if (!m_Selected)
		return 0;

	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	CPoint pt;
	::GetCursorPos(&pt);

	return GetFrame()->ShowContextMenu(menu.GetSubMenu(0), pt);
}

LRESULT CWindowsView::OnWindowFlash(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Selected);
	FLASHWINFO info = { sizeof(info) };
	info.dwFlags = FLASHW_CAPTION;
	info.uCount = 3;
	info.hwnd = m_SelectedHwnd;
	::FlashWindowEx(&info);

	return 0;
}
