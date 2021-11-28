#include "pch.h"
#include "resource.h"
#include "WindowWindowsPage.h"
#include "FormatHelper.h"

const UINT WM_UPDATE = WM_USER + 300;

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
            SetDlgItemText(item.idHandle, L"(none)    ");
            continue;
        }
        text.Format(L"<a>0x%zX</a>", (ULONG_PTR)item.win.m_hWnd);
        SetDlgItemText(item.idHandle, text);
        if (::GetClassName(item.win, text.GetBufferSetLength(128), 128))
            SetDlgItemText(item.idClass, text);
        item.win.GetWindowText(text);
        SetDlgItemText(item.idText, text);
    }
}

LRESULT CWindowWindowsPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    UpdateData();

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
    GetParent().SendMessageToDescendants(WM_UPDATE, 0, reinterpret_cast<LPARAM>(hWnd));
    return 0;
}

