// View.cpp : implementation of the CWindowsView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "FormatHelper.h"
#include "WindowsView.h"

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

		case DataItemType::TextLength:
			text.Format(L"%d", m_SelectedHwnd.GetWindowTextLength());
			break;

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

		case DataItemType::ParentWindow: return FormatHelper::FormatHWndOrNone(::GetAncestor(m_SelectedHwnd, GA_PARENT));
		case DataItemType::NextWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_HWNDNEXT));
		case DataItemType::PrevWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_HWNDPREV));
		case DataItemType::OwnerWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_OWNER));
		case DataItemType::FirstChildWindow: return FormatHelper::FormatHWndOrNone(m_SelectedHwnd.GetWindow(GW_CHILD));

	}
	return text;
}

void CWindowsView::OnFinalMessage(HWND /*hWnd*/) {
	delete this;
}

void CWindowsView::InitTree() {
	HWND hDesktop = ::GetDesktopWindow();
	if (!hDesktop)
		return;
	CWaitCursor wait;

	m_Tree.LockWindowUpdate(TRUE);
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
	if (!m_ShowHiddenWindows && !win.IsWindowVisible())
		return nullptr;
	win.GetWindowText(name);
	if (!m_ShowNoTitleWindows && name.IsEmpty())
		return nullptr;

	if (name.GetLength() > 64)
		name = name.Left(64) + L"...";
	if (!name.IsEmpty())
		name = L"[" + name + L"]";
	WCHAR className[64] = { 0 };
	::GetClassName(hWnd, className, _countof(className));
	text.Format(L"0x%zX (%s) %s", (DWORD_PTR)hWnd, className, (PCWSTR)name);

	HICON hIcon{ nullptr };
	if ((win.GetStyle() & WS_CHILD) == 0) {
		::SendMessageTimeout(hWnd, WM_GETICON, 0, 0, SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 200, (DWORD_PTR*)&hIcon);
	}
	int image = 0;
	if (hIcon)
		image = m_Images.AddIcon(hIcon);
	auto node = m_Tree.InsertItem(text, image, image, hParent, TVI_LAST);
	node.SetData((DWORD_PTR)hWnd);
	if (!win.IsWindowVisible())
		node.SetState(TVIS_CUT, TVIS_CUT);
	//if (!name.IsEmpty())
	//	node.SetState(TVIS_BOLD, TVIS_BOLD);

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
	switch (item.Type) {
		case DataItemType::ProcessId: 
			DWORD pid = 0;
			::GetWindowThreadProcessId(m_SelectedHwnd, &pid);
			return FormatHelper::GetProcessImageName(pid);
			
	}
	return L"";
}

LRESULT CWindowsView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Splitter.SetSplitterExtendedStyle(SPLIT_FLATBAR | SPLIT_PROPORTIONAL);
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 0);
	m_List.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_REPORT | LVS_OWNERDATA, 0);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_Images.Create(16, 16, ILC_COLOR32, 32, 32);
	m_Tree.SetImageList(m_Images, TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	UpdateLayout();
	m_Splitter.SetSplitterPosPct(35);

	InitTree();

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Property", LVCFMT_LEFT, 150);
	cm->AddColumn(L"Value", LVCFMT_LEFT, 220);
	cm->AddColumn(L"Details", LVCFMT_LEFT, 420);
	cm->UpdateColumns();

	m_Items = std::vector<WindowDataItem>{
		{ L"Handle", DataItemType::Handle },
		{ L"Class Name", DataItemType::ClassName },
		{ L"Text", DataItemType::Text },
		{ L"Text Length", DataItemType::TextLength },
		{ L"Style", DataItemType::Style },
		{ L"Extended Style", DataItemType::ExtendedStyle },
		{ L"Process Id", DataItemType::ProcessId },
		{ L"Thread Id", DataItemType::ThreadId },
		{ L"Parent Window", DataItemType::ParentWindow },
		{ L"Owner Window", DataItemType::OwnerWindow },
		{ L"Next Window", DataItemType::NextWindow },
		{ L"Previous Window", DataItemType::PrevWindow },
		{ L"First Child Window", DataItemType::FirstChildWindow},
	};

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
	m_SelectedHwnd.Attach((HWND)m_Selected.GetData());
	UpdateList();

	return 0;
}
