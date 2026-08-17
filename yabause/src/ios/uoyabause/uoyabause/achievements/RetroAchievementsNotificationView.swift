//
//  RetroAchievementsNotificationView.swift
//  uoyabause
//
//  Created by YabaSanshiro on 2025/01/10.
//

import UIKit

/// RetroAchievements notification display system
/// Shows achievement unlocks, leaderboard submissions, etc.
/// Design closely matches Android version for consistency

class RetroAchievementsNotificationView {
    
    // MARK: - Properties
    
    private static let shared = RetroAchievementsNotificationView()
    
    private let notificationDuration: TimeInterval = 6.0
    private let animationDuration: TimeInterval = 0.0 //0.3
    private let maxVisibleNotifications = 3
    
    private var activeNotifications: [NotificationInfo] = []
    private var notificationContainer: UIStackView?
    private weak var currentViewController: UIViewController?
    
    // Challenge indicators
    private var activeChallengeIndicators: [Int: UIView] = [:]
    private var challengeIndicatorContainer: UIStackView?
    
    // Leaderboard trackers
    private var activeLeaderboardTrackers: [Int: UIView] = [:]
    private var leaderboardTrackerContainer: UIStackView?
    
    // Progress indicators (multiple at a time, stacked vertically)
    private var activeProgressIndicators: [Int: UIView] = [:]
    private var progressIndicatorContainer: UIStackView?
    private var progressIndicatorShowTimes: [Int: Date] = [:]
    private let minimumProgressDisplayTime: TimeInterval = 1.5  // Minimum 1.5 seconds display time
    
    // Cache for downloaded images
    private let imageCache = NSCache<NSString, UIImage>()
    
    // MARK: - Data Structures
    
    private struct NotificationInfo {
        let view: UIView
        let dismissTimer: Timer?
        let id: String = UUID().uuidString
    }
    
    // MARK: - Public Interface
    
    /// Show achievement unlocked notification
    static func showAchievementUnlocked(
        achievementId: Int,
        title: String,
        description: String,
        points: Int = 0,
        imageUrl: String? = nil,
        isUnofficial: Bool = false
    ) {
        shared.showAchievementUnlockedInternal(
            achievementId: achievementId,
            title: title,
            description: description,
            points: points,
            imageUrl: imageUrl,
            isUnofficial: isUnofficial
        )
    }
    
    /// Show leaderboard submission notification
    static func showLeaderboardSubmit(
        leaderboardId: Int,
        title: String,
        description: String,
        scoreString: String
    ) {
        shared.showLeaderboardSubmitInternal(
            leaderboardId: leaderboardId,
            title: title,
            description: description,
            scoreString: scoreString
        )
    }
    
    /// Show rich presence update
    static func showRichPresenceUpdate(_ richPresence: String) {
        // For now, just log it
        print("Rich presence: \(richPresence)")
    }
    
    /// Show challenge indicator
    static func showChallengeIndicator(
        achievementId: Int,
        title: String,
        imageUrl: String? = nil
    ) {
        NSLog("DEBUG: showChallengeIndicator called with ID=\(achievementId), title='\(title)'")
        shared.showChallengeIndicatorInternal(
            achievementId: achievementId,
            title: title,
            imageUrl: imageUrl
        )
    }
    
    /// Hide challenge indicator
    static func hideChallengeIndicator(achievementId: Int) {
        shared.hideChallengeIndicatorInternal(achievementId: achievementId)
    }
    
    /// Show leaderboard tracker
    static func showLeaderboardTracker(
        trackerId: Int,
        display: String
    ) {
        shared.showLeaderboardTrackerInternal(
            trackerId: trackerId,
            display: display
        )
    }
    
    /// Hide leaderboard tracker
    static func hideLeaderboardTracker(trackerId: Int) {
        shared.hideLeaderboardTrackerInternal(trackerId: trackerId)
    }
    
    /// Update leaderboard tracker
    static func updateLeaderboardTracker(
        trackerId: Int,
        display: String
    ) {
        shared.updateLeaderboardTrackerInternal(
            trackerId: trackerId,
            display: display
        )
    }
    
    /// Show progress indicator
    static func showProgressIndicator(
        achievementId: Int,
        title: String,
        progress: String,
        percent: Double,
        imageUrl: String? = nil
    ) {
        shared.showProgressIndicatorInternal(
            achievementId: achievementId,
            title: title,
            progress: progress,
            percent: percent,
            imageUrl: imageUrl
        )
    }
    
    /// Update progress indicator
    static func updateProgressIndicator(
        achievementId: Int,
        title: String,
        progress: String,
        percent: Double,
        imageUrl: String? = nil
    ) {
        shared.updateProgressIndicatorInternal(
            achievementId: achievementId,
            title: title,
            progress: progress,
            percent: percent,
            imageUrl: imageUrl
        )
    }
    
    /// Hide progress indicator
    static func hideProgressIndicator() {
        shared.hideProgressIndicatorInternal()
    }
    
    /// Show login success notification
    static func showLoginSuccess(username: String, displayName: String? = nil, points: Int = 0) {
        shared.showLoginSuccessInternal(
            username: username,
            displayName: displayName,
            points: points
        )
    }
    
    /// Show game placard notification
    static func showGamePlacard(
        gameTitle: String,
        imageUrl: String? = nil,
        unlockedAchievements: Int = 0,
        totalAchievements: Int = 0
    ) {
        shared.showGamePlacardInternal(
            gameTitle: gameTitle,
            imageUrl: imageUrl,
            unlockedAchievements: unlockedAchievements,
            totalAchievements: totalAchievements
        )
    }
    
    /// Show game mastery notification
    static func showGameMastery(
        gameTitle: String,
        imageUrl: String? = nil,
        achievementCount: Int = 0,
        points: Int = 0,
        isHardcore: Bool = false,
        username: String? = nil,
        playtime: String? = nil
    ) {
        shared.showGameMasteryInternal(
            gameTitle: gameTitle,
            imageUrl: imageUrl,
            achievementCount: achievementCount,
            points: points,
            isHardcore: isHardcore,
            username: username,
            playtime: playtime
        )
    }
    
    /// Show server error notification
    static func showServerError(_ errorMessage: String) {
        shared.showServerErrorInternal(errorMessage)
    }
    
    /// Set the current view controller for displaying notifications
    static func setViewController(_ viewController: UIViewController?) {
        shared.currentViewController = viewController
    }
    
    /// Clean up resources
    static func cleanup() {
        shared.cleanupInternal()
    }
    
    // MARK: - Internal Implementation
    
    private func showAchievementUnlockedInternal(
        achievementId: Int,
        title: String,
        description: String,
        points: Int,
        imageUrl: String?,
        isUnofficial: Bool
    ) {
        let notificationView = createAchievementNotification(
            title: title,
            description: description,
            points: points,
            imageUrl: imageUrl,
            isUnofficial: isUnofficial
        )
        
        addNotificationToStack(notificationView)
        
        // Download image if URL provided
        if let imageUrl = imageUrl {
            downloadImage(from: imageUrl, cacheKey: "ach_\(achievementId)") { [weak self] image in
                guard let self = self, let image = image else { return }
                
                DispatchQueue.main.async {
                    // Update the image view in the notification
                    if let imageView = notificationView.viewWithTag(100) as? UIImageView {
                        imageView.image = image
                    }
                }
            }
        }
    }
    
    private func showLeaderboardSubmitInternal(
        leaderboardId: Int,
        title: String,
        description: String,
        scoreString: String
    ) {
        let notificationView = createLeaderboardNotification(
            title: title,
            description: description,
            scoreString: scoreString
        )
        
        addNotificationToStack(notificationView)
        
        // Submit to Firebase if score is not empty
        if !scoreString.isEmpty {
            let numericScore = parseScoreString(scoreString)
            // TODO: Submit to Firebase
        }
    }
    
    private func showLoginSuccessInternal(username: String, displayName: String?, points: Int) {
        let title = "RetroAchievements Login"
        let message = "Logged in as \(displayName ?? username)\(points > 0 ? " (\(points) points)" : "")"
        
        let notificationView = createLoginSuccessNotification(title: title, message: message)
        addNotificationToStack(notificationView)
    }
    
    private func showGamePlacardInternal(
        gameTitle: String,
        imageUrl: String?,
        unlockedAchievements: Int,
        totalAchievements: Int
    ) {
        let notificationView = createGamePlacardNotification(
            gameTitle: gameTitle,
            unlockedAchievements: unlockedAchievements,
            totalAchievements: totalAchievements
        )
        
        addNotificationToStack(notificationView)
        
        // Download game image if URL provided
        if let imageUrl = imageUrl {
            downloadImage(from: imageUrl, cacheKey: "game_\(gameTitle.hashValue)") { [weak self] image in
                guard let self = self, let image = image else { return }
                
                DispatchQueue.main.async {
                    if let imageView = notificationView.viewWithTag(100) as? UIImageView {
                        imageView.image = image
                    }
                }
            }
        }
    }
    
    private func showGameMasteryInternal(
        gameTitle: String,
        imageUrl: String?,
        achievementCount: Int,
        points: Int,
        isHardcore: Bool,
        username: String?,
        playtime: String?
    ) {
        let notificationView = createGameMasteryNotification(
            gameTitle: gameTitle,
            achievementCount: achievementCount,
            points: points,
            isHardcore: isHardcore,
            username: username,
            playtime: playtime
        )
        
        addNotificationToStack(notificationView)
        
        // Download game image if URL provided
        if let imageUrl = imageUrl {
            downloadImage(from: imageUrl, cacheKey: "game_\(gameTitle.hashValue)") { [weak self] image in
                guard let self = self, let image = image else { return }
                
                DispatchQueue.main.async {
                    if let imageView = notificationView.viewWithTag(100) as? UIImageView {
                        imageView.image = image
                    }
                }
            }
        }
    }
    
    private func showServerErrorInternal(_ errorMessage: String) {
        let notificationView = createServerErrorNotification(errorMessage)
        addNotificationToStack(notificationView)
    }
    
    private func showChallengeIndicatorInternal(
        achievementId: Int,
        title: String,
        imageUrl: String?
    ) {
        NSLog("DEBUG: showChallengeIndicatorInternal called with ID=\(achievementId), title='\(title)'")
        NSLog("DEBUG: currentViewController is \(currentViewController != nil ? "set" : "nil")")
        
        // If challenge indicator already exists, remove it first
        if activeChallengeIndicators[achievementId] != nil {
            NSLog("DEBUG: Challenge indicator \(achievementId) already exists, removing old one")
            hideChallengeIndicatorInternal(achievementId: achievementId)
        }
        
        NSLog("DEBUG: Creating challenge indicator")
        
        let indicatorView = createChallengeIndicator(
            achievementId: achievementId,
            title: title,
            imageUrl: imageUrl
        )
        
        NSLog("DEBUG: Challenge indicator view created, adding to container")
        
        activeChallengeIndicators[achievementId] = indicatorView
        addChallengeIndicatorToContainer(indicatorView)
        
        // Download image if URL provided
        if let imageUrl = imageUrl {
            downloadImage(from: imageUrl, cacheKey: "challenge_\(achievementId)") { [weak self] image in
                guard let self = self, let image = image else { return }
                
                DispatchQueue.main.async {
                    // Update the image view in the indicator
                    if let imageView = indicatorView.viewWithTag(100) as? UIImageView {
                        imageView.image = image
                    }
                }
            }
        }
    }
    
    private func hideChallengeIndicatorInternal(achievementId: Int) {
        guard let indicatorView = activeChallengeIndicators[achievementId] else { return }
        
        DispatchQueue.main.async { [weak self] in
            UIView.animate(withDuration: 0.2, animations: {
                indicatorView.alpha = 0
                indicatorView.transform = CGAffineTransform(scaleX: 0.8, y: 0.8)
            }) { _ in
                indicatorView.removeFromSuperview()
                self?.activeChallengeIndicators.removeValue(forKey: achievementId)
            }
        }
    }
    
    // MARK: - Notification Stack Management
    
    private func addNotificationToStack(_ notificationView: UIView) {
        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            
            // Ensure notification container exists
            if self.notificationContainer == nil {
                self.createNotificationContainer()
            }
            
            // Remove oldest notification if we've reached the limit
            if self.activeNotifications.count >= self.maxVisibleNotifications {
                self.removeOldestNotification()
            }
            
            // Set up auto-dismiss timer
            let dismissTimer = Timer.scheduledTimer(withTimeInterval: self.notificationDuration, repeats: false) { _ in
                self.removeNotification(notificationView)
            }
            
            // Create notification info
            let notificationInfo = NotificationInfo(view: notificationView, dismissTimer: dismissTimer)
            self.activeNotifications.append(notificationInfo)
            
            // Add to container and animate in
            self.showNotificationInStack(notificationView)
        }
    }
    
    private func createNotificationContainer() {
        guard let viewController = currentViewController else {
            print("No view controller set for notifications")
            return
        }
        
        let container = UIStackView()
        container.axis = .vertical
        container.spacing = 8
        container.alignment = .leading  // Left-aligned for top-left positioning
        container.distribution = .equalSpacing
        container.translatesAutoresizingMaskIntoConstraints = false
        
        viewController.view.addSubview(container)
        
        NSLayoutConstraint.activate([
            container.topAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.topAnchor, constant: 20),
            container.leadingAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.leadingAnchor, constant: 20),
            container.trailingAnchor.constraint(lessThanOrEqualTo: viewController.view.trailingAnchor, constant: -20)
        ])
        
        notificationContainer = container
    }
    
    private func showNotificationInStack(_ notificationView: UIView) {
        guard let container = notificationContainer else {
            print("Notification container not available")
            return
        }
        
        // Start with notification off-screen (slide from top)
        notificationView.transform = CGAffineTransform(translationX: 0, y: -200)
        notificationView.alpha = 0
        
        // Add to container
        container.addArrangedSubview(notificationView)
        
        // UIStackView with .center alignment will handle positioning automatically
        // No additional constraints needed when using center alignment
        
        // Animate in (slide down from top)
        UIView.animate(withDuration: animationDuration) {
            notificationView.transform = .identity
            notificationView.alpha = 1
        }
    }
    
    private func removeNotification(_ notificationView: UIView) {
        // Find and remove the notification info
        if let index = activeNotifications.firstIndex(where: { $0.view == notificationView }) {
            let notificationInfo = activeNotifications[index]
            notificationInfo.dismissTimer?.invalidate()
            activeNotifications.remove(at: index)
        }
        
        // Animate out and remove from container
        UIView.animate(withDuration: animationDuration, animations: {
            notificationView.transform = CGAffineTransform(translationX: 0, y: -200)
            notificationView.alpha = 0
        }) { _ in
            notificationView.removeFromSuperview()
            
            // If no more notifications, remove the container
            if self.activeNotifications.isEmpty {
                self.cleanupNotificationContainer()
            }
        }
    }
    
    private func removeOldestNotification() {
        guard !activeNotifications.isEmpty else { return }
        
        let oldestNotification = activeNotifications.first!
        removeNotification(oldestNotification.view)
    }
    
    private func cleanupNotificationContainer() {
        notificationContainer?.removeFromSuperview()
        notificationContainer = nil
    }

    
    // MARK: - Notification View Creation
    
    private func createAchievementNotification(
        title: String,
        description: String,
        points: Int,
        imageUrl: String?,
        isUnofficial: Bool
    ) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        
        // Background with border (Android-style)
        let borderColor = isUnofficial ? UIColor(hex: "#FFA500") : UIColor(hex: "#FFD700")
        container.backgroundColor = UIColor.black.withAlphaComponent(0.85)
        container.layer.cornerRadius = 8
        container.layer.borderWidth = 2
        container.layer.borderColor = borderColor.cgColor
        
        // Icon
        let iconView = UIImageView()
        iconView.translatesAutoresizingMaskIntoConstraints = false
        iconView.contentMode = .scaleAspectFit
        iconView.tag = 100 // For later image update
        iconView.image = UIImage(systemName: "star.fill")
        iconView.tintColor = borderColor
        container.addSubview(iconView)
        
        // Header label
        let headerLabel = UILabel()
        headerLabel.translatesAutoresizingMaskIntoConstraints = false
        headerLabel.text = isUnofficial ? "Unofficial Achievement Unlocked!" : "Achievement Unlocked!"
        headerLabel.font = .systemFont(ofSize: 10, weight: .bold)
        headerLabel.textColor = UIColor(hex: "#AAAAAA")
        container.addSubview(headerLabel)
        
        // Title label
        let titleLabel = UILabel()
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.text = title
        titleLabel.font = .systemFont(ofSize: 14, weight: .bold)
        titleLabel.textColor = .white
        titleLabel.numberOfLines = 1
        container.addSubview(titleLabel)
        
        // Description label
        let descriptionLabel = UILabel()
        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        descriptionLabel.text = description
        descriptionLabel.font = .systemFont(ofSize: 11)
        descriptionLabel.textColor = UIColor(hex: "#CCCCCC")
        descriptionLabel.numberOfLines = 2
        container.addSubview(descriptionLabel)
        
        // Points label (if points > 0)
        var pointsLabel: UILabel?
        if points > 0 {
            let label = UILabel()
            label.translatesAutoresizingMaskIntoConstraints = false
            label.text = "\(points)pts"
            label.font = .systemFont(ofSize: 10, weight: .bold)
            label.textColor = borderColor
            container.addSubview(label)
            pointsLabel = label
        }
        
        // Set up constraints (Android-style layout)
        let iconWidthConstraint = iconView.widthAnchor.constraint(equalToConstant: 48)
        iconWidthConstraint.priority = UILayoutPriority(999)
        
        let iconHeightConstraint = iconView.heightAnchor.constraint(equalToConstant: 48)
        iconHeightConstraint.priority = UILayoutPriority(999)
        
        NSLayoutConstraint.activate([
            container.widthAnchor.constraint(lessThanOrEqualToConstant: 320),
            container.widthAnchor.constraint(greaterThanOrEqualToConstant: 200),
            container.heightAnchor.constraint(greaterThanOrEqualToConstant: 80),
            
            iconView.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 12),
            iconView.topAnchor.constraint(equalTo: container.topAnchor, constant: 12),
            iconWidthConstraint,
            iconHeightConstraint,
            
            headerLabel.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 8),
            headerLabel.topAnchor.constraint(equalTo: container.topAnchor, constant: 8),
            headerLabel.trailingAnchor.constraint(lessThanOrEqualTo: container.trailingAnchor, constant: -50),
            
            titleLabel.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 8),
            titleLabel.topAnchor.constraint(equalTo: headerLabel.bottomAnchor, constant: 4),
            titleLabel.trailingAnchor.constraint(lessThanOrEqualTo: container.trailingAnchor, constant: -50),
            
            descriptionLabel.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 8),
            descriptionLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            descriptionLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -12),
            descriptionLabel.bottomAnchor.constraint(lessThanOrEqualTo: container.bottomAnchor, constant: -12)
        ])
        
        if let pointsLabel = pointsLabel {
            NSLayoutConstraint.activate([
                pointsLabel.topAnchor.constraint(equalTo: container.topAnchor, constant: 8),
                pointsLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -12)
            ])
        }
        
        return container
    }
    
    private func createLeaderboardNotification(
        title: String,
        description: String,
        scoreString: String
    ) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        
        // Background with blue border (Android-style)
        let borderColor = UIColor(hex: "#00BFFF")
        container.backgroundColor = UIColor.black.withAlphaComponent(0.85)
        container.layer.cornerRadius = 16
        container.layer.borderWidth = 2
        container.layer.borderColor = borderColor.cgColor
        
        // Icon (smaller for compact layout like Android)
        let iconView = UIImageView()
        iconView.translatesAutoresizingMaskIntoConstraints = false
        iconView.contentMode = .scaleAspectFit
        iconView.image = UIImage(systemName: "list.number")  // Better leaderboard icon
        iconView.tintColor = borderColor
        container.addSubview(iconView)
        
        // Text container for better organization
        let textContainer = UIView()
        textContainer.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(textContainer)
        
        // Header label (matches Android)
        let headerLabel = UILabel()
        headerLabel.translatesAutoresizingMaskIntoConstraints = false
        headerLabel.text = "LEADERBOARD"
        headerLabel.font = .systemFont(ofSize: 10, weight: .bold)
        headerLabel.textColor = UIColor(hex: "#AAAAAA")
        textContainer.addSubview(headerLabel)
        
        // Title label
        let titleLabel = UILabel()
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.text = title
        titleLabel.font = .systemFont(ofSize: 14, weight: .bold)
        titleLabel.textColor = .white
        titleLabel.numberOfLines = 1
        textContainer.addSubview(titleLabel)
        
        // Description label
        let descriptionLabel = UILabel()
        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        descriptionLabel.text = description
        descriptionLabel.font = .systemFont(ofSize: 11)
        descriptionLabel.textColor = UIColor(hex: "#CCCCCC")
        descriptionLabel.numberOfLines = 2
        textContainer.addSubview(descriptionLabel)
        
        // Score label (positioned below description) - Larger and more prominent
        let scoreLabel = UILabel()
        scoreLabel.translatesAutoresizingMaskIntoConstraints = false
        let displayScore = (!scoreString.isEmpty && scoreString != "0") ? scoreString : "Submitting..."
        scoreLabel.text = displayScore
        scoreLabel.font = .systemFont(ofSize: 18, weight: .bold)  // Large for prominence
        scoreLabel.textColor = borderColor
        scoreLabel.textAlignment = .center
        scoreLabel.numberOfLines = 1
        textContainer.addSubview(scoreLabel)
        
        // Set up constraints (matching Android layout more closely)
        let iconWidthConstraint = iconView.widthAnchor.constraint(equalToConstant: 48)
        iconWidthConstraint.priority = UILayoutPriority(750)
        
        let iconHeightConstraint = iconView.heightAnchor.constraint(equalToConstant: 48)
        iconHeightConstraint.priority = UILayoutPriority(750)
        
        NSLayoutConstraint.activate([
            // Container constraints - Wider to accommodate full text display
            container.widthAnchor.constraint(lessThanOrEqualToConstant: 400),
            container.widthAnchor.constraint(greaterThanOrEqualToConstant: 280),
            container.heightAnchor.constraint(greaterThanOrEqualToConstant: 110), // Increased for score label
            
            // Icon constraints (smaller, matching Android)
            iconView.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 12),
            iconView.topAnchor.constraint(equalTo: container.topAnchor, constant: 12),
            iconWidthConstraint,
            iconHeightConstraint,
            
            // Text container constraints - Full width minus icon
            textContainer.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 8),
            textContainer.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -12),
            textContainer.topAnchor.constraint(equalTo: container.topAnchor, constant: 8),
            textContainer.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -8),
            
            // Header label constraints
            headerLabel.leadingAnchor.constraint(equalTo: textContainer.leadingAnchor),
            headerLabel.trailingAnchor.constraint(equalTo: textContainer.trailingAnchor),
            headerLabel.topAnchor.constraint(equalTo: textContainer.topAnchor, constant: 2),
            
            // Title label constraints
            titleLabel.leadingAnchor.constraint(equalTo: textContainer.leadingAnchor),
            titleLabel.trailingAnchor.constraint(equalTo: textContainer.trailingAnchor),
            titleLabel.topAnchor.constraint(equalTo: headerLabel.bottomAnchor, constant: 2),
            
            // Description label constraints
            descriptionLabel.leadingAnchor.constraint(equalTo: textContainer.leadingAnchor),
            descriptionLabel.trailingAnchor.constraint(equalTo: textContainer.trailingAnchor),
            descriptionLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 2),
            
            // Score label constraints (below description, centered)
            scoreLabel.leadingAnchor.constraint(equalTo: textContainer.leadingAnchor),
            scoreLabel.trailingAnchor.constraint(equalTo: textContainer.trailingAnchor),
            scoreLabel.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: 6),
            scoreLabel.bottomAnchor.constraint(lessThanOrEqualTo: textContainer.bottomAnchor, constant: -2)
        ])
        
        return container
    }
    
    private func createLoginSuccessNotification(title: String, message: String) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        
        // Background with green border (Android-style)
        container.backgroundColor = UIColor(hex: "#1B5E20").withAlphaComponent(0.85)
        container.layer.cornerRadius = 8
        container.layer.borderWidth = 2
        container.layer.borderColor = UIColor(hex: "#4CAF50").cgColor
        
        // Title label
        let titleLabel = UILabel()
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.text = title
        titleLabel.font = .systemFont(ofSize: 14, weight: .bold)
        titleLabel.textColor = .white
        container.addSubview(titleLabel)
        
        // Message label
        let messageLabel = UILabel()
        messageLabel.translatesAutoresizingMaskIntoConstraints = false
        messageLabel.text = message
        messageLabel.font = .systemFont(ofSize: 12)
        messageLabel.textColor = .white
        messageLabel.numberOfLines = 0
        container.addSubview(messageLabel)
        
        NSLayoutConstraint.activate([
            container.widthAnchor.constraint(lessThanOrEqualToConstant: 280),
            
            titleLabel.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            titleLabel.topAnchor.constraint(equalTo: container.topAnchor, constant: 16),
            titleLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            
            messageLabel.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            messageLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 8),
            messageLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            messageLabel.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16)
        ])
        
        return container
    }
    
    private func createGamePlacardNotification(
        gameTitle: String,
        unlockedAchievements: Int,
        totalAchievements: Int
    ) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        
        // Background with blue/purple border (Android-style)
        container.backgroundColor = UIColor(hex: "#1A237E").withAlphaComponent(0.85)
        container.layer.cornerRadius = 8
        container.layer.borderWidth = 2
        container.layer.borderColor = UIColor(hex: "#3F51B5").cgColor
        
        // Image view
        let imageView = UIImageView()
        imageView.translatesAutoresizingMaskIntoConstraints = false
        imageView.contentMode = .scaleAspectFit
        imageView.tag = 100 // For later image update
        imageView.image = UIImage(systemName: "gamecontroller.fill")
        imageView.tintColor = UIColor(hex: "#3F51B5")
        container.addSubview(imageView)
        
        // Title label
        let titleLabel = UILabel()
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.text = gameTitle
        titleLabel.font = .systemFont(ofSize: 14, weight: .bold)
        titleLabel.textColor = .white
        titleLabel.numberOfLines = 2
        container.addSubview(titleLabel)
        
        // Progress label
        let progressLabel = UILabel()
        progressLabel.translatesAutoresizingMaskIntoConstraints = false
        progressLabel.text = totalAchievements == 0 ?
            "No achievements available" :
            "\(unlockedAchievements) of \(totalAchievements) achievements unlocked"
        progressLabel.font = .systemFont(ofSize: 12)
        progressLabel.textColor = .white
        container.addSubview(progressLabel)
        
        // Set up constraints with priority-based height management
        let containerWidthConstraint = container.widthAnchor.constraint(lessThanOrEqualToConstant: 320)
        containerWidthConstraint.priority = UILayoutPriority(750)
        
        let containerMinHeightConstraint = container.heightAnchor.constraint(greaterThanOrEqualToConstant: 80)
        containerMinHeightConstraint.priority = UILayoutPriority(500)
        
        let imageWidthConstraint = imageView.widthAnchor.constraint(equalToConstant: 48)
        imageWidthConstraint.priority = UILayoutPriority(750)
        
        let imageHeightConstraint = imageView.heightAnchor.constraint(equalToConstant: 48)
        imageHeightConstraint.priority = UILayoutPriority(750)
        
        let bottomConstraint = progressLabel.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16)
        bottomConstraint.priority = UILayoutPriority(750)
        
        NSLayoutConstraint.activate([
            containerWidthConstraint,
            containerMinHeightConstraint,
            
            imageView.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            imageView.topAnchor.constraint(greaterThanOrEqualTo: container.topAnchor, constant: 16),
            imageView.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            imageWidthConstraint,
            imageHeightConstraint,
            
            titleLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: 12),
            titleLabel.topAnchor.constraint(equalTo: container.topAnchor, constant: 16),
            titleLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            
            progressLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: 12),
            progressLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            progressLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            bottomConstraint
        ])
        
        return container
    }
    
    private func createGameMasteryNotification(
        gameTitle: String,
        achievementCount: Int,
        points: Int,
        isHardcore: Bool,
        username: String?,
        playtime: String?
    ) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        
        // Background with special mastery colors (Android-style)
        let borderColor = isHardcore ? UIColor(hex: "#FF6B35") : UIColor(hex: "#FFD700")
        let backgroundColor = isHardcore ? UIColor(hex: "#4A148C") : UIColor(hex: "#1A237E")
        container.backgroundColor = backgroundColor.withAlphaComponent(0.85)
        container.layer.cornerRadius = 10
        container.layer.borderWidth = 3
        container.layer.borderColor = borderColor.cgColor
        
        // Image view
        let imageView = UIImageView()
        imageView.translatesAutoresizingMaskIntoConstraints = false
        imageView.contentMode = .scaleAspectFit
        imageView.tag = 100 // For later image update
        imageView.image = UIImage(systemName: "trophy.fill")
        imageView.tintColor = borderColor
        container.addSubview(imageView)
        
        // Header label
        let headerLabel = UILabel()
        headerLabel.translatesAutoresizingMaskIntoConstraints = false
        headerLabel.text = isHardcore ? "🏆 HARDCORE MASTERY!" : "🎉 GAME MASTERED!"
        headerLabel.font = .systemFont(ofSize: 12, weight: .bold)
        headerLabel.textColor = borderColor
        container.addSubview(headerLabel)
        
        // Title label
        let titleLabel = UILabel()
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.text = gameTitle
        titleLabel.font = .systemFont(ofSize: 16, weight: .bold)
        titleLabel.textColor = .white
        titleLabel.numberOfLines = 2
        container.addSubview(titleLabel)
        
        // Achievement info label
        let achievementLabel = UILabel()
        achievementLabel.translatesAutoresizingMaskIntoConstraints = false
        achievementLabel.text = "\(achievementCount) achievements • \(points) points"
        achievementLabel.font = .systemFont(ofSize: 11)
        achievementLabel.textColor = UIColor(hex: "#CCCCCC")
        container.addSubview(achievementLabel)
        
        // User info label (if provided)
        var userInfoLabel: UILabel?
        if username != nil || playtime != nil {
            let label = UILabel()
            label.translatesAutoresizingMaskIntoConstraints = false
            var info = ""
            if let username = username {
                info += username
            }
            if username != nil && playtime != nil {
                info += " | "
            }
            if let playtime = playtime {
                info += "Playtime: \(playtime)"
            }
            label.text = info
            label.font = .systemFont(ofSize: 10)
            label.textColor = UIColor(hex: "#AAAAAA")
            container.addSubview(label)
            userInfoLabel = label
        }
        
        // Set up constraints
        // Create flexible width constraint for imageView
        let imageWidthConstraint = imageView.widthAnchor.constraint(equalToConstant: 64)
        imageWidthConstraint.priority = UILayoutPriority(750) // Lower priority to avoid conflicts
        
        let imageHeightConstraint = imageView.heightAnchor.constraint(equalToConstant: 64)
        imageHeightConstraint.priority = UILayoutPriority(750) // Lower priority to avoid conflicts
        
        // Add minimum size constraints with even lower priority
        let imageMinWidthConstraint = imageView.widthAnchor.constraint(greaterThanOrEqualToConstant: 32)
        imageMinWidthConstraint.priority = UILayoutPriority(500)
        
        let imageMinHeightConstraint = imageView.heightAnchor.constraint(greaterThanOrEqualToConstant: 32)
        imageMinHeightConstraint.priority = UILayoutPriority(500)
        
        var constraints = [
            container.widthAnchor.constraint(lessThanOrEqualToConstant: 350),
            container.widthAnchor.constraint(greaterThanOrEqualToConstant: 200), // Minimum width to fit content
            container.heightAnchor.constraint(greaterThanOrEqualToConstant: 100),
            
            imageView.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            imageView.topAnchor.constraint(equalTo: container.topAnchor, constant: 16),
            imageWidthConstraint,
            imageHeightConstraint,
            imageMinWidthConstraint,
            imageMinHeightConstraint,
            
            headerLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: 12),
            headerLabel.topAnchor.constraint(equalTo: container.topAnchor, constant: 12),
            headerLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            
            titleLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: 12),
            titleLabel.topAnchor.constraint(equalTo: headerLabel.bottomAnchor, constant: 4),
            titleLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            
            achievementLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: 12),
            achievementLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            achievementLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16)
        ]
        
        if let userInfoLabel = userInfoLabel {
            constraints.append(contentsOf: [
                userInfoLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: 12),
                userInfoLabel.topAnchor.constraint(equalTo: achievementLabel.bottomAnchor, constant: 2),
                userInfoLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
                userInfoLabel.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16)
            ])
        } else {
            constraints.append(
                achievementLabel.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16)
            )
        }
        
        NSLayoutConstraint.activate(constraints)
        
        return container
    }
    
    private func createServerErrorNotification(_ errorMessage: String) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        
        // Background with red border (Android-style)
        container.backgroundColor = UIColor(hex: "#B71C1C").withAlphaComponent(0.85)
        container.layer.cornerRadius = 8
        container.layer.borderWidth = 2
        container.layer.borderColor = UIColor(hex: "#F44336").cgColor
        
        // Error icon
        let iconView = UIImageView()
        iconView.translatesAutoresizingMaskIntoConstraints = false
        iconView.image = UIImage(systemName: "exclamationmark.triangle.fill")
        iconView.tintColor = UIColor(hex: "#F44336")
        container.addSubview(iconView)
        
        // Header label
        let headerLabel = UILabel()
        headerLabel.translatesAutoresizingMaskIntoConstraints = false
        headerLabel.text = "Server Error"
        headerLabel.font = .systemFont(ofSize: 12, weight: .bold)
        headerLabel.textColor = UIColor(hex: "#F44336")
        container.addSubview(headerLabel)
        
        // Message label
        let messageLabel = UILabel()
        messageLabel.translatesAutoresizingMaskIntoConstraints = false
        messageLabel.text = errorMessage
        messageLabel.font = .systemFont(ofSize: 11)
        messageLabel.textColor = .white
        messageLabel.numberOfLines = 3
        container.addSubview(messageLabel)
        
        NSLayoutConstraint.activate([
            container.widthAnchor.constraint(lessThanOrEqualToConstant: 320),
            
            iconView.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            iconView.topAnchor.constraint(equalTo: container.topAnchor, constant: 16),
            iconView.widthAnchor.constraint(equalToConstant: 32),
            iconView.heightAnchor.constraint(equalToConstant: 32),
            
            headerLabel.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 12),
            headerLabel.topAnchor.constraint(equalTo: container.topAnchor, constant: 12),
            headerLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            
            messageLabel.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 12),
            messageLabel.topAnchor.constraint(equalTo: headerLabel.bottomAnchor, constant: 4),
            messageLabel.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            messageLabel.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16)
        ])
        
        return container
    }
    
    // MARK: - Helper Methods
    
    private func parseScoreString(_ scoreString: String) -> Int64 {
        // Handle common score formats from RetroAchievements
        if scoreString.contains(":") {
            // Time format: "01:23.45" or "1:23:45.67"
            let parts = scoreString.split(separator: ":")
            let seconds = Double(parts.last ?? "0") ?? 0
            let minutes = Int64(parts.dropLast().last ?? "0") ?? 0
            let hours = parts.count > 2 ? Int64(parts[0]) ?? 0 : 0
            
            // Convert to milliseconds for precision
            return ((hours * 3600 + minutes * 60) * 1000 + Int64(seconds * 1000))
        } else if scoreString.contains(",") {
            // Score with commas: "123,456"
            return Int64(scoreString.replacingOccurrences(of: ",", with: "")) ?? 0
        } else {
            // Plain number: "12345"
            return Int64(scoreString.trimmingCharacters(in: .whitespaces)) ?? 0
        }
    }
    
    private func downloadImage(from urlString: String, cacheKey: String, completion: @escaping (UIImage?) -> Void) {
        // Check cache first
        if let cachedImage = imageCache.object(forKey: cacheKey as NSString) {
            completion(cachedImage)
            return
        }
        
        guard let url = URL(string: urlString) else {
            completion(nil)
            return
        }
        
        URLSession.shared.dataTask(with: url) { [weak self] data, response, error in
            guard let self = self,
                  let data = data,
                  let image = UIImage(data: data) else {
                completion(nil)
                return
            }
            
            // Cache the image
            self.imageCache.setObject(image, forKey: cacheKey as NSString)
            completion(image)
        }.resume()
    }
    
    // MARK: - Challenge Indicator Management
    
    private func createChallengeIndicator(
        achievementId: Int,
        title: String,
        imageUrl: String?
    ) -> UIView {
        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        container.backgroundColor = UIColor.black.withAlphaComponent(0.7)
        container.layer.cornerRadius = 20
        container.layer.borderWidth = 2
        container.layer.borderColor = UIColor.systemYellow.cgColor
        
        // Icon
        let iconView = UIImageView()
        iconView.translatesAutoresizingMaskIntoConstraints = false
        iconView.contentMode = .scaleAspectFit
        iconView.tag = 100 // For later image update
        iconView.image = UIImage(systemName: "star.fill")
        iconView.tintColor = .systemYellow
        container.addSubview(iconView)
        
        NSLayoutConstraint.activate([
            container.widthAnchor.constraint(equalToConstant: 40),
            container.heightAnchor.constraint(equalToConstant: 40),
            
            iconView.centerXAnchor.constraint(equalTo: container.centerXAnchor),
            iconView.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            iconView.widthAnchor.constraint(equalToConstant: 24),
            iconView.heightAnchor.constraint(equalToConstant: 24)
        ])
        
        return container
    }
    
    private func addChallengeIndicatorToContainer(_ indicatorView: UIView) {
        NSLog("DEBUG: addChallengeIndicatorToContainer called")
        
        DispatchQueue.main.async { [weak self] in
            NSLog("DEBUG: Inside main thread for addChallengeIndicatorToContainer")
            
            guard let self = self,
                  let viewController = self.currentViewController else {
                NSLog("DEBUG: No self or viewController - self=\(self != nil), vc=\(self?.currentViewController != nil)")
                return
            }
            
            NSLog("DEBUG: Setting up challenge indicator container")
            self.setupChallengeIndicatorContainer(in: viewController)
            
            NSLog("DEBUG: Adding indicator to container")
            // Add the indicator to the container
            self.challengeIndicatorContainer?.addArrangedSubview(indicatorView)
            
            NSLog("DEBUG: Starting animation for challenge indicator")
            // Animate in
            indicatorView.alpha = 0
            indicatorView.transform = CGAffineTransform(scaleX: 0.5, y: 0.5)
            
            UIView.animate(withDuration: 0.3) {
                indicatorView.alpha = 1
                indicatorView.transform = .identity
            } completion: { _ in
                NSLog("DEBUG: Challenge indicator animation completed")
            }
        }
    }
    
    private func setupChallengeIndicatorContainer(in viewController: UIViewController) {
        guard challengeIndicatorContainer == nil else { return }
        
        let container = UIStackView()
        container.axis = .vertical
        container.alignment = .leading
        container.spacing = 8
        container.translatesAutoresizingMaskIntoConstraints = false
        
        viewController.view.addSubview(container)
        challengeIndicatorContainer = container
        
        NSLayoutConstraint.activate([
            container.leadingAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            container.bottomAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.bottomAnchor, constant: -100),
            container.widthAnchor.constraint(equalToConstant: 40)
        ])
    }
    
    private func cleanupInternal() {
        // Invalidate all timers
        for notification in activeNotifications {
            notification.dismissTimer?.invalidate()
        }
        activeNotifications.removeAll()
        
        // Clean up notification container
        cleanupNotificationContainer()
        
        // Clean up challenge indicators
        challengeIndicatorContainer?.removeFromSuperview()
        challengeIndicatorContainer = nil
        activeChallengeIndicators.removeAll()
        
        // Clean up leaderboard trackers
        leaderboardTrackerContainer?.removeFromSuperview()
        leaderboardTrackerContainer = nil
        activeLeaderboardTrackers.removeAll()
        
        // Clean up progress indicators
        progressIndicatorContainer?.removeFromSuperview()
        progressIndicatorContainer = nil
        activeProgressIndicators.removeAll()
        progressIndicatorShowTimes.removeAll()
        
        // Clear image cache
        imageCache.removeAllObjects()
    }
    
    // MARK: - Progress Indicator Internal Methods
    
    private func showProgressIndicatorInternal(
        achievementId: Int,
        title: String,
        progress: String,
        percent: Double,
        imageUrl: String?
    ) {
        NSLog("DEBUG: showProgressIndicatorInternal called with achievementId=\(achievementId), title='\(title)', progress='\(progress)'")
        
        guard let viewController = currentViewController ?? getRootViewController() else {
            NSLog("RetroAchievements: No view controller available for progress indicator")
            return
        }
        
        self.currentViewController = viewController
        
        DispatchQueue.main.async {
            // Setup progress indicator container
            self.setupProgressIndicatorContainer(in: viewController)
            
            // Remove existing progress indicator with same ID if it exists
            if let existingIndicator = self.activeProgressIndicators[achievementId] {
                existingIndicator.removeFromSuperview()
                self.activeProgressIndicators.removeValue(forKey: achievementId)
                self.progressIndicatorShowTimes.removeValue(forKey: achievementId)
            }
            
            // Create new progress indicator view
            let progressView = self.createProgressIndicatorView(
                achievementId: achievementId,
                title: title,
                progress: progress,
                percent: percent,
                imageUrl: imageUrl
            )
            
            // Add to container (UIStackView handles vertical arrangement)
            self.progressIndicatorContainer?.addArrangedSubview(progressView)
            self.activeProgressIndicators[achievementId] = progressView
            self.progressIndicatorShowTimes[achievementId] = Date()  // Record show time
            
            
            NSLog("DEBUG: Progress indicator added to container: \(title) - \(progress) (Total indicators: \(self.activeProgressIndicators.count))")
        }
    }
    
    private func updateProgressIndicatorInternal(
        achievementId: Int,
        title: String,
        progress: String,
        percent: Double,
        imageUrl: String?
    ) {
        NSLog("DEBUG: updateProgressIndicatorInternal called with achievementId=\(achievementId), progress='\(progress)'")
        
        DispatchQueue.main.async {
            if let progressView = self.activeProgressIndicators[achievementId] {
                // Find the progress label and update it
                for subview in progressView.subviews {
                    if let label = subview as? UILabel, label.tag == 200 {  // Progress text label
                        label.text = progress
                        NSLog("DEBUG: Updated progress indicator text for achievement \(achievementId): \(progress)")
                        break
                    }
                }
                
                // Update progress bar if available
                for subview in progressView.subviews {
                    if let progressBar = subview as? UIProgressView {
                        progressBar.progress = Float(percent / 100.0)
                        NSLog("DEBUG: Updated progress indicator bar for achievement \(achievementId): \(percent)%")
                        break
                    }
                }
            } else {
                // If progress indicator doesn't exist, create it
                NSLog("DEBUG: Progress indicator for achievement \(achievementId) not found, creating new one")
                self.showProgressIndicatorInternal(
                    achievementId: achievementId,
                    title: title,
                    progress: progress,
                    percent: percent,
                    imageUrl: imageUrl
                )
            }
        }
    }
    
    private func hideProgressIndicatorInternal() {
        NSLog("DEBUG: hideProgressIndicatorInternal called - hiding all progress indicators")
        
        DispatchQueue.main.async {
            for (achievementId, progressView) in self.activeProgressIndicators {
                let currentTime = Date()
                let timeSinceShow = self.progressIndicatorShowTimes[achievementId]?.timeIntervalSince(currentTime) ?? 0
                let elapsedTime = abs(timeSinceShow)
                
                NSLog("DEBUG: Progress indicator \(achievementId) has been shown for \(elapsedTime) seconds")
                
                if elapsedTime < self.minimumProgressDisplayTime {
                    let remainingTime = self.minimumProgressDisplayTime - elapsedTime
                    NSLog("DEBUG: Progress indicator \(achievementId) needs to stay visible for \(remainingTime) more seconds")
                    
                    // Delay hiding until minimum display time is reached
                    DispatchQueue.main.asyncAfter(deadline: .now() + remainingTime) {
                        self.performProgressIndicatorHide(achievementId: achievementId, progressView: progressView)
                    }
                } else {
                    // Can hide immediately
                    self.performProgressIndicatorHide(achievementId: achievementId, progressView: progressView)
                }
            }
        }
    }
    
    private func hideSpecificProgressIndicator(achievementId: Int) {
        NSLog("DEBUG: hideSpecificProgressIndicator called for achievement \(achievementId)")
        
        DispatchQueue.main.async {
            guard let progressView = self.activeProgressIndicators[achievementId] else {
                NSLog("DEBUG: Progress indicator \(achievementId) not found")
                return
            }
            
            let currentTime = Date()
            let timeSinceShow = self.progressIndicatorShowTimes[achievementId]?.timeIntervalSince(currentTime) ?? 0
            let elapsedTime = abs(timeSinceShow)
            
            NSLog("DEBUG: Progress indicator \(achievementId) has been shown for \(elapsedTime) seconds")
            
            if elapsedTime < self.minimumProgressDisplayTime {
                let remainingTime = self.minimumProgressDisplayTime - elapsedTime
                NSLog("DEBUG: Progress indicator \(achievementId) needs to stay visible for \(remainingTime) more seconds")
                
                // Delay hiding until minimum display time is reached
                DispatchQueue.main.asyncAfter(deadline: .now() + remainingTime) {
                    self.performProgressIndicatorHide(achievementId: achievementId, progressView: progressView)
                }
            } else {
                // Can hide immediately
                self.performProgressIndicatorHide(achievementId: achievementId, progressView: progressView)
            }
        }
    }
    
    private func performProgressIndicatorHide(achievementId: Int, progressView: UIView) {
        UIView.animate(withDuration: self.animationDuration, animations: {
            progressView.alpha = 0.0
        }) { _ in
            progressView.removeFromSuperview()
            self.activeProgressIndicators.removeValue(forKey: achievementId)
            self.progressIndicatorShowTimes.removeValue(forKey: achievementId)
            
            // Remove container if no progress indicators left
            if self.activeProgressIndicators.isEmpty {
                self.progressIndicatorContainer?.removeFromSuperview()
                self.progressIndicatorContainer = nil
            }
        }
        
        NSLog("DEBUG: Progress indicator \(achievementId) removed (Remaining: \(self.activeProgressIndicators.count))")
    }
    
    private func setupProgressIndicatorContainer(in viewController: UIViewController) {
        guard progressIndicatorContainer == nil else { return }
        
        let container = UIStackView()
        container.axis = .vertical
        container.alignment = .trailing  // Right-aligned progress indicators
        container.spacing = 8  // Space between multiple progress indicators
        container.translatesAutoresizingMaskIntoConstraints = false
        
        viewController.view.addSubview(container)
        
        // Position in top-right area to avoid game screen interference
        NSLayoutConstraint.activate([
            container.trailingAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.trailingAnchor, constant: -20),
            container.topAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.topAnchor, constant: 20),
            container.widthAnchor.constraint(equalToConstant: 60)  // Fixed width instead of proportional
        ])
        
        progressIndicatorContainer = container
    }
    
    private func createProgressIndicatorView(
        achievementId: Int,
        title: String,
        progress: String,
        percent: Double,
        imageUrl: String?
    ) -> UIView {
        let containerView = UIView()
        containerView.backgroundColor = UIColor.black.withAlphaComponent(0.50)
        containerView.layer.cornerRadius = 12
        containerView.layer.borderWidth = 2
        containerView.layer.borderColor = UIColor(hex: "FFD700").cgColor  // Gold border
        containerView.translatesAutoresizingMaskIntoConstraints = false
        
        
        // Title label
        let titleLabel = UILabel()
        titleLabel.text = title
        titleLabel.textColor = .white
        titleLabel.font = UIFont.boldSystemFont(ofSize: 10)
        titleLabel.numberOfLines = 3
        titleLabel.tag = 100  // Title label
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        // Progress text label
        let progressLabel = UILabel()
        progressLabel.text = progress
        progressLabel.textColor = UIColor(hex: "FFD700")  // Gold color for progress text
        progressLabel.font = UIFont.systemFont(ofSize: 8, weight: .medium)
        progressLabel.numberOfLines = 1
        progressLabel.tag = 200  // Progress text label
        progressLabel.translatesAutoresizingMaskIntoConstraints = false
        
        // Progress bar
        let progressBar = UIProgressView(progressViewStyle: .default)
        progressBar.progress = Float(percent / 100.0)
        progressBar.progressTintColor = UIColor(hex: "FFD700")
        progressBar.trackTintColor = UIColor.darkGray
        progressBar.layer.cornerRadius = 3
        progressBar.clipsToBounds = true
        progressBar.translatesAutoresizingMaskIntoConstraints = false
        
        containerView.addSubview(titleLabel)
        containerView.addSubview(progressLabel)
        containerView.addSubview(progressBar)
        
        NSLayoutConstraint.activate([
            // Title label constraints
            titleLabel.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 8),
            titleLabel.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -8),
            titleLabel.topAnchor.constraint(equalTo: containerView.topAnchor, constant: 6),
            
            // Progress text label constraints
            progressLabel.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 8),
            progressLabel.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -8),
            progressLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 2),
            
            // Progress bar constraints
            progressBar.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 8),
            progressBar.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -8),
            progressBar.topAnchor.constraint(equalTo: progressLabel.bottomAnchor, constant: 2),
            progressBar.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -6),
            progressBar.heightAnchor.constraint(equalToConstant: 6),
            
            // Container size constraints - remove fixed width to allow flexible sizing
            //containerView.heightAnchor.constraint(equalToConstant: 50)
        ])
        
        return containerView
    }
    
    // MARK: - Utility Methods
    
    private func getRootViewController() -> UIViewController? {
        // Try to get the key window's root view controller
        if let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
           let keyWindow = windowScene.windows.first(where: { $0.isKeyWindow }),
           let rootViewController = keyWindow.rootViewController {
            return getTopViewController(from: rootViewController)
        }
        
        // Fallback for older iOS versions
        if let keyWindow = UIApplication.shared.windows.first(where: { $0.isKeyWindow }),
           let rootViewController = keyWindow.rootViewController {
            return getTopViewController(from: rootViewController)
        }
        
        return nil
    }
    
    private func getTopViewController(from viewController: UIViewController) -> UIViewController {
        if let presentedViewController = viewController.presentedViewController {
            return getTopViewController(from: presentedViewController)
        }
        
        if let navigationController = viewController as? UINavigationController {
            if let visibleViewController = navigationController.visibleViewController {
                return getTopViewController(from: visibleViewController)
            }
        }
        
        if let tabBarController = viewController as? UITabBarController {
            if let selectedViewController = tabBarController.selectedViewController {
                return getTopViewController(from: selectedViewController)
            }
        }
        
        return viewController
    }
    
    // MARK: - Leaderboard Tracker Internal Methods
    
    private func showLeaderboardTrackerInternal(trackerId: Int, display: String) {
        NSLog("DEBUG: showLeaderboardTrackerInternal called with trackerId=\(trackerId), display='\(display)'")
        
        guard let viewController = currentViewController ?? getRootViewController() else {
            NSLog("RetroAchievements: No view controller available for leaderboard tracker")
            return
        }
        
        self.currentViewController = viewController
        
        DispatchQueue.main.async {
            self.setupLeaderboardTrackerContainer(in: viewController)
            
            // Remove existing tracker with same ID
            if let existingTracker = self.activeLeaderboardTrackers[trackerId] {
                existingTracker.removeFromSuperview()
                self.activeLeaderboardTrackers.removeValue(forKey: trackerId)
            }
            
            // Create new tracker view
            let trackerView = self.createLeaderboardTrackerView(display: display)
            
            // Add to container
            self.leaderboardTrackerContainer?.addArrangedSubview(trackerView)
            self.activeLeaderboardTrackers[trackerId] = trackerView
            
            NSLog("DEBUG: Leaderboard tracker added to container: \(display)")
        }
    }
    
    private func hideLeaderboardTrackerInternal(trackerId: Int) {
        NSLog("DEBUG: hideLeaderboardTrackerInternal called with trackerId=\(trackerId)")
        
        DispatchQueue.main.async {
            if let trackerView = self.activeLeaderboardTrackers[trackerId] {
                UIView.animate(withDuration: self.animationDuration, animations: {
                    trackerView.alpha = 0.0
                }) { _ in
                    trackerView.removeFromSuperview()
                    self.activeLeaderboardTrackers.removeValue(forKey: trackerId)
                    
                    // Remove container if no trackers left
                    if self.activeLeaderboardTrackers.isEmpty {
                        self.leaderboardTrackerContainer?.removeFromSuperview()
                        self.leaderboardTrackerContainer = nil
                    }
                }
                
                NSLog("DEBUG: Leaderboard tracker removed with ID: \(trackerId)")
            }
        }
    }
    
    private func updateLeaderboardTrackerInternal(trackerId: Int, display: String) {
        NSLog("DEBUG: updateLeaderboardTrackerInternal called with trackerId=\(trackerId), display='\(display)'")
        
        DispatchQueue.main.async {
            if let trackerView = self.activeLeaderboardTrackers[trackerId] {
                // Find the display label and update it
                for subview in trackerView.subviews {
                    if let label = subview as? UILabel {
                        label.text = display
                        NSLog("DEBUG: Updated leaderboard tracker text: \(display)")
                        break
                    }
                }
            } else {
                // If tracker doesn't exist, create it
                NSLog("DEBUG: Tracker not found, creating new one")
                self.showLeaderboardTrackerInternal(trackerId: trackerId, display: display)
            }
        }
    }
    
    private func setupLeaderboardTrackerContainer(in viewController: UIViewController) {
        guard leaderboardTrackerContainer == nil else { return }
        
        let container = UIStackView()
        container.axis = .vertical
        container.alignment = .trailing  // Right-aligned like typical game trackers
        container.spacing = 4
        container.translatesAutoresizingMaskIntoConstraints = false
        
        viewController.view.addSubview(container)
        
        // Position in top-right corner, below any notch/status bar
        let topOffset: CGFloat = 100  // Account for status bar and safe area
        
        NSLayoutConstraint.activate([
            container.trailingAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.trailingAnchor, constant: -16),
            container.topAnchor.constraint(equalTo: viewController.view.safeAreaLayoutGuide.topAnchor, constant: topOffset)
        ])
        
        leaderboardTrackerContainer = container
    }
    
    private func createLeaderboardTrackerView(display: String) -> UIView {
        let containerView = UIView()
        containerView.backgroundColor = UIColor.black.withAlphaComponent(0.8)
        containerView.layer.cornerRadius = 8
        containerView.translatesAutoresizingMaskIntoConstraints = false
        
        let displayLabel = UILabel()
        displayLabel.text = display
        displayLabel.textColor = .white
        displayLabel.font = UIFont.monospacedSystemFont(ofSize: 14, weight: .medium)  // Fixed-width font as recommended
        displayLabel.numberOfLines = 1
        displayLabel.translatesAutoresizingMaskIntoConstraints = false
        
        containerView.addSubview(displayLabel)
        
        NSLayoutConstraint.activate([
            displayLabel.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 12),
            displayLabel.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -12),
            displayLabel.topAnchor.constraint(equalTo: containerView.topAnchor, constant: 8),
            displayLabel.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -8),
            
            // Minimum size constraints
            containerView.heightAnchor.constraint(greaterThanOrEqualToConstant: 32),
            containerView.widthAnchor.constraint(greaterThanOrEqualToConstant: 80)
        ])
        
        return containerView
    }
}

// MARK: - UIColor Extension

extension UIColor {
    convenience init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3: // RGB (12-bit)
            (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
        case 6: // RGB (24-bit)
            (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        case 8: // ARGB (32-bit)
            (a, r, g, b) = (int >> 24, int >> 16 & 0xFF, int >> 8 & 0xFF, int & 0xFF)
        default:
            (a, r, g, b) = (255, 0, 0, 0)
        }
        
        self.init(
            red: CGFloat(r) / 255,
            green: CGFloat(g) / 255,
            blue: CGFloat(b) / 255,
            alpha: CGFloat(a) / 255
        )
    }
}
