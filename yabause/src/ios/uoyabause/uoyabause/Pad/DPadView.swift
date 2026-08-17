//
//  DPadView.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// Circular D-PAD with 8-direction detection
class DPadView: UIView {

    // MARK: - Properties

    /// Touch state for tracking active touch
    private let touchState = TouchState()

    /// Current D-PAD state
    private(set) var currentState = DPadState.neutral

    /// Haptic feedback generator
    private let hapticGenerator = UIImpactFeedbackGenerator(style: .light)

    /// Dead zone ratio (relative to radius)
    var deadZoneRatio: CGFloat = 0.12 {
        didSet {
            deadZoneRatio = max(0.1, min(0.15, deadZoneRatio))
        }
    }

    /// Center point of the D-PAD
    var centerPoint: CGPoint {
        return CGPoint(x: bounds.midX, y: bounds.midY)
    }

    /// Radius of the D-PAD
    var radius: CGFloat {
        return min(bounds.width, bounds.height) / 2
    }

    /// Background image
    var backgroundImage: UIImage? {
        didSet {
            backgroundImageView.image = backgroundImage
        }
    }

    /// Direction indicator view
    private lazy var directionIndicator: UIView = {
        let v = UIView()
        v.backgroundColor = UIColor.white.withAlphaComponent(0.7)
        v.layer.cornerRadius = 8
        v.isHidden = true
        return v
    }()

    /// Background image view
    private lazy var backgroundImageView: UIImageView = {
        let iv = UIImageView()
        iv.contentMode = .scaleAspectFit
        iv.isUserInteractionEnabled = false
        return iv
    }()

    /// Callback when state changes
    /// Parameters: (oldState, newState)
    var onStateChange: ((DPadState, DPadState) -> Void)?

    // MARK: - Initialization

    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setup()
    }

    private func setup() {
        isMultipleTouchEnabled = false  // D-PAD uses first touch only
        backgroundColor = UIColor.black.withAlphaComponent(0.3)

        addSubview(backgroundImageView)
        addSubview(directionIndicator)

        hapticGenerator.prepare()

        // Make circular
        layer.cornerRadius = bounds.width / 2
        clipsToBounds = true
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        backgroundImageView.frame = bounds
        layer.cornerRadius = min(bounds.width, bounds.height) / 2

        // Position direction indicator at center
        let indicatorSize: CGFloat = 16
        directionIndicator.frame = CGRect(
            x: (bounds.width - indicatorSize) / 2,
            y: (bounds.height - indicatorSize) / 2,
            width: indicatorSize,
            height: indicatorSize
        )
        directionIndicator.layer.cornerRadius = indicatorSize / 2
    }

    // MARK: - State Calculation

    /// Calculate D-PAD state for a touch point
    /// - Parameter point: Point in local coordinates
    /// - Returns: DPadState with appropriate directions
    func getState(for point: CGPoint) -> DPadState {
        let dx = point.x - centerPoint.x
        let dy = point.y - centerPoint.y
        let distance = hypot(dx, dy)

        // Dead zone check
        if distance < radius * deadZoneRatio {
            return .neutral
        }

        // Clamp to radius if outside
        if distance > radius {
            return .neutral
        }

        // Calculate angle (0 = right, counter-clockwise in standard math)
        // But screen Y is inverted, so we use (point.y - center.y) directly
        let angle = atan2(dy, dx)

        return DPadState.from(angle: Double(angle))
    }

    // MARK: - Touch Handling

    /// Handle touch for D-PAD
    /// - Parameters:
    ///   - touch: The touch
    ///   - phase: Touch phase
    func handleTouch(_ touch: UITouch, phase: UITouch.Phase) {
        let location = touch.location(in: self)

        switch phase {
        case .began:
            // Only accept first touch
            guard !touchState.isPressed else { return }
            touchState.addTouch(touch)
            updateState(for: location)

        case .moved:
            guard touchState.containsTouch(touch) else { return }
            updateState(for: location)

        case .ended, .cancelled:
            guard touchState.containsTouch(touch) else { return }
            touchState.removeTouch(touch)
            updateState(to: .neutral)

        default:
            break
        }
    }

    private func updateState(for location: CGPoint) {
        let newState = getState(for: location)
        updateState(to: newState)
    }

    private func updateState(to newState: DPadState) {
        guard newState != currentState else { return }

        let oldState = currentState
        currentState = newState

        // Haptic feedback on every direction change (including neutral)
        hapticGenerator.impactOccurred()

        // Update visual indicator
        updateIndicator()

        // Notify delegate
        onStateChange?(oldState, newState)
    }

    private func updateIndicator() {
        if currentState.isActive {
            directionIndicator.isHidden = false

            // Calculate indicator position based on active direction
            var dx: CGFloat = 0
            var dy: CGFloat = 0

            if currentState.right { dx += 1 }
            if currentState.left { dx -= 1 }
            if currentState.down { dy += 1 }
            if currentState.up { dy -= 1 }

            // Normalize
            let length = hypot(dx, dy)
            if length > 0 {
                dx /= length
                dy /= length
            }

            let offset = radius * 0.5
            let indicatorSize = directionIndicator.bounds.width
            directionIndicator.center = CGPoint(
                x: centerPoint.x + dx * offset,
                y: centerPoint.y + dy * offset
            )
        } else {
            directionIndicator.isHidden = true
            directionIndicator.center = centerPoint
        }
    }

    /// Cancel any active touch
    func cancelTouch() {
        if touchState.isPressed {
            touchState.clearAllTouches()
            updateState(to: .neutral)
        }
    }

    /// Check if D-PAD is tracking a specific touch
    func isTrackingTouch(_ touch: UITouch) -> Bool {
        return touchState.containsTouch(touch)
    }
}
