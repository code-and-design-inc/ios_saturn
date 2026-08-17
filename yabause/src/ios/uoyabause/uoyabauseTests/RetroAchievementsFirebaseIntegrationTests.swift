import XCTest
import FirebaseAuth
@testable import uoyabause

class RetroAchievementsFirebaseIntegrationTests: XCTestCase {
    
    var authManager: RetroAchievementsAuthManager!
    var mockDelegate: MockFirebaseDelegate!
    
    override func setUpWithError() throws {
        try super.setUpWithError()
        authManager = RetroAchievementsAuthManager.shared
        mockDelegate = MockFirebaseDelegate()
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
    
    func testFirebaseAuthenticationCheck() {
        XCTAssertFalse(authManager.isFirebaseAuthenticated, "Should not be authenticated with Firebase initially")
    }
    
    func testCanUseRetroAchievements() {
        XCTAssertFalse(authManager.canUseRetroAchievements, "Should not be able to use RetroAchievements without Firebase auth")
    }
    
    func testRetroAchievementsLoginWithoutFirebaseAuth() {
        let expectation = XCTestExpectation(description: "RA login without Firebase auth")
        
        authManager.loginRetroAchievements(username: "testuser", password: "testpass") { success, errorMessage in
            XCTAssertFalse(success, "RetroAchievements login should fail without Firebase auth")
            XCTAssertNotNil(errorMessage, "Error message should be provided")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 5.0)
    }
    
    func testRetroAchievementsManagerIntegration() {
        XCTAssertNotNil(RetroAchievementsManager.shared, "RetroAchievements manager should be available")
        
        let expectation = XCTestExpectation(description: "Manager login test")
        
        authManager.loginRetroAchievements(username: "testuser", password: "testpass") { success, errorMessage in
            XCTAssertFalse(success, "Login should fail with current implementation")
            XCTAssertEqual(errorMessage, "RetroAchievements manager not initialized", "Should provide specific error")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 2.0)
    }
    
    func testRetroAchievementsManagerInitialization() {
        RetroAchievementsManager.initialize()
        
        let expectation = XCTestExpectation(description: "Manager initialized login test")
        
        authManager.loginRetroAchievements(username: "testuser", password: "testpass") { success, errorMessage in
            XCTAssertFalse(success, "Login should fail with placeholder implementation")
            XCTAssertEqual(errorMessage, "RetroAchievements integration not yet implemented", "Should provide implementation status")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 2.0)
    }
    
    func testAutoLoginIntegration() {
        RetroAchievementsManager.initialize()
        
        authManager.autoLoginEnabled = true
        _ = KeychainHelper.saveCredentials(username: "testuser", password: "testpass")
        
        let expectation = XCTestExpectation(description: "Auto-login integration test")
        
        authManager.attemptAutoLogin { success, errorMessage in
            XCTAssertFalse(success, "Auto-login should fail with current implementation")
            XCTAssertNotNil(errorMessage, "Error message should be provided")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 2.0)
    }
    
    func testLogoutIntegration() {
        RetroAchievementsManager.initialize()
        
        authManager.onNativeLoginComplete(success: true, username: "testuser")
        XCTAssertTrue(authManager.isRetroAchievementsLoggedIn, "Should be logged in")
        
        authManager.logoutRetroAchievements()
        
        XCTAssertFalse(authManager.isRetroAchievementsLoggedIn, "Should be logged out")
        XCTAssertTrue(mockDelegate.authStateChangedCalled, "Delegate should be notified")
        XCTAssertFalse(mockDelegate.lastIsLoggedIn, "Delegate should receive logged out state")
    }
    
    func testCredentialPersistenceIntegration() {
        let username = "integrationuser"
        let password = "integrationpass"
        
        authManager.autoLoginEnabled = true
        
        XCTAssertTrue(KeychainHelper.saveCredentials(username: username, password: password), "Should save credentials")
        
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNotNil(credentials, "Should load credentials")
        XCTAssertEqual(credentials?.username, username, "Username should match")
        XCTAssertEqual(credentials?.password, password, "Password should match")
        
        authManager.clearStoredCredentials()
        
        XCTAssertNil(KeychainHelper.loadCredentials(), "Credentials should be cleared")
        XCTAssertFalse(authManager.autoLoginEnabled, "Auto-login should be disabled")
    }
    
    func testStateConsistencyAfterOperations() {
        RetroAchievementsManager.initialize()
        
        authManager.onNativeLoginComplete(success: true, username: "testuser")
        XCTAssertTrue(authManager.isRetroAchievementsLoggedIn, "Should be logged in")
        XCTAssertEqual(authManager.currentUsername, "testuser", "Username should be set")
        
        authManager.onNativeLoginComplete(success: false, username: nil)
        XCTAssertFalse(authManager.isRetroAchievementsLoggedIn, "Should be logged out")
        XCTAssertNil(authManager.currentUsername, "Username should be nil")
        
        authManager.onNativeLoginComplete(success: true, username: "newuser")
        XCTAssertTrue(authManager.isRetroAchievementsLoggedIn, "Should be logged in again")
        XCTAssertEqual(authManager.currentUsername, "newuser", "Username should be updated")
    }
    
    func testConcurrentOperationsIntegration() {
        RetroAchievementsManager.initialize()
        
        let expectation = XCTestExpectation(description: "Concurrent operations")
        expectation.expectedFulfillmentCount = 5
        
        let queue = DispatchQueue.global(qos: .default)
        
        for i in 0..<5 {
            queue.async { [weak self] in
                self?.authManager.onNativeLoginComplete(success: i % 2 == 0, username: "user\(i)")
                
                DispatchQueue.main.async {
                    expectation.fulfill()
                }
            }
        }
        
        wait(for: [expectation], timeout: 5.0)
    }
}

class MockFirebaseDelegate: RetroAchievementsAuthDelegate {
    var authStateChangedCalled = false
    var lastIsLoggedIn = false
    var lastUsername: String?
    var callCount = 0
    
    func authStateDidChange(isLoggedIn: Bool, username: String?) {
        authStateChangedCalled = true
        lastIsLoggedIn = isLoggedIn
        lastUsername = username
        callCount += 1
    }
}