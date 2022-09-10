﻿#include "io/GuidProperty.h"

// Custom.
#include "misc/Error.h"

// External.
#include "fmt/format.h"

namespace ne {
    Guid::Guid(const char* pGuid) : sGuid(pGuid) {
#if DEBUG
        const std::string sGuidInformation =
            "Here is an example GUID: \"00000000-0000-0000-0000-000000000000\"\n"
            "You can generate a random GUID by just googling \"generate GUID\".";

        // Check GUID length.
        if (sGuid.size() != 36) {
            const Error err(fmt::format(
                "The specified GUID \"{}\" should have a length of 36 characters.\n{}",
                sGuid,
                sGuidInformation));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        // Check GUID format.
        if (sGuid[8] != '-' || sGuid[13] != '-' || sGuid[18] != '-' || sGuid[23] != '-') {
            const Error err(
                fmt::format("The specified GUID \"{}\" has incorrect format.\n{}", sGuid, sGuidInformation));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        // Make sure we don't have dots in the GUID as we use them internally in serialized format.
        if (sGuid.contains('.')) {
            const Error err(fmt::format(
                "The specified GUID \"{}\" is invalid because it has dots in it.\n{}",
                sGuid,
                sGuidInformation));
            err.showError();
            throw std::runtime_error(err.getError());
        }
#endif
    }

    std::string Guid::getGuid() const { return sGuid; }
} // namespace ne
