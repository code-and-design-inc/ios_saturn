import Foundation
import FirebaseMessaging
import FirebaseFirestore
import FirebaseAuth
import UserNotifications

class MessagingManager: NSObject {
    static let shared = MessagingManager()
    
    private override init() {
        super.init()
    }
    
    // MARK: - FCM Token Management
    
    /// Save FCM token to Firestore user document
    func saveFCMTokenToFirestore(token: String) {
        guard let currentUser = Auth.auth().currentUser else {
            print("MessagingManager: No authenticated user, skipping FCM token save")
            return
        }
        
        let db = Firestore.firestore()
        let userRef = db.collection("users").document(currentUser.uid)
        
        userRef.setData([
            "fcmToken": token,
            "fcmTokenUpdatedAt": FieldValue.serverTimestamp(),
            "platform": "ios"
        ], merge: true) { error in
            if let error = error {
                print("MessagingManager: Error saving FCM token to Firestore: \(error)")
            } else {
                print("MessagingManager: FCM token saved successfully to Firestore")
            }
        }
    }
    
    /// Remove FCM token from Firestore (called on logout)
    func removeFCMTokenFromFirestore() {
        guard let currentUser = Auth.auth().currentUser else {
            return
        }
        
        let db = Firestore.firestore()
        let userRef = db.collection("users").document(currentUser.uid)
        
        userRef.setData([
            "fcmToken": FieldValue.delete(),
            "fcmTokenUpdatedAt": FieldValue.delete()
        ], merge: true) { error in
            if let error = error {
                print("MessagingManager: Error removing FCM token from Firestore: \(error)")
            } else {
                print("MessagingManager: FCM token removed successfully from Firestore")
            }
        }
    }
    
    // MARK: - Notification Settings

    /// Register default on/off values for notification categories.
    /// UserDefaults.bool(forKey:) returns false for unset keys, so without
    /// this the default-on categories (game_updates, system_notifications)
    /// are never subscribed unless the user opens the notification settings
    /// screen. Must run before the first subscribeToEnabledTopics() call.
    func registerDefaultCategorySettings() {
        for category in NotificationCategory.allCases {
            let key = category.userDefaultsKey
            if UserDefaults.standard.object(forKey: key) == nil {
                UserDefaults.standard.set(category.isEnabledByDefault, forKey: key)
            }
        }
    }

    /// Get current notification permission status
    func getNotificationPermissionStatus(completion: @escaping (UNAuthorizationStatus) -> Void) {
        UNUserNotificationCenter.current().getNotificationSettings { settings in
            DispatchQueue.main.async {
                completion(settings.authorizationStatus)
            }
        }
    }
    
    /// Check if specific notification category is enabled
    func isNotificationCategoryEnabled(category: NotificationCategory) -> Bool {
        return UserDefaults.standard.bool(forKey: category.userDefaultsKey)
    }
    
    /// Enable/disable specific notification category
    func setNotificationCategoryEnabled(category: NotificationCategory, enabled: Bool) {
        UserDefaults.standard.set(enabled, forKey: category.userDefaultsKey)
        
        // Subscribe/unsubscribe from FCM topic
        if enabled {
            subscribeToTopic(topic: category.fcmTopic)
        } else {
            unsubscribeFromTopic(topic: category.fcmTopic)
        }
    }
    
    // MARK: - FCM Topics Management
    
    /// Subscribe to FCM topic
    private func subscribeToTopic(topic: String) {
        Messaging.messaging().subscribe(toTopic: topic) { error in
            if let error = error {
                print("MessagingManager: Error subscribing to topic \(topic): \(error)")
            } else {
                print("MessagingManager: Successfully subscribed to topic: \(topic)")
            }
        }
    }
    
    /// Unsubscribe from FCM topic
    private func unsubscribeFromTopic(topic: String) {
        Messaging.messaging().unsubscribe(fromTopic: topic) { error in
            if let error = error {
                print("MessagingManager: Error unsubscribing from topic \(topic): \(error)")
            } else {
                print("MessagingManager: Successfully unsubscribed from topic: \(topic)")
            }
        }
    }
    
    /// Subscribe to all enabled notification topics
    func subscribeToEnabledTopics() {
        for category in NotificationCategory.allCases {
            if isNotificationCategoryEnabled(category: category) {
                subscribeToTopic(topic: category.fcmTopic)
            }
        }
    }
    
    /// Unsubscribe from all notification topics (used on logout)
    func unsubscribeFromAllTopics() {
        for category in NotificationCategory.allCases {
            unsubscribeFromTopic(topic: category.fcmTopic)
        }
    }
    
    // MARK: - Local Notifications
    
    /// Schedule local notification for RetroAchievements
    func scheduleAchievementNotification(title: String, body: String, achievementId: String) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default
        content.userInfo = [
            "type": "achievement",
            "achievementId": achievementId
        ]
        
        // Trigger immediately
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 0.1, repeats: false)
        let identifier = "achievement_\(achievementId)_\(Date().timeIntervalSince1970)"
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: trigger)
        
        UNUserNotificationCenter.current().add(request) { error in
            if let error = error {
                print("MessagingManager: Error scheduling achievement notification: \(error)")
            }
        }
    }
    
    /// Schedule local notification for leaderboard update
    func scheduleLeaderboardNotification(title: String, body: String, gameId: String) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default
        content.userInfo = [
            "type": "leaderboard",
            "gameId": gameId
        ]
        
        // Trigger immediately
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 0.1, repeats: false)
        let identifier = "leaderboard_\(gameId)_\(Date().timeIntervalSince1970)"
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: trigger)
        
        UNUserNotificationCenter.current().add(request) { error in
            if let error = error {
                print("MessagingManager: Error scheduling leaderboard notification: \(error)")
            }
        }
    }
}

// MARK: - Notification Categories

enum NotificationCategory: String, CaseIterable {
    case achievements = "achievements"
    case leaderboards = "leaderboards" 
    case gameUpdates = "game_updates"
    case systemNotifications = "system_notifications"
    case promotions = "promotions"
    
    var displayName: String {
        switch self {
        case .achievements:
            return NSLocalizedString("Achievements", comment: "Achievements notification category")
        case .leaderboards:
            return NSLocalizedString("Leaderboards", comment: "Leaderboards notification category")
        case .gameUpdates:
            return NSLocalizedString("Game Updates", comment: "Game updates notification category")
        case .systemNotifications:
            return NSLocalizedString("System Notifications", comment: "System notifications category")
        case .promotions:
            return NSLocalizedString("Promotions", comment: "Promotions notification category")
        }
    }
    
    var description: String {
        switch self {
        case .achievements:
            return NSLocalizedString("Notifications for new achievement unlocks", comment: "Achievement notifications description")
        case .leaderboards:
            return NSLocalizedString("Leaderboard ranking change notifications", comment: "Leaderboard notifications description")
        case .gameUpdates:
            return NSLocalizedString("Game support and bug fix notifications", comment: "Game update notifications description")
        case .systemNotifications:
            return NSLocalizedString("Important notifications like system maintenance", comment: "System notifications description")
        case .promotions:
            return NSLocalizedString("New features and special announcements", comment: "Promotions notifications description")
        }
    }
    
    var userDefaultsKey: String {
        return "notification_\(rawValue)_enabled"
    }
    
    var fcmTopic: String {
        return "yabasanshiro_\(rawValue)"
    }
    
    var isEnabledByDefault: Bool {
        switch self {
        case .gameUpdates, .systemNotifications:
            return true
        case .achievements, .leaderboards, .promotions:
            return false
        }
    }
}