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
#pragma once

#ifndef GEM_MULTIPLAYER_MULTIPLAYERUTILS_H
#define GEM_MULTIPLAYER_MULTIPLAYERUTILS_H

#include <GridMate/GridMate.h>
#include <GridMate/Session/Session.h>
#include <GridMate/Session/LANSession.h>

#include <GridMate/Carrier/Driver.h>

#ifdef NET_SUPPORT_SECURE_SOCKET_DRIVER
#   include <GridMate/Carrier/SecureSocketDriver.h>
#endif

#if defined(DURANGO)
#include <GridMate/Session/XBone/XBoneSessionService.h>
#elif defined(ORBIS)
#include <GridMate/Session/PS4/PSNSessionService.h>
#endif

#include <Multiplayer/IMultiplayerGem.h>
#include <CertificateManager/ICertificateManagerGem.h>

namespace Multiplayer
{
    struct Utils
    {
        static GridMate::Driver::BSDSocketFamilyType CVarToFamilyType(const char* str)
        {
            AZ_Assert(str, "Invalid value");

            if (!stricmp(str, "IPv4"))
            {
                return GridMate::Driver::BSD_AF_INET;
            }
            else if (!stricmp(str, "IPv6"))
            {
                return GridMate::Driver::BSD_AF_INET6;
            }
            else
            {
                AZ_Warning("GridMate", false, "Invalid value '%s' for ip version", str);
                return GridMate::Driver::BSD_AF_INET;
            }
        }

        static void InitCarrierDesc(GridMate::CarrierDesc& carrierDesc)
        {
            if (!carrierDesc.m_simulator)
            {
                EBUS_EVENT_RESULT(carrierDesc.m_simulator,Multiplayer::MultiplayerRequestBus,GetSimulator);                
            }

            carrierDesc.m_port = gEnv->pConsole->GetCVar("cl_clientport")->GetIVal();
            carrierDesc.m_connectionTimeoutMS = 10000;
            carrierDesc.m_threadUpdateTimeMS = 30;
            carrierDesc.m_securityData = gEnv->pConsole->GetCVar("gm_securityData")->GetString();
            carrierDesc.m_familyType = CVarToFamilyType(gEnv->pConsole->GetCVar("gm_ipversion")->GetString());
            carrierDesc.m_version = gEnv->pConsole->GetCVar("gm_version")->GetIVal();
            carrierDesc.m_driverIsCrossPlatform = true;

            ApplyDisconnectDetectionSettings(carrierDesc);
        }

        static void ApplyDisconnectDetectionSettings(GridMate::CarrierDesc& carrierDesc)
        {
            carrierDesc.m_enableDisconnectDetection = !!gEnv->pConsole->GetCVar("gm_disconnectDetection")->GetIVal();

            if (gEnv->pConsole->GetCVar("gm_disconnectDetectionRttThreshold"))
            {
                carrierDesc.m_disconnectDetectionRttThreshold = gEnv->pConsole->GetCVar("gm_disconnectDetectionRttThreshold")->GetFVal();
            }

            if (gEnv->pConsole->GetCVar("gm_disconnectDetectionPacketLossThreshold"))
            {
                carrierDesc.m_disconnectDetectionPacketLossThreshold = gEnv->pConsole->GetCVar("gm_disconnectDetectionPacketLossThreshold")->GetFVal();
            }
        }
    };

    struct Durango
    {
        static void StartSessionService(GridMate::IGridMate* gridMate)
        {
#if defined(DURANGO)
            if (!GridMate::HasGridMateService<GridMate::XBoneSessionService>(gridMate))
            {
                GridMate::StartGridMateService<GridMate::XBoneSessionService>(gridMate, GridMate::XBoneSessionServiceDesc());
            }
#endif
        }

        static void StopSessionService(GridMate::IGridMate* gridMate)
        {
#if defined(DURANGO)
            GridMate::StopGridMateService<GridMate::XBoneSessionService>(gridMate);
#endif
        }
    };

    struct Orbis
    {
        static void StartSessionService(GridMate::IGridMate* gridMate)
        {
#if defined(ORBIS)
            if (!GridMate::HasGridMateService<GridMate::PSNSessionService>(gridMate))
            {
                GridMate::PSNSessionServiceDesc desc;
                desc.m_threadStackSize = 256 * 1024;
                GridMate::StartGridMateService<GridMate::PSNSessionService>(gridMate, desc);
            }
#endif
        }

        static void StopSessionService(GridMate::IGridMate* gridMate)
        {
#if defined(ORBIS)
            GridMate::StopGridMateService<GridMate::PSNSessionService>(gridMate);
#endif
        }
    };

    struct LAN
    {
        static void StartSessionService(GridMate::IGridMate* gridMate)
        {
            if (!GridMate::HasGridMateService<GridMate::LANSessionService>(gridMate))
            {
                GridMate::StartGridMateService<GridMate::LANSessionService>(gridMate, GridMate::SessionServiceDesc());
            }
        }

        static void StopSessionService(GridMate::IGridMate* gridMate)
        {
            GridMate::StopGridMateService<GridMate::LANSessionService>(gridMate);
        }
    };
    
    struct NetSec
    {
        static void ConfigureCarrierDescForHost(GridMate::CarrierDesc& carrierDesc)
        {
            bool netSecEnabled = false;
            EBUS_EVENT_RESULT(netSecEnabled, Multiplayer::MultiplayerRequestBus,IsNetSecEnabled);

            if (netSecEnabled)
            {
#if defined(NET_SUPPORT_SECURE_SOCKET_DRIVER)  
                bool hasPublicKey = false;
                bool hasPrivateKey = false;

                EBUS_EVENT_RESULT(hasPublicKey,CertificateManager::CertificateManagerRequestsBus,HasPublicKey);
                EBUS_EVENT_RESULT(hasPrivateKey,CertificateManager::CertificateManagerRequestsBus,HasPrivateKey);

                if (hasPublicKey && hasPrivateKey)
                {
                    GridMate::SecureSocketDesc desc;

                    EBUS_EVENT_RESULT(desc.m_privateKeyPEM,CertificateManager::CertificateManagerRequestsBus,RetrievePrivateKey);
                    EBUS_EVENT_RESULT(desc.m_certificatePEM,CertificateManager::CertificateManagerRequestsBus,RetrievePublicKey);
                    EBUS_EVENT_RESULT(desc.m_certificateAuthorityPEM,CertificateManager::CertificateManagerRequestsBus,RetrieveCertificateAuthority);

                    bool verifyClient = false;
                    EBUS_EVENT_RESULT(verifyClient,Multiplayer::MultiplayerRequestBus,IsNetSecVerifyClient);
                    desc.m_authenticateClient = verifyClient;

                    GridMate::SecureSocketDriver* secureDriver = aznew GridMate::SecureSocketDriver(desc);
                    EBUS_EVENT(Multiplayer::MultiplayerRequestBus,RegisterSecureDriver,secureDriver);

                    carrierDesc.m_driver = secureDriver;
                }
                else
                {
                    CryLog("Unable to use a secure connection because of missing certificate or private key.");
                }
#endif
            }
        }

        static void ConfigureCarrierDescForJoin(GridMate::CarrierDesc& carrierDesc)
        {
            bool netSecEnabled = false;
            EBUS_EVENT_RESULT(netSecEnabled, Multiplayer::MultiplayerRequestBus,IsNetSecEnabled);
            if (netSecEnabled)
            {
#if defined(NET_SUPPORT_SECURE_SOCKET_DRIVER)  
                GridMate::SecureSocketDesc desc;                

                bool hasCertificateAuthority = false;
                EBUS_EVENT_RESULT(hasCertificateAuthority,CertificateManager::CertificateManagerRequestsBus,HasCertificateAuthority);

                if (hasCertificateAuthority)
                {
                    EBUS_EVENT_RESULT(desc.m_privateKeyPEM,CertificateManager::CertificateManagerRequestsBus,RetrievePrivateKey);
                    EBUS_EVENT_RESULT(desc.m_certificatePEM,CertificateManager::CertificateManagerRequestsBus,RetrievePublicKey);
                    EBUS_EVENT_RESULT(desc.m_certificateAuthorityPEM ,CertificateManager::CertificateManagerRequestsBus,RetrieveCertificateAuthority);

                    GridMate::SecureSocketDriver* secureDriver = aznew GridMate::SecureSocketDriver(desc);
                    EBUS_EVENT(Multiplayer::MultiplayerRequestBus,RegisterSecureDriver,secureDriver);

                    carrierDesc.m_driver = secureDriver;
                }
                else
                {
                    CryLog("Unable to use a secure connection because of missing certificate or private key.");
                }
#endif
            }
        }
    };    
}

#endif