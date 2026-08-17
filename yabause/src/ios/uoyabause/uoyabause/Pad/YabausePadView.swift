//
//  YabausePadView.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// Main container view for the on-screen pad
/// Uses bitmap backgrounds with touch areas positioned to match Android layout
class YabausePadView: UIView, AnalogStickDelegate {

    // MARK: - Properties

    /// Delegate for pad input events
    weak var delegate: YabausePadDelegate?

    /// Current configuration
    private(set) var configuration: PadConfiguration

    /// D-PAD view (invisible touch area)
    private(set) var dpad: DPadView!

    /// Analog stick view (shown in analog mode)
    private(set) var analogStick: AnalogStickView!

    /// Button views mapped by button type (invisible touch areas)
    private(set) var buttons: [PadButtons: PadButtonView] = [:]

    /// Previous D-PAD state for change detection
    private var previousDPadState = DPadState.neutral

    /// Whether analog mode is enabled
    /// When true, shows analog stick instead of D-PAD
    var isAnalogMode: Bool = false {
        didSet {
            updateAnalogMode()
        }
    }

    /// Whether the pad is in edit mode
    var isEditing: Bool = false {
        didSet {
            updateEditMode()
        }
    }

    /// Configuration delegate for edit mode
    weak var configurationDelegate: PadConfigurationDelegate?

    // MARK: - Bitmap Image Views

    /// Left pad background (D-PAD area)
    private let padLeftImageView = UIImageView()

    /// Right pad background (Action buttons area)
    private let padRightImageView = UIImageView()

    /// Middle pad background (Start button)
    private let padMiddleImageView = UIImageView()

    /// Top left pad background (L trigger)
    private let padTopLeftImageView = UIImageView()

    /// Top right pad background (R trigger)
    private let padTopRightImageView = UIImageView()

    // MARK: - Layout Constants (from Android coordinates)

    /// Scale factor for the pad images (from configuration)
    private var padScale: CGFloat {
        return CGFloat(configuration.padScale)
    }

    /// Current scale factor (accounts for portrait mode)
    var currentScale: CGFloat {
        return bounds.width > bounds.height ? padScale : padScale * 0.8
    }

    /// Button positions relative to pad_r bitmap (from Android)
    private struct ButtonCoords {
        static let a = CGRect(x: 59, y: 801, width: 213, height: 225)
        static let b = CGRect(x: 268, y: 672, width: 229, height: 221)
        static let c = CGRect(x: 507, y: 577, width: 224, height: 229)
        static let x = CGRect(x: 15, y: 602, width: 149, height: 150)
        static let y = CGRect(x: 202, y: 481, width: 149, height: 148)
        static let z = CGRect(x: 397, y: 409, width: 151, height: 152)
    }

    /// Thumb touch radius for hit detection (scaled by current pad scale)
    private var thumbTouchRadius: CGFloat {
        return CGFloat(configuration.thumbTouchRadius) * currentScale
    }

    /// Debug: Show touch radius visualization
    private let showTouchDebug = false
    private var touchDebugViews: [UITouch: UIView] = [:]

    /// D-PAD position relative to pad_l bitmap
    private struct DPadCoords {
        static let centerX: CGFloat = 347
        static let centerY: CGFloat = 720
        static let size: CGFloat = 460  // rectsize * 2
    }

    /// Trigger positions relative to their bitmaps
    private struct TriggerCoords {
        static let left = CGRect(x: 57, y: 48, width: 379, height: 100)
        static let right = CGRect(x: 338, y: 48, width: 379, height: 100)
    }

    /// Start button position relative to pad_m bitmap
    private struct StartCoords {
        static let rect = CGRect(x: 0, y: 53, width: 185, height: 79)
    }

    // MARK: - Initialization

    init(frame: CGRect, configuration: PadConfiguration = .default) {
        self.configuration = configuration
        super.init(frame: frame)
        setup()
    }

    required init?(coder: NSCoder) {
        self.configuration = .default
        super.init(coder: coder)
        setup()
    }

    private func setup() {
        isMultipleTouchEnabled = true
        backgroundColor = .clear
        isUserInteractionEnabled = true

        setupImageViews()
        setupDPad()
        setupAnalogStick()
        setupButtons()
        applyConfiguration()
    }

    // MARK: - Setup Image Views

    private func setupImageViews() {
        // Configure all image views
        let imageViews = [padLeftImageView, padRightImageView, padMiddleImageView, padTopLeftImageView, padTopRightImageView]
        for imageView in imageViews {
            imageView.contentMode = .scaleToFill
            imageView.isUserInteractionEnabled = false
            addSubview(imageView)
        }

        // Load images based on current theme
        updateThemeImages()
    }

    /// Update pad images based on current theme
    private func updateThemeImages() {
        let theme = configuration.theme
        padLeftImageView.image = theme.image(for: .left)
        padRightImageView.image = theme.image(for: .right)
        padMiddleImageView.image = theme.image(for: .middle)
        padTopLeftImageView.image = theme.image(for: .topLeft)
        padTopRightImageView.image = theme.image(for: .topRight)
    }

    // MARK: - Setup D-PAD

    private func setupDPad() {
        dpad = DPadView(frame: .zero)
        dpad.backgroundColor = .clear
        dpad.deadZoneRatio = CGFloat(configuration.dpadDeadZoneRatio)
        dpad.onStateChange = { [weak self] oldState, newState in
            self?.handleDPadStateChange(from: oldState, to: newState)
        }
        addSubview(dpad)
    }

    // MARK: - Setup Analog Stick

    private func setupAnalogStick() {
        analogStick = AnalogStickView(frame: .zero)
        analogStick.backgroundColor = .clear
        analogStick.delegate = self
        analogStick.isHidden = true  // Hidden by default, shown in analog mode
        addSubview(analogStick)
    }

    // MARK: - Analog Mode

    private func updateAnalogMode() {
        dpad.isHidden = isAnalogMode
        analogStick.isHidden = !isAnalogMode
        padLeftImageView.isHidden = isAnalogMode

        if isAnalogMode {
            // Cancel any active D-PAD touches when switching to analog mode
            dpad.cancelTouch()
        } else {
            // Reset analog stick when switching to digital mode
            analogStick.reset()
        }
    }

    // MARK: - AnalogStickDelegate

    func analogStickChanged(x: UInt8, y: UInt8) {
        delegate?.padView(self, didChangeAnalog: x, y: y)
    }

    // MARK: - Setup Buttons

    private func setupButtons() {
        // Create action buttons (A/B/C, X/Y/Z) - invisible touch areas
        for buttonType in PadButtons.actionButtons {
            let button = ActionButton(button: buttonType, frame: .zero)
            button.backgroundColor = .clear
            setupButtonCallbacks(button)
            buttons[buttonType] = button
            addSubview(button)
        }

        // Create trigger buttons (L/R)
        for buttonType in PadButtons.triggerButtons {
            let button = TriggerButton(button: buttonType, frame: .zero)
            button.backgroundColor = .clear
            setupButtonCallbacks(button)
            buttons[buttonType] = button
            addSubview(button)
        }

        // Create start button
        let startButton = PadButtonView(button: .start, frame: .zero)
        startButton.backgroundColor = .clear
        setupButtonCallbacks(startButton)
        buttons[.start] = startButton
        addSubview(startButton)
    }

    private func setupButtonCallbacks(_ button: PadButtonView) {
        button.onPress = { [weak self] buttonType in
            guard let self = self, !self.isEditing else { return }
            self.delegate?.padView(self, didPress: buttonType)
        }
        button.onRelease = { [weak self] buttonType in
            guard let self = self, !self.isEditing else { return }
            self.delegate?.padView(self, didRelease: buttonType)
        }
    }

    // MARK: - Layout

    override func layoutSubviews() {
        super.layoutSubviews()
        layoutPadImages()
        layoutTouchAreas()
        applyOpacity()
    }

    private func layoutPadImages() {
        let width = bounds.width
        let height = bounds.height
        let isLandscape = width > height

        // Calculate positions similar to Android
        if isLandscape {
            layoutLandscape()
        } else {
            layoutPortrait()
        }
    }

    private func layoutLandscape() {
        let width = bounds.width
        let height = bounds.height
        let safeArea = safeAreaInsets

        // Left pad - D-PAD area (bottom left, inside safe area)
        if let leftImage = padLeftImageView.image {
            let imageWidth = leftImage.size.width * padScale
            let imageHeight = leftImage.size.height * padScale
            let x = safeArea.left - 80 * padScale  // Slightly overlap the edge
            let y = height - imageHeight - safeArea.bottom
            padLeftImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Right pad - Action buttons (bottom right, inside safe area)
        if let rightImage = padRightImageView.image {
            let imageWidth = rightImage.size.width * padScale
            let imageHeight = rightImage.size.height * padScale
            let x = width - imageWidth - safeArea.right + 40 * padScale  // Slightly overlap the edge
            let y = height - imageHeight - safeArea.bottom
            padRightImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Middle pad - Start button (bottom center)
        if let middleImage = padMiddleImageView.image {
            let imageWidth = middleImage.size.width * padScale
            let imageHeight = middleImage.size.height * padScale
            let x = (width - imageWidth) / 2
            let y = height - imageHeight - safeArea.bottom
            padMiddleImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Top left - L trigger (inside safe area, below menu button)
        if let topLeftImage = padTopLeftImageView.image {
            let imageWidth = topLeftImage.size.width * padScale
            let imageHeight = topLeftImage.size.height * padScale
            let x = safeArea.left
            let y = safeArea.top + 50  // Offset to avoid menu button
            padTopLeftImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Top right - R trigger (inside safe area)
        if let topRightImage = padTopRightImageView.image {
            let imageWidth = topRightImage.size.width * padScale
            let imageHeight = topRightImage.size.height * padScale
            let x = width - imageWidth - safeArea.right
            let y = safeArea.top + 50
            padTopRightImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }
    }

    private func layoutPortrait() {
        let width = bounds.width
        let height = bounds.height
        let safeArea = safeAreaInsets

        // Adjust scale for portrait
        let portraitScale = padScale * 1.0

        // Left pad - D-PAD area
        if let leftImage = padLeftImageView.image {
            let imageWidth = leftImage.size.width * portraitScale
            let imageHeight = leftImage.size.height * portraitScale
            let x: CGFloat = -80 * portraitScale
            let y = height - imageHeight - 100 - safeArea.bottom
            padLeftImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Right pad - Action buttons
        if let rightImage = padRightImageView.image {
            let imageWidth = rightImage.size.width * portraitScale
            let imageHeight = rightImage.size.height * portraitScale
            let x = width - imageWidth + 40 * portraitScale
            let y = height - imageHeight - 100 - safeArea.bottom
            padRightImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Middle pad - Start button
        if let middleImage = padMiddleImageView.image {
            let imageWidth = middleImage.size.width * portraitScale
            let imageHeight = middleImage.size.height * portraitScale
            let x = (width - imageWidth) / 2
            let y = height - imageHeight - safeArea.bottom
            padMiddleImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Top left - L trigger
        if let topLeftImage = padTopLeftImageView.image {
            let imageWidth = topLeftImage.size.width * portraitScale
            let imageHeight = topLeftImage.size.height * portraitScale
            let x: CGFloat = -30 * portraitScale
            let y = height - imageHeight - 350 - safeArea.bottom
            padTopLeftImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }

        // Top right - R trigger
        if let topRightImage = padTopRightImageView.image {
            let imageWidth = topRightImage.size.width * portraitScale
            let imageHeight = topRightImage.size.height * portraitScale
            let x = width - imageWidth + 30 * portraitScale
            let y = height - imageHeight - 350 - safeArea.bottom
            padTopRightImageView.frame = CGRect(x: x, y: y, width: imageWidth, height: imageHeight)
        }
    }

    private func layoutTouchAreas() {
        let currentScale = bounds.width > bounds.height ? padScale : padScale * 1.0

        // D-PAD touch area relative to padLeftImageView (2x larger for easier input)
        let dpadCenterInImage = CGPoint(
            x: DPadCoords.centerX * currentScale,
            y: DPadCoords.centerY * currentScale
        )
        let dpadSizeScaled = DPadCoords.size * currentScale * 2.0  // 2x larger hit area
        dpad.frame = CGRect(
            x: padLeftImageView.frame.origin.x + dpadCenterInImage.x - dpadSizeScaled / 2,
            y: padLeftImageView.frame.origin.y + dpadCenterInImage.y - dpadSizeScaled / 2,
            width: dpadSizeScaled,
            height: dpadSizeScaled
        )

        // Analog stick positioned at D-PAD center (10x larger movable area)
        // Use a much larger area - approximately half the screen width for better analog control
        let analogSize: CGFloat = dpadSizeScaled * 0.75 //min(dpadSizeScaled * 8.0, 400 * currentScale)
        analogStick.frame = CGRect(
            x: dpad.frame.midX - analogSize / 2,
            y: dpad.frame.midY - analogSize / 2,
            width: analogSize,
            height: analogSize
        )
        analogStick.maxDistance = CGFloat(configuration.analogStickMaxDistance) * currentScale * 4.0
        analogStick.stickRadius = 25 * currentScale * 4.0

        // Action buttons relative to padRightImageView
        layoutButton(.a, coords: ButtonCoords.a, relativeTo: padRightImageView, scale: currentScale)
        layoutButton(.b, coords: ButtonCoords.b, relativeTo: padRightImageView, scale: currentScale)
        layoutButton(.c, coords: ButtonCoords.c, relativeTo: padRightImageView, scale: currentScale)
        layoutButton(.x, coords: ButtonCoords.x, relativeTo: padRightImageView, scale: currentScale)
        layoutButton(.y, coords: ButtonCoords.y, relativeTo: padRightImageView, scale: currentScale)
        layoutButton(.z, coords: ButtonCoords.z, relativeTo: padRightImageView, scale: currentScale)

        // Triggers relative to their image views
        layoutButton(.leftTrigger, coords: TriggerCoords.left, relativeTo: padTopLeftImageView, scale: currentScale)
        layoutButton(.rightTrigger, coords: TriggerCoords.right, relativeTo: padTopRightImageView, scale: currentScale)

        // Start button relative to padMiddleImageView
        layoutButton(.start, coords: StartCoords.rect, relativeTo: padMiddleImageView, scale: currentScale)
    }

    private func layoutButton(_ buttonType: PadButtons, coords: CGRect, relativeTo imageView: UIImageView, scale: CGFloat) {
        guard let button = buttons[buttonType] else { return }

        let scaledRect = CGRect(
            x: coords.origin.x * scale,
            y: coords.origin.y * scale,
            width: coords.width * scale,
            height: coords.height * scale
        )

        button.frame = CGRect(
            x: imageView.frame.origin.x + scaledRect.origin.x,
            y: imageView.frame.origin.y + scaledRect.origin.y,
            width: scaledRect.width,
            height: scaledRect.height
        )
    }

    // MARK: - Configuration

    private func applyOpacity() {
        let opacity = CGFloat(configuration.opacity)
        padLeftImageView.alpha = opacity
        padRightImageView.alpha = opacity
        padMiddleImageView.alpha = opacity
        padTopLeftImageView.alpha = opacity
        padTopRightImageView.alpha = opacity
    }

    /// Apply current configuration to layout
    func applyConfiguration() {
        setNeedsLayout()
    }

    /// Update configuration
    func updateConfiguration(_ newConfig: PadConfiguration) {
        self.configuration = newConfig
        // Update D-Pad dead zone
        dpad.deadZoneRatio = CGFloat(newConfig.dpadDeadZoneRatio)
        // Update theme images
        updateThemeImages()
        applyConfiguration()
    }

    // MARK: - D-PAD State Handling

    private func handleDPadStateChange(from oldState: DPadState, to newState: DPadState) {
        guard !isEditing else { return }

        // Release old directions
        for button in oldState.activeButtons {
            if !newState.activeButtons.contains(button) {
                delegate?.padView(self, didRelease: button)
            }
        }

        // Press new directions
        for button in newState.activeButtons {
            if !oldState.activeButtons.contains(button) {
                delegate?.padView(self, didPress: button)
            }
        }

        previousDPadState = newState
    }

    // MARK: - Touch Handling

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            handleTouch(touch, phase: .began)
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            handleTouch(touch, phase: .moved)
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            handleTouch(touch, phase: .ended)
        }
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            handleTouch(touch, phase: .cancelled)
        }
    }

    private func handleTouch(_ touch: UITouch, phase: UITouch.Phase) {
        let location = touch.location(in: self)

        // Create thumb-sized touch rect for hit detection
        let thumbRect = CGRect(
            x: location.x - thumbTouchRadius,
            y: location.y - thumbTouchRadius,
            width: thumbTouchRadius * 2,
            height: thumbTouchRadius * 2
        )

        // Debug: Show touch radius
        if showTouchDebug {
            updateTouchDebugView(for: touch, at: location, phase: phase)
        }

        // Handle D-PAD
        if dpad.frame.intersects(thumbRect) || dpad.isTrackingTouch(touch) {
            let dpadLocation = touch.location(in: dpad)
            if dpad.bounds.contains(dpadLocation) || dpad.isTrackingTouch(touch) {
                dpad.handleTouch(touch, phase: phase)
            }
        }

        // Handle buttons - thumb-sized hit detection allows pressing multiple buttons
        for (_, buttonView) in buttons {
            // Check if thumb rect intersects with button frame
            let isHit = buttonView.frame.intersects(thumbRect)

            switch phase {
            case .began:
                if isHit {
                    buttonView.handleTouchBegan(touch)
                }

            case .moved:
                if isHit {
                    if !buttonView.isTrackingTouch(touch) {
                        buttonView.handleTouchBegan(touch)
                    }
                } else {
                    if buttonView.isTrackingTouch(touch) {
                        buttonView.handleTouchEnded(touch)
                    }
                }

            case .ended, .cancelled:
                if buttonView.isTrackingTouch(touch) {
                    buttonView.handleTouchEnded(touch)
                }

            default:
                break
            }
        }
    }

    // MARK: - Debug Touch Visualization

    private func updateTouchDebugView(for touch: UITouch, at location: CGPoint, phase: UITouch.Phase) {
        switch phase {
        case .began:
            let debugView = createTouchDebugView()
            debugView.center = location
            addSubview(debugView)
            touchDebugViews[touch] = debugView

        case .moved:
            touchDebugViews[touch]?.center = location

        case .ended, .cancelled:
            touchDebugViews[touch]?.removeFromSuperview()
            touchDebugViews.removeValue(forKey: touch)

        default:
            break
        }
    }

    private func createTouchDebugView() -> UIView {
        let size = thumbTouchRadius * 2
        let view = UIView(frame: CGRect(x: 0, y: 0, width: size, height: size))
        view.backgroundColor = UIColor.red.withAlphaComponent(0.3)
        view.layer.cornerRadius = thumbTouchRadius
        view.layer.borderWidth = 2
        view.layer.borderColor = UIColor.red.cgColor
        view.isUserInteractionEnabled = false
        return view
    }

    // MARK: - Edit Mode

    private func updateEditMode() {
        if isEditing {
            dpad.cancelTouch()
            for (_, button) in buttons {
                button.cancelAllTouches()
            }
            setupEditGestures()
        } else {
            removeEditGestures()
        }
    }

    private var editGestureRecognizers: [UIPanGestureRecognizer] = []

    private func setupEditGestures() {
        // Pan gesture for each image view group
        let leftPan = UIPanGestureRecognizer(target: self, action: #selector(handleLeftPadDrag(_:)))
        padLeftImageView.isUserInteractionEnabled = true
        padLeftImageView.addGestureRecognizer(leftPan)
        editGestureRecognizers.append(leftPan)

        let rightPan = UIPanGestureRecognizer(target: self, action: #selector(handleRightPadDrag(_:)))
        padRightImageView.isUserInteractionEnabled = true
        padRightImageView.addGestureRecognizer(rightPan)
        editGestureRecognizers.append(rightPan)

        let middlePan = UIPanGestureRecognizer(target: self, action: #selector(handleMiddlePadDrag(_:)))
        padMiddleImageView.isUserInteractionEnabled = true
        padMiddleImageView.addGestureRecognizer(middlePan)
        editGestureRecognizers.append(middlePan)

        let topLeftPan = UIPanGestureRecognizer(target: self, action: #selector(handleTopLeftPadDrag(_:)))
        padTopLeftImageView.isUserInteractionEnabled = true
        padTopLeftImageView.addGestureRecognizer(topLeftPan)
        editGestureRecognizers.append(topLeftPan)

        let topRightPan = UIPanGestureRecognizer(target: self, action: #selector(handleTopRightPadDrag(_:)))
        padTopRightImageView.isUserInteractionEnabled = true
        padTopRightImageView.addGestureRecognizer(topRightPan)
        editGestureRecognizers.append(topRightPan)
    }

    private func removeEditGestures() {
        for gesture in editGestureRecognizers {
            gesture.view?.removeGestureRecognizer(gesture)
        }
        editGestureRecognizers.removeAll()

        padLeftImageView.isUserInteractionEnabled = false
        padRightImageView.isUserInteractionEnabled = false
        padMiddleImageView.isUserInteractionEnabled = false
        padTopLeftImageView.isUserInteractionEnabled = false
        padTopRightImageView.isUserInteractionEnabled = false
    }

    @objc private func handleLeftPadDrag(_ gesture: UIPanGestureRecognizer) {
        handleImageViewDrag(gesture, imageView: padLeftImageView)
    }

    @objc private func handleRightPadDrag(_ gesture: UIPanGestureRecognizer) {
        handleImageViewDrag(gesture, imageView: padRightImageView)
    }

    @objc private func handleMiddlePadDrag(_ gesture: UIPanGestureRecognizer) {
        handleImageViewDrag(gesture, imageView: padMiddleImageView)
    }

    @objc private func handleTopLeftPadDrag(_ gesture: UIPanGestureRecognizer) {
        handleImageViewDrag(gesture, imageView: padTopLeftImageView)
    }

    @objc private func handleTopRightPadDrag(_ gesture: UIPanGestureRecognizer) {
        handleImageViewDrag(gesture, imageView: padTopRightImageView)
    }

    private func handleImageViewDrag(_ gesture: UIPanGestureRecognizer, imageView: UIImageView) {
        guard isEditing else { return }

        let translation = gesture.translation(in: self)
        imageView.center = CGPoint(
            x: imageView.center.x + translation.x,
            y: imageView.center.y + translation.y
        )
        gesture.setTranslation(.zero, in: self)

        // Update touch areas to follow the image
        layoutTouchAreas()

        if gesture.state == .ended {
            saveConfiguration()
        }
    }

    // MARK: - Public Methods

    /// Save current configuration
    func saveConfiguration() {
        configuration.save()
        configurationDelegate?.didSaveConfiguration()
    }

    /// Reset to default configuration
    func resetToDefault() {
        configuration = .default
        setNeedsLayout()
        configurationDelegate?.didRequestReset()
    }

    /// Cancel all active inputs
    func cancelAllInputs() {
        dpad.cancelTouch()
        analogStick.reset()
        for (_, button) in buttons {
            button.cancelAllTouches()
        }
    }
}
