#include "misc/Error.h"

// Standard.
#include <string>
#include <filesystem>

// Custom.
#include "io/Logger.h"
#include "misc/MessageBox.h"

namespace ne {
    Error::Error(std::string_view sMessage, const nostd::source_location location) {
        this->sMessage = sMessage;

        stack.push_back(sourceLocationToInfo(location));
    }

#if defined(WIN32)
    Error::Error(const HRESULT hResult, const nostd::source_location location) {
        LPSTR pErrorText = nullptr;

        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            hResult,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            reinterpret_cast<LPSTR>(&pErrorText),
            0,
            nullptr);

        // Add error code to the beginning of the message.
        std::stringstream hexStream;
        hexStream << std::hex << hResult;
        sMessage = std::format("0x{}: ", hexStream.str());

        if (pErrorText != nullptr) {
            sMessage += std::string_view(pErrorText);
            LocalFree(pErrorText);
        } else {
            sMessage += "unknown error";
        }

        stack.push_back(sourceLocationToInfo(location));
    }

    Error::Error(unsigned long iErrorCode, const nostd::source_location location) {
        LPSTR pMessageBuffer = nullptr;

        // Ask Win32 to give us the string version of that message ID.
        // The parameters we pass in, tell Win32 to create the buffer that holds the message for us
        // (because we don't yet know how long the message string will be).
        const size_t iSize = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            iErrorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&pMessageBuffer,
            0,
            nullptr);

        // Copy the error message.
        this->sMessage = std::string("error code: ");
        this->sMessage += std::to_string(iErrorCode);
        this->sMessage += ", description: ";
        if (pMessageBuffer != nullptr) {
            this->sMessage += std::string(pMessageBuffer, iSize);
        } else {
            this->sMessage += "unknown error";
        }

        if (pMessageBuffer != nullptr) {
            LocalFree(pMessageBuffer);
        }

        stack.push_back(sourceLocationToInfo(location));
    }
#endif

    void Error::addCurrentLocationToErrorStack(const nostd::source_location location) {
        stack.push_back(sourceLocationToInfo(location));
    }

    std::string Error::getFullErrorMessage() const {
        std::string sErrorMessage = "An error occurred: ";
        sErrorMessage += sMessage;
        sErrorMessage += "\nError stack:\n";

        for (const auto& entry : stack) {
            sErrorMessage += "- at ";
            sErrorMessage += entry.sFilename;
            sErrorMessage += ", ";
            sErrorMessage += entry.sLine;
            sErrorMessage += "\n";
        }

        return sErrorMessage;
    }

    std::string Error::getInitialMessage() const { return sMessage; }

    void Error::showError() const {
        const std::string sErrorMessage = getFullErrorMessage();
        Logger::get().error(sErrorMessage);

#if defined(WIN32)
#pragma push_macro("MessageBox")
#undef MessageBox
#endif
        MessageBox::error("Error", sErrorMessage);
#if defined(WIN32)
#pragma pop_macro("MessageBox")
#endif
    }

    SourceLocationInfo Error::sourceLocationToInfo(const nostd::source_location& location) {
        SourceLocationInfo info;
        info.sFilename = std::filesystem::path(location.file_name()).filename().string();
        info.sLine = std::to_string(location.line());

        return info;
    }
} // namespace ne
