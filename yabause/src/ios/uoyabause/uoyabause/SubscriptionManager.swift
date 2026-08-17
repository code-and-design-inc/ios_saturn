//
//  SubscriptionManager.swift
//  YabaSnashiro 
//
//  Created by Claude on 2025/07/31.
//

import Foundation
import StoreKit
import Combine

@available(iOS 15.0, *)
class SubscriptionManager: ObservableObject {
    static let shared = SubscriptionManager()
    
    @Published var subscriptionStatus: SubscriptionStatus = .notSubscribed
    @Published var availableProducts: [Product] = []
    @Published var purchasedProducts: [Product] = []
    
    private var updateListenerTask: Task<Void, Error>? = nil
    
    // サブスクリプション商品ID
    static let monthlyProductId = "yabasanshiro_monthly_premium"
    static let yearlyProductId = "yabasanshiro_yearly_premium"
    
    enum SubscriptionStatus {
        case notSubscribed
        case subscribed(expirationDate: Date)
        case expired
        case loading
    }
    
    private init() {
        updateListenerTask = listenForTransactions()
        Task {
            await requestProducts()
            await updateCustomerProductStatus()
        }
    }
    
    deinit {
        updateListenerTask?.cancel()
    }
    
    func listenForTransactions() -> Task<Void, Error> {
        return Task.detached {
            for await result in Transaction.updates {
                do {
                    let transaction = try self.checkVerified(result)
                    await self.updateCustomerProductStatus()
                    await transaction.finish()
                } catch {
                    print("Transaction failed verification")
                }
            }
        }
    }
    
    @MainActor
    func requestProducts() async {
        do {
            let storeProducts = try await Product.products(for: [
                Self.monthlyProductId,
                Self.yearlyProductId
            ])
            
            availableProducts = storeProducts
        } catch {
            print("Failed product request from the App Store server: \(error)")
        }
    }
    
    func purchase(_ product: Product) async throws -> Transaction? {
        let result = try await product.purchase()
        
        switch result {
        case .success(let verification):
            let transaction = try checkVerified(verification)
            await updateCustomerProductStatus()
            await transaction.finish()
            return transaction
        case .userCancelled, .pending:
            return nil
        default:
            return nil
        }
    }
    
    func checkVerified<T>(_ result: VerificationResult<T>) throws -> T {
        switch result {
        case .unverified:
            throw StoreError.failedVerification
        case .verified(let safe):
            return safe
        }
    }
    
    @MainActor
    func updateCustomerProductStatus() async {
        subscriptionStatus = .loading
        
        for await result in Transaction.currentEntitlements {
            do {
                let transaction = try checkVerified(result)
                
                if transaction.productID == Self.monthlyProductId || transaction.productID == Self.yearlyProductId {
                    if let expirationDate = transaction.expirationDate {
                        if expirationDate > Date() {
                            subscriptionStatus = .subscribed(expirationDate: expirationDate)
                        } else {
                            subscriptionStatus = .expired
                        }
                    } else {
                        subscriptionStatus = .subscribed(expirationDate: Date.distantFuture)
                    }
                    return
                }
            } catch {
                print("Failed to verify transaction")
            }
        }
        
        subscriptionStatus = .notSubscribed
    }
    
    func restorePurchases() async {
        try? await AppStore.sync()
        await updateCustomerProductStatus()
    }
    
    // 便利メソッド
    var isSubscribed: Bool {
        switch subscriptionStatus {
        case .subscribed:
            return true
        default:
            return false
        }
    }
    
    var hasValidSubscription: Bool {
        switch subscriptionStatus {
        case .subscribed(let expirationDate):
            return expirationDate > Date()
        default:
            return false
        }
    }
}

enum StoreError: Error {
    case failedVerification
}

// iOS 14以下でのサポート
class LegacySubscriptionManager: NSObject, ObservableObject {
    static let shared = LegacySubscriptionManager()
    
    @Published var subscriptionStatus: Bool = false
    @Published var availableProducts: [SKProduct] = []
    
    private var productsRequest: SKProductsRequest?
    
    override init() {
        super.init()
        SKPaymentQueue.default().add(self)
        requestProducts()
        updateSubscriptionStatus()
    }
    
    deinit {
        SKPaymentQueue.default().remove(self)
    }
    
    private func requestProducts() {
        let productIdentifiers: Set<String> = [
            SubscriptionManager.monthlyProductId,
            SubscriptionManager.yearlyProductId
        ]
        
        productsRequest = SKProductsRequest(productIdentifiers: productIdentifiers)
        productsRequest?.delegate = self
        productsRequest?.start()
    }
    
    func purchase(_ product: SKProduct) {
        let payment = SKPayment(product: product)
        SKPaymentQueue.default().add(payment)
    }
    
    func restorePurchases() {
        SKPaymentQueue.default().restoreCompletedTransactions()
    }
    
    private func updateSubscriptionStatus() {
        // アプリの起動時やリストア時に現在のレシートを確認
        guard let receiptURL = Bundle.main.appStoreReceiptURL,
              FileManager.default.fileExists(atPath: receiptURL.path) else {
            subscriptionStatus = false
            return
        }
        
        // 簡易的な実装 - 実際のプロダクションでは適切なレシート検証が必要
        subscriptionStatus = UserDefaults.standard.bool(forKey: "hasActiveSubscription")
    }
    
    var hasValidSubscription: Bool {
        return subscriptionStatus
    }
}

extension LegacySubscriptionManager: SKProductsRequestDelegate {
    func productsRequest(_ request: SKProductsRequest, didReceive response: SKProductsResponse) {
        DispatchQueue.main.async {
            self.availableProducts = response.products
        }
    }
}

extension LegacySubscriptionManager: SKPaymentTransactionObserver {
    func paymentQueue(_ queue: SKPaymentQueue, updatedTransactions transactions: [SKPaymentTransaction]) {
        for transaction in transactions {
            switch transaction.transactionState {
            case .purchased:
                UserDefaults.standard.set(true, forKey: "hasActiveSubscription")
                DispatchQueue.main.async {
                    self.subscriptionStatus = true
                }
                SKPaymentQueue.default().finishTransaction(transaction)
                
            case .restored:
                UserDefaults.standard.set(true, forKey: "hasActiveSubscription")
                DispatchQueue.main.async {
                    self.subscriptionStatus = true
                }
                SKPaymentQueue.default().finishTransaction(transaction)
                
            case .failed:
                if let error = transaction.error as NSError?,
                   error.domain == SKErrorDomain,
                   error.code != SKError.paymentCancelled.rawValue {
                    print("Transaction failed: \(error.localizedDescription)")
                }
                SKPaymentQueue.default().finishTransaction(transaction)
                
            case .deferred, .purchasing:
                break
                
            @unknown default:
                break
            }
        }
    }
}

// 統一されたインターフェース
class UnifiedSubscriptionManager {
    static let shared = UnifiedSubscriptionManager()
    
    private init() {}
    
    var hasValidSubscription: Bool {
        if #available(iOS 15.0, *) {
            return SubscriptionManager.shared.hasValidSubscription
        } else {
            return LegacySubscriptionManager.shared.hasValidSubscription
        }
    }
    
    var isSubscribed: Bool {
        if #available(iOS 15.0, *) {
            return SubscriptionManager.shared.isSubscribed
        } else {
            return LegacySubscriptionManager.shared.subscriptionStatus
        }
    }
    
    func restorePurchases() async {
        if #available(iOS 15.0, *) {
            await SubscriptionManager.shared.restorePurchases()
        } else {
            LegacySubscriptionManager.shared.restorePurchases()
        }
    }
}