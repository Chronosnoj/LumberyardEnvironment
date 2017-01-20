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

#include "InAppPurchasesApple.h"

#include "InAppPurchasesModule.h"

#include <InAppPurchases/InAppPurchasesResponseBus.h>

#include <AzCore/JSON/document.h>

namespace InAppPurchases
{
    void InAppPurchasesApple::Initialize()
    {
        m_delegate = [[InAppPurchasesDelegate alloc] init];
        [m_delegate initialize];
    }

    InAppPurchasesApple::~InAppPurchasesApple()
    {
        [m_delegate deinitialize];
        [m_delegate release];
    }

    void InAppPurchasesApple::QueryProductInfo(AZStd::vector<AZStd::string>& productIds) const
    {        
        NSMutableArray* productIdStrings = [[NSMutableArray alloc] init];
        for (int i = 0; i < productIds.size(); i++)
        {
            NSString* productId = [NSString stringWithCString:productIds[i].c_str() encoding:NSUTF8StringEncoding];
            [productIdStrings addObject:productId];
        }
        
        [m_delegate requestProducts:productIdStrings];
        [productIdStrings release];
    }

    void InAppPurchasesApple::QueryProductInfo() const
    {
        NSURL* url = [[NSBundle mainBundle] URLForResource:@"product_ids" withExtension:@"plist"];
        if (url != nil)
        {
            NSMutableArray* productIds = [NSMutableArray arrayWithContentsOfURL:url];
            if (productIds != nil)
            {
                [m_delegate requestProducts:productIds];
            }
            else
            {
                AZ_TracePrintf("LumberyardInAppPurchases", "Unable to find any product ids in product_ids.plist");
            }
        }
        else
        {
            AZ_TracePrintf("LumberyardInAppPurchases", "product_ids.plist does not exist");
        }
    }

    void InAppPurchasesApple::PurchaseProduct(const AZStd::string& productId, const AZStd::string& developerPayload) const
    {
        NSString* productIdString = [NSString stringWithCString:productId.c_str() encoding:NSUTF8StringEncoding];
        if (!developerPayload.empty())
        {
            NSString* developerPayloadString = [NSString stringWithCString:developerPayload.c_str() encoding:NSUTF8StringEncoding];
            [m_delegate purchaseProduct:productIdString withUserName:developerPayloadString];
        }
        else
        {
            [m_delegate purchaseProduct:productIdString withUserName:nil];
        }
    }

    void InAppPurchasesApple::PurchaseProduct(const AZStd::string& productId) const
    {
        PurchaseProduct(productId, "");
    }

    void InAppPurchasesApple::QueryPurchasedProducts() const
    {     
        [m_delegate queryPurchasedProducts];
    }
    
    void InAppPurchasesApple::ConsumePurchase(const AZStd::string& purchaseToken) const
    {
        
    }
    
    void InAppPurchasesApple::FinishTransaction(const AZStd::string& transactionId, bool downloadHostedContent) const
    {
        NSString* transactionIdString = [NSString stringWithCString:transactionId.c_str() encoding:NSASCIIStringEncoding];
        
        if (downloadHostedContent)
        {
            [m_delegate downloadAppleHostedContentAndFinishTransaction:transactionIdString];
        }
        else
        {
            [m_delegate finishTransaction:transactionIdString];
        }
    }

    InAppPurchasesCache* InAppPurchasesApple::GetCache()
    {
        return &m_cache;
    }
}