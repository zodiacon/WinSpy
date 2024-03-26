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

	m_List.InsertColumn(0, L"Property", 0, 180);
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

LRESULT CAutomationTreeView::OnNodeSelected(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	auto hItem = tv->itemNew.hItem;
	auto data = (IUIAutomationElement*)m_Tree.GetItemData(tv->itemNew.hItem);
	UpdateProperties(data);
	return 0;
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
}

void CAutomationTreeView::UpdateProperties(IUIAutomationElement* elem) {
	m_Properties.clear();
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
			{ UIA_IsEnabledPropertyId, L"Editable" },
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

		for (auto& p : props) {
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
	}

	m_List.SetItemCount((int)m_Properties.size());
}

std::wstring CAutomationTreeView::FormatValue(IUIAutomation* pUI, VARIANT const& value) {
	switch (value.vt) {
		case VT_I4:	return std::format(L"{} (0x{:X})", value.intVal, value.intVal);
		case VT_UI4: return std::format(L"{} (0x{:X})", value.uintVal, value.uintVal);
		case VT_BOOL: return value.boolVal ? L"True" : L"False";
		case VT_I4 | VT_ARRAY:
		{
			int* data, count;
			if (S_OK == pUI->IntSafeArrayToNativeArray(value.parray, &data, &count)) {
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
			RECT* rc;
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
				if (S_OK == ::SafeArrayAccessData(value.parray, (void**)&data)) {
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
