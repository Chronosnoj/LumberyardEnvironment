
#include "StdAfx.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include "TestGem_SourbeerSystemComponent.h"

namespace TestGem_Sourbeer
{
    void TestGem_SourbeerSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<TestGem_SourbeerSystemComponent, AZ::Component>()
                ->Version(0)
                ->SerializerForEmptyClass();

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<TestGem_SourbeerSystemComponent>("TestGem_Sourbeer", "[Description of functionality provided by this System Component]")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        // ->Attribute(AZ::Edit::Attributes::Category, "") Set a category
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void TestGem_SourbeerSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("TestGem_SourbeerService"));
    }

    void TestGem_SourbeerSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("TestGem_SourbeerService"));
    }

    void TestGem_SourbeerSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        (void)required;
    }

    void TestGem_SourbeerSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        (void)dependent;
    }

    void TestGem_SourbeerSystemComponent::Init()
    {
    }

    void TestGem_SourbeerSystemComponent::Activate()
    {
        TestGem_SourbeerRequestBus::Handler::BusConnect();
    }

    void TestGem_SourbeerSystemComponent::Deactivate()
    {
        TestGem_SourbeerRequestBus::Handler::BusDisconnect();
    }
}
