
#include "StdAfx.h"
#include <platform_impl.h>

#include "TestGem_SourbeerSystemComponent.h"

#include <IGem.h>

namespace TestGem_Sourbeer
{
    class TestGem_SourbeerModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(TestGem_SourbeerModule, "{0DE8ACD1-C5A6-422A-AB75-391AE58CF762}", CryHooksModule);

        TestGem_SourbeerModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                TestGem_SourbeerSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<TestGem_SourbeerSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(TestGem_Sourbeer_235384c51c1348379d0f720290964e47, TestGem_Sourbeer::TestGem_SourbeerModule)
