#pragma once

// STL.
#include <string>

#if defined(WIN32)
#pragma push_macro("MessageBox")
#undef MessageBox
#pragma push_macro("IGNORE")
#undef IGNORE
#endif

namespace ne {
    /** Enum that represents a clicked button on message box. */
    enum class MessageBoxResult { OK, CANCEL, YES, NO, ABORT, RETRY, IGNORE };

    /** Enum that represents buttons that will be displayed on a message box. */
    enum class MessageBoxChoice { OK, OK_CANCEL, YES_NO, YES_NO_CANCEL, RETRY_CANCEL, ABORT_RETRY_IGNORE };

    /** Message box notification. */
    class MessageBox {
    public:
        /**
         * Show an information message box.
         *
         * @param sTitle  Title of the message box.
         * @param sText   Text (content) of the message box.
         * @param buttons Available buttons for this message box.
         *
         * @return Button that the user pressed.
         *
         * @note This function blocks the current thread until a button is pressed.
         */
        static MessageBoxResult info(
            const std::string& sTitle,
            const std::string& sText,
            MessageBoxChoice buttons = MessageBoxChoice::OK);

        /**
         * Show a question message box.
         *
         * @param sTitle  Title of the message box.
         * @param sText   Text (content) of the message box.
         * @param buttons Available buttons for this message box.
         *
         * @return Button that the user pressed.
         *
         * @note This function blocks the current thread until a button is pressed.
         */
        static MessageBoxResult question(
            const std::string& sTitle,
            const std::string& sText,
            MessageBoxChoice buttons = MessageBoxChoice::OK);

        /**
         * Show a warning message box.
         *
         * @param sTitle  Title of the message box.
         * @param sText   Text (content) of the message box.
         * @param buttons Available buttons for this message box.
         *
         * @return Button that the user pressed.
         *
         * @note This function blocks the current thread until a button is pressed.
         */
        static MessageBoxResult warning(
            const std::string& sTitle,
            const std::string& sText,
            MessageBoxChoice buttons = MessageBoxChoice::OK);

        /**
         * Show an error message box.
         *
         * @param sTitle  Title of the message box.
         * @param sText   Text (content) of the message box.
         * @param buttons Available buttons for this message box.
         *
         * @return Button that the user pressed.
         *
         * @note This function blocks the current thread until a button is pressed.
         */
        static MessageBoxResult error(
            const std::string& sTitle,
            const std::string& sText,
            MessageBoxChoice buttons = MessageBoxChoice::OK);
    };
} // namespace ne

#if defined(WIN32)
#pragma pop_macro("IGNORE")
#pragma pop_macro("MessageBox")
#endif