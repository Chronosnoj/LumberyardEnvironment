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

#include <InAppPurchases/InAppPurchasesInterface.h>

#include <jni.h>

namespace InAppPurchases
{
    class InAppPurchasesAndroid
        : public InAppPurchasesInterface
    {
    public:
        void Initialize() override;
        
        ~InAppPurchasesAndroid();

        void QueryProductInfo(AZStd::vector<AZStd::string>& productIds) const override;
        void QueryProductInfo() const override;

        void PurchaseProduct(const AZStd::string& productId, const AZStd::string& developerPayload) const override;
        void PurchaseProduct(const AZStd::string& productId) const override;

        void QueryPurchasedProducts() const override;
        
        void ConsumePurchase(const AZStd::string& purchaseToken) const override;
        
        void FinishTransaction(const AZStd::string& transactionId, bool downloadHostedContent) const override;

        InAppPurchasesCache* GetCache() override;

    private:
        jobject m_billingInstance;
    };
    


    class ProductDetailsAndroid
        : public ProductDetails
    {
    public:
        AZ_RTTI(ProductDetailsAndroid, "{59A14DA4-B224-4BBD-B43E-8C7BC2EEFEB5}", ProductDetails);

        const AZStd::string& GetProductType() const { return m_productType; }
        
        void SetProductType(const AZStd::string& productType) { m_productType = productType; }
        
    protected:
        AZStd::string m_productType;
    };
    


    class PurchasedProductDetailsAndroid
        : public PurchasedProductDetails
    {
    public:
        AZ_RTTI(PurchasedProductDetailsAndroid, "{86A7072A-4661-4DAA-A811-F9279B089859}", PurchasedProductDetails);

        const AZStd::string& GetPurchaseSignature() const { return m_purchaseSignature; }
        const AZStd::string& GetPackageName() const { return m_packageName; }
        const AZStd::string& GetPurchaseToken() const { return m_purchaseToken; }
        bool GetIsAutoRenewing() const { return m_autoRenewing; }
        
        void SetPurchaseSignature(const AZStd::string& purchaseSignature) { m_purchaseSignature = purchaseSignature; }
        void SetPackageName(const AZStd::string& packageName) { m_packageName = packageName; }
        void SetPurchaseToken(const AZStd::string& purchaseToken) { m_purchaseToken = purchaseToken; }
        void SetIsAutoRenewing(bool autoRenewing) { m_autoRenewing = autoRenewing; }
        
    protected:
        AZStd::string m_purchaseSignature;
        AZStd::string m_packageName;
        AZStd::string m_purchaseToken;
        bool m_autoRenewing;
    };
}