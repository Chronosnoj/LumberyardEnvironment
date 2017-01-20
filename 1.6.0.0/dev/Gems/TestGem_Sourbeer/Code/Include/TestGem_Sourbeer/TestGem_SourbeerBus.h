
#pragma once

#include <AzCore/EBus/EBus.h>

namespace TestGem_Sourbeer
{
    class TestGem_SourbeerRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        // Put your public methods here
    };
    using TestGem_SourbeerRequestBus = AZ::EBus<TestGem_SourbeerRequests>;
} // namespace TestGem_Sourbeer
