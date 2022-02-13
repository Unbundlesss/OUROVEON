//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  turn windows error codes into strings
//

#include "pch.h"

#if OURO_PLATFORM_WIN

#include "win32/errors.h"

namespace win32 {

//----------------------------------------------------------------------------------------------------------------------
std::string FormatErrorCode( const LONG errorCode )
{
    const size_t returnBufferSize = 1024;
    char returnBuffer[returnBufferSize]{ 0 };

    // drop the error number in at the start
    const int indentPoint = sprintf_s( returnBuffer, "0x%08X ", errorCode );

    DWORD retCode = FormatMessageA(
                                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                    nullptr,
                                    errorCode,
                                    MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                                    &returnBuffer[indentPoint],
                                    returnBufferSize - indentPoint,
                                    nullptr );

    if ( retCode == 0 )
    {
        return std::string{};
    }

    return std::string{ returnBuffer };
}

//----------------------------------------------------------------------------------------------------------------------
std::string FormatLastErrorCode()
{
    DWORD errorCode = ::GetLastError();
    if ( errorCode == 0 )
        return std::string{};

    return FormatErrorCode( errorCode );
}

//----------------------------------------------------------------------------------------------------------------------
const char* GetStringFromWM( uint32_t wmcode )
{
    switch ( wmcode )
    {
    case WM_SHOWWINDOW:                     return ( "WM_SHOWWINDOW" );
    case WM_KEYUP:                          return ( "WM_KEYUP" );
    case WM_APPCOMMAND:                     return ( "WM_APPCOMMAND" );
    case WM_INPUTLANGCHANGEREQUEST:         return ( "WM_INPUTLANGCHANGEREQUEST" );
    case WM_NULL:                           return ( "WM_NULL" );
    case WM_CREATE:                         return ( "WM_CREATE" );
    case WM_DESTROY:                        return ( "WM_DESTROY" );
    case WM_MOVE:                           return ( "WM_MOVE" );
    case WM_SIZE:                           return ( "WM_SIZE" );
    case WM_ACTIVATE:                       return ( "WM_ACTIVATE" );
    case WM_SETFOCUS:                       return ( "WM_SETFOCUS" );
    case WM_KILLFOCUS:                      return ( "WM_KILLFOCUS" );
    case WM_ENABLE:                         return ( "WM_ENABLE" );
    case WM_SETREDRAW:                      return ( "WM_SETREDRAW" );
    case WM_SETTEXT:                        return ( "WM_SETTEXT" );
    case WM_GETTEXT:                        return ( "WM_GETTEXT" );
    case WM_GETTEXTLENGTH:                  return ( "WM_GETTEXTLENGTH" );
    case WM_PAINT:                          return ( "WM_PAINT" );
    case WM_CLOSE:                          return ( "WM_CLOSE" );
    case WM_QUERYENDSESSION:                return ( "WM_QUERYENDSESSION" );
    case WM_QUERYOPEN:                      return ( "WM_QUERYOPEN" );
    case WM_ENDSESSION:                     return ( "WM_ENDSESSION" );
    case WM_QUIT:                           return ( "WM_QUIT" );
    case WM_ERASEBKGND:                     return ( "WM_ERASEBKGND" );
    case WM_SYSCOLORCHANGE:                 return ( "WM_SYSCOLORCHANGE" );
    case WM_WININICHANGE:                   return ( "WM_WININICHANGE" );
    case WM_DEVMODECHANGE:                  return ( "WM_DEVMODECHANGE" );
    case WM_ACTIVATEAPP:                    return ( "WM_ACTIVATEAPP" );
    case WM_FONTCHANGE:                     return ( "WM_FONTCHANGE" );
    case WM_TIMECHANGE:                     return ( "WM_TIMECHANGE" );
    case WM_CANCELMODE:                     return ( "WM_CANCELMODE" );
    case WM_SETCURSOR:                      return ( "WM_SETCURSOR" );
    case WM_MOUSEACTIVATE:                  return ( "WM_MOUSEACTIVATE" );
    case WM_CHILDACTIVATE:                  return ( "WM_CHILDACTIVATE" );
    case WM_QUEUESYNC:                      return ( "WM_QUEUESYNC" );
    case WM_GETMINMAXINFO:                  return ( "WM_GETMINMAXINFO" );
    case WM_PAINTICON:                      return ( "WM_PAINTICON" );
    case WM_ICONERASEBKGND:                 return ( "WM_ICONERASEBKGND" );
    case WM_NEXTDLGCTL:                     return ( "WM_NEXTDLGCTL" );
    case WM_SPOOLERSTATUS:                  return ( "WM_SPOOLERSTATUS" );
    case WM_DRAWITEM:                       return ( "WM_DRAWITEM" );
    case WM_MEASUREITEM:                    return ( "WM_MEASUREITEM" );
    case WM_DELETEITEM:                     return ( "WM_DELETEITEM" );
    case WM_VKEYTOITEM:                     return ( "WM_VKEYTOITEM" );
    case WM_CHARTOITEM:                     return ( "WM_CHARTOITEM" );
    case WM_SETFONT:                        return ( "WM_SETFONT" );
    case WM_GETFONT:                        return ( "WM_GETFONT" );
    case WM_SETHOTKEY:                      return ( "WM_SETHOTKEY" );
    case WM_GETHOTKEY:                      return ( "WM_GETHOTKEY" );
    case WM_QUERYDRAGICON:                  return ( "WM_QUERYDRAGICON" );
    case WM_COMPAREITEM:                    return ( "WM_COMPAREITEM" );
    case WM_GETOBJECT:                      return ( "WM_GETOBJECT" );
    case WM_COMPACTING:                     return ( "WM_COMPACTING" );
    case WM_COMMNOTIFY:                     return ( "WM_COMMNOTIFY" );
    case WM_WINDOWPOSCHANGING:              return ( "WM_WINDOWPOSCHANGING" );
    case WM_WINDOWPOSCHANGED:               return ( "WM_WINDOWPOSCHANGED" );
    case WM_POWER:                          return ( "WM_POWER" );
    case WM_COPYDATA:                       return ( "WM_COPYDATA" );
    case WM_CANCELJOURNAL:                  return ( "WM_CANCELJOURNAL" );
    case WM_NOTIFY:                         return ( "WM_NOTIFY" );
    case WM_INPUTLANGCHANGE:                return ( "WM_INPUTLANGCHANGE" );
    case WM_TCARD:                          return ( "WM_TCARD" );
    case WM_HELP:                           return ( "WM_HELP" );
    case WM_USERCHANGED:                    return ( "WM_USERCHANGED" );
    case WM_NOTIFYFORMAT:                   return ( "WM_NOTIFYFORMAT" );
    case WM_CONTEXTMENU:                    return ( "WM_CONTEXTMENU" );
    case WM_STYLECHANGING:                  return ( "WM_STYLECHANGING" );
    case WM_STYLECHANGED:                   return ( "WM_STYLECHANGED" );
    case WM_DISPLAYCHANGE:                  return ( "WM_DISPLAYCHANGE" );
    case WM_GETICON:                        return ( "WM_GETICON" );
    case WM_SETICON:                        return ( "WM_SETICON" );
    case WM_NCCREATE:                       return ( "WM_NCCREATE" );
    case WM_NCDESTROY:                      return ( "WM_NCDESTROY" );
    case WM_NCCALCSIZE:                     return ( "WM_NCCALCSIZE" );
    case WM_NCHITTEST:                      return ( "WM_NCHITTEST" );
    case WM_NCPAINT:                        return ( "WM_NCPAINT" );
    case WM_NCACTIVATE:                     return ( "WM_NCACTIVATE" );
    case WM_GETDLGCODE:                     return ( "WM_GETDLGCODE" );
    case WM_SYNCPAINT:                      return ( "WM_SYNCPAINT" );
    case WM_NCMOUSEMOVE:                    return ( "WM_NCMOUSEMOVE" );
    case WM_NCLBUTTONDOWN:                  return ( "WM_NCLBUTTONDOWN" );
    case WM_NCLBUTTONUP:                    return ( "WM_NCLBUTTONUP" );
    case WM_NCLBUTTONDBLCLK:                return ( "WM_NCLBUTTONDBLCLK" );
    case WM_NCRBUTTONDOWN:                  return ( "WM_NCRBUTTONDOWN" );
    case WM_NCRBUTTONUP:                    return ( "WM_NCRBUTTONUP" );
    case WM_NCRBUTTONDBLCLK:                return ( "WM_NCRBUTTONDBLCLK" );
    case WM_NCMBUTTONDOWN:                  return ( "WM_NCMBUTTONDOWN" );
    case WM_NCMBUTTONUP:                    return ( "WM_NCMBUTTONUP" );
    case WM_NCMBUTTONDBLCLK:                return ( "WM_NCMBUTTONDBLCLK" );
    case WM_NCXBUTTONDOWN:                  return ( "WM_NCXBUTTONDOWN" );
    case WM_NCXBUTTONUP:                    return ( "WM_NCXBUTTONUP" );
    case WM_NCXBUTTONDBLCLK:                return ( "WM_NCXBUTTONDBLCLK" );
    case WM_INPUT:                          return ( "WM_INPUT" );
    case WM_KEYDOWN:                        return ( "WM_KEYDOWN" );
    case WM_CHAR:                           return ( "WM_CHAR" );
    case WM_DEADCHAR:                       return ( "WM_DEADCHAR" );
    case WM_SYSKEYDOWN:                     return ( "WM_SYSKEYDOWN" );
    case WM_SYSKEYUP:                       return ( "WM_SYSKEYUP" );
    case WM_SYSCHAR:                        return ( "WM_SYSCHAR" );
    case WM_SYSDEADCHAR:                    return ( "WM_SYSDEADCHAR" );
    case WM_UNICHAR:                        return ( "WM_UNICHAR" );
    case WM_INITDIALOG:                     return ( "WM_INITDIALOG" );
    case WM_COMMAND:                        return ( "WM_COMMAND" );
    case WM_SYSCOMMAND:                     return ( "WM_SYSCOMMAND" );
    case WM_TIMER:                          return ( "WM_TIMER" );
    case WM_HSCROLL:                        return ( "WM_HSCROLL" );
    case WM_VSCROLL:                        return ( "WM_VSCROLL" );
    case WM_INITMENU:                       return ( "WM_INITMENU" );
    case WM_INITMENUPOPUP:                  return ( "WM_INITMENUPOPUP" );
    case WM_GESTURE:                        return ( "WM_GESTURE" );
    case WM_GESTURENOTIFY:                  return ( "WM_GESTURENOTIFY" );
    case WM_MENUSELECT:                     return ( "WM_MENUSELECT" );
    case WM_MENUCHAR:                       return ( "WM_MENUCHAR" );
    case WM_ENTERIDLE:                      return ( "WM_ENTERIDLE" );
    case WM_MENURBUTTONUP:                  return ( "WM_MENURBUTTONUP" );
    case WM_MENUDRAG:                       return ( "WM_MENUDRAG" );
    case WM_MENUGETOBJECT:                  return ( "WM_MENUGETOBJECT" );
    case WM_UNINITMENUPOPUP:                return ( "WM_UNINITMENUPOPUP" );
    case WM_MENUCOMMAND:                    return ( "WM_MENUCOMMAND" );
    case WM_CHANGEUISTATE:                  return ( "WM_CHANGEUISTATE" );
    case WM_UPDATEUISTATE:                  return ( "WM_UPDATEUISTATE" );
    case WM_QUERYUISTATE:                   return ( "WM_QUERYUISTATE" );
    case WM_CTLCOLORMSGBOX:                 return ( "WM_CTLCOLORMSGBOX" );
    case WM_CTLCOLOREDIT:                   return ( "WM_CTLCOLOREDIT" );
    case WM_CTLCOLORLISTBOX:                return ( "WM_CTLCOLORLISTBOX" );
    case WM_CTLCOLORBTN:                    return ( "WM_CTLCOLORBTN" );
    case WM_CTLCOLORDLG:                    return ( "WM_CTLCOLORDLG" );
    case WM_CTLCOLORSCROLLBAR:              return ( "WM_CTLCOLORSCROLLBAR" );
    case WM_CTLCOLORSTATIC:                 return ( "WM_CTLCOLORSTATIC" );
    case WM_MOUSEMOVE:                      return ( "WM_MOUSEMOVE" );
    case WM_LBUTTONDOWN:                    return ( "WM_LBUTTONDOWN" );
    case WM_LBUTTONUP:                      return ( "WM_LBUTTONUP" );
    case WM_LBUTTONDBLCLK:                  return ( "WM_LBUTTONDBLCLK" );
    case WM_RBUTTONDOWN:                    return ( "WM_RBUTTONDOWN" );
    case WM_RBUTTONUP:                      return ( "WM_RBUTTONUP" );
    case WM_RBUTTONDBLCLK:                  return ( "WM_RBUTTONDBLCLK" );
    case WM_MBUTTONDOWN:                    return ( "WM_MBUTTONDOWN" );
    case WM_MBUTTONUP:                      return ( "WM_MBUTTONUP" );
    case WM_MBUTTONDBLCLK:                  return ( "WM_MBUTTONDBLCLK" );
    case WM_MOUSEWHEEL:                     return ( "WM_MOUSEWHEEL" );
    case WM_XBUTTONDOWN:                    return ( "WM_XBUTTONDOWN" );
    case WM_XBUTTONUP:                      return ( "WM_XBUTTONUP" );
    case WM_XBUTTONDBLCLK:                  return ( "WM_XBUTTONDBLCLK" );
    case WM_MOUSEHWHEEL:                    return ( "WM_MOUSEHWHEEL" );
    case WM_PARENTNOTIFY:                   return ( "WM_PARENTNOTIFY" );
    case WM_ENTERMENULOOP:                  return ( "WM_ENTERMENULOOP" );
    case WM_EXITMENULOOP:                   return ( "WM_EXITMENULOOP" );
    case WM_NEXTMENU:                       return ( "WM_NEXTMENU" );
    case WM_SIZING:                         return ( "WM_SIZING" );
    case WM_CAPTURECHANGED:                 return ( "WM_CAPTURECHANGED" );
    case WM_MOVING:                         return ( "WM_MOVING" );
    case WM_POWERBROADCAST:                 return ( "WM_POWERBROADCAST" );
    case WM_DEVICECHANGE:                   return ( "WM_DEVICECHANGE" );
    case WM_MDICREATE:                      return ( "WM_MDICREATE" );
    case WM_MDIDESTROY:                     return ( "WM_MDIDESTROY" );
    case WM_MDIACTIVATE:                    return ( "WM_MDIACTIVATE" );
    case WM_MDIRESTORE:                     return ( "WM_MDIRESTORE" );
    case WM_MDINEXT:                        return ( "WM_MDINEXT" );
    case WM_MDIMAXIMIZE:                    return ( "WM_MDIMAXIMIZE" );
    case WM_MDITILE:                        return ( "WM_MDITILE" );
    case WM_MDICASCADE:                     return ( "WM_MDICASCADE" );
    case WM_MDIICONARRANGE:                 return ( "WM_MDIICONARRANGE" );
    case WM_MDIGETACTIVE:                   return ( "WM_MDIGETACTIVE" );
    case WM_MDISETMENU:                     return ( "WM_MDISETMENU" );
    case WM_ENTERSIZEMOVE:                  return ( "WM_ENTERSIZEMOVE" );
    case WM_EXITSIZEMOVE:                   return ( "WM_EXITSIZEMOVE" );
    case WM_DROPFILES:                      return ( "WM_DROPFILES" );
    case WM_MDIREFRESHMENU:                 return ( "WM_MDIREFRESHMENU" );
    case WM_TOUCH:                          return ( "WM_TOUCH" );
    case WM_MOUSEHOVER:                     return ( "WM_MOUSEHOVER" );
    case WM_MOUSELEAVE:                     return ( "WM_MOUSELEAVE" );
    case WM_NCMOUSEHOVER:                   return ( "WM_NCMOUSEHOVER" );
    case WM_NCMOUSELEAVE:                   return ( "WM_NCMOUSELEAVE" );
    case WM_CUT:                            return ( "WM_CUT" );
    case WM_COPY:                           return ( "WM_COPY" );
    case WM_PASTE:                          return ( "WM_PASTE" );
    case WM_CLEAR:                          return ( "WM_CLEAR" );
    case WM_UNDO:                           return ( "WM_UNDO" );
    case WM_RENDERFORMAT:                   return ( "WM_RENDERFORMAT" );
    case WM_RENDERALLFORMATS:               return ( "WM_RENDERALLFORMATS" );
    case WM_DESTROYCLIPBOARD:               return ( "WM_DESTROYCLIPBOARD" );
    case WM_DRAWCLIPBOARD:                  return ( "WM_DRAWCLIPBOARD" );
    case WM_PAINTCLIPBOARD:                 return ( "WM_PAINTCLIPBOARD" );
    case WM_VSCROLLCLIPBOARD:               return ( "WM_VSCROLLCLIPBOARD" );
    case WM_SIZECLIPBOARD:                  return ( "WM_SIZECLIPBOARD" );
    case WM_ASKCBFORMATNAME:                return ( "WM_ASKCBFORMATNAME" );
    case WM_CHANGECBCHAIN:                  return ( "WM_CHANGECBCHAIN" );
    case WM_HSCROLLCLIPBOARD:               return ( "WM_HSCROLLCLIPBOARD" );
    case WM_QUERYNEWPALETTE:                return ( "WM_QUERYNEWPALETTE" );
    case WM_PALETTEISCHANGING:              return ( "WM_PALETTEISCHANGING" );
    case WM_PALETTECHANGED:                 return ( "WM_PALETTECHANGED" );
    case WM_HOTKEY:                         return ( "WM_HOTKEY" );
    case WM_PRINT:                          return ( "WM_PRINT" );
    case WM_PRINTCLIENT:                    return ( "WM_PRINTCLIENT" );
    case WM_THEMECHANGED:                   return ( "WM_THEMECHANGED" );
    case WM_CLIPBOARDUPDATE:                return ( "WM_CLIPBOARDUPDATE" );
    case WM_DWMCOMPOSITIONCHANGED:          return ( "WM_DWMCOMPOSITIONCHANGED" );
    case WM_DWMNCRENDERINGCHANGED:          return ( "WM_DWMNCRENDERINGCHANGED" );
    case WM_DWMCOLORIZATIONCOLORCHANGED:    return ( "WM_DWMCOLORIZATIONCOLORCHANGED" );
    case WM_DWMWINDOWMAXIMIZEDCHANGE:       return ( "WM_DWMWINDOWMAXIMIZEDCHANGE" );
    case WM_DWMSENDICONICTHUMBNAIL:         return ( "WM_DWMSENDICONICTHUMBNAIL" );
    case WM_DWMSENDICONICLIVEPREVIEWBITMAP: return ( "WM_DWMSENDICONICLIVEPREVIEWBITMAP" );
    case WM_GETTITLEBARINFOEX:              return ( "WM_GETTITLEBARINFOEX" );
    case WM_HANDHELDFIRST:                  return ( "WM_HANDHELDFIRST" );
    case WM_HANDHELDLAST:                   return ( "WM_HANDHELDLAST" );
    case WM_AFXFIRST:                       return ( "WM_AFXFIRST" );
    case WM_AFXLAST:                        return ( "WM_AFXLAST" );
    case WM_PENWINFIRST:                    return ( "WM_PENWINFIRST" );
    case WM_PENWINLAST:                     return ( "WM_PENWINLAST" );
    case WM_APP:                            return ( "WM_APP" );
    case WM_USER:                           return ( "WM_USER" );
    case WM_IME_SETCONTEXT:                 return ( "WM_IME_SETCONTEXT" );
    case WM_IME_NOTIFY:                     return ( "WM_IME_NOTIFY" );
    case WM_IME_CONTROL:                    return ( "WM_IME_CONTROL" );
    case WM_IME_COMPOSITIONFULL:            return ( "WM_IME_COMPOSITIONFULL" );
    case WM_IME_SELECT:                     return ( "WM_IME_SELECT" );
    case WM_IME_CHAR:                       return ( "WM_IME_CHAR" );
    case WM_IME_REQUEST:                    return ( "WM_IME_REQUEST" );
    case WM_IME_KEYDOWN:                    return ( "WM_IME_KEYDOWN" );
    case WM_IME_KEYUP:                      return ( "WM_IME_KEYUP" );
    case WM_IME_STARTCOMPOSITION:           return ( "WM_IME_STARTCOMPOSITION" );
    case WM_IME_ENDCOMPOSITION:             return ( "WM_IME_ENDCOMPOSITION" );
    case WM_IME_COMPOSITION:                return ( "WM_IME_COMPOSITION" );

        default:
        {
            // ha ha! thread safety!
            static char buf[64];
            sprintf_s( buf, ( "UNKNOWN: 0x%04X" ), wmcode );
            return buf;
        }
    }
    __assume( false );
}

} // namespace win32

#endif // OURO_PLATFORM_WIN