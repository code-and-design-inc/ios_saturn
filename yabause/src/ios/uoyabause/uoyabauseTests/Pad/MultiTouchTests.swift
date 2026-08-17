//
//  MultiTouchTests.swift
//  uoyabauseTests
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import XCTest
@testable import YabaSnashiro

/// Integration tests for multi-button simultaneous press functionality
final class MultiTouchTests: XCTestCase {

    var buttonA: PadButtonView!
    var buttonB: PadButtonView!
    var buttonC: PadButtonView!

    var pressedButtons: Set<PadButtons>!
    var releasedButtons: Set<PadButtons>!

    override func setUp() {
        super.setUp()

        // Create buttons with different positions
        buttonA = PadButtonView(button: .a, frame: CGRect(x: 0, y: 0, width: 60, height: 60))
        buttonB = PadButtonView(button: .b, frame: CGRect(x: 70, y: 0, width: 60, height: 60))
        buttonC = PadButtonView(button: .c, frame: CGRect(x: 140, y: 0, width: 60, height: 60))

        pressedButtons = Set<PadButtons>()
        releasedButtons = Set<PadButtons>()

        // Set up callbacks
        for button in [buttonA, buttonB, buttonC] {
            button?.onPress = { [weak self] btn in
                self?.pressedButtons.insert(btn)
            }
            button?.onRelease = { [weak self] btn in
                self?.releasedButtons.insert(btn)
            }
        }
    }

    override func tearDown() {
        buttonA = nil
        buttonB = nil
        buttonC = nil
        pressedButtons = nil
        releasedButtons = nil
        super.tearDown()
    }

    // MARK: - Two Button Simultaneous Press Tests

    func testTwoButtonSimultaneousPress_differentFingers() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)

        // Press A and B simultaneously
        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)

        XCTAssertTrue(pressedButtons.contains(.a))
        XCTAssertTrue(pressedButtons.contains(.b))
        XCTAssertEqual(pressedButtons.count, 2)

        XCTAssertTrue(buttonA.isPressed)
        XCTAssertTrue(buttonB.isPressed)
    }

    func testTwoButtonSimultaneousPress_releaseOne() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)

        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)

        // Release only A
        buttonA.handleTouchEnded(touchA)

        XCTAssertTrue(releasedButtons.contains(.a))
        XCTAssertFalse(releasedButtons.contains(.b))
        XCTAssertFalse(buttonA.isPressed)
        XCTAssertTrue(buttonB.isPressed)
    }

    func testTwoButtonSimultaneousPress_releaseBoth() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)

        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)
        buttonA.handleTouchEnded(touchA)
        buttonB.handleTouchEnded(touchB)

        XCTAssertTrue(releasedButtons.contains(.a))
        XCTAssertTrue(releasedButtons.contains(.b))
        XCTAssertFalse(buttonA.isPressed)
        XCTAssertFalse(buttonB.isPressed)
    }

    // MARK: - Three Button Simultaneous Press Tests

    func testThreeButtonSimultaneousPress() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)
        let touchC = MockTouch(id: 3)

        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)
        buttonC.handleTouchBegan(touchC)

        XCTAssertEqual(pressedButtons.count, 3)
        XCTAssertTrue(pressedButtons.contains(.a))
        XCTAssertTrue(pressedButtons.contains(.b))
        XCTAssertTrue(pressedButtons.contains(.c))

        XCTAssertTrue(buttonA.isPressed)
        XCTAssertTrue(buttonB.isPressed)
        XCTAssertTrue(buttonC.isPressed)
    }

    func testThreeButtonPress_releaseMiddle() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)
        let touchC = MockTouch(id: 3)

        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)
        buttonC.handleTouchBegan(touchC)

        // Release middle button
        buttonB.handleTouchEnded(touchB)

        XCTAssertTrue(buttonA.isPressed)
        XCTAssertFalse(buttonB.isPressed)
        XCTAssertTrue(buttonC.isPressed)
    }

    // MARK: - Single Finger Multi-Button Tests (Sliding)

    func testSingleFingerSlide_pressAReleaseThenPressB() {
        let touch = MockTouch(id: 1)

        // Press A
        buttonA.handleTouchBegan(touch)
        XCTAssertTrue(buttonA.isPressed)
        XCTAssertTrue(pressedButtons.contains(.a))

        // Slide off A (release)
        buttonA.handleTouchEnded(touch)
        XCTAssertFalse(buttonA.isPressed)
        XCTAssertTrue(releasedButtons.contains(.a))

        // Slide onto B (new press)
        buttonB.handleTouchBegan(touch)
        XCTAssertTrue(buttonB.isPressed)
        XCTAssertTrue(pressedButtons.contains(.b))
    }

    // MARK: - Fighting Game Command Tests

    func testFightingGameCommand_quarterCircleForward() {
        // Simulate: Down -> Down-Right -> Right + A (quarter circle + attack)
        let touchDPad = MockTouch(id: 1)
        let touchA = MockTouch(id: 2)

        // This is a conceptual test - in real scenario, D-PAD would handle direction changes
        // Here we test the button timing

        // Press A at the end of motion
        buttonA.handleTouchBegan(touchA)
        XCTAssertTrue(buttonA.isPressed)
        XCTAssertTrue(pressedButtons.contains(.a))
    }

    func testFightingGameCommand_multiButtonSpecial() {
        // Simulate pressing A+B+C simultaneously (common for supers)
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)
        let touchC = MockTouch(id: 3)

        // All three pressed almost simultaneously
        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)
        buttonC.handleTouchBegan(touchC)

        XCTAssertEqual(pressedButtons.count, 3)
        XCTAssertTrue(buttonA.isPressed)
        XCTAssertTrue(buttonB.isPressed)
        XCTAssertTrue(buttonC.isPressed)

        // Release all
        buttonA.handleTouchEnded(touchA)
        buttonB.handleTouchEnded(touchB)
        buttonC.handleTouchEnded(touchC)

        XCTAssertFalse(buttonA.isPressed)
        XCTAssertFalse(buttonB.isPressed)
        XCTAssertFalse(buttonC.isPressed)
    }

    // MARK: - Rapid Input Tests

    func testRapidAlternatingPress() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)

        // Rapid alternating presses (like mashing)
        for _ in 0..<5 {
            buttonA.handleTouchBegan(touchA)
            buttonB.handleTouchBegan(touchB)
            buttonA.handleTouchEnded(touchA)
            buttonB.handleTouchEnded(touchB)
        }

        // Should end with both released
        XCTAssertFalse(buttonA.isPressed)
        XCTAssertFalse(buttonB.isPressed)
    }

    // MARK: - Cancel All Tests

    func testCancelAll_multipleButtons() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)
        let touchC = MockTouch(id: 3)

        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)
        buttonC.handleTouchBegan(touchC)

        // Cancel all buttons (e.g., app goes to background)
        buttonA.cancelAllTouches()
        buttonB.cancelAllTouches()
        buttonC.cancelAllTouches()

        XCTAssertFalse(buttonA.isPressed)
        XCTAssertFalse(buttonB.isPressed)
        XCTAssertFalse(buttonC.isPressed)

        XCTAssertTrue(releasedButtons.contains(.a))
        XCTAssertTrue(releasedButtons.contains(.b))
        XCTAssertTrue(releasedButtons.contains(.c))
    }

    // MARK: - Touch Tracking Isolation Tests

    func testTouchIsolation_touchOnlyAffectsItsButton() {
        let touchA = MockTouch(id: 1)

        // Touch A
        buttonA.handleTouchBegan(touchA)

        // Check that B and C are not affected
        XCTAssertFalse(buttonB.isPressed)
        XCTAssertFalse(buttonC.isPressed)
        XCTAssertFalse(buttonB.isTrackingTouch(touchA))
        XCTAssertFalse(buttonC.isTrackingTouch(touchA))
    }

    func testTouchIsolation_endingWrongTouchNoEffect() {
        let touchA = MockTouch(id: 1)
        let touchB = MockTouch(id: 2)

        buttonA.handleTouchBegan(touchA)
        buttonB.handleTouchBegan(touchB)

        // Try to end touchB on buttonA (wrong button)
        buttonA.handleTouchEnded(touchB)

        // A should still be pressed (touchB wasn't tracked by A)
        XCTAssertTrue(buttonA.isPressed)
    }
}
