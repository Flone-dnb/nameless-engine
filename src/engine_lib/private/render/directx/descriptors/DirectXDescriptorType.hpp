#pragma once

namespace ne {
    /** Types of descriptors that point to resources and specify how a resource should be used. */
    enum class DirectXDescriptorType : size_t {
        RTV = 0,
        DSV,
        CBV,
        SRV,
        UAV,

        END // marks the size of this enum
    };
}
