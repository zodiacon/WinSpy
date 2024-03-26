#include "pch.h"
#include "AutomationTreeView.h"
#include "WindowHelper.h"

#pragma comment(lib, "oleacc")

LRESULT CAutomationTreeView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 0, IDC_TREE);
	m_List.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL);
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

	m_List.InsertColumn(0, L"Property", 0, 150);
	m_List.InsertColumn(1, L"Value", 0, 350);

	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_Tree.SetImageList(WindowHelper::GetImageList(), TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	m_Splitter.SetSplitterPosPct(35);

	InitTree();

	return 0;
}

LRESULT CAutomationTreeView::OnNodeExpanding(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action != TVE_EXPAND)
		return FALSE;

	auto hItem = tv->itemNew.hItem;
	if (m_Tree.GetChildItem(hItem))
		return FALSE;

	auto elem = (IUIAutomationElement*)m_Tree.GetItemData(hItem);
	if (elem) {
		EnumChildElements(m_spUIWalker, elem, hItem);
		return FALSE;
	}
	return TRUE;
}

LRESULT CAutomationTreeView::OnNodeExpanded(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action != TVE_COLLAPSE)
		return FALSE;

	//
	// delete child nodes
	//
	auto hItem = tv->itemNew.hItem;
	HTREEITEM hChild;
	while (hChild = m_Tree.GetChildItem(hItem))
		m_Tree.DeleteItem(hChild);

	TVITEM tvi{ sizeof(tvi) };
	tvi.cChildren = 1;
	tvi.mask = TVIF_CHILDREN;
	tvi.hItem = hItem;
	m_Tree.SetItem(&tvi);

	return FALSE;
}

LRESULT CAutomationTreeView::OnNodeDeleted(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	auto data = (IUIAutomationElement*)m_Tree.GetItemData(tv->itemOld.hItem);
	if (data)
		data->Release();

	return 0;
}

void CAutomationTreeView::OnActivate(bool active) {
	if (active) {
		UpdateUI();
	}
}

CString CAutomationTreeView::GetColumnText(HWND, int row, int col) const {
	return CString();
}

HTREEITEM CAutomationTreeView::AddElement(IUIAutomationElement* e, HTREEITEM hParent, HTREEITEM hAfter) {
	CComBSTR name, cls, id;
	e->get_CurrentName(&name);
	e->get_CurrentClassName(&cls);
	int pid = 0;
	e->get_CurrentProcessId(&pid);
	e->get_CurrentAutomationId(&id);
	UIA_HWND hWnd = nullptr;
	e->get_CurrentNativeWindowHandle(&hWnd);
	CString text;
	text.Format(L"%s [%s] (ID: %s, PID: %d, HWND: 0x%p)", name.m_str, cls.m_str, id.m_str, pid, hWnd);

	auto hItem = m_Tree.InsertItem(text, hParent, hAfter);
	e->AddRef();
	m_Tree.SetItemData(hItem, (DWORD_PTR)e);
	return hItem;
}

void CAutomationTreeView::EnumChildElements(IUIAutomationTreeWalker* pWalker, IUIAutomationElement* root, HTREEITEM hParent, HTREEITEM hAfter) {
	CComPtr<IUIAutomationElement> spElem;
	pWalker->GetFirstChildElement(root, &spElem);
	int pid = 0;
	while (spElem) {
		spElem->get_CurrentProcessId(&pid);
		if (pid != ::GetCurrentProcessId()) {
			auto node = AddElement(spElem, hParent, hAfter);
			//EnumChildElements(pWalker, spElem, node, TVI_LAST, depth + 1);
			CComPtr<IUIAutomationElement> spChild;
			pWalker->GetFirstChildElement(spElem, &spChild);
			if (spChild) {
				TVITEM tvi{ sizeof(tvi) };
				tvi.cChildren = 1;
				tvi.mask = TVIF_CHILDREN;
				tvi.hItem = node;
				m_Tree.SetItem(&tvi);
			}
		}
		CComPtr<IUIAutomationElement> spNext;
		pWalker->GetNextSiblingElement(spElem, &spNext);
		spElem = spNext;
	}
}

void CAutomationTreeView::InitTree() {
	CWaitCursor wait;
	CComPtr<IUIAutomation> spUI;
	spUI.CoCreateInstance(__uuidof(CUIAutomation));
	ATLASSERT(spUI);
	if (spUI == nullptr)
		return;

	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	CComPtr<IUIAutomationTreeWalker> spWalker;
	spUI->get_RawViewWalker(&spWalker);
	ATLASSERT(spWalker);
	m_spUIWalker = spWalker;

	CComPtr<IUIAutomationElement> spRoot;
	spUI->GetRootElement(&spRoot);
	auto node = AddElement(spRoot);

	EnumChildElements(spWalker, spRoot, node);
	m_Tree.Expand(node, TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
}

void CAutomationTreeView::UpdateUI() {
}
