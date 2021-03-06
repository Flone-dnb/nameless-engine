#include "error.h"

// STL.
#include <string>
#include <filesystem>

// Custom.
#include "io/Logger.h"
#include "misc/MessageBox.h"

namespace ne {
    Error::Error(std::string_view sMessage, const std::source_location location) {
        this->sMessage = sMessage;

        stack.push_back(location);
    }

#if defined(WIN32)
    Error::Error(const HRESULT hResult, const std::source_location location) {
        LPSTR errorText = nullptr;

        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            hResult,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            reinterpret_cast<LPSTR>(&errorText),
            0,
            nullptr);

        // Add error code to the beginning of the message.
        std::stringstream hexStream;
        hexStream << std::hex << hResult;
        sMessage = std::format("0x{}: ", hexStream.str());

        if (errorText) {
            sMessage += std::string_view(errorText);
            LocalFree(errorText);
        } else {
            sMessage += "unknown error";
        }

        stack.push_back(location);
    }

    Error::Error(unsigned long errorCode, const std::source_location location) {
        LPSTR messageBuffer = nullptr;

        // Ask Win32 to give us the string version of that message ID.
        // The parameters we pass in, tell Win32 to create the buffer that holds the message for us
        // (because we don't yet know how long the message string will be).
        const size_t iSize = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer,
            0,
            nullptr);

        // Copy the error message.
        this->sMessage = std::string("error code: ");
        this->sMessage += std::to_string(errorCode);
        this->sMessage += ", description: ";
        if (messageBuffer) {
            this->sMessage += std::string(messageBuffer, iSize);
        } else {
            this->sMessage += "unknown error";
        }

        if (messageBuffer) {
            LocalFree(messageBuffer);
        }

        stack.push_back(location);
    }
#endif

    void Error::addEntry(const std::source_location location) { stack.push_back(location); }

    std::string Error::getError() const {
        std::string sErrorMessage = "An error occurred: ";
        sErrorMessage += sMessage;
        sErrorMessage += "\nError stack:\n";

        for (const auto entry : stack) {
            sErrorMessage += "- at ";
            sErrorMessage += std::filesystem::path(entry.file_name()).filename().string();
            sErrorMessage += ", ";
            sErrorMessage += std::to_string(entry.line());
            sErrorMessage += "\n";
        }

        return sErrorMessage;
    }

    std::string Error::getInitialMessage() const { return sMessage; }

    void Error::showError() const {
        const std::string sErrorMessage = getError();
        Logger::get().error(sErrorMessage, "");

#if defined(WIN32)
#pragma push_macro("MessageBox")
#undef MessageBox
#endif
        MessageBox::error("Error", sErrorMessage);
#if defined(WIN32)
#pragma pop_macro("MessageBox")
#endif
    }
} // namespace ne