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

#include <AzCore/EBus/EBus.h>
#include <InAppPurchases/InAppPurchasesInterface.h>

namespace InAppPurchases
{
    class InAppPurchasesResponse
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual void ProductInfoRetrieved(const AZStd::vector<AZStd::unique_ptr<ProductDetails const> >& productDetails) { (void)productDetails; }
        virtual void PurchasedProductsRetrieved(const AZStd::vector<AZStd::unique_ptr<PurchasedProductDetails const> >& purchasedProductDetails) { (void)purchasedProductDetails;  }
        virtual void NewProductPurchased(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void PurchaseCancelled(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void PurchaseRefunded(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void PurchaseFailed(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void HostedContentDownloadComplete(const AZStd::string& transactionId, AZStd::string& downloadedFileLocation) { (void)downloadedFileLocation; (void)transactionId; }
        virtual void HostedContentDownloadFailed(const AZStd::string& transactionId, const AZStd::string& contentId) { (void)transactionId; (void)contentId; }
    };

    using InAppPurchasesResponseBus = AZ::EBus<InAppPurchasesResponse>;
}
