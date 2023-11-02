#include "pch.h"
#include "AutomationTreeView.h"
#include "WindowHelper.h"
#include <UIAutomation.h>

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

void CAutomationTreeView::OnActivate(bool active) {
	if (active) {
		UpdateUI();
	}
}

CString CAutomationTreeView::GetColumnText(HWND, int row, int col) const {

	return CString();
}

HTREEITEM CAutomationTreeView::AddElement(IUIAutomationElement* e, HTREEITEM hParent, HTREEITEM hAfter) {
	CComBSTR name, role, id;
	e->get_CurrentName(&name);
	if (name.Length() == 0) {
		e->get_CurrentClassName(&name);
	}
	e->get_CurrentAriaRole(&role);
	int pid = 0;
	e->get_CurrentProcessId(&pid);
	e->get_CurrentAutomationId(&id);
	UIA_HWND hWnd = nullptr;
	e->get_CurrentNativeWindowHandle(&hWnd);
	CString text;
	text.Format(L"%s (ID: %s, PID: %d, Role: %s)", name.m_str, id.m_str, pid, role.m_str);

	return m_Tree.InsertItem(text, hParent, hAfter);
}

void CAutomationTreeView::EnumChildElements(IUIAutomationTreeWalker* pWalker, IUIAutomationElement* root, HTREEITEM hParent, HTREEITEM hAfter, int depth) {
	CComPtr<IUIAutomationElement> spElem;
	pWalker->GetFirstChildElement(root, &spElem);
	int pid = 0;
	while (spElem) {
		auto node = AddElement(spElem, hParent, hAfter);
		spElem->get_CurrentProcessId(&pid);
		if (pid != ::GetCurrentProcessId())
			EnumChildElements(pWalker, spElem, node, TVI_LAST, depth + 1);
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
	CComPtr<IUIAutomationCondition> spTrueCond;
	spUI->CreateTrueCondition(&spTrueCond);

	CComPtr<IUIAutomationElement> spRoot;
	spUI->GetRootElement(&spRoot);
	auto node = AddElement(spRoot);

	EnumChildElements(spWalker, spRoot, node);
	m_Tree.Expand(node, TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
}

void CAutomationTreeView::UpdateUI() {
}
