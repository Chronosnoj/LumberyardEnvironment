////////////////////////////////////////////////////////////////
// This file was automatically created by WAF
// WARNING! All modifications will be lost!
////////////////////////////////////////////////////////////////

void SetupAndroidDescriptor(const char* gameName, AZ::ComponentApplication::Descriptor &desc)
{
    if(stricmp(gameName, "BeachCity") == 0)
    {
        desc.m_useExistingAllocator = false;
        desc.m_grabAllMemory = false;
        desc.m_allocationRecords = true;
        desc.m_autoIntegrityCheck = false;
        desc.m_markUnallocatedMemory = true;
        desc.m_doNotUsePools = false;
        desc.m_pageSize = 65536;
        desc.m_poolPageSize = 4096;
        desc.m_memoryBlockAlignment = 65536;
        desc.m_memoryBlocksByteSize = 0;
        desc.m_reservedOS = 0;
        desc.m_reservedDebug = 0;
        desc.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_STACK_IF_NO_FILE_LINE;
        desc.m_stackRecordLevels = 5;
        desc.m_enableDrilling = true;
        desc.m_x360IsPhysicalMemory = false;
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "LmbrCentral";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Rain.e5f049ad7f534847a89c27b7339cf6a6.v0.1.0";
    }
    if(stricmp(gameName, "GameSDK") == 0)
    {
        desc.m_useExistingAllocator = false;
        desc.m_grabAllMemory = false;
        desc.m_allocationRecords = true;
        desc.m_autoIntegrityCheck = false;
        desc.m_markUnallocatedMemory = true;
        desc.m_doNotUsePools = false;
        desc.m_pageSize = 65536;
        desc.m_poolPageSize = 4096;
        desc.m_memoryBlockAlignment = 65536;
        desc.m_memoryBlocksByteSize = 0;
        desc.m_reservedOS = 0;
        desc.m_reservedDebug = 0;
        desc.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_STACK_IF_NO_FILE_LINE;
        desc.m_stackRecordLevels = 5;
        desc.m_enableDrilling = true;
        desc.m_x360IsPhysicalMemory = false;
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "LmbrCentral";
    }
    if(stricmp(gameName, "SamplesProject") == 0)
    {
        desc.m_useExistingAllocator = false;
        desc.m_grabAllMemory = false;
        desc.m_allocationRecords = true;
        desc.m_autoIntegrityCheck = false;
        desc.m_markUnallocatedMemory = true;
        desc.m_doNotUsePools = false;
        desc.m_pageSize = 65536;
        desc.m_poolPageSize = 4096;
        desc.m_memoryBlockAlignment = 65536;
        desc.m_memoryBlocksByteSize = 0;
        desc.m_reservedOS = 0;
        desc.m_reservedDebug = 0;
        desc.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_STACK_IF_NO_FILE_LINE;
        desc.m_stackRecordLevels = 5;
        desc.m_enableDrilling = true;
        desc.m_x360IsPhysicalMemory = false;
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "LmbrCentral";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.AWS.0945e21b7ae848ac80b4ec1f34c459cc.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.StartingPointInput.09f4bedeee614358bc36788e77f97e51.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.HMDFramework.24a3427048184feba39ba2cf75d45c4c.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.OpenVR.3b1c41404cb147fe87e25bf72e035faa.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.StaticData.4a4bf593603c4f329c76c2a10779311b.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.LightningArc.4c28210b23544635aa15be668dbff15d.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.CameraFramework.54f2763fe191432fa681ce4a354eedf5.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.InputManagementFramework.59b1b2acc1974aae9f18faddcaddac5b.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Gestures.6056556b6088413984309c4a413593ad.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.CertificateManager.659cffff33b14a10835bafc6ea623f98.v0.0.1";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.StartingPointMovement.73d8779dc28a4123b7c9ed76217464af.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.GameLift.76de765796504906b73be7365a9bff06.v2.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.StartingPointCamera.834070b9537d44df83559e2045c3859f.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Snow.8827e54450f040dbaed1776ba4b35b43.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Boids.97e596870cf645e184e43d5a5ccda857.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Substance.a2f08ba9713f485a8485d7588e5b120f.v2.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Metastream.c02d7efe05134983b5699d9ee7594c3a.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Oculus.c32178b3c4e94fbead069bd92ff9b04a.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.UserLoginDefault.c9c25313197f489a8e9e8e6037fc62eb.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.GameEffectSystem.d378b5a7b47747d0a7aa741945df58f3.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Multiplayer.d3ed407a19bb4c0d92b7c4872313d600.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Tornadoes.d48ca459d0644521aad37f08466ef83a.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Rain.e5f049ad7f534847a89c27b7339cf6a6.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Camera.f910686b6725452fbfc4671f95f733c6.v0.1.0";
    }
    if(stricmp(gameName, "MultiplayerProject") == 0)
    {
        desc.m_useExistingAllocator = false;
        desc.m_grabAllMemory = false;
        desc.m_allocationRecords = true;
        desc.m_autoIntegrityCheck = false;
        desc.m_markUnallocatedMemory = true;
        desc.m_doNotUsePools = false;
        desc.m_pageSize = 65536;
        desc.m_poolPageSize = 4096;
        desc.m_memoryBlockAlignment = 65536;
        desc.m_memoryBlocksByteSize = 0;
        desc.m_reservedOS = 0;
        desc.m_reservedDebug = 0;
        desc.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_STACK_IF_NO_FILE_LINE;
        desc.m_stackRecordLevels = 5;
        desc.m_enableDrilling = true;
        desc.m_x360IsPhysicalMemory = false;
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "LmbrCentral";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.CameraFramework.54f2763fe191432fa681ce4a354eedf5.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Gestures.6056556b6088413984309c4a413593ad.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.CertificateManager.659cffff33b14a10835bafc6ea623f98.v0.0.1";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.GameLift.76de765796504906b73be7365a9bff06.v2.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.UserLoginDefault.c9c25313197f489a8e9e8e6037fc62eb.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Multiplayer.d3ed407a19bb4c0d92b7c4872313d600.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Camera.f910686b6725452fbfc4671f95f733c6.v0.1.0";
    }
    if(stricmp(gameName, "FeatureTests") == 0)
    {
        desc.m_useExistingAllocator = false;
        desc.m_grabAllMemory = false;
        desc.m_allocationRecords = true;
        desc.m_autoIntegrityCheck = false;
        desc.m_markUnallocatedMemory = true;
        desc.m_doNotUsePools = false;
        desc.m_pageSize = 65536;
        desc.m_poolPageSize = 4096;
        desc.m_memoryBlockAlignment = 65536;
        desc.m_memoryBlocksByteSize = 0;
        desc.m_reservedOS = 0;
        desc.m_reservedDebug = 0;
        desc.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_STACK_IF_NO_FILE_LINE;
        desc.m_stackRecordLevels = 5;
        desc.m_enableDrilling = true;
        desc.m_x360IsPhysicalMemory = false;
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "LmbrCentral";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.CameraFramework.54f2763fe191432fa681ce4a354eedf5.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Gestures.6056556b6088413984309c4a413593ad.v1.0.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.ProcessLifeManagement.698948526ada4d13bada933e2d7ee463.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Snow.8827e54450f040dbaed1776ba4b35b43.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Rain.e5f049ad7f534847a89c27b7339cf6a6.v0.1.0";
        desc.m_modules.push_back();
        desc.m_modules.back().m_dynamicLibraryPath = "Gem.Camera.f910686b6725452fbfc4671f95f733c6.v0.1.0";
    }
}
