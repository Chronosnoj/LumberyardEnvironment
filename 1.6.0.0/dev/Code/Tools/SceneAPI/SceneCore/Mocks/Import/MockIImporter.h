#pragma once

/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/


#include <AzTest/AzTest.h>
#include <SceneAPI/SceneCore/Import/IImporter.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>

namespace AZ {
    namespace SceneAPI {
        namespace Import {
            class MockIImporter
                : public IImporter
            {
            public:
                MOCK_CONST_METHOD2(PopulateFromFile,
                    bool(const char* path, AZ::SceneAPI::Containers::Scene & scene));
                MOCK_CONST_METHOD2(PopulateFromFile,
                    bool(const std::string & path, AZ::SceneAPI::Containers::Scene & scene));
            };
        } // namespace Import
    } // namespace SceneAPI
}  // namespace AZ
