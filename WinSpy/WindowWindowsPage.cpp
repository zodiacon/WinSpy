#include "pch.h"
#include "resource.h"
#include "WindowWindowsPage.h"
#include "FormatHelper.h"

#include "PropertiesDlg.h"

// (WM_PAGE_WINDOW_CHANGED)
const UINT WM_UPDATE = WM_PAGE_WINDOW_CHANGED;

void CWindowWindowsPage::UpdateData() {
    const struct {
        CWindow win;
        UINT idHandle, idClass, idText;
    } windows[] = {
        { m_Win.GetParent(), IDC_HANDLE, IDC_CLASS, IDC_TEXT },
        { m_Win.GetWindow(GW_HWNDNEXT), IDC_HANDLE2, IDC_CLASS2, IDC_TEXT2 },
        { m_Win.GetWindow(GW_HWNDPREV), IDC_HANDLE3, IDC_CLASS3, IDC_TEXT3 },
        { m_Win.GetWindow(GW_CHILD), IDC_HANDLE4, IDC_CLASS4, IDC_TEXT4 },
    };

    CString text;
    for (auto& item : windows) {
        if (!::IsWindow(item.win.m_hWnd)) {
            // clear what the previous window (this page can move to another one) left
            SetDlgItemText(item.idHandle, L"(none)    ");
            SetDlgItemText(item.idClass, L"");
            SetDlgItemText(item.idText, L"");
            continue;
        }
        text.Format(L"<a>0x%zX</a>", (ULONG_PTR)item.win.m_hWnd);
        SetDlgItemText(item.idHandle, text);
        WCHAR className[128];
        SetDlgItemText(item.idClass, ::GetClassName(item.win, className, _countof(className)) ? className : L"");
        item.win.GetWindowText(text);
        SetDlgItemText(item.idText, text);
    }
}

CString CWindowWindowsPage::GetSnapshot() const {
    CString text;
    text.Format(L"%p %p %p %p", m_Win.GetParent().m_hWnd, m_Win.GetWindow(GW_HWNDNEXT).m_hWnd,
        m_Win.GetWindow(GW_HWNDPREV).m_hWnd, m_Win.GetWindow(GW_CHILD).m_hWnd);
    return text;
}

int CWindowWindowsPage::OnSetActive() {
    SetTimer(1, 1000);
    return 0;
}

BOOL CWindowWindowsPage::OnKillActive() {
    KillTimer(1);
    return FALSE;
}

LRESULT CWindowWindowsPage::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
    if (id == 1 && m_Win.IsWindow()) {
        if (auto snapshot = GetSnapshot(); snapshot != m_Snapshot) {
            m_Snapshot = snapshot;
            UpdateData();
        }
    }
    return 0;
}

LRESULT CWindowWindowsPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    UpdateData();
    m_Snapshot = GetSnapshot();
    // the controls follow the page's size (the sheet is resizable; the layout is in the resources)
    InitDynamicLayout(false, false);

    return 0;
}

LRESULT CWindowWindowsPage::OnClickHandle(int, LPNMHDR hdr, BOOL&) {
    auto id = (UINT)hdr->idFrom;
    CString text;
    GetDlgItemText(id, text);
    auto hWnd = (HWND)FormatHelper::ParseHex(text.Mid(3, text.GetLength() - 6));
    m_Win.Detach();
    m_Win.Attach(hWnd);
    UpdateData();
    m_Snapshot = GetSnapshot();
    GetParent().SendMessageToDescendants(WM_UPDATE, 0, reinterpret_cast<LPARAM>(hWnd));
    return 0;
}

