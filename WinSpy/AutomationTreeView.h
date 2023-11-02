#pragma once

#include "ViewBase.h"
#include <TreeViewHelper.h>
#include <VirtualListView.h>
#include <CustomSplitterWindow.h>

struct IUIAutomationElement;
struct IUIAutomationTreeWalker;

class CAutomationTreeView :
	public CViewBase<CAutomationTreeView>,
	public CVirtualListView<CAutomationTreeView>,
	public CTreeViewHelper<CAutomationTreeView> {
public:
	explicit CAutomationTreeView(IMainFrame* frame) : CViewBase(frame) {}

	void OnActivate(bool active);

	CString GetColumnText(HWND, int row, int col) const;

protected:
	enum { IDC_TREE = 123 };

	BEGIN_MSG_MAP(CAutomationTreeView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		//NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		//NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		CHAIN_MSG_MAP(CTreeViewHelper<CAutomationTreeView>)
		CHAIN_MSG_MAP(CVirtualListView<CAutomationTreeView>)
		CHAIN_MSG_MAP(CViewBase<CAutomationTreeView>)
	END_MSG_MAP()

	LRESULT OnTreeNodeRightClick(HTREEITEM hItem, CPoint const& pt);
	LRESULT OnTreeNodeDoubleClick(HTREEITEM hItem, CPoint const& pt);

private:
	HTREEITEM AddElement(IUIAutomationElement* element, HTREEITEM hParent = TVI_ROOT, HTREEITEM hAfter = TVI_SORT);
	void EnumChildElements(IUIAutomationTreeWalker* pWalker, IUIAutomationElement* root, HTREEITEM hParent = TVI_ROOT, HTREEITEM hAfter = TVI_SORT, int depth = 0);

	void InitTree();
	void UpdateUI();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CCustomSplitterWindow m_Splitter;
	CTreeViewCtrl m_Tree;
	CListViewCtrl m_List;
};
