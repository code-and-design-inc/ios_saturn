//
//  TouchStateTests.swift
//  uoyabauseTests
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import XCTest
@testable import YabaSnashiro

/// Mock UITouch for testing purposes
/// Note: UITouch cannot be instantiated directly, so we use a subclass
class MockTouch: UITouch {
    private let touchId: Int

    init(id: Int) {
        self.touchId = id
        super.init()
    }

    override var hash: Int {
        return touchId
    }

    override func isEqual(_ object: Any?) -> Bool {
        guard let other = object as? MockTouch else { return false }
        return touchId == other.touchId
    }
}

final class TouchStateTests: XCTestCase {

    var touchState: TouchState!

    override func setUp() {
        super.setUp()
        touchState = TouchState()
    }

    override func tearDown() {
        touchState = nil
        super.tearDown()
    }

    // MARK: - Initial State Tests

    func testInitialState_isNotPressed() {
        XCTAssertFalse(touchState.isPressed, "Initial state should not be pressed")
    }

    func testInitialState_touchCountIsZero() {
        XCTAssertEqual(touchState.touchCount, 0, "Initial touch count should be 0")
    }

    func testInitialState_allTouchesIsEmpty() {
        XCTAssertTrue(touchState.allTouches.isEmpty, "Initial touches set should be empty")
    }

    // MARK: - Add Touch Tests

    func testAddTouch_firstTouchReturnsTrue() {
        let touch = MockTouch(id: 1)
        let wasFirst = touchState.addTouch(touch)
        XCTAssertTrue(wasFirst, "First touch should return true")
    }

    func testAddTouch_secondTouchReturnsFalse() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        touchState.addTouch(touch1)
        let wasFirst = touchState.addTouch(touch2)

        XCTAssertFalse(wasFirst, "Second touch should return false")
    }

    func testAddTouch_incrementsTouchCount() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        touchState.addTouch(touch1)
        XCTAssertEqual(touchState.touchCount, 1)

        touchState.addTouch(touch2)
        XCTAssertEqual(touchState.touchCount, 2)
    }

    func testAddTouch_setsIsPressed() {
        let touch = MockTouch(id: 1)
        touchState.addTouch(touch)
        XCTAssertTrue(touchState.isPressed, "Should be pressed after adding touch")
    }

    func testAddTouch_sameTouchTwiceDoesNotDuplicate() {
        let touch = MockTouch(id: 1)

        touchState.addTouch(touch)
        touchState.addTouch(touch)

        XCTAssertEqual(touchState.touchCount, 1, "Same touch should not be duplicated")
    }

    // MARK: - Remove Touch Tests

    func testRemoveTouch_lastTouchReturnsTrue() {
        let touch = MockTouch(id: 1)
        touchState.addTouch(touch)

        let wasLast = touchState.removeTouch(touch)
        XCTAssertTrue(wasLast, "Removing last touch should return true")
    }

    func testRemoveTouch_notLastTouchReturnsFalse() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        touchState.addTouch(touch1)
        touchState.addTouch(touch2)

        let wasLast = touchState.removeTouch(touch1)
        XCTAssertFalse(wasLast, "Removing non-last touch should return false")
    }

    func testRemoveTouch_decrementsTouchCount() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        touchState.addTouch(touch1)
        touchState.addTouch(touch2)
        XCTAssertEqual(touchState.touchCount, 2)

        touchState.removeTouch(touch1)
        XCTAssertEqual(touchState.touchCount, 1)
    }

    func testRemoveTouch_allTouchesRemoved_isNotPressed() {
        let touch = MockTouch(id: 1)
        touchState.addTouch(touch)
        touchState.removeTouch(touch)

        XCTAssertFalse(touchState.isPressed, "Should not be pressed after all touches removed")
    }

    func testRemoveTouch_nonexistentTouchReturnsTrue() {
        let touch = MockTouch(id: 1)
        let wasLast = touchState.removeTouch(touch)
        XCTAssertTrue(wasLast, "Removing from empty set should return true (isEmpty)")
    }

    // MARK: - Clear All Touches Tests

    func testClearAllTouches_removesAllTouches() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)
        let touch3 = MockTouch(id: 3)

        touchState.addTouch(touch1)
        touchState.addTouch(touch2)
        touchState.addTouch(touch3)

        touchState.clearAllTouches()

        XCTAssertEqual(touchState.touchCount, 0)
        XCTAssertFalse(touchState.isPressed)
        XCTAssertTrue(touchState.allTouches.isEmpty)
    }

    func testClearAllTouches_onEmptyState_noError() {
        // Should not crash or error
        touchState.clearAllTouches()
        XCTAssertEqual(touchState.touchCount, 0)
    }

    // MARK: - Contains Touch Tests

    func testContainsTouch_addedTouchReturnsTrue() {
        let touch = MockTouch(id: 1)
        touchState.addTouch(touch)

        XCTAssertTrue(touchState.containsTouch(touch))
    }

    func testContainsTouch_notAddedTouchReturnsFalse() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        touchState.addTouch(touch1)

        XCTAssertFalse(touchState.containsTouch(touch2))
    }

    func testContainsTouch_removedTouchReturnsFalse() {
        let touch = MockTouch(id: 1)
        touchState.addTouch(touch)
        touchState.removeTouch(touch)

        XCTAssertFalse(touchState.containsTouch(touch))
    }

    // MARK: - All Touches Tests

    func testAllTouches_returnsAllAddedTouches() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)

        touchState.addTouch(touch1)
        touchState.addTouch(touch2)

        let allTouches = touchState.allTouches
        XCTAssertEqual(allTouches.count, 2)
        XCTAssertTrue(allTouches.contains(touch1))
        XCTAssertTrue(allTouches.contains(touch2))
    }

    // MARK: - Thread Safety Tests

    func testThreadSafety_concurrentAddAndRemove() {
        let expectation = XCTestExpectation(description: "Concurrent operations complete")
        expectation.expectedFulfillmentCount = 100

        let queue = DispatchQueue(label: "test.concurrent", attributes: .concurrent)

        for i in 0..<100 {
            queue.async {
                let touch = MockTouch(id: i)
                self.touchState.addTouch(touch)
                _ = self.touchState.isPressed
                _ = self.touchState.touchCount
                self.touchState.removeTouch(touch)
                expectation.fulfill()
            }
        }

        wait(for: [expectation], timeout: 5.0)

        // After all operations, should be empty
        XCTAssertFalse(touchState.isPressed)
        XCTAssertEqual(touchState.touchCount, 0)
    }

    // MARK: - Multi-Touch Scenario Tests

    func testMultiTouchScenario_threeFingerPress() {
        let touch1 = MockTouch(id: 1)
        let touch2 = MockTouch(id: 2)
        let touch3 = MockTouch(id: 3)

        // Simulate three finger press
        XCTAssertTrue(touchState.addTouch(touch1), "First finger should trigger press")
        XCTAssertFalse(touchState.addTouch(touch2), "Second finger should not trigger press")
        XCTAssertFalse(touchState.addTouch(touch3), "Third finger should not trigger press")

        XCTAssertEqual(touchState.touchCount, 3)
        XCTAssertTrue(touchState.isPressed)

        // Remove middle finger - should not release
        XCTAssertFalse(touchState.removeTouch(touch2), "Removing middle finger should not release")
        XCTAssertTrue(touchState.isPressed)

        // Remove remaining fingers
        XCTAssertFalse(touchState.removeTouch(touch1), "Still one finger remaining")
        XCTAssertTrue(touchState.removeTouch(touch3), "Last finger should trigger release")

        XCTAssertFalse(touchState.isPressed)
    }
}
