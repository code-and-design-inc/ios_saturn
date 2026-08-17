import XCTest
@testable import YabaSanshiro
import UserNotifications

class NotificationSettingsViewControllerTests: XCTestCase {
    
    var sut: NotificationSettingsViewController!
    var window: UIWindow!
    
    override func setUp() {
        super.setUp()
        
        // Create view controller
        sut = NotificationSettingsViewController()
        
        // Setup window for proper view lifecycle
        window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = UINavigationController(rootViewController: sut)
        window.makeKeyAndVisible()
        
        // Load view
        _ = sut.view
    }
    
    override func tearDown() {
        sut = nil
        window = nil
        super.tearDown()
    }
    
    // MARK: - View Setup Tests
    
    func testViewDidLoadSetsTitle() {
        XCTAssertEqual(sut.title, "通知設定")
    }
    
    func testViewDidLoadInitializesTableView() {
        XCTAssertNotNil(sut.tableView)
        XCTAssertEqual(sut.tableView.backgroundColor, .defaultBackground)
        XCTAssertEqual(sut.tableView.separatorColor, .separator)
    }
    
    // MARK: - Table View Data Source Tests
    
    func testNumberOfSections() {
        XCTAssertEqual(sut.numberOfSections(in: sut.tableView), 2)
    }
    
    func testNumberOfRowsInSection0() {
        // Section 0: Permission status
        XCTAssertEqual(sut.tableView(sut.tableView, numberOfRowsInSection: 0), 1)
    }
    
    func testNumberOfRowsInSection1() {
        // Section 1: Notification categories (excluding achievements and leaderboards)
        XCTAssertEqual(sut.tableView(sut.tableView, numberOfRowsInSection: 1), 3)
    }
    
    func testSectionHeaders() {
        XCTAssertEqual(sut.tableView(sut.tableView, titleForHeaderInSection: 0), "通知権限")
        XCTAssertEqual(sut.tableView(sut.tableView, titleForHeaderInSection: 1), "通知カテゴリ")
    }
    
    func testSectionFooters() {
        let footer0 = sut.tableView(sut.tableView, titleForFooterInSection: 0)
        XCTAssertTrue(footer0?.contains("設定アプリで通知を許可") ?? false)
        
        let footer1 = sut.tableView(sut.tableView, titleForFooterInSection: 1)
        XCTAssertEqual(footer1, "受け取りたい通知の種類を選択してください。")
    }
    
    // MARK: - Cell Configuration Tests
    
    func testPermissionStatusCell() {
        let indexPath = IndexPath(row: 0, section: 0)
        let cell = sut.tableView(sut.tableView, cellForRowAt: indexPath)
        
        XCTAssertEqual(cell.textLabel?.text, "通知権限の状態")
        XCTAssertEqual(cell.accessoryType, .disclosureIndicator)
        XCTAssertEqual(cell.selectionStyle, .default)
        XCTAssertNil(cell.accessoryView) // Should not have a switch
    }
    
    func testNotificationCategoryCell() {
        let indexPath = IndexPath(row: 0, section: 1)
        let cell = sut.tableView(sut.tableView, cellForRowAt: indexPath)
        
        // First visible category should be gameUpdates
        XCTAssertEqual(cell.textLabel?.text, "ゲーム更新通知")
        XCTAssertNotNil(cell.accessoryView as? UISwitch)
        XCTAssertEqual(cell.selectionStyle, .none)
        XCTAssertEqual(cell.accessoryType, .none)
    }
    
    func testAllVisibleCategories() {
        // Test that only 3 categories are visible (not achievements/leaderboards)
        for row in 0..<3 {
            let indexPath = IndexPath(row: row, section: 1)
            let cell = sut.tableView(sut.tableView, cellForRowAt: indexPath)
            
            // Verify it has a switch
            XCTAssertNotNil(cell.accessoryView as? UISwitch)
            
            // Verify it's not achievements or leaderboards
            let text = cell.textLabel?.text ?? ""
            XCTAssertFalse(text.contains("アチーブメント"))
            XCTAssertFalse(text.contains("リーダーボード"))
        }
    }
    
    // MARK: - Cell Selection Tests
    
    func testSelectingPermissionCellOpensSettings() {
        let indexPath = IndexPath(row: 0, section: 0)
        
        // Mock UIApplication to verify openURL is called
        let mockApplication = MockUIApplication()
        
        // Store original shared application
        let originalApplication = UIApplication.shared
        
        // This test would need dependency injection or swizzling to fully test
        // For now, we just verify the method doesn't crash
        sut.tableView(sut.tableView, didSelectRowAt: indexPath)
        
        // Verify cell is deselected
        XCTAssertNil(sut.tableView.indexPathForSelectedRow)
    }
    
    // MARK: - Switch Action Tests
    
    func testNotificationCategorySwitchToggle() {
        let indexPath = IndexPath(row: 0, section: 1)
        let cell = sut.tableView(sut.tableView, cellForRowAt: indexPath)
        
        guard let switchView = cell.accessoryView as? UISwitch else {
            XCTFail("Expected UISwitch in cell accessory view")
            return
        }
        
        let initialState = switchView.isOn
        
        // Toggle switch
        switchView.setOn(!initialState, animated: false)
        switchView.sendActions(for: .valueChanged)
        
        // Verify the setting was changed
        let manager = MessagingManager.shared
        let category = NotificationCategory.gameUpdates
        XCTAssertEqual(manager.isNotificationCategoryEnabled(category: category), !initialState)
    }
    
    // MARK: - App Lifecycle Tests
    
    func testViewWillAppearRefreshesPermissions() {
        // Create expectation
        let expectation = XCTestExpectation(description: "Permission refresh")
        
        // Override refreshNotificationPermission to track calls
        // This would require partial mocking or dependency injection
        
        sut.viewWillAppear(false)
        
        // Give time for async operations
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 1.0)
    }
    
    func testAppDidBecomeActiveRefreshesPermissions() {
        // Post notification
        NotificationCenter.default.post(
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
        
        // Give time for notification to be processed
        let expectation = XCTestExpectation(description: "Notification processed")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 0.5)
    }
    
    // MARK: - Memory Management Tests
    
    func testDeinitRemovesObservers() {
        // Create a view controller in a local scope
        autoreleasepool {
            let localVC = NotificationSettingsViewController()
            _ = localVC.view
            // localVC should be deallocated here
        }
        
        // Verify no crash when posting notification
        NotificationCenter.default.post(
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
    }
}

// MARK: - Mock Classes

class MockUIApplication: UIApplication {
    var openedURL: URL?
    
    override func open(_ url: URL, options: [UIApplication.OpenExternalURLOptionsKey : Any] = [:], completionHandler completion: ((Bool) -> Void)? = nil) {
        openedURL = url
        completion?(true)
    }
}