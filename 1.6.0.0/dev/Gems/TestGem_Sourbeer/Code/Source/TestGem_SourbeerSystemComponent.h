
#pragma once

#include <AzCore/Component/Component.h>

#include <TestGem_Sourbeer/TestGem_SourbeerBus.h>

namespace TestGem_Sourbeer
{
    class TestGem_SourbeerSystemComponent
        : public AZ::Component
        , protected TestGem_SourbeerRequestBus::Handler
    {
    public:
        AZ_COMPONENT(TestGem_SourbeerSystemComponent, "{70389091-6105-485D-B0AC-D5ABA239381F}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // TestGem_SourbeerRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////
    };
}
