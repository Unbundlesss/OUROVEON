
#include <windows.h>
#include <shellapi.h>

int xpOpenURL( const char* url )
{
    HINSTANCE rc;

    /* https://msdn.microsoft.com/en-us/library/windows/desktop/bb762153%28v=vs.85%29.aspx */
    rc = ShellExecuteA( NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);

    return (rc > ((HINSTANCE)32)) ? 0 : -1;
}
