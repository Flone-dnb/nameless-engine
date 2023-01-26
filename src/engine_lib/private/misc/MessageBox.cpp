#include "misc/MessageBox.h"

// External.
#include "portable-file-dialogs.h"
#if defined(WIN32)
#pragma push_macro("MessageBox")
#undef MessageBox
#pragma push_macro("IGNORE")
#undef IGNORE
#endif

namespace ne {
    pfd::choice convertChoice(MessageBoxChoice buttons) {
        switch (buttons) {
        case (MessageBoxChoice::OK):
            return pfd::choice::ok;
        case (MessageBoxChoice::YES_NO):
            return pfd::choice::yes_no;
        case (MessageBoxChoice::YES_NO_CANCEL):
            return pfd::choice::yes_no_cancel;
        case (MessageBoxChoice::RETRY_CANCEL):
            return pfd::choice::retry_cancel;
        case (MessageBoxChoice::OK_CANCEL):
            return pfd::choice::ok_cancel;
        case (MessageBoxChoice::ABORT_RETRY_IGNORE):
            return pfd::choice::abort_retry_ignore;
        }

        throw std::runtime_error("unhandled case");
    }

    MessageBoxResult convertResult(pfd::button result) {
        switch (result) {
        case (pfd::button::ok):
            return MessageBoxResult::OK;
        case (pfd::button::no):
            return MessageBoxResult::NO;
        case (pfd::button::yes):
            return MessageBoxResult::YES;
        case (pfd::button::cancel):
            return MessageBoxResult::CANCEL;
        case (pfd::button::abort):
            return MessageBoxResult::ABORT;
        case (pfd::button::retry):
            return MessageBoxResult::RETRY;
        case (pfd::button::ignore):
            return MessageBoxResult::IGNORE;
        }

        throw std::runtime_error("unhandled case");
    }

    MessageBoxResult
    MessageBox::info(const std::string& sTitle, const std::string& sText, MessageBoxChoice buttons) {
        return convertResult(pfd::message(sTitle, sText, convertChoice(buttons), pfd::icon::info).result());
    }

    MessageBoxResult
    MessageBox::question(const std::string& sTitle, const std::string& sText, MessageBoxChoice buttons) {
        return convertResult(
            pfd::message(sTitle, sText, convertChoice(buttons), pfd::icon::question).result());
    }

    MessageBoxResult
    MessageBox::warning(const std::string& sTitle, const std::string& sText, MessageBoxChoice buttons) {
        return convertResult(
            pfd::message(sTitle, sText, convertChoice(buttons), pfd::icon::warning).result());
    }

    MessageBoxResult
    MessageBox::error(const std::string& sTitle, const std::string& sText, MessageBoxChoice buttons) {
        return convertResult(pfd::message( // NOLINT: potential memory leak in `pfd`
                                 sTitle,
                                 sText,
                                 convertChoice(buttons),
                                 pfd::icon::error)
                                 .result());
    }
} // namespace ne

#if defined(WIN32)
#pragma pop_macro("IGNORE")
#pragma pop_macro("MessageBox")
#endif
