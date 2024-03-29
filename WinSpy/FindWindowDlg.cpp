#include "pch.h"
#include "resource.h"
#include "FindWindowDlg.h"
#include "WindowHelper.h"
#include "ProcessHelper.h"
#include "FormatHelper.h"

HWND CFindWindowDlg::GetSelectedHwnd() const {
	return m_SelectedHwnd;
}

void CFindWindowDlg::ClearWindowDetails() {
	SetDlgItemText(IDC_HANDLE, L"");
	SetDlgItemText(IDC_TEXT, L"");
	SetDlgItemText(IDC_CLASSNAME, L"");
	SetDlgItemText(IDC_THREAD, L"");
	SetDlgItemText(IDC_PROCESS, L"");
	if (m_hCursorWnd)
		WindowHelper::HighlightBorder(m_hCursorWnd, false);
	m_WinDrag.ShowWindow(SW_SHOW);
	if (IsDlgButtonChecked(IDC_HIDE) == BST_CHECKED) {
		::ShowWindow(m_pFrame->GetHwnd(), SW_SHOW);
	}
}

void CFindWindowDlg::SelectWindow(HWND hWnd) {
	CString text;
	text.Format(L"0x%zX", DWORD_PTR(hWnd));
	SetDlgItemText(IDC_HANDLE, text);
	WCHAR clsName[128];
	m_SelectedHwnd = hWnd;
	if (::GetClassName(hWnd, clsName, _countof(clsName)))
		SetDlgItemText(IDC_CLASSNAME, clsName);
	CWindow win(hWnd);
	win.GetWindowText(text);
	SetDlgItemText(IDC_TEXT, text);
	DWORD pid;
	auto tid = ::GetWindowThreadProcessId(hWnd, &pid);
	SetDlgItemInt(IDC_THREAD, tid, FALSE);
	text.Format(L"%s (%d)", (PCWSTR)ProcessHelper::GetProcessImageName(pid), pid);
	SetDlgItemText(IDC_PROCESS, text);
}

LRESULT CFindWindowDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_WINDOWSEARCH);
	m_WinDrag.SubclassWindow(GetDlgItem(IDC_TARGET));
	m_WinDrag.SetIcon(AtlLoadIconImage(IDI_TARGET, 0, 32, 32));
	m_DragCursor = AtlLoadIconImage(IDI_TARGET, 0, ::GetSystemMetrics(SM_CXCURSOR), ::GetSystemMetrics(SM_CYCURSOR));

	return 0;
}

LRESULT CFindWindowDlg::OnCloseCmd(WORD, WORD id, HWND, BOOL&) {
	if (id == IDCANCEL && m_Capture) {
		ReleaseCapture();
		m_Capture = false;
		ClearWindowDetails();
		return 0;
	}
	if (id == IDOK) {
		CString text;
		GetDlgItemText(IDC_HANDLE, text);
		m_SelectedHwnd = (HWND)FormatHelper::ParseHex(text);
	}

	EndDialog(id);
	return 0;
}

LRESULT CFindWindowDlg::OnSearch(WORD, WORD id, HWND, BOOL&) {
	CString handle;
	GetDlgItemText(IDC_HANDLE, handle);
	if (!handle.IsEmpty()) {
		auto hWnd = (HWND)FormatHelper::ParseHex(handle);
		if (::IsWindow(hWnd)) {
			SelectWindow(hWnd);
		}
		else {
			AtlMessageBox(m_hWnd, (PCWSTR)(L"Cannot locate window with handle " + handle), L"Window Finder", MB_ICONERROR);
		}
		return 0;
	}
	CString text, cls;
	GetDlgItemText(IDC_TEXT, text);
	GetDlgItemText(IDC_CLASSNAME, cls);
	if (text.IsEmpty() && cls.IsEmpty()) {
		AtlMessageBox(m_hWnd, L"Specify handle, class and/or text to search", L"Window Finder", MB_ICONWARNING);
		return 0;
	}

	auto hWnd = ::FindWindowEx(nullptr, nullptr, cls, text);
	if (hWnd)
		SelectWindow(hWnd);
	else
		AtlMessageBox(m_hWnd, L"Cannot locate window with this class/text", L"Window Finder", MB_ICONERROR);
	return 0;
}

LRESULT CFindWindowDlg::OnMouseDown(UINT, WPARAM, LPARAM, BOOL&) {
	m_WinDrag.SetCapture();
	m_Capture = true;
	m_WinDrag.ShowWindow(SW_HIDE);
	if (IsDlgButtonChecked(IDC_HIDE) == BST_CHECKED) {
		::ShowWindow(m_pFrame->GetHwnd(), SW_HIDE);
	}
	return 0;
}

LRESULT CFindWindowDlg::OnMouseUp(UINT, WPARAM, LPARAM, BOOL&) {
	ReleaseCapture();
	m_Capture = false;
	m_WinDrag.ShowWindow(SW_SHOW);
	SetCursor(AtlLoadSysCursor(IDC_ARROW));
	if(m_hCursorWnd)
		WindowHelper::HighlightBorder(m_hCursorWnd, false);
	if (IsDlgButtonChecked(IDC_HIDE) == BST_CHECKED) {
		::ShowWindow(m_pFrame->GetHwnd(), SW_SHOW);
	}
	return 0;
}

LRESULT CFindWindowDlg::OnMouseMove(UINT, WPARAM, LPARAM lp, BOOL&) {
	if (m_Capture) {
		::SetCursor(m_DragCursor);
		CPoint pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
		m_WinDrag.ClientToScreen(&pt);
		auto hWnd = ::WindowFromPoint(pt);
		DWORD pid, tid;
		if ((tid = ::GetWindowThreadProcessId(hWnd, &pid)) && pid == ::GetCurrentProcessId())
			return 0;

		if (m_hCursorWnd && m_hCursorWnd != hWnd)
			WindowHelper::HighlightBorder(m_hCursorWnd, false);
		if (hWnd != m_hCursorWnd) {
			WindowHelper::HighlightBorder(hWnd);
			m_hCursorWnd.Detach();
			m_hCursorWnd.Attach(hWnd);
			SelectWindow(hWnd);
		}
	}
	return 0;
}
