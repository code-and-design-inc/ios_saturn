import XCTest
@testable import uoyabause

class KeychainHelperTests: XCTestCase {
    
    let testKey = "test_keychain_key"
    let testValue = "test_keychain_value"
    let testUsername = "testuser"
    let testPassword = "testpassword"
    
    override func setUpWithError() throws {
        try super.setUpWithError()
        _ = KeychainHelper.delete(key: testKey)
        _ = KeychainHelper.deleteCredentials()
    }
    
    override func tearDownWithError() throws {
        _ = KeychainHelper.delete(key: testKey)
        _ = KeychainHelper.deleteCredentials()
        try super.tearDownWithError()
    }
    
    func testSaveAndLoadValue() {
        let success = KeychainHelper.save(key: testKey, value: testValue)
        XCTAssertTrue(success, "Should successfully save value to keychain")
        
        let retrievedValue = KeychainHelper.load(key: testKey)
        XCTAssertEqual(retrievedValue, testValue, "Retrieved value should match saved value")
    }
    
    func testSaveEmptyValue() {
        let success = KeychainHelper.save(key: testKey, value: "")
        XCTAssertTrue(success, "Should successfully save empty value to keychain")
        
        let retrievedValue = KeychainHelper.load(key: testKey)
        XCTAssertEqual(retrievedValue, "", "Retrieved value should be empty string")
    }
    
    func testLoadNonExistentKey() {
        let retrievedValue = KeychainHelper.load(key: "non_existent_key")
        XCTAssertNil(retrievedValue, "Should return nil for non-existent key")
    }
    
    func testOverwriteExistingValue() {
        let firstValue = "first_value"
        let secondValue = "second_value"
        
        _ = KeychainHelper.save(key: testKey, value: firstValue)
        let success = KeychainHelper.save(key: testKey, value: secondValue)
        
        XCTAssertTrue(success, "Should successfully overwrite existing value")
        
        let retrievedValue = KeychainHelper.load(key: testKey)
        XCTAssertEqual(retrievedValue, secondValue, "Retrieved value should be the updated value")
    }
    
    func testDeleteExistingValue() {
        _ = KeychainHelper.save(key: testKey, value: testValue)
        
        let deleteSuccess = KeychainHelper.delete(key: testKey)
        XCTAssertTrue(deleteSuccess, "Should successfully delete existing value")
        
        let retrievedValue = KeychainHelper.load(key: testKey)
        XCTAssertNil(retrievedValue, "Retrieved value should be nil after deletion")
    }
    
    func testDeleteNonExistentValue() {
        let deleteSuccess = KeychainHelper.delete(key: "non_existent_key")
        XCTAssertTrue(deleteSuccess, "Should return success when deleting non-existent key")
    }
    
    func testSaveCredentials() {
        let success = KeychainHelper.saveCredentials(username: testUsername, password: testPassword)
        XCTAssertTrue(success, "Should successfully save credentials")
        
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNotNil(credentials, "Should be able to load saved credentials")
        XCTAssertEqual(credentials?.username, testUsername, "Username should match")
        XCTAssertEqual(credentials?.password, testPassword, "Password should match")
    }
    
    func testSaveCredentialsWithSpecialCharacters() {
        let specialUsername = "user@example.com"
        let specialPassword = "p@ssw0rd!#$%"
        
        let success = KeychainHelper.saveCredentials(username: specialUsername, password: specialPassword)
        XCTAssertTrue(success, "Should successfully save credentials with special characters")
        
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNotNil(credentials, "Should be able to load saved credentials")
        XCTAssertEqual(credentials?.username, specialUsername, "Username with special characters should match")
        XCTAssertEqual(credentials?.password, specialPassword, "Password with special characters should match")
    }
    
    func testSaveCredentialsWithEmptyValues() {
        let success = KeychainHelper.saveCredentials(username: "", password: "")
        XCTAssertTrue(success, "Should successfully save empty credentials")
        
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNotNil(credentials, "Should be able to load empty credentials")
        XCTAssertEqual(credentials?.username, "", "Empty username should match")
        XCTAssertEqual(credentials?.password, "", "Empty password should match")
    }
    
    func testLoadCredentialsWhenNoneExist() {
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNil(credentials, "Should return nil when no credentials exist")
    }
    
    func testDeleteCredentials() {
        _ = KeychainHelper.saveCredentials(username: testUsername, password: testPassword)
        
        let deleteSuccess = KeychainHelper.deleteCredentials()
        XCTAssertTrue(deleteSuccess, "Should successfully delete credentials")
        
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNil(credentials, "Credentials should be nil after deletion")
    }
    
    func testDeleteCredentialsWhenNoneExist() {
        let deleteSuccess = KeychainHelper.deleteCredentials()
        XCTAssertTrue(deleteSuccess, "Should return success when deleting non-existent credentials")
    }
    
    func testOverwriteCredentials() {
        let firstUsername = "first_user"
        let firstPassword = "first_pass"
        let secondUsername = "second_user"
        let secondPassword = "second_pass"
        
        _ = KeychainHelper.saveCredentials(username: firstUsername, password: firstPassword)
        let success = KeychainHelper.saveCredentials(username: secondUsername, password: secondPassword)
        
        XCTAssertTrue(success, "Should successfully overwrite existing credentials")
        
        let credentials = KeychainHelper.loadCredentials()
        XCTAssertNotNil(credentials, "Should be able to load updated credentials")
        XCTAssertEqual(credentials?.username, secondUsername, "Username should be updated")
        XCTAssertEqual(credentials?.password, secondPassword, "Password should be updated")
    }
    
    func testConcurrentKeychainAccess() {
        let expectation = XCTestExpectation(description: "Concurrent keychain access")
        expectation.expectedFulfillmentCount = 10
        
        let queue = DispatchQueue.global(qos: .default)
        
        for i in 0..<10 {
            queue.async {
                let key = "concurrent_test_\(i)"
                let value = "value_\(i)"
                
                _ = KeychainHelper.save(key: key, value: value)
                let retrieved = KeychainHelper.load(key: key)
                
                XCTAssertEqual(retrieved, value, "Concurrent access should work correctly")
                
                _ = KeychainHelper.delete(key: key)
                expectation.fulfill()
            }
        }
        
        wait(for: [expectation], timeout: 5.0)
    }
    
    func testKeychainPerformance() {
        measure {
            for i in 0..<100 {
                let key = "perf_test_\(i)"
                let value = "performance_value_\(i)"
                
                _ = KeychainHelper.save(key: key, value: value)
                _ = KeychainHelper.load(key: key)
                _ = KeychainHelper.delete(key: key)
            }
        }
    }
}