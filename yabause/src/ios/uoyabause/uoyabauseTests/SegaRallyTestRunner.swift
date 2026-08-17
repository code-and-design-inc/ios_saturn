//
//  SegaRallyTestRunner.swift
//  uoyabauseTests
//
//  Created by AI Assistant on 2024/12/19.
//  Copyright © 2024 devMiyax. All rights reserved.
//

import XCTest
import Foundation

/// SEGA Rally テストの実行とレポート生成を行うクラス
class SegaRallyTestRunner {
    
    /// 全てのSEGA Rallyテストを実行し、結果をレポートする
    static func runAllTests() {
        print("=== SEGA Rally Tests Starting ===")
        
        let testSuite = SegaRallyTests()
        var passedTests = 0
        var failedTests = 0
        var totalTests = 0
        
        // テストメソッドのリスト
        let testMethods: [(String, () -> Void)] = [
            ("testSegaRallyRecordInitialization", testSuite.testSegaRallyRecordInitialization),
            ("testSegaRallyRecordToTotalMilliseconds", testSuite.testSegaRallyRecordToTotalMilliseconds),
            ("testSegaRallyRecordToTotalMillisecondsZero", testSuite.testSegaRallyRecordToTotalMillisecondsZero),
            ("testSegaRallyBackupInitialization", testSuite.testSegaRallyBackupInitialization),
            ("testSegaRallyBackupWithSmallData", testSuite.testSegaRallyBackupWithSmallData),
            ("testSegaRallyInitialization", testSuite.testSegaRallyInitialization),
            ("testFormatSegaRallyTime", testSuite.testFormatSegaRallyTime),
            ("testOnBackUpUpdatedWithValidFile", testSuite.testOnBackUpUpdatedWithValidFile),
            ("testOnBackUpUpdatedWithInvalidFile", testSuite.testOnBackUpUpdatedWithInvalidFile),
            ("testOnBackUpUpdatedWithEmptyGameId", testSuite.testOnBackUpUpdatedWithEmptyGameId)
        ]
        
        // 各テストを実行
        for (testName, testMethod) in testMethods {
            totalTests += 1
            print("Running: \(testName)")
            
            do {
                testSuite.setUp()
                testMethod()
                testSuite.tearDown()
                print("✅ PASSED: \(testName)")
                passedTests += 1
            } catch {
                print("❌ FAILED: \(testName) - \(error)")
                failedTests += 1
            }
        }
        
        // 結果レポート
        print("\n=== SEGA Rally Test Results ===")
        print("Total Tests: \(totalTests)")
        print("Passed: \(passedTests)")
        print("Failed: \(failedTests)")
        print("Success Rate: \(String(format: "%.1f", Double(passedTests) / Double(totalTests) * 100))%")
        
        if failedTests == 0 {
            print("🎉 All tests passed!")
        } else {
            print("⚠️ Some tests failed. Please review the implementation.")
        }
        
        print("=== SEGA Rally Tests Completed ===\n")
    }
    
    /// 特定のテストカテゴリのみを実行
    static func runCategoryTests(_ category: TestCategory) {
        print("=== Running \(category.rawValue) Tests ===")
        
        let testSuite = SegaRallyTests()
        var passedTests = 0
        var failedTests = 0
        var totalTests = 0
        
        let testMethods: [(String, () -> Void)]
        
        switch category {
        case .record:
            testMethods = [
                ("testSegaRallyRecordInitialization", testSuite.testSegaRallyRecordInitialization),
                ("testSegaRallyRecordToTotalMilliseconds", testSuite.testSegaRallyRecordToTotalMilliseconds),
                ("testSegaRallyRecordToTotalMillisecondsZero", testSuite.testSegaRallyRecordToTotalMillisecondsZero)
            ]
        case .backup:
            testMethods = [
                ("testSegaRallyBackupInitialization", testSuite.testSegaRallyBackupInitialization),
                ("testSegaRallyBackupWithSmallData", testSuite.testSegaRallyBackupWithSmallData)
            ]
        case .game:
            testMethods = [
                ("testSegaRallyInitialization", testSuite.testSegaRallyInitialization),
                ("testOnBackUpUpdatedWithValidFile", testSuite.testOnBackUpUpdatedWithValidFile),
                ("testOnBackUpUpdatedWithInvalidFile", testSuite.testOnBackUpUpdatedWithInvalidFile),
                ("testOnBackUpUpdatedWithEmptyGameId", testSuite.testOnBackUpUpdatedWithEmptyGameId)
            ]
        case .utility:
            testMethods = [
                ("testFormatSegaRallyTime", testSuite.testFormatSegaRallyTime)
            ]
        }
        
        // 各テストを実行
        for (testName, testMethod) in testMethods {
            totalTests += 1
            print("Running: \(testName)")
            
            do {
                testSuite.setUp()
                testMethod()
                testSuite.tearDown()
                print("✅ PASSED: \(testName)")
                passedTests += 1
            } catch {
                print("❌ FAILED: \(testName) - \(error)")
                failedTests += 1
            }
        }
        
        // 結果レポート
        print("\n=== \(category.rawValue) Test Results ===")
        print("Total Tests: \(totalTests)")
        print("Passed: \(passedTests)")
        print("Failed: \(failedTests)")
        print("Success Rate: \(String(format: "%.1f", Double(passedTests) / Double(totalTests) * 100))%")
        print("=== \(category.rawValue) Tests Completed ===\n")
    }
    
    /// パフォーマンステストを実行
    static func runPerformanceTests() {
        print("=== Running Performance Tests ===")
        
        let testSuite = SegaRallyTests()
        
        print("Running: testSegaRallyBackupPerformance")
        testSuite.testSegaRallyBackupPerformance()
        print("✅ COMPLETED: testSegaRallyBackupPerformance")
        
        print("Running: testSegaRallyRecordToTotalMillisecondsPerformance")
        testSuite.testSegaRallyRecordToTotalMillisecondsPerformance()
        print("✅ COMPLETED: testSegaRallyRecordToTotalMillisecondsPerformance")
        
        print("=== Performance Tests Completed ===\n")
    }
    
    /// テストデータの検証
    static func validateTestData() {
        print("=== Validating Test Data ===")
        
        // テスト用のバイナリデータを作成して検証
        var testData = Data(count: 0x1000)
        
        // Desert stage data
        testData[0x0909] = 0x02 // minutes = 2
        testData[0x090A] = 0x35 // seconds = 35
        testData[0x090B] = 0x67 // milliseconds = 670
        
        let backup = SegaRallyBackup(bin: testData)
        
        print("Desert stage validation:")
        print("  Minutes: \(backup.records[0].minutes) (expected: 2)")
        print("  Seconds: \(backup.records[0].seconds) (expected: 35)")
        print("  Milliseconds: \(backup.records[0].milliseconds) (expected: 670)")
        print("  Total MS: \(backup.records[0].toTotalMilliseconds()) (expected: 155670)")
        
        let isValid = backup.records[0].minutes == 2 &&
                     backup.records[0].seconds == 35 &&
                     backup.records[0].milliseconds == 670 &&
                     backup.records[0].toTotalMilliseconds() == 155670
        
        if isValid {
            print("✅ Test data validation passed")
        } else {
            print("❌ Test data validation failed")
        }
        
        print("=== Test Data Validation Completed ===\n")
    }
}

/// テストカテゴリの列挙型
enum TestCategory: String {
    case record = "Record"
    case backup = "Backup"
    case game = "Game"
    case utility = "Utility"
}

/// テスト実行のエントリーポイント
class SegaRallyTestMain {
    
    /// メインのテスト実行メソッド
    static func main() {
        print("🏁 SEGA Rally Championship Test Suite 🏁\n")
        
        // 1. データ検証
        SegaRallyTestRunner.validateTestData()
        
        // 2. 全テスト実行
        SegaRallyTestRunner.runAllTests()
        
        // 3. カテゴリ別テスト（オプション）
        // SegaRallyTestRunner.runCategoryTests(.record)
        // SegaRallyTestRunner.runCategoryTests(.backup)
        // SegaRallyTestRunner.runCategoryTests(.game)
        // SegaRallyTestRunner.runCategoryTests(.utility)
        
        // 4. パフォーマンステスト（オプション）
        // SegaRallyTestRunner.runPerformanceTests()
        
        print("🏆 SEGA Rally Test Suite Completed 🏆")
    }
}
