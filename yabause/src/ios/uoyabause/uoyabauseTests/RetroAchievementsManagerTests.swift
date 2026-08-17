import XCTest
@testable import uoyabause

class RetroAchievementsManagerTests: XCTestCase {
    
    var manager: RetroAchievementsManager?
    
    override func setUpWithError() throws {
        try super.setUpWithError()
        RetroAchievementsManager.initialize()
        manager = RetroAchievementsManager.shared
    }
    
    override func tearDownWithError() throws {
        manager = nil
        try super.tearDownWithError()
    }
    
    func testSingletonInitialization() {
        XCTAssertNotNil(RetroAchievementsManager.shared, "Manager should be initialized")
        
        let instance1 = RetroAchievementsManager.shared
        let instance2 = RetroAchievementsManager.shared
        
        XCTAssertTrue(instance1 === instance2, "Manager should be a singleton")
    }
    
    func testInitializeCreatesSharedInstance() {
        RetroAchievementsManager.shared = nil
        XCTAssertNil(RetroAchievementsManager.shared, "Shared instance should be nil")
        
        RetroAchievementsManager.initialize()
        XCTAssertNotNil(RetroAchievementsManager.shared, "Initialize should create shared instance")
    }
    
    func testInitializeDoesNotOverwriteExistingInstance() {
        let originalInstance = RetroAchievementsManager.shared
        
        RetroAchievementsManager.initialize()
        
        XCTAssertTrue(originalInstance === RetroAchievementsManager.shared, "Initialize should not overwrite existing instance")
    }
    
    func testInitialUserState() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        XCTAssertFalse(manager.isUserLoggedIn, "User should not be logged in initially")
    }
    
    func testLoginUserWithInvalidCredentials() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        let expectation = XCTestExpectation(description: "Login completion")
        
        manager.loginUser(username: "testuser", password: "testpass") { success, errorMessage in
            XCTAssertFalse(success, "Login should fail with current placeholder implementation")
            XCTAssertNotNil(errorMessage, "Error message should be provided")
            XCTAssertEqual(errorMessage, "RetroAchievements integration not yet implemented", "Should return expected error message")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 1.0)
    }
    
    func testLoginUserWithEmptyCredentials() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        let expectation = XCTestExpectation(description: "Login completion")
        
        manager.loginUser(username: "", password: "") { success, errorMessage in
            XCTAssertFalse(success, "Login should fail with empty credentials")
            XCTAssertNotNil(errorMessage, "Error message should be provided")
            expectation.fulfill()
        }
        
        wait(for: [expectation], timeout: 1.0)
    }
    
    func testLogout() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        XCTAssertNoThrow(manager.logout(), "Logout should not throw")
    }
    
    func testSetHardcoreEnabled() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        XCTAssertNoThrow(manager.setHardcoreEnabled(true), "Setting hardcore mode should not throw")
        XCTAssertNoThrow(manager.setHardcoreEnabled(false), "Disabling hardcore mode should not throw")
    }
    
    func testOnLoginStateChanged() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        XCTAssertNoThrow(manager.onLoginStateChanged(isLoggedIn: true), "Login state change should not throw")
        XCTAssertNoThrow(manager.onLoginStateChanged(isLoggedIn: false), "Login state change should not throw")
    }
    
    func testManagerPerformance() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        measure {
            for _ in 0..<1000 {
                manager.setHardcoreEnabled(true)
                manager.onLoginStateChanged(isLoggedIn: false)
                _ = manager.isUserLoggedIn
            }
        }
    }
    
    func testConcurrentAccess() {
        guard let manager = manager else {
            XCTFail("Manager should be initialized")
            return
        }
        
        let expectation = XCTestExpectation(description: "Concurrent access")
        expectation.expectedFulfillmentCount = 10
        
        let queue = DispatchQueue.global(qos: .default)
        
        for i in 0..<10 {
            queue.async {
                manager.setHardcoreEnabled(i % 2 == 0)
                manager.onLoginStateChanged(isLoggedIn: i % 2 == 1)
                _ = manager.isUserLoggedIn
                expectation.fulfill()
            }
        }
        
        wait(for: [expectation], timeout: 5.0)
    }
}