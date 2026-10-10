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
#include "AppSettings.h"


BOOL CWindowsView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

void CWindowsView::OnActivate(bool activate) {
	if (activate) {
		UpdateUI();
		// what changed in the list's windows while away (style changes, for one, raise no events)
		m_WindowsView.Refresh();
	}
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

LRESULT CWindowsView::OnRefreshAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	InitTree();
	return 0;
}

void CWindowsView::InitTree() {
	if (!m_SearchText.IsEmpty()) {
		InitSearchTree();
		return;
	}

	WindowHelper::PruneIconMap();
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

	AddMessageOnlyWindows();

	m_DesktopNode.Expand(TVE_EXPAND);
	m_Tree.LockWindowUpdate(FALSE);
	m_DesktopNode.Select();
	m_DesktopNode.EnsureVisible();
	m_Tree.SetScrollPos(SB_HORZ, 0);
}

void CWindowsView::InitSearchTree() {
	CWaitCursor wait;
	m_TotalVisibleWindows = m_TotalWindows = m_TopLevelWindows = 0;
	m_DesktopNode = nullptr;
	auto hDesktop = ::GetDesktopWindow();

	// all the windows (EnumChildWindows goes through all the descendants)
	std::vector<HWND> windows;
	::EnumWindows([](auto hWnd, auto lp) -> BOOL {
		reinterpret_cast<std::vector<HWND>*>(lp)->push_back(hWnd);
		::EnumChildWindows(hWnd, [](auto hChild, auto lp) -> BOOL {
			reinterpret_cast<std::vector<HWND>*>(lp)->push_back(hChild);
			return TRUE;
			}, lp);
		return TRUE;
		}, reinterpret_cast<LPARAM>(&windows));

	m_Tree.LockWindowUpdate(TRUE);
	m_Deleting = true;
	m_Tree.DeleteAllItems();
	m_WindowMap.clear();
	m_Deleting = false;
	m_DesktopNode = AddNode(hDesktop, TVI_ROOT);

	int found = 0;
	CTreeItem first;
	std::vector<HWND> chain;
	for (auto hWnd : windows) {
		// not WinSpy's own windows: its search box always matches
		DWORD pid;
		::GetWindowThreadProcessId(hWnd, &pid);
		if (pid == ::GetCurrentProcessId() || !PassesFilter(hWnd) || !MatchesSearch(hWnd))
			continue;

		// the window and its ancestors, from the top down (the ancestors whatever the View filters say)
		chain.clear();
		for (auto h = hWnd; h && h != hDesktop; h = ::GetAncestor(h, GA_PARENT))
			chain.push_back(h);
		HTREEITEM hParent = m_DesktopNode;
		for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
			if (auto node = m_WindowMap.find(*it); node != m_WindowMap.end())
				hParent = node->second;
			else
				hParent = AddNode(*it, hParent, true);
		}
		if (!first)
			first = CTreeItem(hParent, &m_Tree);
		if (++found == MaxSearchResults)
			break;
	}

	// all that was found is in view
	for (auto& [hWnd, hItem] : m_WindowMap)
		m_Tree.Expand(hItem, TVE_EXPAND);
	m_Tree.LockWindowUpdate(FALSE);
	auto select = first ? first : m_DesktopNode;
	select.Select();
	select.EnsureVisible();
}

bool CWindowsView::MatchesSearch(HWND hWnd) const {
	// the handle, the class name and the title (m_SearchText is upper case)
	CString text;
	text.Format(L"0x%zX ", (DWORD_PTR)hWnd);
	text += WindowHelper::GetWindowClassName(hWnd) + L" " + WindowHelper::GetWindowText(hWnd);
	return text.MakeUpper().Find(m_SearchText) >= 0;
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

bool CWindowsView::PassesFilter(HWND hWnd) const {
	return (m_ShowHiddenWindows || ::IsWindowVisible(hWnd)) && (m_ShowNoTitleWindows || ::GetWindowTextLength(hWnd) > 0);
}

CString CWindowsView::GetNodeText(HWND hWnd) {
	CString name;
	CWindow(hWnd).GetWindowText(name);
	if (name.GetLength() > 64)
		name = name.Left(64) + L"...";
	if (!name.IsEmpty())
		name = L"[" + name + L"]";
	WCHAR className[64] = { 0 };
	::GetClassName(hWnd, className, _countof(className));
	CString text;
	text.Format(L"0x%zX (%s) %s", (DWORD_PTR)hWnd, className, (PCWSTR)name);
	return text;
}

CTreeItem CWindowsView::AddNode(HWND hWnd, HTREEITEM hParent, bool force) {
	CWindow win(hWnd);
	m_TotalWindows++;
	if (win.IsWindowVisible())
		m_TotalVisibleWindows++;

	if (m_DesktopNode) {
		if (::GetAncestor(hWnd, GA_PARENT) == (HWND)m_DesktopNode.GetData())
			m_TopLevelWindows++;

		if (!force && !PassesFilter(hWnd))
			return nullptr;
	}

	HICON hIcon{ nullptr };
	int image = 0;
	if ((win.GetStyle() & WS_CHILD) == 0) {
		auto& icons = WindowHelper::GetIconMap();
		if (auto it = icons.find(hWnd); it == icons.end()) {
			hIcon = WindowHelper::GetWindowOrProcessIcon(hWnd);
			if (hIcon) {
				icons.insert({ hWnd, image = WindowHelper::AddIcon(hIcon) });
			}
		}
		else {
			image = it->second;
		}
	}

	auto node = m_Tree.InsertItem(GetNodeText(hWnd), image, image, hParent, TVI_LAST);
	node.SetData((DWORD_PTR)hWnd);
	m_WindowMap.insert({ hWnd, node });

	if (!win.IsWindowVisible())
		node.SetState(TVIS_CUT, TVIS_CUT);

	// a "plus" button: the children are added when the node is first expanded (search results have theirs already)
	if (m_DesktopNode && m_SearchText.IsEmpty() && win.GetWindow(GW_CHILD))
		node.AddTail(L"*", 0);
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
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	
	//m_TreeFrame.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	//m_Tree.Attach(m_TreeFrame.m_hWndClient);

	//ToolBarButtonInfo buttons[] = {
	//	{ ID_VIEW_REFRESH, IDI_REFRESH },
	//	{ 0 },
	//	{ ID_VIEW_HIDDENWINDOWS, IDI_WINDOW_HIDDEN },
	//	{ ID_VIEW_EMPTYTITLEWINDOWS, IDI_WINDOW_NOTEXT },
	//	{ ID_WINDOW_PROPERTIES, IDI_WINPROP },
	//};

	//m_TreeFrame.CreateAndInitToolBar(buttons, _countof(buttons));

	// the search box, above both panes
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	CRect rc(0, 0, WindowHelper::DpiScale(300), WindowHelper::DpiScale(22));
	m_Search.Create(m_hWndToolBar, &rc, nullptr, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_SEARCH);
	m_Search.SetFont(WindowHelper::GetUIFont());
	m_Search.SetCueBannerText(L"Search windows by class, title or handle (Ctrl+F)");
	AddSimpleReBarBand(m_Search, nullptr, FALSE, 0, TRUE);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 0);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
	m_Tree.SetImageList(WindowHelper::GetImageList(), TVSIL_NORMAL);

	m_WindowsView.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Splitter.SetSplitterPanes(m_Tree, m_WindowsView);
	UpdateLayout();
	m_Splitter.SetSplitterPosPct(AppSettings::Get().WindowsSplitter());

	m_ShowHiddenWindows = AppSettings::Get().WindowsShowHidden() != 0;
	m_ShowNoTitleWindows = AppSettings::Get().WindowsShowNoTitle() != 0;
	InitTree();
	m_WindowMap.reserve(256);

	WindowEvents::Subscribe(this);

	return 0;
}

LRESULT CWindowsView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	WindowEvents::Unsubscribe(this);
	AppSettings::Get().WindowsSplitter(m_Splitter.GetSplitterPosPct());
	bHandled = FALSE;
	return 0;
}

LRESULT CWindowsView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	switch (id) {
		case TimerKeyboardSelect:
			KillTimer(id);
			m_Selected = m_Tree.GetSelectedItem();
			NodeSelected();
			break;

		case TimerEvents:
			KillTimer(id);
			ProcessWindowEvents();
			break;

		case TimerSearch:
			// the search text settled
			KillTimer(id);
			m_Search.GetWindowText(m_SearchText);
			m_SearchText.Trim();
			m_SearchText.MakeUpper();
			InitTree();
			break;
	}
	return 0;
}

LRESULT CWindowsView::OnSearchChanged(WORD, WORD, HWND, BOOL&) {
	// searching waits for a pause in the typing
	SetTimer(TimerSearch, 300);
	return 0;
}

LRESULT CWindowsView::OnFind(WORD, WORD, HWND, BOOL&) {
	m_Search.SetFocus();
	m_Search.SetSelAll();
	return 0;
}

void CWindowsView::OnWindowEvent(DWORD event, HWND hWnd) {
	if (event == EVENT_OBJECT_DESTROY)
		m_EventsDestroyed.insert(hWnd);
	else
		m_EventsChanged.insert(hWnd);
	// handled together a little later, as windows come and go in bursts (the first event of a batch sets the timer)
	if (m_EventsDestroyed.size() + m_EventsChanged.size() == 1)
		SetTimer(TimerEvents, 250);
}

void CWindowsView::ProcessWindowEvents() {
	auto destroyed = std::move(m_EventsDestroyed);
	auto changed = std::move(m_EventsChanged);
	m_EventsDestroyed.clear();
	m_EventsChanged.clear();

	// does the list (the selected window and its children) show this window?
	bool listChanged = false;
	auto inList = [&](HWND hWnd) {
		return hWnd == m_SelectedHwnd.m_hWnd || m_WindowsView.IsListed(hWnd)
			|| (m_SelectedHwnd && ::IsWindow(hWnd) && ::GetAncestor(hWnd, GA_PARENT) == m_SelectedHwnd.m_hWnd);
	};

	for (auto hWnd : destroyed) {
		listChanged = listChanged || inList(hWnd);
		if (auto it = m_WindowMap.find(hWnd); it != m_WindowMap.end())
			m_Tree.DeleteItem(it->second);
	}
	for (auto hWnd : changed) {
		if (destroyed.contains(hWnd))
			continue;
		listChanged = listChanged || inList(hWnd);
		UpdateWindowNode(hWnd);
	}
	if (listChanged)
		m_WindowsView.Refresh();
}

void CWindowsView::UpdateWindowNode(HWND hWnd) {
	auto it = m_WindowMap.find(hWnd);
	if (!::IsWindow(hWnd)) {
		if (it != m_WindowMap.end())
			m_Tree.DeleteItem(it->second);
		return;
	}

	auto hParent = ::GetAncestor(hWnd, GA_PARENT);
	if (it != m_WindowMap.end()) {
		CTreeItem node(it->second, &m_Tree);
		// moved to another parent, or now left out by the View filters (search results keep their ancestors)
		bool moved = (HWND)node.GetParent().GetData() != hParent;
		if (!moved && (PassesFilter(hWnd) || !m_SearchText.IsEmpty())) {
			node.SetText(GetNodeText(hWnd));
			node.SetState(::IsWindowVisible(hWnd) ? 0 : TVIS_CUT, TVIS_CUT);
			return;
		}
		node.Delete();
		if (!moved)
			return;
	}

	// not in the tree: it goes under its parent's node, if that node's children are there (search results are not extended)
	if (!m_SearchText.IsEmpty())
		return;
	auto parent = m_WindowMap.find(hParent);
	if (parent == m_WindowMap.end())
		return;
	CTreeItem parentNode(parent->second, &m_Tree);
	auto child = parentNode.GetChild();
	if (child && child.GetData() == 0)
		return;		// added when the node is expanded
	if (!child && parentNode != m_DesktopNode) {
		// its first child: now it gets a "plus" button
		parentNode.AddTail(L"*", 0);
		return;
	}
	AddNode(hWnd, parentNode);
}

void CWindowsView::NodeSelected() {
	m_SelectedHwnd.Detach();
	auto data = m_Selected.GetData();
	auto hWnd = (HWND)data;
	if (data != MessageOnlyWindowsNode && !::IsWindow(hWnd))	// window is probably destroyed
		m_Selected.Delete();
	else {
		if (data == MessageOnlyWindowsNode)
			m_WindowsView.UpdateList(nullptr);
		else {
			m_SelectedHwnd.Attach(hWnd);
			m_WindowsView.UpdateList(hWnd);
		}
	}
	UpdateUI();
}

LRESULT CWindowsView::OnNodeExpanding(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action == TVE_EXPAND)
		EnsureChildren(tv->itemNew.hItem);
	return 0;
}

void CWindowsView::EnsureChildren(HTREEITEM hItem) {
	// the children are added when a node is first expanded (until then it has a placeholder child)
	auto child = m_Tree.GetChildItem(hItem);
	if (child && child.GetData() == 0) {
		child.Delete();
		AddChildWindows(hItem);
	}
}

LocateResult CWindowsView::LocateWindow(HWND hWnd) {
	if (!::IsWindow(hWnd) || !m_DesktopNode)
		return LocateResult::NotInTree;

	// the window's ancestors, from the top-level one down
	std::vector<HWND> chain;
	auto hDesktop = (HWND)m_DesktopNode.GetData();
	for (auto h = hWnd; h && h != hDesktop; h = ::GetAncestor(h, GA_PARENT))
		chain.push_back(h);

	HTREEITEM hItem = m_DesktopNode;
	for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
		EnsureChildren(hItem);
		auto node = m_WindowMap.find(*it);
		if (node == m_WindowMap.end())
			return LocateResult::NotInTree;
		hItem = node->second;
	}
	m_Tree.SelectItem(hItem);
	m_Tree.EnsureVisible(hItem);
	m_Tree.SetFocus();
	return LocateResult::Found;
}

LRESULT CWindowsView::OnLocateWindow(UINT, WPARAM, LPARAM lParam, BOOL&) {
	return (LRESULT)LocateWindow(reinterpret_cast<HWND>(lParam));
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
		SetTimer(TimerKeyboardSelect, 250, nullptr);
	}
	else {
		m_Selected.m_hTreeItem = tv->itemNew.hItem;
		m_Selected.m_pTreeView = &m_Tree;
		NodeSelected();
	}

	return 0;
}

HWND CWindowsView::GetTargetWindow() const {
	// the window commands act on the list's selected row when it has the focus
	if (m_WindowsView && ::GetFocus() == m_WindowsView.m_hWndClient)
		return m_WindowsView.GetSelectedListHwnd();
	return m_SelectedHwnd;
}

LRESULT CWindowsView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	InitTree();
	return 0;
}

LRESULT CWindowsView::OnToggleHiddenWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowHiddenWindows = !m_ShowHiddenWindows;
	AppSettings::Get().WindowsShowHidden(m_ShowHiddenWindows);
	UpdateUI();
	InitTree();
	return 0;
}

LRESULT CWindowsView::OnToggleEmptyTitleWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowNoTitleWindows = !m_ShowNoTitleWindows;
	AppSettings::Get().WindowsShowNoTitle(m_ShowNoTitleWindows);
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

	return GetFrame()->ShowPopupMenu(menu.GetSubMenu(0), pt);
}

LRESULT CWindowsView::OnTreeNodeDoubleClick(HTREEITEM hItem, CPoint const& pt) {
	WindowHelper::ShowWindowProperties((HWND)m_Tree.GetItemData(hItem));
	return 1;
}

LRESULT CWindowsView::OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Tree.SetFocus();
	return 0;
}

CTreeItem CWindowsView::AddMessageOnlyWindows() {
	m_MsgOnlyNode = m_Tree.InsertItem(L"Message Only Windows", TVI_ROOT, TVI_LAST);
	m_MsgOnlyNode.SetData(MessageOnlyWindowsNode);
	return m_MsgOnlyNode;
}

