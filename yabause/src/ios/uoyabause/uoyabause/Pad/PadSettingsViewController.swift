//
//  PadSettingsViewController.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// View controller for pad settings (thumb touch radius, D-Pad dead zone, opacity)
/// Displays YabausePadView as full-screen overlay with settings controls on top
class PadSettingsViewController: UIViewController {

    // MARK: - Properties

    private var configuration: PadConfiguration
    private var onConfigurationChanged: ((PadConfiguration) -> Void)?

    // MARK: - UI Elements

    /// Full-screen pad view overlay
    private var padView: YabausePadView!

    /// Touch radius visualization overlays
    private let touchRadiusOverlay = UIView()

    /// Dead zone visualization overlay
    private let deadZoneOverlay = UIView()

    /// Analog stick max distance visualization overlay
    private let analogStickOverlay = UIView()

    /// Settings panel (semi-transparent container)
    private let settingsPanel = UIView()
    private var settingsStack: UIStackView!
    private var legendView: UIStackView?
    private var resetButton: UIButton?

    /// Theme selector
    private let themeSegmentedControl = UISegmentedControl(items: PadTheme.allCases.map { $0.displayName })

    /// Sliders
    private let padScaleSlider = UISlider()
    private let padScaleValueLabel = UILabel()

    private let thumbRadiusSlider = UISlider()
    private let thumbRadiusValueLabel = UILabel()

    private let deadZoneSlider = UISlider()
    private let deadZoneValueLabel = UILabel()

    private let opacitySlider = UISlider()
    private let opacityValueLabel = UILabel()

    private let analogStickDistanceSlider = UISlider()
    private let analogStickDistanceValueLabel = UILabel()

    // MARK: - Initialization

    init(configuration: PadConfiguration, onChanged: ((PadConfiguration) -> Void)? = nil) {
        self.configuration = configuration
        self.onConfigurationChanged = onChanged
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        self.configuration = .default
        super.init(coder: coder)
    }

    // MARK: - Lifecycle

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupNavigationBar()
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        // Save configuration when leaving
        configuration.save()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()

        // Force padView to layout immediately
        padView.setNeedsLayout()
        padView.layoutIfNeeded()

        // Update overlays after padView layout is complete
        DispatchQueue.main.async { [weak self] in
            self?.updateTouchRadiusOverlays()
            self?.updateDeadZoneOverlay()
            self?.updateAnalogStickOverlay()
        }
    }

    // MARK: - Setup

    private func setupUI() {
        title = NSLocalizedString("Pad Settings", comment: "")
        view.backgroundColor = .black

        // Setup full-screen pad view
        setupPadView()

        // Setup visualization overlays
        setupOverlays()

        // Setup settings panel
        setupSettingsPanel()

        // Setup sliders
        setupSliders()
    }

    private func setupPadView() {
        // Create full-screen YabausePadView
        padView = YabausePadView(frame: view.bounds, configuration: configuration)
        padView.isUserInteractionEnabled = false  // Disable touch for preview
        padView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(padView)

        NSLayoutConstraint.activate([
            padView.topAnchor.constraint(equalTo: view.topAnchor),
            padView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            padView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            padView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
    }

    private func setupOverlays() {
        // Touch radius overlay
        touchRadiusOverlay.translatesAutoresizingMaskIntoConstraints = false
        touchRadiusOverlay.isUserInteractionEnabled = false
        touchRadiusOverlay.backgroundColor = .clear
        view.addSubview(touchRadiusOverlay)

        // Dead zone overlay
        deadZoneOverlay.translatesAutoresizingMaskIntoConstraints = false
        deadZoneOverlay.isUserInteractionEnabled = false
        deadZoneOverlay.backgroundColor = .clear
        view.addSubview(deadZoneOverlay)

        // Analog stick overlay
        analogStickOverlay.translatesAutoresizingMaskIntoConstraints = false
        analogStickOverlay.isUserInteractionEnabled = false
        analogStickOverlay.backgroundColor = .clear
        view.addSubview(analogStickOverlay)

        NSLayoutConstraint.activate([
            touchRadiusOverlay.topAnchor.constraint(equalTo: view.topAnchor),
            touchRadiusOverlay.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            touchRadiusOverlay.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            touchRadiusOverlay.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            deadZoneOverlay.topAnchor.constraint(equalTo: view.topAnchor),
            deadZoneOverlay.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            deadZoneOverlay.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            deadZoneOverlay.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            analogStickOverlay.topAnchor.constraint(equalTo: view.topAnchor),
            analogStickOverlay.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            analogStickOverlay.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            analogStickOverlay.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
    }

    private func setupSettingsPanel() {
        // Semi-transparent settings panel at the top
        settingsPanel.backgroundColor = UIColor.black.withAlphaComponent(0.7)
        settingsPanel.layer.cornerRadius = 8
        settingsPanel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(settingsPanel)

        NSLayoutConstraint.activate([
            settingsPanel.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 8),
            settingsPanel.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            settingsPanel.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16)
        ])

        // Create settings stack
        settingsStack = UIStackView()
        settingsStack.axis = .vertical
        settingsStack.spacing = 8
        settingsStack.translatesAutoresizingMaskIntoConstraints = false
        settingsPanel.addSubview(settingsStack)

        NSLayoutConstraint.activate([
            settingsStack.topAnchor.constraint(equalTo: settingsPanel.topAnchor, constant: 8),
            settingsStack.leadingAnchor.constraint(equalTo: settingsPanel.leadingAnchor, constant: 12),
            settingsStack.trailingAnchor.constraint(equalTo: settingsPanel.trailingAnchor, constant: -12),
            settingsStack.bottomAnchor.constraint(equalTo: settingsPanel.bottomAnchor, constant: -8)
        ])

        // Create rows for sliders
        let padScaleRow = createCompactSettingRow(
            title: NSLocalizedString("Pad Size", comment: ""),
            slider: padScaleSlider,
            valueLabel: padScaleValueLabel
        )

        let thumbRadiusRow = createCompactSettingRow(
            title: NSLocalizedString("Touch Radius", comment: ""),
            slider: thumbRadiusSlider,
            valueLabel: thumbRadiusValueLabel
        )

        let deadZoneRow = createCompactSettingRow(
            title: NSLocalizedString("Dead Zone", comment: ""),
            slider: deadZoneSlider,
            valueLabel: deadZoneValueLabel
        )

        let opacityRow = createCompactSettingRow(
            title: NSLocalizedString("Opacity", comment: ""),
            slider: opacitySlider,
            valueLabel: opacityValueLabel
        )

        let analogStickDistanceRow = createCompactSettingRow(
            title: NSLocalizedString("Analog Range", comment: "Analog stick max distance"),
            slider: analogStickDistanceSlider,
            valueLabel: analogStickDistanceValueLabel
        )

        // Row 0: Theme selector
        let themeRow = createThemeRow()
        settingsStack.addArrangedSubview(themeRow)

        // Row 1: Pad Size + Touch Radius
        let row1 = UIStackView(arrangedSubviews: [padScaleRow, thumbRadiusRow])
        row1.axis = .horizontal
        row1.spacing = 16
        row1.distribution = .fillEqually
        settingsStack.addArrangedSubview(row1)

        // Row 2: Dead Zone + Opacity
        let row2 = UIStackView(arrangedSubviews: [deadZoneRow, opacityRow])
        row2.axis = .horizontal
        row2.spacing = 16
        row2.distribution = .fillEqually
        settingsStack.addArrangedSubview(row2)

        // Row 3: Analog Stick Range (full width for better visibility)
        settingsStack.addArrangedSubview(analogStickDistanceRow)

        // Row 4: Legend + Reset button
        legendView = createCompactLegendView()
        resetButton = UIButton(type: .system)
        resetButton?.setTitle(NSLocalizedString("Reset", comment: ""), for: .normal)
        resetButton?.setTitleColor(.appError, for: .normal)
        resetButton?.titleLabel?.font = .systemFont(ofSize: 12)
        resetButton?.addTarget(self, action: #selector(resetButtonTapped), for: .touchUpInside)

        let row3 = UIStackView(arrangedSubviews: [legendView!, resetButton!])
        row3.axis = .horizontal
        row3.spacing = 16
        row3.alignment = .center
        settingsStack.addArrangedSubview(row3)
    }

    private func createThemeRow() -> UIView {
        let container = UIView()

        let titleLabel = UILabel()
        titleLabel.text = NSLocalizedString("Theme", comment: "Pad theme selector")
        titleLabel.font = .systemFont(ofSize: 11)
        titleLabel.textColor = .white
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(titleLabel)

        // Configure segmented control
        themeSegmentedControl.selectedSegmentIndex = PadTheme.allCases.firstIndex(of: configuration.theme) ?? 0
        themeSegmentedControl.addTarget(self, action: #selector(themeChanged(_:)), for: .valueChanged)
        themeSegmentedControl.translatesAutoresizingMaskIntoConstraints = false

        // Style the segmented control for dark background
        themeSegmentedControl.setTitleTextAttributes([.foregroundColor: UIColor.white, .font: UIFont.systemFont(ofSize: 11)], for: .normal)
        themeSegmentedControl.setTitleTextAttributes([.foregroundColor: UIColor.black, .font: UIFont.systemFont(ofSize: 11)], for: .selected)
        container.addSubview(themeSegmentedControl)

        NSLayoutConstraint.activate([
            titleLabel.topAnchor.constraint(equalTo: container.topAnchor),
            titleLabel.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            titleLabel.bottomAnchor.constraint(equalTo: container.bottomAnchor),

            themeSegmentedControl.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            themeSegmentedControl.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            themeSegmentedControl.heightAnchor.constraint(equalToConstant: 28)
        ])

        return container
    }

    private func createCompactSettingRow(title: String, slider: UISlider, valueLabel: UILabel) -> UIView {
        let container = UIView()

        let titleLabel = UILabel()
        titleLabel.text = title
        titleLabel.font = .systemFont(ofSize: 11)
        titleLabel.textColor = .white
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(titleLabel)

        valueLabel.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
        valueLabel.textColor = .lightGray
        valueLabel.textAlignment = .right
        valueLabel.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(valueLabel)

        slider.translatesAutoresizingMaskIntoConstraints = false
        slider.tintColor = .tint
        container.addSubview(slider)

        NSLayoutConstraint.activate([
            titleLabel.topAnchor.constraint(equalTo: container.topAnchor),
            titleLabel.leadingAnchor.constraint(equalTo: container.leadingAnchor),

            valueLabel.topAnchor.constraint(equalTo: container.topAnchor),
            valueLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            valueLabel.widthAnchor.constraint(equalToConstant: 50),

            slider.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 2),
            slider.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            slider.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            slider.bottomAnchor.constraint(equalTo: container.bottomAnchor)
        ])

        return container
    }

    private func createCompactLegendView() -> UIStackView {
        let stack = UIStackView()
        stack.axis = .horizontal
        stack.spacing = 12
        stack.alignment = .center

        // Touch radius legend
        let touchRadiusLegend = createCompactLegendItem(
            color: UIColor.red.withAlphaComponent(0.3),
            borderColor: UIColor.red.withAlphaComponent(0.6),
            text: NSLocalizedString("Touch", comment: "")
        )

        // Dead zone legend
        let deadZoneLegend = createCompactLegendItem(
            color: UIColor.gray.withAlphaComponent(0.5),
            borderColor: UIColor.white.withAlphaComponent(0.7),
            text: NSLocalizedString("Dead", comment: "")
        )

        // D-Pad hit area legend
        let hitAreaLegend = createCompactLegendItem(
            color: .clear,
            borderColor: UIColor.cyan.withAlphaComponent(0.5),
            text: NSLocalizedString("D-Pad", comment: "")
        )

        // Analog stick range legend
        let analogLegend = createCompactLegendItem(
            color: UIColor.green.withAlphaComponent(0.2),
            borderColor: UIColor.green.withAlphaComponent(0.6),
            text: NSLocalizedString("Analog", comment: "")
        )

        stack.addArrangedSubview(touchRadiusLegend)
        stack.addArrangedSubview(deadZoneLegend)
        stack.addArrangedSubview(hitAreaLegend)
        stack.addArrangedSubview(analogLegend)

        return stack
    }

    private func createCompactLegendItem(color: UIColor, borderColor: UIColor, text: String) -> UIStackView {
        let stack = UIStackView()
        stack.axis = .horizontal
        stack.spacing = 2
        stack.alignment = .center

        let indicator = UIView()
        indicator.backgroundColor = color
        indicator.layer.cornerRadius = 4
        indicator.layer.borderWidth = 1
        indicator.layer.borderColor = borderColor.cgColor
        indicator.translatesAutoresizingMaskIntoConstraints = false

        let label = UILabel()
        label.text = text
        label.font = .systemFont(ofSize: 9)
        label.textColor = .lightGray

        NSLayoutConstraint.activate([
            indicator.widthAnchor.constraint(equalToConstant: 8),
            indicator.heightAnchor.constraint(equalToConstant: 8)
        ])

        stack.addArrangedSubview(indicator)
        stack.addArrangedSubview(label)

        return stack
    }

    private func setupNavigationBar() {
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .done,
            target: self,
            action: #selector(doneButtonTapped)
        )
    }

    private func setupSliders() {
        // Pad Scale Slider (0.2 - 0.5)
        padScaleSlider.minimumValue = 0.2
        padScaleSlider.maximumValue = 0.4
        padScaleSlider.value = configuration.padScale
        padScaleSlider.addTarget(self, action: #selector(padScaleChanged(_:)), for: .valueChanged)
        updatePadScaleLabel()

        // Thumb Radius Slider (50 - 150)
        thumbRadiusSlider.minimumValue = 40
        thumbRadiusSlider.maximumValue = 80
        thumbRadiusSlider.value = configuration.thumbTouchRadius
        thumbRadiusSlider.addTarget(self, action: #selector(thumbRadiusChanged(_:)), for: .valueChanged)
        updateThumbRadiusLabel()

        // Dead Zone Slider (0.1 - 0.3)
        deadZoneSlider.minimumValue = 0.1
        deadZoneSlider.maximumValue = 0.3
        deadZoneSlider.value = configuration.dpadDeadZoneRatio
        deadZoneSlider.addTarget(self, action: #selector(deadZoneChanged(_:)), for: .valueChanged)
        updateDeadZoneLabel()

        // Opacity Slider (0 - 1)
        opacitySlider.minimumValue = 0
        opacitySlider.maximumValue = 1
        opacitySlider.value = configuration.opacity
        opacitySlider.addTarget(self, action: #selector(opacityChanged(_:)), for: .valueChanged)
        updateOpacityLabel()

        // Analog Stick Distance Slider (20 - 100)
        analogStickDistanceSlider.minimumValue = 20
        analogStickDistanceSlider.maximumValue = 100
        analogStickDistanceSlider.value = configuration.analogStickMaxDistance
        analogStickDistanceSlider.addTarget(self, action: #selector(analogStickDistanceChanged(_:)), for: .valueChanged)
        updateAnalogStickDistanceLabel()
    }

    // MARK: - Overlay Updates

    private func updateTouchRadiusOverlays() {
        // Remove existing indicators
        touchRadiusOverlay.subviews.forEach { $0.removeFromSuperview() }

        // Scale thumb radius by current pad scale to match actual button sizes
        let thumbRadius = CGFloat(configuration.thumbTouchRadius) * padView.currentScale

        // Get B and C button positions
        guard let buttonB = padView.buttons[.b],
              let buttonC = padView.buttons[.c] else { return }

        // Convert button centers to overlay coordinate system
        let buttonBCenter = buttonB.superview?.convert(buttonB.center, to: view) ?? buttonB.center
        let buttonCCenter = buttonC.superview?.convert(buttonC.center, to: view) ?? buttonC.center

        // Calculate the midpoint between B and C buttons
        let midpoint = CGPoint(
            x: (buttonBCenter.x + buttonCCenter.x) / 2,
            y: (buttonBCenter.y + buttonCCenter.y) / 2
        )

        // Create a single touch radius circle at the midpoint to visualize thumb touch size
        let circleView = createTouchRadiusCircle(radius: thumbRadius)
        circleView.center = midpoint
        touchRadiusOverlay.addSubview(circleView)

        // Add label to indicate this is the touch size
        let label = UILabel()
        label.text = NSLocalizedString("Touch Size", comment: "")
        label.font = .systemFont(ofSize: 10)
        label.textColor = .white
        label.textAlignment = .center
        label.sizeToFit()
        label.center = CGPoint(x: midpoint.x, y: midpoint.y + thumbRadius + 12)
        touchRadiusOverlay.addSubview(label)
    }

    private func createTouchRadiusCircle(radius: CGFloat) -> UIView {
        let diameter = radius * 2
        let circle = UIView(frame: CGRect(x: 0, y: 0, width: diameter, height: diameter))
        circle.backgroundColor = UIColor.red.withAlphaComponent(0.3)
        circle.layer.cornerRadius = radius
        circle.layer.borderWidth = 1
        circle.layer.borderColor = UIColor.red.withAlphaComponent(0.6).cgColor
        return circle
    }

    private func updateDeadZoneOverlay() {
        // Remove existing indicators
        deadZoneOverlay.subviews.forEach { $0.removeFromSuperview() }

        // Get D-Pad from padView
        guard let dpad = padView.dpad else { return }

        // Convert D-Pad center to overlay coordinate system
        let dpadCenter = dpad.superview?.convert(dpad.center, to: view) ?? dpad.center
        let dpadRadius = dpad.bounds.width / 2
        let deadZoneRadius = dpadRadius * CGFloat(configuration.dpadDeadZoneRatio)

        // Create dead zone circle
        let deadZoneCircle = UIView()
        deadZoneCircle.backgroundColor = UIColor.gray.withAlphaComponent(0.5)
        deadZoneCircle.layer.cornerRadius = deadZoneRadius
        deadZoneCircle.layer.borderWidth = 1
        deadZoneCircle.layer.borderColor = UIColor.white.withAlphaComponent(0.7).cgColor
        deadZoneCircle.frame = CGRect(
            x: dpadCenter.x - deadZoneRadius,
            y: dpadCenter.y - deadZoneRadius,
            width: deadZoneRadius * 2,
            height: deadZoneRadius * 2
        )
        deadZoneOverlay.addSubview(deadZoneCircle)

        // Create D-Pad hit area indicator (outer ring)
        let hitAreaView = UIView()
        hitAreaView.backgroundColor = .clear
        hitAreaView.layer.cornerRadius = dpadRadius
        hitAreaView.layer.borderWidth = 2
        hitAreaView.layer.borderColor = UIColor.cyan.withAlphaComponent(0.5).cgColor
        hitAreaView.frame = CGRect(
            x: dpadCenter.x - dpadRadius,
            y: dpadCenter.y - dpadRadius,
            width: dpadRadius * 2,
            height: dpadRadius * 2
        )
        deadZoneOverlay.addSubview(hitAreaView)
    }

    private func updateAnalogStickOverlay() {
        // Remove existing indicators
        analogStickOverlay.subviews.forEach { $0.removeFromSuperview() }

        // Get analog stick from padView
        guard let analogStick = padView.analogStick else { return }

        // Convert analog stick center to overlay coordinate system
        let analogCenter = analogStick.superview?.convert(analogStick.center, to: view) ?? analogStick.center
        let maxDistance = CGFloat(configuration.analogStickMaxDistance) * padView.currentScale * 4.0

        // Create analog stick range circle
        let rangeCircle = UIView()
        rangeCircle.backgroundColor = UIColor.green.withAlphaComponent(0.2)
        rangeCircle.layer.cornerRadius = maxDistance
        rangeCircle.layer.borderWidth = 2
        rangeCircle.layer.borderColor = UIColor.green.withAlphaComponent(0.6).cgColor
        rangeCircle.frame = CGRect(
            x: analogCenter.x - maxDistance,
            y: analogCenter.y - maxDistance,
            width: maxDistance * 2,
            height: maxDistance * 2
        )
        analogStickOverlay.addSubview(rangeCircle)

        // Add label to indicate this is the analog range
        let label = UILabel()
        label.text = NSLocalizedString("Analog Range", comment: "")
        label.font = .systemFont(ofSize: 10)
        label.textColor = .white
        label.textAlignment = .center
        label.sizeToFit()
        label.center = CGPoint(x: analogCenter.x, y: analogCenter.y + maxDistance + 12)
        analogStickOverlay.addSubview(label)
    }

    // MARK: - Slider Value Updates

    private func updatePadScaleLabel() {
        let percentage = Int(configuration.padScale * 100)
        padScaleValueLabel.text = "\(percentage)%"
    }

    private func updateThumbRadiusLabel() {
        thumbRadiusValueLabel.text = String(format: "%.0f pt", configuration.thumbTouchRadius)
    }

    private func updateDeadZoneLabel() {
        let percentage = Int(configuration.dpadDeadZoneRatio * 100)
        deadZoneValueLabel.text = "\(percentage)%"
    }

    private func updateOpacityLabel() {
        let percentage = Int(configuration.opacity * 100)
        opacityValueLabel.text = "\(percentage)%"
    }

    private func updateAnalogStickDistanceLabel() {
        analogStickDistanceValueLabel.text = String(format: "%.0f pt", configuration.analogStickMaxDistance)
    }

    // MARK: - Actions

    @objc private func padScaleChanged(_ slider: UISlider) {
        configuration.padScale = slider.value
        updatePadScaleLabel()
        padView.updateConfiguration(configuration)
        // Force padView to re-layout with new scale before updating overlays
        padView.setNeedsLayout()
        padView.layoutIfNeeded()
        // Update overlays after pad layout is complete
        DispatchQueue.main.async { [weak self] in
            self?.updateTouchRadiusOverlays()
            self?.updateDeadZoneOverlay()
            self?.updateAnalogStickOverlay()
        }
        notifyConfigurationChanged()
    }

    @objc private func thumbRadiusChanged(_ slider: UISlider) {
        configuration.thumbTouchRadius = slider.value
        updateThumbRadiusLabel()
        updateTouchRadiusOverlays()
        notifyConfigurationChanged()
    }

    @objc private func deadZoneChanged(_ slider: UISlider) {
        configuration.dpadDeadZoneRatio = slider.value
        updateDeadZoneLabel()
        padView.updateConfiguration(configuration)
        updateDeadZoneOverlay()
        notifyConfigurationChanged()
    }

    @objc private func opacityChanged(_ slider: UISlider) {
        configuration.opacity = slider.value
        updateOpacityLabel()
        padView.updateConfiguration(configuration)
        notifyConfigurationChanged()
    }

    @objc private func analogStickDistanceChanged(_ slider: UISlider) {
        configuration.analogStickMaxDistance = slider.value
        updateAnalogStickDistanceLabel()
        padView.updateConfiguration(configuration)
        updateAnalogStickOverlay()
        notifyConfigurationChanged()
    }

    @objc private func themeChanged(_ segmentedControl: UISegmentedControl) {
        let selectedTheme = PadTheme.allCases[segmentedControl.selectedSegmentIndex]
        configuration.theme = selectedTheme
        padView.updateConfiguration(configuration)
        notifyConfigurationChanged()
    }

    @objc private func doneButtonTapped() {
        configuration.save()
        // Check if presented modally or pushed onto navigation stack
        if presentingViewController != nil || navigationController?.presentingViewController != nil {
            dismiss(animated: true)
        } else {
            navigationController?.popViewController(animated: true)
        }
    }

    @objc private func resetButtonTapped() {
        let alert = UIAlertController(
            title: NSLocalizedString("Reset to Default", comment: ""),
            message: NSLocalizedString("Are you sure you want to reset all pad settings to default?", comment: ""),
            preferredStyle: .alert
        )

        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: ""), style: .cancel))
        alert.addAction(UIAlertAction(title: NSLocalizedString("Reset", comment: ""), style: .destructive) { [weak self] _ in
            self?.performReset()
        })

        present(alert, animated: true)
    }

    private func performReset() {
        configuration = .default

        // Update sliders
        padScaleSlider.value = configuration.padScale
        thumbRadiusSlider.value = configuration.thumbTouchRadius
        deadZoneSlider.value = configuration.dpadDeadZoneRatio
        opacitySlider.value = configuration.opacity
        analogStickDistanceSlider.value = configuration.analogStickMaxDistance

        // Update theme selector
        themeSegmentedControl.selectedSegmentIndex = PadTheme.allCases.firstIndex(of: configuration.theme) ?? 0

        // Update labels
        updatePadScaleLabel()
        updateThumbRadiusLabel()
        updateDeadZoneLabel()
        updateOpacityLabel()
        updateAnalogStickDistanceLabel()

        // Update pad view
        padView.updateConfiguration(configuration)
        // Force padView to re-layout with new scale before updating overlays
        padView.setNeedsLayout()
        padView.layoutIfNeeded()

        // Update overlays
        DispatchQueue.main.async { [weak self] in
            self?.updateTouchRadiusOverlays()
            self?.updateDeadZoneOverlay()
            self?.updateAnalogStickOverlay()
        }

        notifyConfigurationChanged()
    }

    private func notifyConfigurationChanged() {
        onConfigurationChanged?(configuration)
    }
}
