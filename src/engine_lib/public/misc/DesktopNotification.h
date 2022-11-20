#pragma once

// Standard.
#include <string>

namespace ne {
    /** Show desktop notifications (popups). */
    class DesktopNotification {
    public:
        /**
         * Show information notification.
         *
         * @param sTitle  Notification title.
         * @param sText   Notification text (content).
         */
        static void info(const std::string& sTitle, const std::string& sText);

        /**
         * Show warning notification.
         *
         * @param sTitle  Notification title.
         * @param sText   Notification text (content).
         */
        static void warning(const std::string& sTitle, const std::string& sText);

        /**
         * Show error notification.
         *
         * @param sTitle  Notification title.
         * @param sText   Notification text (content).
         */
        static void error(const std::string& sTitle, const std::string& sText);
    };
} // namespace ne
