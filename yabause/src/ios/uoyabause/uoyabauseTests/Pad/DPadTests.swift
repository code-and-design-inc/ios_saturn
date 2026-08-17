//
//  DPadTests.swift
//  uoyabauseTests
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import XCTest
@testable import YabaSnashiro

final class DPadTests: XCTestCase {

    var dpadView: DPadView!

    override func setUp() {
        super.setUp()
        // Create a 100x100 D-PAD centered at (50, 50)
        dpadView = DPadView(frame: CGRect(x: 0, y: 0, width: 100, height: 100))
        dpadView.layoutSubviews()
    }

    override func tearDown() {
        dpadView = nil
        super.tearDown()
    }

    // MARK: - Dead Zone Tests

    func testDeadZone_centerPoint_returnsNeutral() {
        let center = CGPoint(x: 50, y: 50)
        let state = dpadView.getState(for: center)

        XCTAssertEqual(state, .neutral)
        XCTAssertFalse(state.isActive)
    }

    func testDeadZone_veryCloseToCenter_returnsNeutral() {
        // Default dead zone is 12% of radius (50), so within 6 pixels should be neutral
        let nearCenter = CGPoint(x: 53, y: 50)  // 3 pixels from center
        let state = dpadView.getState(for: nearCenter)

        XCTAssertEqual(state, .neutral)
    }

    func testDeadZone_justOutsideDeadZone_returnsDirection() {
        // Just outside dead zone (12% of 50 = 6), so 10 pixels should detect
        let point = CGPoint(x: 60, y: 50)  // 10 pixels right
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.right)
        XCTAssertFalse(state.left)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.down)
    }

    func testDeadZone_outsideRadius_returnsNeutral() {
        // Outside the D-PAD completely
        let outside = CGPoint(x: 110, y: 50)  // 60 pixels from center (radius is 50)
        let state = dpadView.getState(for: outside)

        XCTAssertEqual(state, .neutral)
    }

    // MARK: - Cardinal Direction Tests

    func testDirection_right() {
        let point = CGPoint(x: 80, y: 50)  // Pure right
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.right)
        XCTAssertFalse(state.left)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.down)
    }

    func testDirection_left() {
        let point = CGPoint(x: 20, y: 50)  // Pure left
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.left)
        XCTAssertFalse(state.right)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.down)
    }

    func testDirection_up() {
        // Note: In screen coordinates, Y increases downward
        // So "up" on D-PAD is actually lower Y value
        let point = CGPoint(x: 50, y: 20)  // Up (lower Y in screen coords)
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.up)
        XCTAssertFalse(state.down)
        XCTAssertFalse(state.left)
        XCTAssertFalse(state.right)
    }

    func testDirection_down() {
        let point = CGPoint(x: 50, y: 80)  // Down (higher Y in screen coords)
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.down)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.left)
        XCTAssertFalse(state.right)
    }

    // MARK: - Diagonal Direction Tests

    func testDirection_upRight() {
        let point = CGPoint(x: 80, y: 20)  // Upper right
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.up)
        XCTAssertTrue(state.right)
        XCTAssertFalse(state.down)
        XCTAssertFalse(state.left)
    }

    func testDirection_upLeft() {
        let point = CGPoint(x: 20, y: 20)  // Upper left
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.up)
        XCTAssertTrue(state.left)
        XCTAssertFalse(state.down)
        XCTAssertFalse(state.right)
    }

    func testDirection_downRight() {
        let point = CGPoint(x: 80, y: 80)  // Lower right
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.down)
        XCTAssertTrue(state.right)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.left)
    }

    func testDirection_downLeft() {
        let point = CGPoint(x: 20, y: 80)  // Lower left
        let state = dpadView.getState(for: point)

        XCTAssertTrue(state.down)
        XCTAssertTrue(state.left)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.right)
    }

    // MARK: - Boundary Angle Tests (22.5 degree sectors)

    func testBoundaryAngle_rightToUpRight() {
        // Boundary between Right and Up-Right is at -22.5 degrees (337.5 degrees)
        // At 100x100 D-PAD, that's approximately:
        // x = 50 + 30 * cos(-22.5°) ≈ 77.7
        // y = 50 + 30 * sin(-22.5°) ≈ 38.5

        // Just before boundary (should be pure right)
        let justRight = CGPoint(x: 78, y: 46)
        let stateRight = dpadView.getState(for: justRight)
        XCTAssertTrue(stateRight.right, "Should be right")

        // Just after boundary (should be up-right)
        let justUpRight = CGPoint(x: 75, y: 35)
        let stateUpRight = dpadView.getState(for: justUpRight)
        XCTAssertTrue(stateUpRight.up && stateUpRight.right, "Should be up-right")
    }

    // MARK: - Active Buttons Tests

    func testActiveButtons_neutral() {
        let state = DPadState.neutral
        XCTAssertTrue(state.activeButtons.isEmpty)
    }

    func testActiveButtons_singleDirection() {
        var state = DPadState()
        state.up = true
        XCTAssertEqual(state.activeButtons, [.up])
    }

    func testActiveButtons_diagonal() {
        var state = DPadState()
        state.up = true
        state.right = true
        let buttons = state.activeButtons
        XCTAssertEqual(buttons.count, 2)
        XCTAssertTrue(buttons.contains(.up))
        XCTAssertTrue(buttons.contains(.right))
    }

    // MARK: - DPadState.from(angle:) Tests

    func testFromAngle_rightDirection() {
        // Angle 0 = Right
        let state = DPadState.from(angle: 0)
        XCTAssertTrue(state.right)
        XCTAssertFalse(state.left)
        XCTAssertFalse(state.up)
        XCTAssertFalse(state.down)
    }

    func testFromAngle_leftDirection() {
        // Angle π = Left
        let state = DPadState.from(angle: .pi)
        XCTAssertTrue(state.left)
        XCTAssertFalse(state.right)
    }

    func testFromAngle_downDirection() {
        // Angle π/2 = Down (in screen coordinates)
        let state = DPadState.from(angle: .pi / 2)
        XCTAssertTrue(state.down)
        XCTAssertFalse(state.up)
    }

    func testFromAngle_upDirection() {
        // Angle -π/2 = Up (in screen coordinates)
        let state = DPadState.from(angle: -.pi / 2)
        XCTAssertTrue(state.up)
        XCTAssertFalse(state.down)
    }

    func testFromAngle_upRightDirection() {
        // Angle -π/4 = Up-Right
        let state = DPadState.from(angle: -.pi / 4)
        XCTAssertTrue(state.up)
        XCTAssertTrue(state.right)
    }

    func testFromAngle_downRightDirection() {
        // Angle π/4 = Down-Right
        let state = DPadState.from(angle: .pi / 4)
        XCTAssertTrue(state.down)
        XCTAssertTrue(state.right)
    }

    func testFromAngle_downLeftDirection() {
        // Angle 3π/4 = Down-Left
        let state = DPadState.from(angle: 3 * .pi / 4)
        XCTAssertTrue(state.down)
        XCTAssertTrue(state.left)
    }

    func testFromAngle_upLeftDirection() {
        // Angle -3π/4 = Up-Left
        let state = DPadState.from(angle: -3 * .pi / 4)
        XCTAssertTrue(state.up)
        XCTAssertTrue(state.left)
    }

    // MARK: - Negative Angle Normalization Tests

    func testFromAngle_negativeAnglesNormalized() {
        // Both should produce same result
        let statePositive = DPadState.from(angle: .pi)
        let stateNegative = DPadState.from(angle: -.pi)

        // Both represent left direction
        XCTAssertEqual(statePositive.left, stateNegative.left)
    }

    // MARK: - Properties Tests

    func testCenterPoint() {
        XCTAssertEqual(dpadView.centerPoint.x, 50)
        XCTAssertEqual(dpadView.centerPoint.y, 50)
    }

    func testRadius() {
        XCTAssertEqual(dpadView.radius, 50)
    }

    func testDeadZoneRatio_clampedToValidRange() {
        dpadView.deadZoneRatio = 0.05  // Below min
        XCTAssertGreaterThanOrEqual(dpadView.deadZoneRatio, 0.1)

        dpadView.deadZoneRatio = 0.3   // Above max
        XCTAssertLessThanOrEqual(dpadView.deadZoneRatio, 0.15)
    }
}
