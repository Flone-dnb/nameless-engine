﻿#include "misc/OpenLinkInBrowser.h"

#if defined(WIN32)
#include <Windows.h>
#undef MessageBox
#undef IGNORE
#endif

namespace ne {
    void OpenLinkInBrowser::open(const std::string& sLink) {
#if defined(WIN32)
        ShellExecuteA(nullptr, "open", sLink.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __linux__
        system((std::string("xdg-open ") + sLink).c_str());
#endif
    }
} // namespace ne