//
//  AnalogStickView.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// Protocol for analog stick input events
protocol AnalogStickDelegate: AnyObject {
    /// Called when analog stick position changes
    /// - Parameters:
    ///   - x: X axis value (0-255, 128 = center)
    ///   - y: Y axis value (0-255, 128 = center)
    func analogStickChanged(x: UInt8, y: UInt8)
}

/// On-screen analog stick control
/// Provides continuous analog input with visual feedback
class AnalogStickView: UIView {

    // MARK: - Properties

    weak var delegate: AnalogStickDelegate?

    /// Maximum distance the stick can move from center (in points)
    var maxDistance: CGFloat = 55

    /// Size of the movable stick circle
    var stickRadius: CGFloat = 40

    /// Current stick position relative to center (0,0 = center)
    private(set) var stickOffset: CGPoint = .zero {
        didSet {
            // stickOffsetが更新されたら即座に再描画
            setNeedsDisplay()
        }
    }

    // MARK: - Private Properties

    private var isDragging = false
    private var isAtMaxDistance = false
    private let feedbackGenerator = UIImpactFeedbackGenerator(style: .medium)

    /// タッチ開始時の位置（動的な中心点として使用）
    private var dynamicCenter: CGPoint? {
        didSet {
            // dynamicCenterが更新されたら即座に再描画
            setNeedsDisplay()
        }
    }

    // MARK: - Computed Properties

    /// Default center point of the view
    private var defaultCenterPoint: CGPoint {
        return CGPoint(x: bounds.midX, y: bounds.midY)
    }

    /// Active center point (dynamic when touched, default otherwise)
    private var activeCenterPoint: CGPoint {
        return dynamicCenter ?? defaultCenterPoint
    }

    /// Current stick center position
    private var stickCenter: CGPoint {
        return CGPoint(x: activeCenterPoint.x + stickOffset.x, y: activeCenterPoint.y + stickOffset.y)
    }

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
        backgroundColor = .clear
        isMultipleTouchEnabled = false
        feedbackGenerator.prepare()
    }

    // MARK: - Drawing

    override func draw(_ rect: CGRect) {
        guard let context = UIGraphicsGetCurrentContext() else { return }

        // Draw movable range circle (outline only)
        let rangeColor = UIColor.white.withAlphaComponent(0.3).cgColor
        context.setStrokeColor(rangeColor)
        context.setLineWidth(2)
        let rangeRect = CGRect(
            x: activeCenterPoint.x - maxDistance,
            y: activeCenterPoint.y - maxDistance,
            width: maxDistance * 2,
            height: maxDistance * 2
        )
        context.strokeEllipse(in: rangeRect)

        // Draw center point indicator
        let centerIndicatorSize: CGFloat = 8
        context.setFillColor(UIColor.white.withAlphaComponent(0.4).cgColor)
        let centerRect = CGRect(
            x: activeCenterPoint.x - centerIndicatorSize / 2,
            y: activeCenterPoint.y - centerIndicatorSize / 2,
            width: centerIndicatorSize,
            height: centerIndicatorSize
        )
        context.fillEllipse(in: centerRect)

        // Draw stick circle
        let stickColor = UIColor.white.withAlphaComponent(0.8).cgColor
        context.setFillColor(stickColor)

        let stickRect = CGRect(
            x: stickCenter.x - stickRadius,
            y: stickCenter.y - stickRadius,
            width: stickRadius * 2,
            height: stickRadius * 2
        )
        context.fillEllipse(in: stickRect)

        // Draw stick border
        context.setStrokeColor(UIColor.white.cgColor)
        context.setLineWidth(2)
        context.strokeEllipse(in: stickRect)
    }

    // MARK: - Touch Handling

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first else { return }
        let touchLocation = touch.location(in: self)
        let distanceFromStick = hypot(touchLocation.x - stickCenter.x, touchLocation.y - stickCenter.y)

        // Only start dragging if touch is on the stick
        if distanceFromStick <= maxDistance + 10 {
            triggerHapticFeedback()
            isDragging = true
            // タッチ位置を新しい中心点として設定（指が触れた瞬間に反映）
            // Note: dynamicCenterのdidSetで自動的に再描画される
            dynamicCenter = touchLocation
            stickOffset = .zero  // 中心からスタート
            sendAnalogValue()  // (128, 128) を送信
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard isDragging, let origin = dynamicCenter, let touch = touches.first else { return }

        // 現在のタッチ位置から動的中心点へのオフセットを計算
        let currentLocation = touch.location(in: self)
        var newOffset = CGPoint(
            x: currentLocation.x - origin.x,
            y: currentLocation.y - origin.y
        )

        // Clamp to max distance
        let distance = hypot(newOffset.x, newOffset.y)
        if distance > maxDistance {
            let angle = atan2(newOffset.y, newOffset.x)
            newOffset = CGPoint(
                x: maxDistance * cos(angle),
                y: maxDistance * sin(angle)
            )

            // Haptic feedback when reaching max distance
            if !isAtMaxDistance {
                triggerHapticFeedback()
                isAtMaxDistance = true
            }
        } else {
            isAtMaxDistance = false
        }

        // Note: stickOffsetのdidSetで自動的に再描画される
        stickOffset = newOffset
        sendAnalogValue()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard isDragging else { return }

        isDragging = false
        isAtMaxDistance = false

        // 動的中心点とスティック位置をリセット
        // Note: dynamicCenterのdidSetで自動的に再描画される
        dynamicCenter = nil
        stickOffset = .zero

        // Send center position
        sendAnalogValue()
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        touchesEnded(touches, with: event)
    }

    // MARK: - Analog Value Calculation

    private func sendAnalogValue() {
        // Convert offset to 0-255 range (128 = center)
        let normalizedX = (stickOffset.x / maxDistance) * 128.0 + 128.0
        let normalizedY = (stickOffset.y / maxDistance) * 128.0 + 128.0

        let x = UInt8(clamping: Int(normalizedX))
        let y = UInt8(clamping: Int(normalizedY))

        delegate?.analogStickChanged(x: x, y: y)
    }

    // MARK: - Haptic Feedback

    private func triggerHapticFeedback() {
        switch UIDevice.current.feedbackSupportLevel {
        case .feedbackGenerator:
            feedbackGenerator.impactOccurred()
        case .basic, .unsupported:
            UIDevice.current.vibrate()
        }
    }

    // MARK: - Public Methods

    /// Reset stick to center position
    func reset() {
        // Note: dynamicCenterのdidSetで自動的に再描画される
        dynamicCenter = nil
        stickOffset = .zero
        isDragging = false
        isAtMaxDistance = false
        sendAnalogValue()
    }
}
