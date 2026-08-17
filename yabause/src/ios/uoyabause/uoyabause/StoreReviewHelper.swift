import UIKit
import StoreKit

class StoreReviewHelper {
    static let shared = StoreReviewHelper()
    private init() {}
    
    // アプリの起動回数を保存するキー
    private let launchCountKey = "app_launch_count"
    
    // レビューリクエストを表示した日時を保存するキー
    private let lastReviewRequestDateKey = "last_review_request_date"
    
    func incrementAppLaunchCount() {
        let count = UserDefaults.standard.integer(forKey: launchCountKey)
        UserDefaults.standard.set(count + 1, forKey: launchCountKey)
    }
    
    func requestReviewIfAppropriate() {
        let count = UserDefaults.standard.integer(forKey: launchCountKey)
        let lastRequestDate = UserDefaults.standard.object(forKey: lastReviewRequestDateKey) as? Date
        
        // 以下の条件でレビューを表示:
        // 1. アプリの起動回数が3回以上
        // 2. 前回のレビュー要求から7日以上経過している（または初回）
        if count >= 3 {
            let daysSinceLastRequest = lastRequestDate.map {
                Calendar.current.dateComponents([.day], from: $0, to: Date()).day ?? 0
            } ?? Int.max
            
            if daysSinceLastRequest >= 7 {
                if #available(iOS 14.0, *) {
                    if let scene = UIApplication.shared.connectedScenes.first(where: { $0.activationState == .foregroundActive }) as? UIWindowScene {
                        SKStoreReviewController.requestReview(in: scene)
                    }
                } else if #available(iOS 10.3, *) {
                    SKStoreReviewController.requestReview()
                }
                
                UserDefaults.standard.set(Date(), forKey: lastReviewRequestDateKey)
            }
        }
    }
}