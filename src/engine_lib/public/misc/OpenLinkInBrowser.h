#pragma once

// STL.
#include <string>

namespace ne {
    /** Simple class that provides a static function to open a link in default browser. */
    class OpenLinkInBrowser {
    public:
        /**
         * Opens a web link in user's default browser.
         *
         * @param sLink Web link. Example: "https://github.com/Flone-dnb/nameless-engine".
         */
        static void open(const std::string& sLink);
    };
} // namespace ne
