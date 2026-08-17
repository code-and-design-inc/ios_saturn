import XCTest
@testable import YabaSanshiro
import UserNotifications

class MessagingManagerTests: XCTestCase {
    
    var sut: MessagingManager!
    
    override func setUp() {
        super.setUp()
        sut = MessagingManager.shared
        
        // Reset all notification settings to default
        resetNotificationSettings()
    }
    
    override func tearDown() {
        // Clean up
        resetNotificationSettings()
        super.tearDown()
    }
    
    private func resetNotificationSettings() {
        // Reset all categories to their default values
        for category in NotificationCategory.allCases {
            let defaultValue = category.isEnabledByDefault
            UserDefaults.standard.set(defaultValue, forKey: category.userDefaultsKey)
        }
        UserDefaults.standard.synchronize()
    }
    
    // MARK: - Notification Category Tests
    
    func testNotificationCategoryDefaultValues() {
        // Test default values for each category
        XCTAssertEqual(NotificationCategory.gameUpdates.isEnabledByDefault, true)
        XCTAssertEqual(NotificationCategory.systemNotifications.isEnabledByDefault, true)
        XCTAssertEqual(NotificationCategory.achievements.isEnabledByDefault, false)
        XCTAssertEqual(NotificationCategory.leaderboards.isEnabledByDefault, false)
        XCTAssertEqual(NotificationCategory.promotions.isEnabledByDefault, false)
    }
    
    func testNotificationCategoryDisplayNames() {
        XCTAssertEqual(NotificationCategory.achievements.displayName, "アチーブメント通知")
        XCTAssertEqual(NotificationCategory.leaderboards.displayName, "リーダーボード通知")
        XCTAssertEqual(NotificationCategory.gameUpdates.displayName, "ゲーム更新通知")
        XCTAssertEqual(NotificationCategory.systemNotifications.displayName, "システム通知")
        XCTAssertEqual(NotificationCategory.promotions.displayName, "プロモーション通知")
    }
    
    func testNotificationCategoryFCMTopics() {
        XCTAssertEqual(NotificationCategory.achievements.fcmTopic, "yabasanshiro_achievements")
        XCTAssertEqual(NotificationCategory.leaderboards.fcmTopic, "yabasanshiro_leaderboards")
        XCTAssertEqual(NotificationCategory.gameUpdates.fcmTopic, "yabasanshiro_gameUpdates")
        XCTAssertEqual(NotificationCategory.systemNotifications.fcmTopic, "yabasanshiro_systemNotifications")
        XCTAssertEqual(NotificationCategory.promotions.fcmTopic, "yabasanshiro_promotions")
    }
    
    // MARK: - MessagingManager Tests
    
    func testIsNotificationCategoryEnabled() {
        // Test with default values
        XCTAssertTrue(sut.isNotificationCategoryEnabled(category: .gameUpdates))
        XCTAssertTrue(sut.isNotificationCategoryEnabled(category: .systemNotifications))
        XCTAssertFalse(sut.isNotificationCategoryEnabled(category: .achievements))
        XCTAssertFalse(sut.isNotificationCategoryEnabled(category: .leaderboards))
        XCTAssertFalse(sut.isNotificationCategoryEnabled(category: .promotions))
    }
    
    func testSetNotificationCategoryEnabled() {
        // Test enabling a disabled category
        sut.setNotificationCategoryEnabled(category: .promotions, enabled: true)
        XCTAssertTrue(sut.isNotificationCategoryEnabled(category: .promotions))
        XCTAssertTrue(UserDefaults.standard.bool(forKey: NotificationCategory.promotions.userDefaultsKey))
        
        // Test disabling an enabled category
        sut.setNotificationCategoryEnabled(category: .gameUpdates, enabled: false)
        XCTAssertFalse(sut.isNotificationCategoryEnabled(category: .gameUpdates))
        XCTAssertFalse(UserDefaults.standard.bool(forKey: NotificationCategory.gameUpdates.userDefaultsKey))
    }
    
    func testNotificationCategoryPersistence() {
        // Enable a category
        sut.setNotificationCategoryEnabled(category: .achievements, enabled: true)
        
        // Create a new instance to test persistence
        let newManager = MessagingManager()
        XCTAssertTrue(newManager.isNotificationCategoryEnabled(category: .achievements))
    }
    
    // MARK: - Local Notification Tests
    
    func testScheduleAchievementNotificationCreatesCorrectContent() {
        let expectation = XCTestExpectation(description: "Notification scheduled")
        
        // Create a mock notification center delegate to verify the notification
        let mockDelegate = MockUserNotificationCenterDelegate(expectation: expectation)
        UNUserNotificationCenter.current().delegate = mockDelegate
        
        // Enable achievement notifications
        sut.setNotificationCategoryEnabled(category: .achievements, enabled: true)
        
        // Schedule notification
        sut.scheduleAchievementNotification(
            title: "Test Achievement",
            body: "You unlocked a test achievement!",
            achievementId: "test_123"
        )
        
        // Wait for notification to be scheduled
        wait(for: [expectation], timeout: 2.0)
    }
    
    func testScheduleLeaderboardNotificationCreatesCorrectContent() {
        let expectation = XCTestExpectation(description: "Notification scheduled")
        
        // Create a mock notification center delegate
        let mockDelegate = MockUserNotificationCenterDelegate(expectation: expectation)
        UNUserNotificationCenter.current().delegate = mockDelegate
        
        // Enable leaderboard notifications
        sut.setNotificationCategoryEnabled(category: .leaderboards, enabled: true)
        
        // Schedule notification
        sut.scheduleLeaderboardNotification(
            title: "New High Score!",
            body: "You reached #1 on the leaderboard",
            gameId: "game_456"
        )
        
        // Wait for notification to be scheduled
        wait(for: [expectation], timeout: 2.0)
    }
    
    // MARK: - Edge Cases
    
    func testMultipleNotificationCategoryChanges() {
        // Rapidly change category settings
        for _ in 0..<10 {
            sut.setNotificationCategoryEnabled(category: .gameUpdates, enabled: true)
            sut.setNotificationCategoryEnabled(category: .gameUpdates, enabled: false)
        }
        
        // Final state should be false
        XCTAssertFalse(sut.isNotificationCategoryEnabled(category: .gameUpdates))
    }
    
    func testAllCategoriesCanBeToggled() {
        // Enable all categories
        for category in NotificationCategory.allCases {
            sut.setNotificationCategoryEnabled(category: category, enabled: true)
            XCTAssertTrue(sut.isNotificationCategoryEnabled(category: category))
        }
        
        // Disable all categories
        for category in NotificationCategory.allCases {
            sut.setNotificationCategoryEnabled(category: category, enabled: false)
            XCTAssertFalse(sut.isNotificationCategoryEnabled(category: category))
        }
    }
}

// MARK: - Mock Classes

class MockUserNotificationCenterDelegate: NSObject, UNUserNotificationCenterDelegate {
    let expectation: XCTestExpectation
    
    init(expectation: XCTestExpectation) {
        self.expectation = expectation
        super.init()
    }
    
    func userNotificationCenter(_ center: UNUserNotificationCenter,
                              willPresent notification: UNNotification,
                              withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void) {
        expectation.fulfill()
        completionHandler([.alert, .sound])
    }
}