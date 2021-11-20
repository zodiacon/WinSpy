#include "pch.h"
#include "MessageDecoder.h"
#include "FormatHelper.h"

#define CASE_STR(x) case x: return L#x 

CString MessageDecoder::Decode(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
        CPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return L"Keys: [" + MouseKey((int)wp) + L"] Pos: " + FormatHelper::FormatPoint(pt);
    }
    if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) {
        CString text;
        text.Format(L"VK: %u", (DWORD)wp);
        return text;
    }

    switch (msg) {
        case WM_SYSCOMMAND:
        {
            CPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            auto cmd = SysCommandToString((DWORD)wp);
            CString pos;
            if (pt.y > 1) {
                pos = L"Cursor: " + FormatHelper::FormatPoint(pt);
            }
            return L"Cmd: " + cmd + L" " + pos;
        }
        case WM_SETREDRAW:
            return CString(L"Redraw: ") + (wp ? L"True" : L"False");

        case WM_SIZE:
        {
            CPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            auto size = L" Size: " + FormatHelper::FormatPoint(pt);
            return L"Type: " + SizeParamToString((DWORD)wp) + size;
        }
    }

    return L"";
}

CString MessageDecoder::MouseKey(int key) {
    static const struct {
        int key;
        PCWSTR name;
    } keys[] = {
        { MK_SHIFT, L"Shift" },
        { MK_CONTROL, L"Ctrl" },
        { MK_LBUTTON, L"LButton" },
        { MK_RBUTTON, L"RButton" },
        { MK_MBUTTON, L"MButton" },
        { MK_XBUTTON1, L"XButton1" },
        { MK_XBUTTON2, L"XButton2" },
    };

    CString text;
    for (auto& item : keys) {
        if ((key & item.key) == item.key)
            text += item.name + CString(L", ");
    }
    if (!text.IsEmpty())
        text = text.Left(text.GetLength() - 2);
    else
        text = L"None";

    return text;
}

CString MessageDecoder::SysCommandToString(DWORD cmd) {
    switch (cmd) {
        CASE_STR(SC_CLOSE);
        CASE_STR(SC_CONTEXTHELP);
        CASE_STR(SC_DEFAULT);
        CASE_STR(SC_HOTKEY);
        CASE_STR(SC_HSCROLL);
        CASE_STR(SCF_ISSECURE);
        CASE_STR(SC_KEYMENU);
        CASE_STR(SC_MAXIMIZE);
        CASE_STR(SC_MINIMIZE);
        CASE_STR(SC_MONITORPOWER);
        CASE_STR(SC_MOUSEMENU);
        CASE_STR(SC_MOVE);
        CASE_STR(SC_NEXTWINDOW);
        CASE_STR(SC_PREVWINDOW);
        CASE_STR(SC_RESTORE);
        CASE_STR(SC_SCREENSAVE);
        CASE_STR(SC_SIZE);
        CASE_STR(SC_TASKLIST);
        CASE_STR(SC_VSCROLL);
    }
    CString text;
    text.Format(L"0x%X", cmd);
    return text;
}

CString MessageDecoder::SizeParamToString(DWORD type) {
    switch (type) {
        CASE_STR(SIZE_MAXHIDE);
        CASE_STR(SIZE_MAXIMIZED);
        CASE_STR(SIZE_MAXSHOW);
        CASE_STR(SIZE_MINIMIZED);
        CASE_STR(SIZE_RESTORED);
    }
    CString text;
    text.Format(L"%u", type);
    return text;
}
