//
//  PadButtonView.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// Enhanced button view with multi-touch support
/// This replaces the basic PadButton class for advanced touch handling
class PadButtonView: UIView {

    // MARK: - Properties

    /// The button type this view represents
    let button: PadButtons

    /// Touch state for multi-touch tracking
    private let touchState = TouchState()

    /// Haptic feedback generator
    private let hapticGenerator = UIImpactFeedbackGenerator(style: .light)

    /// Normal state image
    var normalImage: UIImage? {
        didSet {
            updateAppearance()
        }
    }

    /// Pressed state image
    var pressedImage: UIImage? {
        didSet {
            updateAppearance()
        }
    }

    /// Image view for button appearance
    private lazy var imageView: UIImageView = {
        let iv = UIImageView()
        iv.contentMode = .scaleAspectFit
        iv.isUserInteractionEnabled = false
        return iv
    }()

    /// Label for button name (fallback when no image)
    private lazy var label: UILabel = {
        let l = UILabel()
        l.textAlignment = .center
        l.font = UIFont.boldSystemFont(ofSize: 14)
        l.textColor = .white
        l.isUserInteractionEnabled = false
        return l
    }()

    /// Whether the button is currently pressed
    var isPressed: Bool {
        return touchState.isPressed
    }

    /// Callback when button is pressed
    var onPress: ((PadButtons) -> Void)?

    /// Callback when button is released
    var onRelease: ((PadButtons) -> Void)?

    // MARK: - Initialization

    init(button: PadButtons, frame: CGRect = .zero) {
        self.button = button
        super.init(frame: frame)
        setup()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setup() {
        isMultipleTouchEnabled = true
        backgroundColor = UIColor.black.withAlphaComponent(0.0)
        layer.cornerRadius = 8
        clipsToBounds = true

        // Add subviews
        addSubview(imageView)
        addSubview(label)

        // Set label text
        label.text = "" //button.displayName

        // Prepare haptic
        hapticGenerator.prepare()

        updateAppearance()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        imageView.frame = bounds
        label.frame = bounds
    }

    // MARK: - Appearance

    private func updateAppearance() {
        if isPressed {
            imageView.image = pressedImage ?? normalImage
            backgroundColor = UIColor.white.withAlphaComponent(0.5)
        } else {
            imageView.image = normalImage
            backgroundColor = UIColor.black.withAlphaComponent(0.0)
        }

        // Show label only if no image
        label.isHidden = (normalImage != nil)
    }

    // MARK: - Touch Handling

    /// Handle touch began for this button
    /// - Parameter touch: The touch that began
    func handleTouchBegan(_ touch: UITouch) {
        let justPressed = touchState.addTouch(touch)
        if justPressed {
            hapticGenerator.impactOccurred()
            onPress?(button)
            updateAppearance()
        }
    }

    /// Handle touch ended for this button
    /// - Parameter touch: The touch that ended
    func handleTouchEnded(_ touch: UITouch) {
        guard touchState.containsTouch(touch) else { return }
        let justReleased = touchState.removeTouch(touch)
        if justReleased {
            onRelease?(button)
            updateAppearance()
        }
    }

    /// Cancel all active touches
    func cancelAllTouches() {
        let wasPressed = isPressed
        touchState.clearAllTouches()
        if wasPressed {
            onRelease?(button)
            updateAppearance()
        }
    }

    /// Check if a point is within this button's bounds
    /// - Parameter point: Point in superview coordinates
    /// - Returns: True if point is within bounds
    func containsPoint(_ point: CGPoint) -> Bool {
        return frame.contains(point)
    }

    /// Check if this button is tracking a specific touch
    /// - Parameter touch: The touch to check
    /// - Returns: True if tracking this touch
    func isTrackingTouch(_ touch: UITouch) -> Bool {
        return touchState.containsTouch(touch)
    }
}

// MARK: - ActionButton (A/B/C, X/Y/Z buttons)

/// Specialized button for action buttons with circular appearance
class ActionButton: PadButtonView {

    override init(button: PadButtons, frame: CGRect = .zero) {
        super.init(button: button, frame: frame)
        setupActionButtonAppearance()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupActionButtonAppearance() {
        // Make circular
        layer.cornerRadius = bounds.width / 2
        clipsToBounds = true
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        layer.cornerRadius = min(bounds.width, bounds.height) / 2
    }
}

// MARK: - TriggerButton (L/R buttons)

/// Specialized button for trigger buttons with rounded rectangle appearance
class TriggerButton: PadButtonView {

    override init(button: PadButtons, frame: CGRect = .zero) {
        super.init(button: button, frame: frame)
        setupTriggerAppearance()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupTriggerAppearance() {
        layer.cornerRadius = 12
    }
}
