import UIKit

class RetroAchievementsGamePlacardView: UIView {
    
    // MARK: - UI Components
    private lazy var containerView: UIView = {
        let view = UIView()
        view.backgroundColor = UIColor.systemBackground.withAlphaComponent(0.6)
        view.layer.cornerRadius = 12
        view.layer.shadowColor = UIColor.black.cgColor
        view.layer.shadowOffset = CGSize(width: 0, height: 4)
        view.layer.shadowRadius = 8
        view.layer.shadowOpacity = 0.3
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }()
    
    private lazy var gameImageView: UIImageView = {
        let imageView = UIImageView()
        imageView.contentMode = .scaleAspectFit
        imageView.layer.cornerRadius = 8
        imageView.layer.masksToBounds = true
        imageView.backgroundColor = .systemGray5
        imageView.image = UIImage(systemName: "gamecontroller.fill")
        // Tint color will be set in configureTheme
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
    }()
    
    private lazy var titleLabel: UILabel = {
        let label = UILabel()
        let isIPhone = UIDevice.current.userInterfaceIdiom == .phone
        label.font = UIFont.boldSystemFont(ofSize: isIPhone ? 14 : 18)
        label.textColor = .label
        label.numberOfLines = 2
        label.adjustsFontSizeToFitWidth = true
        label.minimumScaleFactor = 0.8
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private lazy var consoleLabel: UILabel = {
        let label = UILabel()
        let isIPhone = UIDevice.current.userInterfaceIdiom == .phone
        label.font = UIFont.systemFont(ofSize: isIPhone ? 10 : 14)
        label.textColor = .secondaryLabel
        label.adjustsFontSizeToFitWidth = true
        label.minimumScaleFactor = 0.8
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private lazy var achievementCountLabel: UILabel = {
        let label = UILabel()
        let isIPhone = UIDevice.current.userInterfaceIdiom == .phone
        label.font = UIFont.boldSystemFont(ofSize: isIPhone ? 12 : 16)
        // Text color will be set in configureTheme
        label.numberOfLines = 0
        label.adjustsFontSizeToFitWidth = true
        label.minimumScaleFactor = 0.7
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private lazy var progressBar: UIProgressView = {
        let progress = UIProgressView(progressViewStyle: .default)
        // Progress tint color will be set in configureTheme
        progress.trackTintColor = .systemGray5
        progress.translatesAutoresizingMaskIntoConstraints = false
        progress.progress = 0.0 // Explicitly set initial progress
        return progress
    }()
    
    private lazy var progressLabel: UILabel = {
        let label = UILabel()
        label.font = UIFont.systemFont(ofSize: 12)
        label.textColor = .secondaryLabel
        label.textAlignment = .right
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private lazy var hardcoreModeLabel: UILabel = {
        let label = UILabel()
        let isIPhone = UIDevice.current.userInterfaceIdiom == .phone
        label.font = UIFont.boldSystemFont(ofSize: isIPhone ? 10 : 12)
        label.textColor = .systemRed
        label.text = "🏆 HARDCORE MODE"
        label.adjustsFontSizeToFitWidth = true
        label.minimumScaleFactor = 0.8
        label.isHidden = true
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    // MARK: - Initialization
    override init(frame: CGRect) {
        super.init(frame: frame)
        setupUI()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupUI()
    }
    
    // MARK: - Setup
    private func setupUI() {
        backgroundColor = .clear
        
        addSubview(containerView)
        containerView.addSubview(gameImageView)
        containerView.addSubview(titleLabel)
        containerView.addSubview(consoleLabel)
        containerView.addSubview(achievementCountLabel)
        containerView.addSubview(progressBar)
        containerView.addSubview(progressLabel)
        containerView.addSubview(hardcoreModeLabel)
        
        // Set content compression resistance priorities to prevent text truncation
        let isIPhone = UIDevice.current.userInterfaceIdiom == .phone
        let resistancePriority: UILayoutPriority = isIPhone ? .required : .defaultHigh
        
        titleLabel.setContentCompressionResistancePriority(resistancePriority, for: .horizontal)
        consoleLabel.setContentCompressionResistancePriority(resistancePriority, for: .horizontal)
        achievementCountLabel.setContentCompressionResistancePriority(resistancePriority, for: .horizontal)
        hardcoreModeLabel.setContentCompressionResistancePriority(resistancePriority, for: .horizontal)
        progressLabel.setContentCompressionResistancePriority(resistancePriority, for: .horizontal)
        
        let bottomConstraint = containerView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -8)
        bottomConstraint.priority = UILayoutPriority(999)
        
        let leadingConstraint = containerView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 16)
        leadingConstraint.priority = UILayoutPriority(999)
        
        let trailingConstraint = containerView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -16)
        trailingConstraint.priority = UILayoutPriority(999)
        
        NSLayoutConstraint.activate([
            // Container view constraints
            containerView.topAnchor.constraint(equalTo: topAnchor, constant: 8),
            leadingConstraint,
            trailingConstraint,
            bottomConstraint,
            
        ])
        
        // Create flexible constraints for internal elements
        let imageSize: CGFloat = isIPhone ? 40 : 64
        let imageWidthConstraint = gameImageView.widthAnchor.constraint(equalToConstant: imageSize)
        imageWidthConstraint.priority = UILayoutPriority(950)
        
        let imageHeightConstraint = gameImageView.heightAnchor.constraint(equalToConstant: imageSize)
        imageHeightConstraint.priority = UILayoutPriority(950)
        
        let titleTrailingConstraint = titleLabel.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor, constant: isIPhone ? -8 : -16)
        titleTrailingConstraint.priority = UILayoutPriority(isIPhone ? 999 : 950)
        
        let consoleTrailingConstraint = consoleLabel.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor, constant: isIPhone ? -8 : -16)
        consoleTrailingConstraint.priority = UILayoutPriority(isIPhone ? 999 : 950)

        let padding: CGFloat = isIPhone ? 8 : 16
        let spacing: CGFloat = isIPhone ? 6 : 12

        let achievementTrailingConstraint = achievementCountLabel.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor, constant: CGFloat(-padding))
        achievementTrailingConstraint.priority = UILayoutPriority(950)
        
        
        NSLayoutConstraint.activate([
            // Game image constraints
            gameImageView.topAnchor.constraint(equalTo: containerView.topAnchor, constant: padding),
            gameImageView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: padding),
            imageWidthConstraint,
            imageHeightConstraint,
            
            // Title label constraints
            titleLabel.topAnchor.constraint(equalTo: containerView.topAnchor, constant: padding),
            titleLabel.leadingAnchor.constraint(equalTo: gameImageView.trailingAnchor, constant: spacing),
            titleTrailingConstraint,
            
            // Console label constraints
            consoleLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            consoleLabel.leadingAnchor.constraint(equalTo: gameImageView.trailingAnchor, constant: spacing),
            consoleTrailingConstraint,
            
            // Achievement count label constraints
            achievementCountLabel.topAnchor.constraint(equalTo: consoleLabel.bottomAnchor, constant: isIPhone ? 6 : 8),
            achievementCountLabel.leadingAnchor.constraint(equalTo: gameImageView.trailingAnchor, constant: spacing),
            achievementTrailingConstraint
        ])
        
        // Create more flexible constraints for remaining elements
        let hardcoreTrailingConstraint = hardcoreModeLabel.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -padding)
        hardcoreTrailingConstraint.priority = UILayoutPriority(950)
        
        let progressLeadingConstraint = progressBar.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: padding)
        progressLeadingConstraint.priority = UILayoutPriority(950)
        
        let progressTrailingConstraint = progressBar.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -padding)
        progressTrailingConstraint.priority = UILayoutPriority(950)
        
        let progressLabelTrailingConstraint = progressLabel.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -padding)
        progressLabelTrailingConstraint.priority = UILayoutPriority(950)
        
        let progressBottomConstraint = progressLabel.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -padding)
        progressBottomConstraint.priority = UILayoutPriority(950)
        
        NSLayoutConstraint.activate([
            // Hardcore mode label constraints - moved to separate line below achievement count
            hardcoreModeLabel.topAnchor.constraint(equalTo: achievementCountLabel.bottomAnchor, constant: 2),
            hardcoreModeLabel.leadingAnchor.constraint(equalTo: gameImageView.trailingAnchor, constant: spacing),
            hardcoreTrailingConstraint,
            
            // Progress bar constraints - positioned after hardcore label for better spacing
            progressBar.topAnchor.constraint(greaterThanOrEqualTo: gameImageView.bottomAnchor, constant: padding),
            progressBar.topAnchor.constraint(greaterThanOrEqualTo: hardcoreModeLabel.bottomAnchor, constant: isIPhone ? 4 : 8),
            progressLeadingConstraint,
            progressTrailingConstraint,
            progressBar.heightAnchor.constraint(equalToConstant: 4), // Explicit height for visibility
            
            // Progress label constraints
            progressLabel.topAnchor.constraint(equalTo: progressBar.bottomAnchor, constant: 4),
            progressLabelTrailingConstraint,
            progressBottomConstraint
        ])
    }
    
    // MARK: - Public Methods
    func configure(gameTitle: String, consoleName: String, achievementCount: Int, unlockedCount: Int, isHardcoreMode: Bool) {
        titleLabel.text = gameTitle
        consoleLabel.text = consoleName
        
        // Debug logging
        NSLog("RetroAchievements Placard: Configuring with unlocked=\(unlockedCount), total=\(achievementCount)")
        
        if achievementCount > 0 {
            achievementCountLabel.text = "\(unlockedCount)/\(achievementCount) achievements"
            let progress = Float(unlockedCount) / Float(achievementCount)
            NSLog("RetroAchievements Placard: Progress calculated as \(progress) (\(Int(progress * 100))%)")
            
            // Force layout update before setting progress
            progressBar.layoutIfNeeded()
            progressBar.setProgress(progress, animated: true)
            progressLabel.text = "\(Int(progress * 100))% complete"
            
            // Debug: Check progress bar state after setting
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) { [weak self] in
                if let currentProgress = self?.progressBar.progress {
                    NSLog("RetroAchievements Placard: Progress bar actual value after setting: \(currentProgress)")
                }
            }
        } else {
            achievementCountLabel.text = "No achievements available"
            progressBar.setProgress(0, animated: false)
            progressLabel.text = ""
        }
        
        // Configure visual theme based on mode
        configureTheme(isHardcoreMode: isHardcoreMode)
    }
    
    private func configureTheme(isHardcoreMode: Bool) {
        if isHardcoreMode {
            // Hardcore mode: Dark red background with bright red accents
            containerView.backgroundColor = UIColor(red: 0.2, green: 0.0, blue: 0.0, alpha: 0.9) // Dark red background
            containerView.layer.borderWidth = 2
            containerView.layer.borderColor = UIColor.systemRed.cgColor
            
            // Use white text for better visibility on dark background
            titleLabel.textColor = .white
            consoleLabel.textColor = UIColor(white: 0.9, alpha: 1.0) // Slightly dimmed white
            progressLabel.textColor = UIColor(white: 0.8, alpha: 1.0) // Even more dimmed white
            
            gameImageView.tintColor = .systemRed
            achievementCountLabel.textColor = .systemRed
            progressBar.progressTintColor = .systemRed
            hardcoreModeLabel.text = "🔥 HARDCORE MODE"
            hardcoreModeLabel.textColor = .systemRed
            hardcoreModeLabel.isHidden = false
        } else {
            // Soft mode: Dark blue background with bright blue accents
            containerView.backgroundColor = UIColor(red: 0.0, green: 0.0, blue: 0.2, alpha: 0.9) // Dark blue background
            containerView.layer.borderWidth = 2
            containerView.layer.borderColor = UIColor.systemBlue.cgColor
            
            // Use white text for better visibility on dark background
            titleLabel.textColor = .white
            consoleLabel.textColor = UIColor(white: 0.9, alpha: 1.0) // Slightly dimmed white
            progressLabel.textColor = UIColor(white: 0.8, alpha: 1.0) // Even more dimmed white
            
            gameImageView.tintColor = .systemBlue
            achievementCountLabel.textColor = .systemBlue
            progressBar.progressTintColor = .systemBlue
            hardcoreModeLabel.text = "🎮 CASUAL MODE"
            hardcoreModeLabel.textColor = .systemBlue
            hardcoreModeLabel.isHidden = false
        }
    }
    
    func show(in parentView: UIView, duration: TimeInterval = 0.3) {
        parentView.addSubview(self)
        translatesAutoresizingMaskIntoConstraints = false
        
        // Adaptive width based on device type
        let isIPhone = UIDevice.current.userInterfaceIdiom == .phone
        let trailingConstant: CGFloat = isIPhone ? -16 : -20
        
        if isIPhone {
            // iPhone: Ensure minimum width to prevent text compression
            NSLayoutConstraint.activate([
                topAnchor.constraint(equalTo: parentView.safeAreaLayoutGuide.topAnchor, constant: 20),
                trailingAnchor.constraint(equalTo: parentView.trailingAnchor, constant: trailingConstant),
                leadingAnchor.constraint(greaterThanOrEqualTo: parentView.leadingAnchor, constant: 8),
                widthAnchor.constraint(greaterThanOrEqualToConstant: 240), // Minimum width
                widthAnchor.constraint(lessThanOrEqualToConstant: 320) // Maximum width
                
            ])
            
            // Add horizontal padding to content
            setContentHuggingPriority(.defaultHigh, for: .horizontal)
            setContentCompressionResistancePriority(.defaultHigh, for: .horizontal)
        } else {
            // iPad: Use original behavior
            NSLayoutConstraint.activate([
                topAnchor.constraint(equalTo: parentView.safeAreaLayoutGuide.topAnchor, constant: 20),
                trailingAnchor.constraint(equalTo: parentView.trailingAnchor, constant: trailingConstant),
                widthAnchor.constraint(equalTo: parentView.widthAnchor, multiplier: 0.25)
            ])
        }
        
        // Set content hugging and compression resistance to prevent constraint conflicts
        setContentHuggingPriority(.required, for: .vertical)
        setContentCompressionResistancePriority(.required, for: .vertical)
        
        // Use CALayer animation to avoid triggering parent view updates
        self.layer.opacity = 0
        self.layer.transform = CATransform3DMakeTranslation(0, -50, 0)
        
        // Create opacity animation
        let opacityAnimation = CABasicAnimation(keyPath: "opacity")
        opacityAnimation.fromValue = 0
        opacityAnimation.toValue = 1
        opacityAnimation.duration = duration
        
        // Create transform animation
        let transformAnimation = CABasicAnimation(keyPath: "transform")
        transformAnimation.fromValue = CATransform3DMakeTranslation(0, -50, 0)
        transformAnimation.toValue = CATransform3DIdentity
        transformAnimation.duration = duration
        transformAnimation.timingFunction = CAMediaTimingFunction(name: .easeOut)
        
        // Apply animations
        self.layer.opacity = 1
        self.layer.transform = CATransform3DIdentity
        self.layer.add(opacityAnimation, forKey: "fadeIn")
        self.layer.add(transformAnimation, forKey: "slideIn")
        
        // Auto-hide after 4 seconds
        DispatchQueue.main.asyncAfter(deadline: .now() + 4.0) {
            self.hide(duration: duration)
        }
    }
    
    func hide(duration: TimeInterval = 0.3) {
        // Use CALayer animation to avoid triggering parent view updates
        let opacityAnimation = CABasicAnimation(keyPath: "opacity")
        opacityAnimation.fromValue = 1
        opacityAnimation.toValue = 0
        opacityAnimation.duration = duration
        opacityAnimation.fillMode = .forwards
        opacityAnimation.isRemovedOnCompletion = false
        
        let transformAnimation = CABasicAnimation(keyPath: "transform")
        transformAnimation.fromValue = CATransform3DIdentity
        transformAnimation.toValue = CATransform3DMakeTranslation(0, -50, 0)
        transformAnimation.duration = duration
        transformAnimation.fillMode = .forwards
        transformAnimation.isRemovedOnCompletion = false
        transformAnimation.timingFunction = CAMediaTimingFunction(name: .easeIn)
        
        // Add animations
        self.layer.add(opacityAnimation, forKey: "fadeOut")
        self.layer.add(transformAnimation, forKey: "slideOut")
        
        // Remove from superview after animation
        DispatchQueue.main.asyncAfter(deadline: .now() + duration) {
            self.removeFromSuperview()
        }
    }
    
    func updateGameImage(_ image: UIImage) {
        // Simply update the image without animation to avoid parent view updates
        self.gameImageView.image = image
        self.gameImageView.contentMode = .scaleAspectFill
    }
}

// MARK: - Game Placard Manager
class RetroAchievementsGamePlacardManager {
    
    static let shared = RetroAchievementsGamePlacardManager()
    
    private var currentPlacard: RetroAchievementsGamePlacardView?
    
    private init() {}
    
    func showGamePlacard(gameTitle: String, consoleName: String, achievementCount: Int, unlockedCount: Int = 0, isHardcoreMode: Bool = false) {
        // Remove existing placard
        currentPlacard?.removeFromSuperview()
        
        // Get GameViewController directly
        var gameViewController: UIViewController?
        
        // Start from root and find GameViewController
        if let rootVC = UIApplication.shared.windows.first?.rootViewController {
            gameViewController = findGameViewController(from: rootVC)
        }
        
        guard let targetViewController = gameViewController else {
            NSLog("RetroAchievements: Could not find GameViewController for placard display")
            return
        }
        
        // Create and show new placard
        let placard = RetroAchievementsGamePlacardView()
        placard.configure(
            gameTitle: gameTitle,
            consoleName: consoleName,
            achievementCount: achievementCount,
            unlockedCount: unlockedCount,
            isHardcoreMode: isHardcoreMode
        )
        
        placard.show(in: targetViewController.view)
        currentPlacard = placard
    }
    
    private func findGameViewController(from viewController: UIViewController) -> UIViewController? {
        // Check if this is GameViewController (check class name)
        let className = String(describing: type(of: viewController))
        NSLog("RetroAchievements: Checking view controller: \(className)")
        
        // Check for GameViewController (may include module name like "uoyabause.GameViewController")
        if className.contains("GameViewController") && !className.contains("GameMainViewController") {
            NSLog("RetroAchievements: Found GameViewController: \(className)")
            return viewController
        }
        
        // Check child view controllers
        for child in viewController.children {
            if let found = findGameViewController(from: child) {
                return found
            }
        }
        
        // Check presented view controllers
        if let presented = viewController.presentedViewController {
            if let found = findGameViewController(from: presented) {
                return found
            }
        }
        
        // Check navigation controller
        if let nav = viewController as? UINavigationController {
            for vc in nav.viewControllers {
                if let found = findGameViewController(from: vc) {
                    return found
                }
            }
        }
        
        // Check tab bar controller
        if let tab = viewController as? UITabBarController {
            if let viewControllers = tab.viewControllers {
                for vc in viewControllers {
                    if let found = findGameViewController(from: vc) {
                        return found
                    }
                }
            }
        }
        
        return nil
    }
    
    /**
     * Show game placard from rcheevos callback parameters
     * This method is called from the C wrapper callback system
     */
    func showGamePlacardFromCallback(gameTitle: String,
                                   imageUrl: String?,
                                   unlockedAchievements: Int,
                                   totalAchievements: Int,
                                   unlockedPoints: Int,
                                   totalPoints: Int,
                                   hasUnsupported: Bool) {
        
        NSLog("RetroAchievements: Showing game placard from callback - \(gameTitle)")
        NSLog("RetroAchievements: Achievements: \(unlockedAchievements)/\(totalAchievements), Points: \(unlockedPoints)/\(totalPoints)")
        
        // Remove existing placard
        currentPlacard?.removeFromSuperview()
        
        // Get GameViewController directly
        var gameViewController: UIViewController?
        
        // Start from root and find GameViewController
        if let rootVC = UIApplication.shared.windows.first?.rootViewController {
            gameViewController = findGameViewController(from: rootVC)
        }
        
        guard let targetViewController = gameViewController else {
            NSLog("RetroAchievements: Could not find GameViewController for placard display")
            return
        }
        
        // Get hardcore mode state from RetroAchievements manager
        let isHardcoreMode = RetroAchievementsManager.shared?.getHardcoreEnabled() ?? false
        
        // Create and show new placard
        let placard = RetroAchievementsGamePlacardView()
        placard.configure(
            gameTitle: gameTitle,
            consoleName: "Sega Saturn", // Fixed for Saturn emulator
            achievementCount: totalAchievements,
            unlockedCount: unlockedAchievements,
            isHardcoreMode: isHardcoreMode
        )
        
        placard.show(in: targetViewController.view)
        currentPlacard = placard
        
        // TODO: Load game image from imageUrl if provided
        if let imageUrl = imageUrl, !imageUrl.isEmpty {
            loadGameImage(from: imageUrl, into: placard)
        }
    }
    
    private func loadGameImage(from urlString: String, into placard: RetroAchievementsGamePlacardView) {
        guard let url = URL(string: urlString) else {
            NSLog("RetroAchievements: Invalid image URL: \(urlString)")
            return
        }
        
        URLSession.shared.dataTask(with: url) { data, response, error in
            guard let data = data,
                  let image = UIImage(data: data) else {
                NSLog("RetroAchievements: Failed to load game image: \(error?.localizedDescription ?? "Unknown error")")
                return
            }
            
            DispatchQueue.main.async {
                placard.updateGameImage(image)
            }
        }.resume()
    }
    
    func hidePlacard() {
        currentPlacard?.hide()
        currentPlacard = nil
    }
}

// MARK: - UIViewController Extension
extension UIViewController {
    func topMostViewController() -> UIViewController {
        if let presented = presentedViewController {
            return presented.topMostViewController()
        }
        if let navigation = self as? UINavigationController {
            return navigation.visibleViewController?.topMostViewController() ?? self
        }
        if let tab = self as? UITabBarController {
            return tab.selectedViewController?.topMostViewController() ?? self
        }
        return self
    }
}
