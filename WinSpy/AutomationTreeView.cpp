#include "pch.h"
#include "AutomationTreeView.h"
#include "WindowHelper.h"
#include "AppSettings.h"

#pragma comment(lib, "oleacc")

namespace {
	// for the Value pattern's Set Value action
	class CSetValueDlg : public CDialogImpl<CSetValueDlg> {
	public:
		enum { IDD = IDD_SETVALUE };

		explicit CSetValueDlg(CString const& value) : m_Value(value) {}

		CString const& GetValue() const {
			return m_Value;
		}

		BEGIN_MSG_MAP(CSetValueDlg)
			MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
			COMMAND_RANGE_HANDLER(IDOK, IDCANCEL, OnCloseCmd)
		END_MSG_MAP()

	private:
		LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
			SetDlgItemText(IDC_VALUE, m_Value);
			CenterWindow(GetParent());
			return TRUE;
		}

		LRESULT OnCloseCmd(WORD, WORD id, HWND, BOOL&) {
			if (id == IDOK)
				GetDlgItemText(IDC_VALUE, m_Value);
			EndDialog(id);
			return 0;
		}

		CString m_Value;
	};

	bool IsPropertyTrue(IUIAutomationElement* elem, PROPERTYID id) {
		CComVariant value;
		return S_OK == elem->GetCurrentPropertyValue(id, &value) && value.vt == VT_BOOL && value.boolVal;
	}

	// the property's value, if it is of the given type (not the "not supported" value)
	bool GetProperty(IUIAutomationElement* elem, PROPERTYID id, VARTYPE type, CComVariant& value) {
		return S_OK == elem->GetCurrentPropertyValue(id, &value) && value.vt == type;
	}
}

LRESULT CAutomationTreeView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	ToolBarButtonInfo buttons[] = {
		{ ID_WINDOW_HIGHLIGHT, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Highlight" },
		{ ID_WINDOW_LOCATEINTREE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Locate Window" },
		{ 0 },
		{ ID_AUTOMATION_INVOKE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Invoke" },
		{ ID_AUTOMATION_TOGGLE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Toggle" },
		{ ID_AUTOMATION_EXPAND, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Expand" },
		{ ID_AUTOMATION_COLLAPSE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Collapse" },
		{ ID_AUTOMATION_SELECT, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Select" },
		{ ID_AUTOMATION_SETVALUE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Set Value..." },
		{ ID_AUTOMATION_DEFAULTACTION, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Default Action" },
		{ ID_AUTOMATION_SETFOCUS, 0, BTNS_BUTTON | BTNS_AUTOSIZE, L"Set Focus" },
	};
	CreateAndInitToolBar(buttons, _countof(buttons));

	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 0, IDC_TREE);
	m_List.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL);
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

	m_List.InsertColumn(0, L"Property", 0, 180);
	m_List.InsertColumn(1, L"Value", 0, 350);

	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_Tree.SetImageList(WindowHelper::GetImageList(), TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	m_Splitter.SetSplitterPosPct(AppSettings::Get().AutomationSplitter());

	InitTree();
	UpdateUI();

	return 0;
}

LRESULT CAutomationTreeView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	AppSettings::Get().AutomationSplitter(m_Splitter.GetSplitterPosPct());
	bHandled = FALSE;
	return 0;
}

LRESULT CAutomationTreeView::OnLocateElement(UINT, WPARAM how, LPARAM lParam, BOOL&) {
	if (m_spUI == nullptr)
		return (LRESULT)LocateResult::NotInTree;

	CComPtr<IUIAutomationElement> spElem;
	if (how == LocateElementByPoint)
		m_spUI->ElementFromPoint(*reinterpret_cast<POINT const*>(lParam), &spElem);
	else
		m_spUI->ElementFromHandle(reinterpret_cast<HWND>(lParam), &spElem);

	int pid = 0;
	if (spElem)
		spElem->get_CurrentProcessId(&pid);
	// WinSpy's own elements are not in the tree
	if (!spElem || pid == (int)::GetCurrentProcessId())
		return (LRESULT)LocateResult::NotInTree;

	auto result = LocateElement(spElem);
	if (result == LocateResult::Found && how == LocateElementByPoint)
		HighlightElement(spElem);
	return (LRESULT)result;
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

LRESULT CAutomationTreeView::OnNodeSelected(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	auto data = (IUIAutomationElement*)m_Tree.GetItemData(tv->itemNew.hItem);
	UpdateProperties(data);
	UpdateUI();
	return 0;
}

LRESULT CAutomationTreeView::OnNodeDeleted(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	auto data = (IUIAutomationElement*)m_Tree.GetItemData(tv->itemOld.hItem);
	if (data)
		data->Release();

	return 0;
}

LRESULT CAutomationTreeView::OnTreeRightClick(int, LPNMHDR, BOOL&) {
	CPoint pt;
	::GetCursorPos(&pt);
	CPoint client(pt);
	m_Tree.ScreenToClient(&client);
	auto hItem = m_Tree.HitTest(client, nullptr);
	if (!hItem)
		return 0;

	m_Tree.SelectItem(hItem);
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	auto popup = menu.GetSubMenu(4);
	for (UINT id = ID_AUTOMATION_INVOKE; id <= ID_AUTOMATION_SCROLLINTOVIEW; id++)
		popup.EnableMenuItem(id, IsActionEnabled(id, m_Patterns) ? MF_ENABLED : MF_GRAYED);
	return GetFrame()->ShowPopupMenu(popup, pt);
}

LRESULT CAutomationTreeView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	InitTree();
	UpdateProperties(nullptr);
	UpdateUI();
	return 0;
}

LRESULT CAutomationTreeView::OnHighlight(WORD, WORD, HWND, BOOL&) {
	auto elem = GetSelectedElement();
	if (!elem || !HighlightElement(elem))
		::MessageBeep(MB_ICONWARNING);
	return 0;
}

LRESULT CAutomationTreeView::OnElementAction(WORD, WORD id, HWND, BOOL&) {
	auto elem = GetSelectedElement();
	if (!elem || !IsActionEnabled(id, m_Patterns)) {
		::MessageBeep(MB_ICONWARNING);
		return 0;
	}

	auto hr = DoAction(id, elem);
	if (FAILED(hr)) {
		CString text;
		text.Format(L"The action failed (0x%08X).", hr);
		AtlMessageBox(m_hWnd, (PCWSTR)text, IDS_TITLE, MB_ICONERROR);
	}
	// the action may have changed the element's state
	UpdateProperties(elem);
	UpdateUI();
	return 0;
}

HRESULT CAutomationTreeView::DoAction(UINT id, IUIAutomationElement* elem) {
	switch (id) {
		case ID_AUTOMATION_INVOKE:
		{
			CComPtr<IUIAutomationInvokePattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&spPattern));
			return spPattern ? spPattern->Invoke() : FAILED(hr) ? hr : E_NOINTERFACE;
		}
		case ID_AUTOMATION_TOGGLE:
		{
			CComPtr<IUIAutomationTogglePattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_TogglePatternId, IID_PPV_ARGS(&spPattern));
			return spPattern ? spPattern->Toggle() : FAILED(hr) ? hr : E_NOINTERFACE;
		}
		case ID_AUTOMATION_EXPAND:
		case ID_AUTOMATION_COLLAPSE:
		{
			CComPtr<IUIAutomationExpandCollapsePattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&spPattern));
			if (!spPattern)
				return FAILED(hr) ? hr : E_NOINTERFACE;
			return id == ID_AUTOMATION_EXPAND ? spPattern->Expand() : spPattern->Collapse();
		}
		case ID_AUTOMATION_SELECT:
		{
			CComPtr<IUIAutomationSelectionItemPattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(&spPattern));
			return spPattern ? spPattern->Select() : FAILED(hr) ? hr : E_NOINTERFACE;
		}
		case ID_AUTOMATION_SETVALUE:
		{
			CComPtr<IUIAutomationValuePattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&spPattern));
			if (!spPattern)
				return FAILED(hr) ? hr : E_NOINTERFACE;
			CComBSTR value;
			spPattern->get_CurrentValue(&value);
			CSetValueDlg dlg(value.m_str);
			if (dlg.DoModal(m_hWnd) != IDOK)
				return S_OK;
			return spPattern->SetValue(CComBSTR(dlg.GetValue()));
		}
		case ID_AUTOMATION_DEFAULTACTION:
		{
			CComPtr<IUIAutomationLegacyIAccessiblePattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, IID_PPV_ARGS(&spPattern));
			return spPattern ? spPattern->DoDefaultAction() : FAILED(hr) ? hr : E_NOINTERFACE;
		}
		case ID_AUTOMATION_SETFOCUS:
			return elem->SetFocus();
		case ID_AUTOMATION_SCROLLINTOVIEW:
		{
			CComPtr<IUIAutomationScrollItemPattern> spPattern;
			auto hr = elem->GetCurrentPatternAs(UIA_ScrollItemPatternId, IID_PPV_ARGS(&spPattern));
			return spPattern ? spPattern->ScrollIntoView() : FAILED(hr) ? hr : E_NOINTERFACE;
		}
	}
	return E_NOTIMPL;
}

bool CAutomationTreeView::IsActionEnabled(UINT id, ElementPatterns const& p) {
	switch (id) {
		case ID_AUTOMATION_INVOKE: return p.Invoke;
		case ID_AUTOMATION_TOGGLE: return p.Toggle;
		case ID_AUTOMATION_EXPAND:
		case ID_AUTOMATION_COLLAPSE: return p.ExpandCollapse;
		case ID_AUTOMATION_SELECT: return p.SelectionItem;
		case ID_AUTOMATION_SETVALUE: return p.Value && !p.ValueReadOnly;
		case ID_AUTOMATION_DEFAULTACTION: return p.LegacyIAccessible;
		case ID_AUTOMATION_SCROLLINTOVIEW: return p.ScrollItem;
		case ID_AUTOMATION_SETFOCUS: return true;
	}
	return false;
}

void CAutomationTreeView::OnActivate(bool active) {
	if (active) {
		UpdateUI();
	}
}

CString CAutomationTreeView::GetColumnText(HWND, int row, int col) const {
	auto& p = m_Properties[row];
	switch (col) {
		case 0: return p.Name;
		case 1: return p.Value.c_str();
	}
	return L"";
}

HTREEITEM CAutomationTreeView::AddElement(IUIAutomationElement* e, HTREEITEM hParent, HTREEITEM hAfter) {
	CComBSTR name, cls, id;
	e->get_CurrentName(&name);
	e->get_CurrentClassName(&cls);
	CString text;
	text.Format(L"%s [%s]", name.m_str, cls.m_str);

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

HTREEITEM CAutomationTreeView::FindChildNode(HTREEITEM hParent, IUIAutomationElement* element) {
	auto parent = (IUIAutomationElement*)m_Tree.GetItemData(hParent);
	auto find = [&]() -> HTREEITEM {
		for (auto hChild = m_Tree.GetChildItem(hParent); hChild; hChild = m_Tree.GetNextSiblingItem(hChild)) {
			BOOL same = FALSE;
			auto elem = (IUIAutomationElement*)m_Tree.GetItemData(hChild);
			if (elem && SUCCEEDED(m_spUI->CompareElements(elem, element, &same)) && same)
				return hChild;
		}
		return nullptr;
		};

	// expanding a node by code notifies only the first time, so its children are enumerated here
	bool enumerated = false;
	if (!m_Tree.GetChildItem(hParent)) {
		EnumChildElements(m_spUIWalker, parent, hParent);
		enumerated = true;
	}
	auto hItem = find();
	if (!hItem && !enumerated) {
		// the children were enumerated before the element was there
		HTREEITEM hChild;
		while ((hChild = m_Tree.GetChildItem(hParent)) != nullptr)
			m_Tree.DeleteItem(hChild);
		EnumChildElements(m_spUIWalker, parent, hParent);
		hItem = find();
	}
	return hItem;
}

LocateResult CAutomationTreeView::LocateElement(IUIAutomationElement* element) {
	auto hItem = m_Tree.GetRootItem();
	if (!hItem || !m_spUIWalker)
		return LocateResult::NotInTree;

	// the element and its ancestors, up to the root
	std::vector<CComPtr<IUIAutomationElement>> chain;
	CComPtr<IUIAutomationElement> spElem(element);
	while (spElem) {
		chain.push_back(spElem);
		CComPtr<IUIAutomationElement> spParent;
		m_spUIWalker->GetParentElement(spElem, &spParent);
		spElem = spParent;
	}

	BOOL same = FALSE;
	if (chain.empty() || FAILED(m_spUI->CompareElements(chain.back(), (IUIAutomationElement*)m_Tree.GetItemData(hItem), &same)) || !same)
		return LocateResult::NotInTree;

	CWaitCursor wait;
	m_Tree.SetRedraw(FALSE);
	for (auto it = chain.rbegin() + 1; it != chain.rend() && hItem; ++it) {
		hItem = FindChildNode(hItem, *it);
		if (hItem && it + 1 != chain.rend())
			m_Tree.Expand(hItem, TVE_EXPAND);
	}
	m_Tree.SetRedraw(TRUE);
	if (!hItem)
		return LocateResult::NotInTree;

	// the nodes on the way are expanded
	for (auto hParent = m_Tree.GetParentItem(hItem); hParent; hParent = m_Tree.GetParentItem(hParent))
		m_Tree.Expand(hParent, TVE_EXPAND);
	m_Tree.SelectItem(hItem);
	m_Tree.EnsureVisible(hItem);
	m_Tree.SetFocus();
	return LocateResult::Found;
}

IUIAutomationElement* CAutomationTreeView::GetSelectedElement() const {
	auto hItem = m_Tree.GetSelectedItem();
	return hItem ? (IUIAutomationElement*)m_Tree.GetItemData(hItem) : nullptr;
}

HWND CAutomationTreeView::GetElementWindow(IUIAutomationElement* element) const {
	if (!element || !m_spUIWalker)
		return nullptr;

	CComPtr<IUIAutomationElement> spElem(element);
	while (spElem) {
		UIA_HWND hWnd = nullptr;
		if (SUCCEEDED(spElem->get_CurrentNativeWindowHandle(&hWnd)) && hWnd)
			return (HWND)hWnd;
		CComPtr<IUIAutomationElement> spParent;
		m_spUIWalker->GetParentElement(spElem, &spParent);
		spElem = spParent;
	}
	return nullptr;
}

bool CAutomationTreeView::HighlightElement(IUIAutomationElement* element) {
	RECT rc{};
	if (FAILED(element->get_CurrentBoundingRectangle(&rc)) || ::IsRectEmpty(&rc))
		return false;

	WindowHelper::HighlightRect(rc);
	return true;
}

void CAutomationTreeView::InitTree() {
	CWaitCursor wait;
	if (m_spUI == nullptr) {
		m_spUI.CoCreateInstance(__uuidof(CUIAutomation));
		if (m_spUI == nullptr)
			return;
	}
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	CComPtr<IUIAutomationTreeWalker> spWalker;
	m_spUI->get_RawViewWalker(&spWalker);
	ATLASSERT(spWalker);
	m_spUIWalker = spWalker;

	CComPtr<IUIAutomationElement> spRoot;
	m_spUI->GetRootElement(&spRoot);
	auto node = AddElement(spRoot);

	EnumChildElements(spWalker, spRoot, node);
	m_Tree.Expand(node, TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
}

void CAutomationTreeView::UpdateUI() {
	auto elem = GetSelectedElement();
	UIEnable(ID_WINDOW_HIGHLIGHT, elem != nullptr);
	UIEnable(ID_WINDOW_LOCATEINTREE, elem != nullptr);
	for (UINT id = ID_AUTOMATION_INVOKE; id <= ID_AUTOMATION_SCROLLINTOVIEW; id++)
		UIEnable(id, elem && IsActionEnabled(id, m_Patterns));
}

void CAutomationTreeView::UpdateProperties(IUIAutomationElement* elem) {
	m_Properties.clear();
	m_Patterns = {};
	if (elem) {
		static const struct {
			PROPERTYID id;
			PCWSTR text;
		} props[] = {
			{ UIA_NamePropertyId, L"Name" },
			{ UIA_RuntimeIdPropertyId, L"Runtime ID" },
			{ UIA_ClassNamePropertyId, L"Class Name" },
			{ UIA_ControlTypePropertyId, L"Control Type" },
			{ UIA_LocalizedControlTypePropertyId, L"Localized Control Type" },
			{ UIA_FullDescriptionPropertyId, L"Full Description" },
			{ UIA_AcceleratorKeyPropertyId, L"Accelerator Key" },
			{ UIA_AccessKeyPropertyId, L"Access Key" },
			{ UIA_AutomationIdPropertyId, L"Automation ID" },
			{ UIA_BoundingRectanglePropertyId, L"Bounding Rectangle" },
			{ UIA_CenterPointPropertyId, L"Center Point" },
			{ UIA_HasKeyboardFocusPropertyId, L"Has Focus" },
			{ UIA_HelpTextPropertyId, L"Help Text" },
			{ UIA_ItemStatusPropertyId, L"Item Status" },
			{ UIA_NativeWindowHandlePropertyId, L"Window Handle" },
			{ UIA_ProcessIdPropertyId, L"Process ID" },
			{ UIA_SizePropertyId, L"Size" },
			{ UIA_OrientationPropertyId, L"Orientation" },
			{ UIA_OutlineColorPropertyId, L"Outline Color" },
			{ UIA_FillColorPropertyId, L"Fill Color" },
			{ UIA_PositionInSetPropertyId, L"Position in Set" },
			{ UIA_OutlineThicknessPropertyId, L"Outline Thickness" },
			{ UIA_IsEnabledPropertyId, L"Enabled" },
			{ UIA_IsOffscreenPropertyId, L"Off Screen" },
			{ UIA_IsPasswordPropertyId, L"Password" },
			{ UIA_ValueIsReadOnlyPropertyId, L"Read Only" },
			{ UIA_ItemTypePropertyId, L"Item Type" },
			{ UIA_SizeOfSetPropertyId, L"Size of Set" },
			{ UIA_VisualEffectsPropertyId, L"Visual Effects" },
			{ UIA_LiveSettingPropertyId, L"Live Setting" },
			{ UIA_LevelPropertyId, L"Level" },
			{ UIA_IsPeripheralPropertyId, L"Peripheral" },
			{ UIA_IsKeyboardFocusablePropertyId, L"Keyboard Focusable" },
			{ UIA_IsDialogPropertyId, L"Dialog" },
			{ UIA_IsControlElementPropertyId, L"Control Element" },
			{ UIA_FrameworkIdPropertyId, L"Framework ID" },
		};

		bool valuePattern = IsPropertyTrue(elem, UIA_IsValuePatternAvailablePropertyId);
		for (auto& p : props) {
			// the Value pattern's (it has a default otherwise)
			if (p.id == UIA_ValueIsReadOnlyPropertyId && !valuePattern)
				continue;
			CComVariant value;
			if (S_OK == elem->GetCurrentPropertyValue(p.id, &value)) {
				ItemData data;
				data.Value = FormatValue(m_spUI, value);
				if (data.Value.empty() && S_OK == value.ChangeType(VT_BSTR)) {
					data.Value = value.bstrVal;
				}
				if (!data.Value.empty()) {
					data.Name = p.text;
					m_Properties.push_back(std::move(data));
				}
			}
		}
		AddPatternProperties(elem);
	}

	m_List.SetItemCount((int)m_Properties.size());
}

void CAutomationTreeView::AddPatternProperties(IUIAutomationElement* elem) {
	static const struct {
		PROPERTYID id;
		PCWSTR name;
	} patterns[] = {
		{ UIA_IsInvokePatternAvailablePropertyId, L"Invoke" },
		{ UIA_IsTogglePatternAvailablePropertyId, L"Toggle" },
		{ UIA_IsExpandCollapsePatternAvailablePropertyId, L"ExpandCollapse" },
		{ UIA_IsSelectionItemPatternAvailablePropertyId, L"SelectionItem" },
		{ UIA_IsSelectionPatternAvailablePropertyId, L"Selection" },
		{ UIA_IsValuePatternAvailablePropertyId, L"Value" },
		{ UIA_IsRangeValuePatternAvailablePropertyId, L"RangeValue" },
		{ UIA_IsScrollPatternAvailablePropertyId, L"Scroll" },
		{ UIA_IsScrollItemPatternAvailablePropertyId, L"ScrollItem" },
		{ UIA_IsGridPatternAvailablePropertyId, L"Grid" },
		{ UIA_IsGridItemPatternAvailablePropertyId, L"GridItem" },
		{ UIA_IsTablePatternAvailablePropertyId, L"Table" },
		{ UIA_IsTableItemPatternAvailablePropertyId, L"TableItem" },
		{ UIA_IsTextPatternAvailablePropertyId, L"Text" },
		{ UIA_IsWindowPatternAvailablePropertyId, L"Window" },
		{ UIA_IsTransformPatternAvailablePropertyId, L"Transform" },
		{ UIA_IsDockPatternAvailablePropertyId, L"Dock" },
		{ UIA_IsMultipleViewPatternAvailablePropertyId, L"MultipleView" },
		{ UIA_IsItemContainerPatternAvailablePropertyId, L"ItemContainer" },
		{ UIA_IsVirtualizedItemPatternAvailablePropertyId, L"VirtualizedItem" },
		{ UIA_IsSynchronizedInputPatternAvailablePropertyId, L"SynchronizedInput" },
		{ UIA_IsLegacyIAccessiblePatternAvailablePropertyId, L"LegacyIAccessible" },
	};

	std::wstring names;
	for (auto& p : patterns) {
		if (IsPropertyTrue(elem, p.id)) {
			if (!names.empty())
				names += L", ";
			names += p.name;
		}
	}
	if (!names.empty())
		m_Properties.push_back({ L"Patterns", std::move(names) });

	auto& pt = m_Patterns;
	pt.Invoke = IsPropertyTrue(elem, UIA_IsInvokePatternAvailablePropertyId);
	pt.Toggle = IsPropertyTrue(elem, UIA_IsTogglePatternAvailablePropertyId);
	pt.ExpandCollapse = IsPropertyTrue(elem, UIA_IsExpandCollapsePatternAvailablePropertyId);
	pt.SelectionItem = IsPropertyTrue(elem, UIA_IsSelectionItemPatternAvailablePropertyId);
	pt.Value = IsPropertyTrue(elem, UIA_IsValuePatternAvailablePropertyId);
	pt.ValueReadOnly = IsPropertyTrue(elem, UIA_ValueIsReadOnlyPropertyId);
	pt.LegacyIAccessible = IsPropertyTrue(elem, UIA_IsLegacyIAccessiblePatternAvailablePropertyId);
	pt.ScrollItem = IsPropertyTrue(elem, UIA_IsScrollItemPatternAvailablePropertyId);

	// the patterns' state
	CComVariant value;
	if (pt.Value && GetProperty(elem, UIA_ValueValuePropertyId, VT_BSTR, value))
		m_Properties.push_back({ L"Value", value.bstrVal });
	if (pt.Toggle && GetProperty(elem, UIA_ToggleToggleStatePropertyId, VT_I4, value)) {
		static PCWSTR const states[] = { L"Off", L"On", L"Indeterminate" };
		m_Properties.push_back({ L"Toggle State", value.lVal >= 0 && value.lVal < _countof(states) ? states[value.lVal] : std::to_wstring(value.lVal) });
	}
	if (pt.ExpandCollapse && GetProperty(elem, UIA_ExpandCollapseExpandCollapseStatePropertyId, VT_I4, value)) {
		static PCWSTR const states[] = { L"Collapsed", L"Expanded", L"Partially Expanded", L"Leaf Node" };
		m_Properties.push_back({ L"Expand/Collapse State", value.lVal >= 0 && value.lVal < _countof(states) ? states[value.lVal] : std::to_wstring(value.lVal) });
	}
	if (pt.SelectionItem && GetProperty(elem, UIA_SelectionItemIsSelectedPropertyId, VT_BOOL, value))
		m_Properties.push_back({ L"Selected", value.boolVal ? L"True" : L"False" });
	if (IsPropertyTrue(elem, UIA_IsRangeValuePatternAvailablePropertyId)) {
		CComVariant min, max;
		if (GetProperty(elem, UIA_RangeValueValuePropertyId, VT_R8, value) && GetProperty(elem, UIA_RangeValueMinimumPropertyId, VT_R8, min)
			&& GetProperty(elem, UIA_RangeValueMaximumPropertyId, VT_R8, max))
			m_Properties.push_back({ L"Range Value", std::format(L"{} ({} to {})", value.dblVal, min.dblVal, max.dblVal) });
	}
	if (IsPropertyTrue(elem, UIA_IsScrollPatternAvailablePropertyId)) {
		CComVariant h, v;
		if (GetProperty(elem, UIA_ScrollHorizontalScrollPercentPropertyId, VT_R8, h) && GetProperty(elem, UIA_ScrollVerticalScrollPercentPropertyId, VT_R8, v)) {
			// -1: no scrolling in that direction
			auto percent = [](double p) { return p < 0 ? std::wstring(L"None") : std::format(L"{:.1f}%", p); };
			m_Properties.push_back({ L"Scroll Position", std::format(L"Horizontal: {}, Vertical: {}", percent(h.dblVal), percent(v.dblVal)) });
		}
	}
	if (IsPropertyTrue(elem, UIA_IsWindowPatternAvailablePropertyId) && GetProperty(elem, UIA_WindowWindowVisualStatePropertyId, VT_I4, value)) {
		static PCWSTR const states[] = { L"Normal", L"Maximized", L"Minimized" };
		auto state = value.lVal >= 0 && value.lVal < _countof(states) ? std::wstring(states[value.lVal]) : std::to_wstring(value.lVal);
		if (GetProperty(elem, UIA_WindowIsModalPropertyId, VT_BOOL, value) && value.boolVal)
			state += L", Modal";
		m_Properties.push_back({ L"Window State", std::move(state) });
	}
	if (pt.LegacyIAccessible && GetProperty(elem, UIA_LegacyIAccessibleDefaultActionPropertyId, VT_BSTR, value) && value.bstrVal && *value.bstrVal)
		m_Properties.push_back({ L"Default Action", value.bstrVal });
	else
		pt.LegacyIAccessible = false;	// nothing to do by default
}

std::wstring CAutomationTreeView::FormatValue(IUIAutomation* pUI, VARIANT const& value) {
	switch (value.vt) {
		case VT_I4:	return std::format(L"{} (0x{:X})", value.intVal, value.intVal);
		case VT_UI4: return std::format(L"{} (0x{:X})", value.uintVal, value.uintVal);
		case VT_BOOL: return value.boolVal ? L"True" : L"False";
		case VT_I4 | VT_ARRAY:
		{
			CComHeapPtr<int> data;
			int count;
			if (S_OK == pUI->IntSafeArrayToNativeArray(value.parray, &data, &count) && count > 0) {
				std::wstring result(L"(");
				for (int i = 0; i < count; i++) {
					result += std::format(L"{}", data[i]);
					if (i < count - 1)
						result += L", ";
				}
				result += L")";
				return result;
			}
			break;
		}
		case VT_R8 | VT_ARRAY:
		{
			CComHeapPtr<RECT> rc;
			int count;
			if (S_OK == pUI->SafeArrayToRectNativeArray(value.parray, &rc, &count) && count > 0) {
				std::wstring result(L"(");
				for (int i = 0; i < count; i++) {
					result += std::format(L"[{},{}-{},{}]", rc[i].left, rc[i].top, rc[i].right, rc[i].bottom);
					if (i < count - 1)
						result += L", ";
				}
				result += L")";
				return result;
			}
			else {
				double* data;
				count = value.parray->rgsabound[0].cElements;
				if (count > 0 && S_OK == ::SafeArrayAccessData(value.parray, (void**)&data)) {
					std::wstring result(L"(");
					for (int i = 0; i < count; i++) {
						result += std::format(L"{}", data[i]);
						if (i < count - 1)
							result += L", ";
					}
					::SafeArrayUnaccessData(value.parray);
					result += L")";
					return result;
				}
			}
			break;
		}
	}
	return L"";
}
