#pragma once

template<typename T>
class CTreeViewManager {
protected:
	BEGIN_MSG_MAP(CTreeViewManager)
		NOTIFY_CODE_HANDLER(NM_RCLICK, OnRightClick)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnTreeSelectionChanged)
	END_MSG_MAP()

	LRESULT OnTreeSelectionChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
		return 0;
	}

	LRESULT OnRightClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
		CTreeViewCtrl tv(pnmh->hwndFrom);
		CPoint pt;
		::GetCursorPos(&pt);
		CPoint pt2(pt);
		tv.ScreenToClient(&pt2);
		auto hItem = tv.HitTest(pt2, nullptr);
		if (!hItem)
			return 0;

		tv.SelectItem(hItem);
		auto pT = static_cast<T*>(this);
		return pT->OnTreeNodeRightClick(hItem, pt);
	}

private:
	//
	// overridables
	//
	LRESULT OnTreeNodeRightClick(HTREEITEM hItem, CPoint const& pt) {
		return 0;
	}
};

