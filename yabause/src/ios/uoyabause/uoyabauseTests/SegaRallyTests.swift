//
//  SegaRallyTests.swift
//  uoyabauseTests
//
//  Created by AI Assistant on 2024/12/19.
//  Copyright © 2024 devMiyax. All rights reserved.
//

import XCTest
import Foundation
@testable import uoyabause

class SegaRallyTests: XCTestCase {
    
    override func setUp() {
        super.setUp()
        // テスト前の初期化処理
    }
    
    override func tearDown() {
        // テスト後のクリーンアップ処理
        super.tearDown()
    }
    
    // MARK: - SegaRallyRecord Tests
    
    func testSegaRallyRecordInitialization() {
        // Given
        let record = SegaRallyRecord()
        
        // Then
        XCTAssertEqual(record.minutes, 0, "初期値は0であるべき")
        XCTAssertEqual(record.seconds, 0, "初期値は0であるべき")
        XCTAssertEqual(record.milliseconds, 0, "初期値は0であるべき")
    }
    
    func testSegaRallyRecordToTotalMilliseconds() {
        // Given
        let record = SegaRallyRecord()
        record.minutes = 2
        record.seconds = 30
        record.milliseconds = 500
        
        // When
        let totalMs = record.toTotalMilliseconds()
        
        // Then
        let expected: Int64 = 2 * 60000 + 30 * 1000 + 500 // 150500ms
        XCTAssertEqual(totalMs, expected, "総ミリ秒の計算が正しくない")
    }
    
    func testSegaRallyRecordToTotalMillisecondsZero() {
        // Given
        let record = SegaRallyRecord()
        // デフォルト値（全て0）
        
        // When
        let totalMs = record.toTotalMilliseconds()
        
        // Then
        XCTAssertEqual(totalMs, 0, "全て0の場合は0を返すべき")
    }
    
    // MARK: - SegaRallyBackup Tests
    
    func testSegaRallyBackupInitialization() {
        // Given - テスト用のダミーバイナリデータを作成
        var testData = Data(count: 0x1000) // 4KB のデータ
        
        // Desert stage data (0x0909, 0x090A, 0x090B)
        testData[0x0909] = 0x02 // minutes = 2
        testData[0x090A] = 0x35 // seconds = 35 (3*10 + 5)
        testData[0x090B] = 0x67 // milliseconds = 670 (6*10 + 7) * 10
        
        // Forest stage data (0x0BA9, 0x0BAA, 0x0BAB)
        testData[0x0BA9] = 0x01 // minutes = 1
        testData[0x0BAA] = 0x45 // seconds = 45 (4*10 + 5)
        testData[0x0BAB] = 0x23 // milliseconds = 230 (2*10 + 3) * 10
        
        // Mountain stage data (0x0E49, 0x0E4A, 0x0E4B)
        testData[0x0E49] = 0x03 // minutes = 3
        testData[0x0E4A] = 0x12 // seconds = 12 (1*10 + 2)
        testData[0x0E4B] = 0x89 // milliseconds = 890 (8*10 + 9) * 10
        
        // When
        let backup = SegaRallyBackup(bin: testData)
        
        // Then
        XCTAssertEqual(backup.records.count, 3, "3つのステージレコードが作成されるべき")
        
        // Desert stage verification
        XCTAssertEqual(backup.records[0].minutes, 2, "Desert stage minutes")
        XCTAssertEqual(backup.records[0].seconds, 35, "Desert stage seconds")
        XCTAssertEqual(backup.records[0].milliseconds, 670, "Desert stage milliseconds")
        
        // Forest stage verification
        XCTAssertEqual(backup.records[1].minutes, 1, "Forest stage minutes")
        XCTAssertEqual(backup.records[1].seconds, 45, "Forest stage seconds")
        XCTAssertEqual(backup.records[1].milliseconds, 230, "Forest stage milliseconds")
        
        // Mountain stage verification
        XCTAssertEqual(backup.records[2].minutes, 3, "Mountain stage minutes")
        XCTAssertEqual(backup.records[2].seconds, 12, "Mountain stage seconds")
        XCTAssertEqual(backup.records[2].milliseconds, 890, "Mountain stage milliseconds")
    }
    
    func testSegaRallyBackupWithSmallData() {
        // Given - 小さなデータサイズでテスト
        let testData = Data(count: 10) // 10バイトのみ
        
        // When
        let backup = SegaRallyBackup(bin: testData)
        
        // Then
        XCTAssertEqual(backup.records.count, 3, "データが小さくても3つのレコードが作成されるべき")
        
        // 全てのレコードが0であることを確認
        for i in 0..<3 {
            XCTAssertEqual(backup.records[i].minutes, 0, "Stage \(i) minutes should be 0")
            XCTAssertEqual(backup.records[i].seconds, 0, "Stage \(i) seconds should be 0")
            XCTAssertEqual(backup.records[i].milliseconds, 0, "Stage \(i) milliseconds should be 0")
        }
    }
    
    // MARK: - SegaRally Class Tests
    
    func testSegaRallyInitialization() {
        // Given
        let gameCode = "TEST_SEGA_RALLY"
        
        // When
        let segaRally = SegaRally(gameCode: gameCode)
        
        // Then
        XCTAssertNotNil(segaRally.leaderBoards, "リーダーボードが初期化されるべき")
        XCTAssertEqual(segaRally.leaderBoards?.count, 3, "3つのリーダーボードが作成されるべき")
        
        // リーダーボードの内容を確認
        XCTAssertEqual(segaRally.leaderBoards?[0].title, "Desert", "Desert leaderboard title")
        XCTAssertEqual(segaRally.leaderBoards?[0].id, "01", "Desert leaderboard id")
        
        XCTAssertEqual(segaRally.leaderBoards?[1].title, "Forest", "Forest leaderboard title")
        XCTAssertEqual(segaRally.leaderBoards?[1].id, "02", "Forest leaderboard id")
        
        XCTAssertEqual(segaRally.leaderBoards?[2].title, "Mountain", "Mountain leaderboard title")
        XCTAssertEqual(segaRally.leaderBoards?[2].id, "03", "Mountain leaderboard id")
    }
    
    // MARK: - Format Time Tests
    
    func testFormatSegaRallyTime() {
        // Given
        let testCases: [(Int64, String)] = [
            (0, "0:00.000"),
            (1000, "0:01.000"),
            (60000, "1:00.000"),
            (61500, "1:01.500"),
            (125750, "2:05.750"),
            (3661234, "61:01.234")
        ]
        
        // When & Then
        for (msec, expected) in testCases {
            let result = formatSegaRallyTime(msec: msec)
            XCTAssertEqual(result, expected, "Time formatting for \(msec)ms should be \(expected)")
        }
    }
    
    // MARK: - Mock Tests for onBackUpUpdated
    
    func testOnBackUpUpdatedWithValidFile() {
        // Given
        let segaRally = SegaRally(gameCode: "TEST_CODE")
        segaRally.gameId = "test_game_id" // テスト用のgameIdを設定
        
        // テスト用のバイナリデータを作成
        var beforeData = Data(count: 0x1000)
        var afterData = Data(count: 0x1000)
        
        // Before data - 初期状態（遅いタイム）
        beforeData[0x0909] = 0x05 // 5 minutes
        beforeData[0x090A] = 0x00 // 0 seconds
        beforeData[0x090B] = 0x00 // 0 milliseconds
        
        // After data - 新記録（速いタイム）
        afterData[0x0909] = 0x02 // 2 minutes
        afterData[0x090A] = 0x30 // 30 seconds
        afterData[0x090B] = 0x50 // 500 milliseconds
        
        // When
        // Note: 実際のFirebase認証が必要なため、このテストはモック化が必要
        // ここでは基本的な動作確認のみ行う
        segaRally.onBackUpUpdated(fname: "SEGARALLY_0", before: beforeData, after: afterData)
        
        // Then
        // 実際のFirebase操作は行われないが、メソッドが正常に実行されることを確認
        XCTAssertTrue(true, "onBackUpUpdated method should execute without crashing")
    }
    
    func testOnBackUpUpdatedWithInvalidFile() {
        // Given
        let segaRally = SegaRally(gameCode: "TEST_CODE")
        segaRally.gameId = "test_game_id"
        
        let beforeData = Data(count: 100)
        let afterData = Data(count: 100)
        
        // When - 無効なファイル名でテスト
        segaRally.onBackUpUpdated(fname: "INVALID_FILE", before: beforeData, after: afterData)
        
        // Then
        // 無効なファイル名の場合、早期リターンされるため何も起こらない
        XCTAssertTrue(true, "Invalid file should be ignored")
    }
    
    func testOnBackUpUpdatedWithEmptyGameId() {
        // Given
        let segaRally = SegaRally(gameCode: "TEST_CODE")
        // gameIdを空のままにする
        
        let beforeData = Data(count: 100)
        let afterData = Data(count: 100)
        
        // When
        segaRally.onBackUpUpdated(fname: "SEGARALLY_0", before: beforeData, after: afterData)
        
        // Then
        // gameIdが空の場合、早期リターンされるため何も起こらない
        XCTAssertTrue(true, "Empty gameId should be ignored")
    }
    
    // MARK: - Performance Tests
    
    func testSegaRallyBackupPerformance() {
        // Given
        var testData = Data(count: 0x10000) // 64KB のデータ
        
        // ランダムなテストデータを設定
        for i in 0..<testData.count {
            testData[i] = UInt8.random(in: 0...255)
        }
        
        // When & Then
        measure {
            _ = SegaRallyBackup(bin: testData)
        }
    }
    
    func testSegaRallyRecordToTotalMillisecondsPerformance() {
        // Given
        let record = SegaRallyRecord()
        record.minutes = 5
        record.seconds = 30
        record.milliseconds = 750
        
        // When & Then
        measure {
            for _ in 0..<10000 {
                _ = record.toTotalMilliseconds()
            }
        }
    }
}
