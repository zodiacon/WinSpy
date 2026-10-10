#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "WindowCommands.h"
#include <VirtualListView.h>
#include <CustomSplitterWindow.h>
#include <UIAutomation.h>

class CAutomationTreeView :
	public CViewBase<CAutomationTreeView>,
	public CVirtualListView<CAutomationTreeView> {
public:
	explicit CAutomationTreeView(IMainFrame* frame) : CViewBase(frame) {}

	void OnActivate(bool active);

	CString GetColumnText(HWND, int row, int col) const;

protected:
	enum { IDC_TREE = 123 };

	BEGIN_MSG_MAP(CAutomationTreeView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_LOCATE_ELEMENT, OnLocateElement)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDED, OnNodeExpanded)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnNodeDeleted)
		NOTIFY_HANDLER(IDC_TREE, NM_RCLICK, OnTreeRightClick)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_WINDOW_HIGHLIGHT, OnHighlight)
		COMMAND_RANGE_HANDLER(ID_AUTOMATION_INVOKE, ID_AUTOMATION_SCROLLINTOVIEW, OnElementAction)
		// the other window commands act on the element's window (or its nearest ancestor's)
		if (uMsg == WM_COMMAND && WindowCommands::IsWindowCommand(LOWORD(wParam))) {
			WindowCommands::Execute(LOWORD(wParam), GetElementWindow(GetSelectedElement()), GetFrame());
			return TRUE;
		}
		CHAIN_MSG_MAP(CVirtualListView<CAutomationTreeView>)
		CHAIN_MSG_MAP(CViewBase<CAutomationTreeView>)
	END_MSG_MAP()

private:
	HTREEITEM AddElement(IUIAutomationElement* element, HTREEITEM hParent = TVI_ROOT, HTREEITEM hAfter = TVI_SORT);
	void EnumChildElements(IUIAutomationTreeWalker* pWalker, IUIAutomationElement* root, HTREEITEM hParent = TVI_ROOT, HTREEITEM hAfter = TVI_SORT);
	// the child node of the element (its children are enumerated if they have not been yet, or again if it is not among them)
	HTREEITEM FindChildNode(HTREEITEM hParent, IUIAutomationElement* element);
	LocateResult LocateElement(IUIAutomationElement* element);
	IUIAutomationElement* GetSelectedElement() const;
	// the element's window, or its nearest ancestor's that has one
	HWND GetElementWindow(IUIAutomationElement* element) const;
	static bool HighlightElement(IUIAutomationElement* element);
	HRESULT DoAction(UINT id, IUIAutomationElement* element);

	void InitTree();
	void UpdateUI();
	void UpdateProperties(IUIAutomationElement* elem);
	void AddPatternProperties(IUIAutomationElement* elem);
	static std::wstring FormatValue(IUIAutomation* pUI, VARIANT const& value);

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLocateElement(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanded(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeDeleted(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeRightClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHighlight(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnElementAction(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	struct ItemData {
		PCWSTR Name;
		std::wstring Value;
	};

	// the selected element's patterns that have actions
	struct ElementPatterns {
		bool Invoke{}, Toggle{}, ExpandCollapse{}, SelectionItem{}, Value{}, ValueReadOnly{}, LegacyIAccessible{}, ScrollItem{};
	};
	// whether the action's command can be used with these patterns
	static bool IsActionEnabled(UINT id, ElementPatterns const& patterns);

	CCustomSplitterWindow m_Splitter;
	CTreeViewCtrl m_Tree;
	CListViewCtrl m_List;
	CComPtr<IUIAutomationTreeWalker> m_spUIWalker;
	CComPtr<IUIAutomation> m_spUI;
	std::vector<ItemData> m_Properties;
	ElementPatterns m_Patterns;
};
