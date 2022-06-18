#include "misc/DesktopNotification.h"

// External.
#include "portable-file-dialogs.h"

namespace ne {
    void DesktopNotification::info(const std::string& sTitle, const std::string& sText) {
        pfd::notify(sTitle, sText, pfd::icon::info);
    }

    void DesktopNotification::warning(const std::string& sTitle, const std::string& sText) {
        pfd::notify(sTitle, sText, pfd::icon::warning);
    }

    void DesktopNotification::error(const std::string& sTitle, const std::string& sText) {
        pfd::notify(sTitle, sText, pfd::icon::error);
    }
} // namespace ne
