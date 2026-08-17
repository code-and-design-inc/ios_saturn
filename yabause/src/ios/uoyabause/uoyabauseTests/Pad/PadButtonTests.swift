//
//  PadButtonTests.swift
//  uoyabauseTests
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import XCTest
@testable import YabaSnashiro

final class PadButtonTests: XCTestCase {

    var buttonView: PadButtonView!
    var pressedButtons: [PadButtons]!
    var releasedButtons: [PadButtons]!

    override func setUp() {
        super.setUp()
        buttonView = PadButtonView(button: .a, frame: CGRect(x: 0, y: 0, width: 60, height: 60))
        pressedButtons = []
        releasedButtons = []

        buttonView.onPress = { [weak self] button in
            self?.pressedButtons.append(button)
        }
        buttonView.onRelease = { [weak self] button in
            self?.releasedButtons.append(button)
        }
    }

    override func tearDown() {
        buttonView = nil
        pressedButtons = nil
        releasedButtons = nil
        super.tearDown()
    }

    // MARK: - Initialization Tests

    func testInit_buttonTypeIsSet() {
        let buttonA = PadButtonView(button: .a)
        XCTAssertEqual(buttonA.button, .a)

        let buttonB = PadButtonView(button: .b)
        XCTAssertEqual(buttonB.button, .b)
    }

    func testInit_isNotPressed() {
        XCTAssertFalse(buttonView.isPressed)
    }

    func testInit_multipleTouchEnabled() {
        XCTAssertTrue(buttonView.isMultipleTouchEnabled)
    }

    // MARK: - Touch Began Tests

    func testHandleTouchBegan_singleTouch_firesOnPress() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)

        XCTAssertEqual(pressedButtons.count, 1)
        XCTAssertEqual(pressedButtons.first, .a)
    }

    func testHandleTouchBegan_singleTouch_setIsPressed() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)

        XCTAssertTrue(buttonView.isPressed)
    }

    func testHandleTouchBegan_multipleTouch_firesOnPressOnlyOnce() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        buttonView.handleTouchBegan(touch1)
        buttonView.handleTouchBegan(touch2)

        // Should only fire once for the first touch
        XCTAssertEqual(pressedButtons.count, 1)
    }

    // MARK: - Touch Ended Tests

    func testHandleTouchEnded_lastTouch_firesOnRelease() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)
        buttonView.handleTouchEnded(touch)

        XCTAssertEqual(releasedButtons.count, 1)
        XCTAssertEqual(releasedButtons.first, .a)
    }

    func testHandleTouchEnded_lastTouch_clearsIsPressed() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)
        buttonView.handleTouchEnded(touch)

        XCTAssertFalse(buttonView.isPressed)
    }

    func testHandleTouchEnded_notLastTouch_doesNotFireOnRelease() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        buttonView.handleTouchBegan(touch1)
        buttonView.handleTouchBegan(touch2)
        buttonView.handleTouchEnded(touch1)

        // Should not fire release while still touched
        XCTAssertEqual(releasedButtons.count, 0)
        XCTAssertTrue(buttonView.isPressed)
    }

    func testHandleTouchEnded_untrackedTouch_noEffect() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        buttonView.handleTouchBegan(touch1)
        buttonView.handleTouchEnded(touch2) // Not tracked

        // Should not release
        XCTAssertEqual(releasedButtons.count, 0)
        XCTAssertTrue(buttonView.isPressed)
    }

    // MARK: - Cancel All Touches Tests

    func testCancelAllTouches_whilePressed_firesOnRelease() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)
        buttonView.cancelAllTouches()

        XCTAssertEqual(releasedButtons.count, 1)
        XCTAssertFalse(buttonView.isPressed)
    }

    func testCancelAllTouches_whileNotPressed_doesNotFireOnRelease() {
        buttonView.cancelAllTouches()

        XCTAssertEqual(releasedButtons.count, 0)
    }

    func testCancelAllTouches_multipleTouch_firesOnReleaseOnce() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        buttonView.handleTouchBegan(touch1)
        buttonView.handleTouchBegan(touch2)
        buttonView.cancelAllTouches()

        XCTAssertEqual(releasedButtons.count, 1)
    }

    // MARK: - Point Contains Tests

    func testContainsPoint_insideBounds_returnsTrue() {
        buttonView.frame = CGRect(x: 100, y: 100, width: 60, height: 60)

        XCTAssertTrue(buttonView.containsPoint(CGPoint(x: 130, y: 130)))
        XCTAssertTrue(buttonView.containsPoint(CGPoint(x: 100, y: 100)))
        XCTAssertTrue(buttonView.containsPoint(CGPoint(x: 159, y: 159)))
    }

    func testContainsPoint_outsideBounds_returnsFalse() {
        buttonView.frame = CGRect(x: 100, y: 100, width: 60, height: 60)

        XCTAssertFalse(buttonView.containsPoint(CGPoint(x: 99, y: 130)))
        XCTAssertFalse(buttonView.containsPoint(CGPoint(x: 130, y: 99)))
        XCTAssertFalse(buttonView.containsPoint(CGPoint(x: 161, y: 130)))
    }

    // MARK: - Touch Tracking Tests

    func testIsTrackingTouch_addedTouch_returnsTrue() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)

        XCTAssertTrue(buttonView.isTrackingTouch(touch))
    }

    func testIsTrackingTouch_notAddedTouch_returnsFalse() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        buttonView.handleTouchBegan(touch1)

        XCTAssertFalse(buttonView.isTrackingTouch(touch2))
    }

    func testIsTrackingTouch_removedTouch_returnsFalse() {
        let touch = MockTouch(id: 1)
        buttonView.handleTouchBegan(touch)
        buttonView.handleTouchEnded(touch)

        XCTAssertFalse(buttonView.isTrackingTouch(touch))
    }

    // MARK: - ActionButton Tests

    func testActionButton_isCircular() {
        let actionButton = ActionButton(button: .a, frame: CGRect(x: 0, y: 0, width: 60, height: 60))
        actionButton.layoutSubviews()

        XCTAssertEqual(actionButton.layer.cornerRadius, 30)
    }

    // MARK: - TriggerButton Tests

    func testTriggerButton_hasRoundedCorners() {
        let triggerButton = TriggerButton(button: .leftTrigger, frame: CGRect(x: 0, y: 0, width: 80, height: 40))

        XCTAssertEqual(triggerButton.layer.cornerRadius, 12)
    }

    // MARK: - Multi-Touch Scenario Tests

    func testScenario_rapidPressRelease() {
        let touch = MockTouch(id: 1)

        // Rapid press-release cycle
        for _ in 0..<10 {
            buttonView.handleTouchBegan(touch)
            buttonView.handleTouchEnded(touch)
        }

        XCTAssertEqual(pressedButtons.count, 10)
        XCTAssertEqual(releasedButtons.count, 10)
        XCTAssertFalse(buttonView.isPressed)
    }

    func testScenario_twoFingerOnSameButton() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        // First finger presses
        buttonView.handleTouchBegan(touch1)
        XCTAssertEqual(pressedButtons.count, 1)
        XCTAssertTrue(buttonView.isPressed)

        // Second finger also on button
        buttonView.handleTouchBegan(touch2)
        XCTAssertEqual(pressedButtons.count, 1) // No additional press

        // First finger lifts
        buttonView.handleTouchEnded(touch1)
        XCTAssertEqual(releasedButtons.count, 0) // Not released yet
        XCTAssertTrue(buttonView.isPressed)

        // Second finger lifts
        buttonView.handleTouchEnded(touch2)
        XCTAssertEqual(releasedButtons.count, 1) // Now released
        XCTAssertFalse(buttonView.isPressed)
    }
}
