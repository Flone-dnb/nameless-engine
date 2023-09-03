#include "misc/OpenLinkInBrowser.h"

#if defined(WIN32)
#include <Windows.h>
#undef MessageBox
#undef IGNORE
#elif __linux__
#include <stdio.h>
#endif

namespace ne {
    void OpenLinkInBrowser::open(const std::string& sLink) {
#if defined(WIN32)
        ShellExecuteA(nullptr, "open", sLink.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __linux__
        system((std::string("xdg-open ") + sLink).c_str()); // NOLINT: not thread safe
#endif
    }
} // namespace ne
