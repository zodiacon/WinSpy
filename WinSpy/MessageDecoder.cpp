#include "pch.h"
#include "MessageDecoder.h"
#include "FormatHelper.h"
#include "WindowHelper.h"
#include "hooks.h"

#define CASE_STR(x) case x: return L#x 

namespace {
    CString Format(PCWSTR format, ...) {
        CString text;
        va_list args;
        va_start(args, format);
        text.FormatV(format, args);
        va_end(args);
        return text;
    }

    CString PointFromLParam(LPARAM lp) {
        return FormatHelper::FormatPoint(CPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
    }

    CString Hwnd(ULONG_PTR h) {
        return FormatHelper::FormatHWndOrNone((HWND)h);
    }

    CString Handle(ULONG_PTR h) {
        return Format(L"0x%zX", h);
    }
}

//
// only values are decoded: a pointer in a WPARAM/LPARAM points into the target process
//
CString MessageDecoder::Decode(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            return L"Keys: [" + MouseKey(GET_KEYSTATE_WPARAM(wp)) + Format(L"] Delta: %d Screen: ", GET_WHEEL_DELTA_WPARAM(wp)) + PointFromLParam(lp);

        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
            return L"Keys: [" + MouseKey(GET_KEYSTATE_WPARAM(wp)) + Format(L"] Button: XButton%d Pos: ", GET_XBUTTON_WPARAM(wp)) + PointFromLParam(lp);

        case WM_MOUSEHOVER:
            return L"Keys: [" + MouseKey((int)wp) + L"] Pos: " + PointFromLParam(lp);
    }

    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)
        return L"Keys: [" + MouseKey((int)wp) + L"] Pos: " + PointFromLParam(lp);

    if ((msg >= WM_NCMOUSEMOVE && msg <= WM_NCXBUTTONDBLCLK) || msg == WM_NCMOUSEHOVER)
        return L"Hit: " + HitTestToString((short)LOWORD(wp)) + L" Screen: " + PointFromLParam(lp);

    switch (msg) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            WCHAR name[64]{};
            ::GetKeyNameText((LONG)lp, name, _countof(name));
            auto text = Format(L"VK: 0x%02X", (DWORD)wp);
            if (*name)
                text += Format(L" (%s)", name);
            return text + L" " + KeyDataToString(lp);
        }

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
        case WM_SYSDEADCHAR:
        case WM_IME_CHAR:
        {
            auto ch = (WCHAR)wp;
            auto text = ch >= 0x20 ? Format(L"Char: '%c' (U+%04X)", ch, ch) : Format(L"Char: U+%04X", ch);
            return text + L" " + KeyDataToString(lp);
        }

        case WM_UNICHAR:
            return wp == UNICODE_NOCHAR ? CString(L"UNICODE_NOCHAR") : Format(L"Char: U+%04X", (DWORD)wp);

        case WM_HOTKEY:
        {
            static const struct { UINT mod; PCWSTR name; } mods[] = {
                { MOD_CONTROL, L"Ctrl" }, { MOD_ALT, L"Alt" }, { MOD_SHIFT, L"Shift" }, { MOD_WIN, L"Win" },
            };
            CString text;
            for (auto& m : mods)
                if (LOWORD(lp) & m.mod)
                    text += m.name + CString(L"+");
            return Format(L"ID: %d Key: %s0x%02X", (int)wp, (PCWSTR)text, HIWORD(lp));
        }

        case WM_TIMER:
            return Format(L"ID: %zu", wp) + (lp ? L" Callback: " + Handle(lp) : CString());

        case WM_COMMAND:
            if (lp == 0)
                return Format(L"ID: %u Source: %s", LOWORD(wp), HIWORD(wp) == 0 ? L"Menu" : HIWORD(wp) == 1 ? L"Accelerator" : L"?");
            return Format(L"ID: %u Notify: %u Control: ", LOWORD(wp), HIWORD(wp)) + Hwnd(lp);

        case WM_SYSCOMMAND:
        {
            // the cursor position is given only for commands chosen with the mouse
            auto text = L"Cmd: " + SysCommandToString((DWORD)wp);
            if (GET_Y_LPARAM(lp) > 0)
                text += L" Screen: " + PointFromLParam(lp);
            return text;
        }

        case WM_NOTIFY:
            return Format(L"Control ID: %zu", wp);

        case WM_SETREDRAW:
            return CString(L"Redraw: ") + (wp ? L"True" : L"False");

        case WM_SIZE:
            return L"Type: " + SizeParamToString((DWORD)wp) + Format(L" Size: %u x %u", LOWORD(lp), HIWORD(lp));

        case WM_MOVE:
            return L"Pos: " + PointFromLParam(lp);

        case WM_ACTIVATE:
        {
            PCWSTR state = LOWORD(wp) == WA_ACTIVE ? L"WA_ACTIVE" : LOWORD(wp) == WA_CLICKACTIVE ? L"WA_CLICKACTIVE" : L"WA_INACTIVE";
            return Format(L"%s%s Other: ", state, HIWORD(wp) ? L" (Minimized)" : L"") + Hwnd(lp);
        }

        case WM_ACTIVATEAPP:
            return Format(L"Active: %s Other Thread: %u", wp ? L"True" : L"False", (DWORD)lp);

        case WM_NCACTIVATE:
        case WM_ENABLE:
            return CString(wp ? L"True" : L"False");

        case WM_MOUSEACTIVATE:
            return L"Top Level: " + Hwnd(wp) + L" Hit: " + HitTestToString((short)LOWORD(lp)) + L" Mouse: " + WindowHelper::WindowMessageToString(HIWORD(lp));

        case WM_SETCURSOR:
            return L"Window: " + Hwnd(wp) + L" Hit: " + HitTestToString((short)LOWORD(lp)) + L" Mouse: " + WindowHelper::WindowMessageToString(HIWORD(lp));

        case WM_NCHITTEST:
            return L"Screen: " + PointFromLParam(lp);

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            return L"Other: " + Hwnd(wp);

        case WM_CAPTURECHANGED:
            return L"New Capture: " + Hwnd(lp);

        case WM_SHOWWINDOW:
        {
            static PCWSTR const status[] = { L"", L"SW_PARENTCLOSING", L"SW_OTHERZOOM", L"SW_PARENTOPENING", L"SW_OTHERUNZOOM" };
            CString text(wp ? L"Show" : L"Hide");
            if (lp > 0 && lp < _countof(status))
                text += CString(L" ") + status[lp];
            return text;
        }

        case WM_CONTEXTMENU:
            if (lp == -1)
                return L"Window: " + Hwnd(wp) + L" (Keyboard)";
            return L"Window: " + Hwnd(wp) + L" Screen: " + PointFromLParam(lp);

        case WM_HSCROLL:
        case WM_VSCROLL:
        {
            auto text = L"Request: " + ScrollRequestToString(LOWORD(wp), msg == WM_HSCROLL);
            if (LOWORD(wp) == SB_THUMBPOSITION || LOWORD(wp) == SB_THUMBTRACK)
                text += Format(L" Pos: %u", HIWORD(wp));
            if (lp)
                text += L" Scroll Bar: " + Hwnd(lp);
            return text;
        }

        case WM_MENUSELECT:
            if (HIWORD(wp) == 0xFFFF && lp == 0)
                return L"Menu Closed";
            return Format(L"Item: %u Flags: 0x%X Menu: ", LOWORD(wp), HIWORD(wp)) + Handle(lp);

        case WM_INITMENU:
            return L"Menu: " + Handle(wp);

        case WM_INITMENUPOPUP:
            return L"Menu: " + Handle(wp) + Format(L" Index: %u%s", LOWORD(lp), HIWORD(lp) ? L" (Window Menu)" : L"");

        case WM_MENUCHAR:
            return Format(L"Char: U+%04X Menu: ", LOWORD(wp)) + Handle(lp);

        case WM_CTLCOLORMSGBOX:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
            return L"HDC: " + Handle(wp) + L" Control: " + Hwnd(lp);

        case WM_ERASEBKGND:
        case WM_ICONERASEBKGND:
            return L"HDC: " + Handle(wp);

        case WM_PRINTCLIENT:
        case WM_PRINT:
            return L"HDC: " + Handle(wp) + Format(L" Flags: 0x%X", (DWORD)lp);

        case WM_SETFONT:
            return L"Font: " + Handle(wp) + (lp ? L" (Redraw)" : L"");

        case WM_GETICON:
        case WM_SETICON:
        {
            PCWSTR type = wp == ICON_BIG ? L"ICON_BIG" : wp == ICON_SMALL ? L"ICON_SMALL" : wp == ICON_SMALL2 ? L"ICON_SMALL2" : L"?";
            return CString(L"Type: ") + type + (msg == WM_SETICON ? L" Icon: " + Handle(lp) : CString());
        }

        case WM_GETTEXT:
            return Format(L"Buffer: %zu chars", wp);

        case WM_QUIT:
            return Format(L"Exit Code: %zd", (INT_PTR)wp);

        case WM_INPUT:
            return CString(GET_RAWINPUT_CODE_WPARAM(wp) == RIM_INPUT ? L"RIM_INPUT" : L"RIM_INPUTSINK") + L" Raw Input: " + Handle(lp);

        case WM_INPUTLANGCHANGEREQUEST:
            return Format(L"Flags: 0x%X Layout: ", (DWORD)wp) + Handle(lp);

        case WM_INPUTLANGCHANGE:
            return Format(L"Charset: %u Layout: ", (DWORD)wp) + Handle(lp);

        case WM_APPCOMMAND:
            return Format(L"Command: %u Device: 0x%X Keys: [", GET_APPCOMMAND_LPARAM(lp), GET_DEVICE_LPARAM(lp)) + MouseKey(GET_KEYSTATE_LPARAM(lp)) + L"] Window: " + Hwnd(wp);

        case WM_DISPLAYCHANGE:
            return Format(L"Bits/Pixel: %u Size: %u x %u", (DWORD)wp, LOWORD(lp), HIWORD(lp));

        case WM_DPICHANGED:
            return Format(L"DPI: %u x %u", LOWORD(wp), HIWORD(wp));

        case WM_POINTERDOWN:
        case WM_POINTERUP:
        case WM_POINTERUPDATE:
        case WM_POINTERENTER:
        case WM_POINTERLEAVE:
        case WM_NCPOINTERDOWN:
        case WM_NCPOINTERUP:
        case WM_NCPOINTERUPDATE:
            return Format(L"Pointer ID: %u Screen: ", GET_POINTERID_WPARAM(wp)) + PointFromLParam(lp);

        case WM_POINTERWHEEL:
        case WM_POINTERHWHEEL:
            return Format(L"Pointer ID: %u Delta: %d Screen: ", GET_POINTERID_WPARAM(wp), GET_WHEEL_DELTA_WPARAM(wp)) + PointFromLParam(lp);
    }

    return L"";
}

CString MessageDecoder::DecodeExtra(ExtraDataType type, BYTE const* data, DWORD size) {
    auto has = [&](DWORD needed) { return size >= needed; };
    switch (type) {
        case ExtraDataType::Text:
            return L"Text: \"" + CString((PCWSTR)data, size / sizeof(WCHAR)) + L"\"";

        case ExtraDataType::WindowPos:
        {
            if (!has(sizeof(WINDOWPOS)))
                break;
            auto wp = (WINDOWPOS const*)data;
            static const struct { UINT flag; PCWSTR name; } flags[] = {
                { SWP_NOSIZE, L"NOSIZE" }, { SWP_NOMOVE, L"NOMOVE" }, { SWP_NOZORDER, L"NOZORDER" }, { SWP_NOREDRAW, L"NOREDRAW" },
                { SWP_NOACTIVATE, L"NOACTIVATE" }, { SWP_FRAMECHANGED, L"FRAMECHANGED" }, { SWP_SHOWWINDOW, L"SHOWWINDOW" },
                { SWP_HIDEWINDOW, L"HIDEWINDOW" }, { SWP_NOCOPYBITS, L"NOCOPYBITS" }, { SWP_NOOWNERZORDER, L"NOOWNERZORDER" },
                { SWP_NOSENDCHANGING, L"NOSENDCHANGING" }, { SWP_DEFERERASE, L"DEFERERASE" }, { SWP_ASYNCWINDOWPOS, L"ASYNCWINDOWPOS" },
            };
            CString names;
            for (auto& f : flags)
                if (wp->flags & f.flag)
                    names += (names.IsEmpty() ? L"" : L"|") + CString(f.name);
            return Format(L"Pos: (%d,%d) Size: %d x %d After: ", wp->x, wp->y, wp->cx, wp->cy) + Hwnd((ULONG_PTR)wp->hwndInsertAfter)
                + L" Flags: " + (names.IsEmpty() ? CString(L"0") : names);
        }

        case ExtraDataType::Notify:
        {
            if (!has(sizeof(NMHDR)))
                break;
            auto hdr = (NMHDR const*)data;
            return L"From: " + Hwnd((ULONG_PTR)hdr->hwndFrom) + Format(L" ID: %zu Code: %d", hdr->idFrom, (int)hdr->code);
        }

        case ExtraDataType::Style:
        {
            if (!has(sizeof(STYLESTRUCT)))
                break;
            auto ss = (STYLESTRUCT const*)data;
            return Format(L"Old: 0x%08X New: 0x%08X", ss->styleOld, ss->styleNew);
        }

        case ExtraDataType::MinMaxInfo:
        {
            if (!has(sizeof(MINMAXINFO)))
                break;
            auto mmi = (MINMAXINFO const*)data;
            return Format(L"Max: %d x %d at (%d,%d) Track: %d x %d - %d x %d",
                mmi->ptMaxSize.x, mmi->ptMaxSize.y, mmi->ptMaxPosition.x, mmi->ptMaxPosition.y,
                mmi->ptMinTrackSize.x, mmi->ptMinTrackSize.y, mmi->ptMaxTrackSize.x, mmi->ptMaxTrackSize.y);
        }

        case ExtraDataType::Create:
        {
            if (!has(sizeof(CreateData)))
                break;
            auto cd = (CreateData const*)data;
            CString name(cd->Name, (int)wcsnlen(cd->Name, _countof(cd->Name)));
            return Format(L"Pos: (%d,%d) Size: %d x %d Style: 0x%08X ExStyle: 0x%08X", cd->x, cd->y, cd->cx, cd->cy, cd->Style, cd->ExStyle)
                + (name.IsEmpty() ? CString() : L" Name: \"" + name + L"\"");
        }

        case ExtraDataType::Rect:
            if (!has(sizeof(RECT)))
                break;
            return L"Rect: " + FormatHelper::RectToString(*(RECT const*)data);
    }
    return L"";
}

CString MessageDecoder::KeyDataToString(LPARAM lp) {
    auto text = Format(L"Repeat: %u Scan: 0x%02X", LOWORD(lp), (DWORD)(lp >> 16) & 0xFF);
    if (lp & (1 << 24))
        text += L" Extended";
    if (lp & (1 << 29))
        text += L" Alt";
    if (lp & (1 << 30))
        text += L" WasDown";
    if (lp & (1u << 31))
        text += L" Released";
    return text;
}

CString MessageDecoder::HitTestToString(int hit) {
    switch (hit) {
        CASE_STR(HTERROR);
        CASE_STR(HTTRANSPARENT);
        CASE_STR(HTNOWHERE);
        CASE_STR(HTCLIENT);
        CASE_STR(HTCAPTION);
        CASE_STR(HTSYSMENU);
        CASE_STR(HTGROWBOX);
        CASE_STR(HTMENU);
        CASE_STR(HTHSCROLL);
        CASE_STR(HTVSCROLL);
        CASE_STR(HTMINBUTTON);
        CASE_STR(HTMAXBUTTON);
        CASE_STR(HTLEFT);
        CASE_STR(HTRIGHT);
        CASE_STR(HTTOP);
        CASE_STR(HTTOPLEFT);
        CASE_STR(HTTOPRIGHT);
        CASE_STR(HTBOTTOM);
        CASE_STR(HTBOTTOMLEFT);
        CASE_STR(HTBOTTOMRIGHT);
        CASE_STR(HTBORDER);
        CASE_STR(HTOBJECT);
        CASE_STR(HTCLOSE);
        CASE_STR(HTHELP);
    }
    return Format(L"%d", hit);
}

CString MessageDecoder::ScrollRequestToString(int request, bool horizontal) {
    switch (request) {
        case SB_LINEUP: return horizontal ? L"SB_LINELEFT" : L"SB_LINEUP";
        case SB_LINEDOWN: return horizontal ? L"SB_LINERIGHT" : L"SB_LINEDOWN";
        case SB_PAGEUP: return horizontal ? L"SB_PAGELEFT" : L"SB_PAGEUP";
        case SB_PAGEDOWN: return horizontal ? L"SB_PAGERIGHT" : L"SB_PAGEDOWN";
        case SB_TOP: return horizontal ? L"SB_LEFT" : L"SB_TOP";
        case SB_BOTTOM: return horizontal ? L"SB_RIGHT" : L"SB_BOTTOM";
        CASE_STR(SB_THUMBPOSITION);
        CASE_STR(SB_THUMBTRACK);
        CASE_STR(SB_ENDSCROLL);
    }
    return Format(L"%d", request);
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
    // the low four bits are used internally by Windows (e.g. SC_MOVE | HTCAPTION when dragging a caption)
    switch (cmd & 0xFFF0) {
        CASE_STR(SC_CLOSE);
        CASE_STR(SC_CONTEXTHELP);
        CASE_STR(SC_DEFAULT);
        CASE_STR(SC_HOTKEY);
        CASE_STR(SC_HSCROLL);
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
