import XCTest
import FirebaseAuth
@testable import uoyabause

class RetroAchievementsAuthManagerTests: XCTestCase {
    
    var authManager: RetroAchievementsAuthManager!
    var mockDelegate: MockAuthDelegate!
    
    override func setUpWithError() throws {
        try super.setUpWithError()
        authManager = RetroAchievementsAuthManager.shared
        mockDelegate = MockAuthDelegate()
        authManager.delegate = mockDelegate
        
        UserDefaults.standard.removeObject(forKey: "ra_username")
        UserDefaults.standard.removeObject(forKey: "ra_auto_login")
        UserDefaults.standard.removeObject(forKey: "ra_is_logged_in")
        
        _ = KeychainHelper.deleteCredentials()
    }
    
    override func tearDownWithError() throws {
        authManager.delegate = nil
        mockDelegate = nil
        
        UserDefaults.standard.removeObject(forKey: "ra_username")
        UserDefaults.standard.removeObject(forKey: "ra_auto_login")
        UserDefaults.standard.removeObject(forKey: "ra_is_logged_in")
        
        _ = KeychainHelper.deleteCredentials()
        
        try super.tearDownWithError()
    }
    
    func testSingletonInstance() {
        let instance1 = RetroAchievementsAuthManager.shared
        let instance2 = RetroAchievementsAuthManager.shared
        
        XCTAssertTrue(instance1 === instance2, "AuthManager should be a singleton")
    }
    
    func testInitialState() {
        XCTAssertFalse(authManager.isRetroAchievementsLoggedIn, "Should not be logged in initially")
        XCTAssertNil(authManager.currentUsername, "Username should be nil initially")
        XCTAssertFalse(authManager.autoLoginEnabled, "Auto-login should be disabled initially")
    }
    
    func testAutoLoginProperty() {
        authManager.autoLoginEnabled = true
        XCTAssertTrue(authManager.autoLoginEnabled, "Auto-login should be enabled")
        
        authManager.autoLoginEnabled = false
        XCTAssertFalse(authManager.autoLoginEnabled, "Auto-login should be disabled")
    }
    
    func testLoginStateManagement() {
        authManager.onNativeLoginComplete(success: true, username: "testuser")
        
        XCTAssertTrue(authManager.isRetroAchievementsLoggedIn, "Should be logged in after successful login")
        XCTAssertEqual(authManager.currentUsername, "testuser", "Username should be set")
    }
    
    func testLogout() {
        authManager.onNativeLoginComplete(success: true, username: "testuser")
        XCTAssertTrue(authManager.isRetroAchievementsLoggedIn, "Should be logged in")
        
        authManager.logoutRetroAchievements()
        
        XCTAssertFalse(authManager.isRetroAchievementsLoggedIn, "Should be logged out")
        XCTAssertNil(authManager.currentUsername, "Username should be nil after logout")
    }
    
    func testDelegateNotification() {
        authManager.onNativeLoginComplete(success: true, username: "testuser")
        
        XCTAssertTrue(mockDelegate.authStateChangedCalled, "Delegate should be notified")
        XCTAssertTrue(mockDelegate.lastIsLoggedIn, "Delegate should receive logged in state")
        XCTAssertEqual(mockDelegate.lastUsername, "testuser", "Delegate should receive username")
    }
    
    func testLoginFailure() {
        authManager.onNativeLoginComplete(success: false, username: nil)
        
        XCTAssertFalse(authManager.isRetroAchievementsLoggedIn, "Should not be logged in after failed login")
        XCTAssertNil(authManager.currentUsername, "Username should be nil after failed login")
        XCTAssertTrue(mockDelegate.authStateChangedCalled, "Delegate should be notified of failure")
        XCTAssertFalse(mockDelegate.lastIsLoggedIn, "Delegate should receive logged out state")
    }
    
    func testClearStoredCredentials() {
        authManager.autoLoginEnabled = true
        _ = KeychainHelper.saveCredentials(username: "testuser", password: "testpass")
        
        authManager.clearStoredCredentials()
        
        XCTAssertFalse(authManager.autoLoginEnabled, "Auto-login should be disabled")
        XCTAssertNil(KeychainHelper.loadCredentials(), "Credentials should be cleared from keychain")
    }
    
    func testAttemptAutoLoginWithoutCredentials() {
        let expectation = XCTestExpectation(description: "Auto-login completion")
        
        authManager.attemptAutoLogin { success, errorMessage in
            XCTAssertFalse(success, "Auto-login should fail without credentials")
            XCTAssertNotNil(errorMessage, "Error message should be provided")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 1.0)
    }
    
    func testAttemptAutoLoginWithDisabledAutoLogin() {
        _ = KeychainHelper.saveCredentials(username: "testuser", password: "testpass")
        authManager.autoLoginEnabled = false
        
        let expectation = XCTestExpectation(description: "Auto-login completion")
        
        authManager.attemptAutoLogin { success, errorMessage in
            XCTAssertFalse(success, "Auto-login should fail when disabled")
            XCTAssertNotNil(errorMessage, "Error message should be provided")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 1.0)
    }
}

class MockAuthDelegate: RetroAchievementsAuthDelegate {
    var authStateChangedCalled = false
    var lastIsLoggedIn = false
    var lastUsername: String?
    
    func authStateDidChange(isLoggedIn: Bool, username: String?) {
        authStateChangedCalled = true
        lastIsLoggedIn = isLoggedIn
        lastUsername = username
    }
}