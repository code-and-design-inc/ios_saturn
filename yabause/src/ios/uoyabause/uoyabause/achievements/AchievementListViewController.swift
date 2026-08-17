import UIKit
import GameController

// MARK: - Achievement Category
enum AchievementCategory: String, CaseIterable {
    case unlocked = "unlocked"
    case locked = "locked"
    case recent = "recent"
    
    var displayName: String {
        switch self {
        case .unlocked:
            return NSLocalizedString("Unlocked", comment: "Unlocked achievements category")
        case .locked:
            return NSLocalizedString("Locked", comment: "Locked achievements category")
        case .recent:
            return NSLocalizedString("Recent", comment: "Recent achievements category")
        }
    }
}

// MARK: - Achievement Sort Order
enum AchievementSortOrder: String, CaseIterable {
    case title = "title"
    case points = "points"
    case dateUnlocked = "dateUnlocked"
    
    var displayName: String {
        switch self {
        case .title:
            return NSLocalizedString("Title", comment: "Sort by title")
        case .points:
            return NSLocalizedString("Points", comment: "Sort by points")
        case .dateUnlocked:
            return NSLocalizedString("Date Unlocked", comment: "Sort by date unlocked")
        }
    }
}

// MARK: - Achievement List View Controller
class AchievementListViewController: UIViewController {
    
    // MARK: - Properties
    private var achievements: [RAAchievement] = []
    private var filteredAchievements: [RAAchievement] = []
    private var currentCategory: AchievementCategory = .unlocked
    private var currentSortOrder: AchievementSortOrder = .title
    private var isAscending = true
    
    // GameController support
    private var gameControllerObserver: Any?
    private var selectedItemIndex: Int = 0
    
    // Overall progress UI removed - now shown in title bar
    
    // MARK: - UI Elements
    private lazy var hardcoreModeSwitch: UISwitch = {
        let switchControl = UISwitch()
        switchControl.addTarget(self, action: #selector(hardcoreModeSwitchChanged(_:)), for: .valueChanged)
        switchControl.translatesAutoresizingMaskIntoConstraints = false
        return switchControl
    }()
    
    private lazy var hardcoreModeLabel: UILabel = {
        let label = UILabel()
        label.text = NSLocalizedString("Hardcore Mode", comment: "Hardcore mode label")
        label.font = UIFont.systemFont(ofSize: 16)
        label.textColor = .adaptiveTextColor
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private lazy var hardcoreModeContainer: UIView = {
        let container = UIView()
        container.backgroundColor = .colorPrimary
        container.layer.cornerRadius = 8
        container.translatesAutoresizingMaskIntoConstraints = false
        
        container.addSubview(hardcoreModeLabel)
        container.addSubview(hardcoreModeSwitch)
        
        NSLayoutConstraint.activate([
            hardcoreModeLabel.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            hardcoreModeLabel.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            
            hardcoreModeSwitch.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            hardcoreModeSwitch.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            
            container.heightAnchor.constraint(equalToConstant: 50)
        ])
        
        return container
    }()
    
    private lazy var segmentedControl: UISegmentedControl = {
        let items = AchievementCategory.allCases.map { $0.displayName }
        let control = UISegmentedControl(items: items)
        control.selectedSegmentIndex = 0
        control.addTarget(self, action: #selector(categoryChanged(_:)), for: .valueChanged)
        control.translatesAutoresizingMaskIntoConstraints = false
        return control
    }()
    
    private lazy var searchController: UISearchController = {
        let controller = UISearchController(searchResultsController: nil)
        controller.searchResultsUpdater = self
        controller.obscuresBackgroundDuringPresentation = false
        controller.searchBar.placeholder = NSLocalizedString("Search achievements...", comment: "Achievement search placeholder")
        return controller
    }()
    
    private lazy var collectionView: UICollectionView = {
        let layout = createLayout()
        let collectionView = UICollectionView(frame: .zero, collectionViewLayout: layout)
        collectionView.backgroundColor = .defaultBackground
        collectionView.delegate = self
        collectionView.dataSource = self
        collectionView.translatesAutoresizingMaskIntoConstraints = false
        
        // Register cells
        collectionView.register(AchievementCollectionViewCell.self, forCellWithReuseIdentifier: AchievementCollectionViewCell.identifier)
        // Header registration removed - using overall progress view instead
        
        return collectionView
    }()
    
    private lazy var emptyStateView: UIView = {
        let view = UIView()
        view.backgroundColor = .defaultBackground
        view.translatesAutoresizingMaskIntoConstraints = false
        
        let imageView = UIImageView(image: UIImage(systemName: "trophy.circle"))
        imageView.tintColor = .appDisable
        imageView.contentMode = .scaleAspectFit
        imageView.translatesAutoresizingMaskIntoConstraints = false
        
        let titleLabel = UILabel()
        titleLabel.text = NSLocalizedString("No Achievements", comment: "Empty state title")
        titleLabel.font = UIFont.boldSystemFont(ofSize: 20)
        titleLabel.textColor = .appDisable
        titleLabel.textAlignment = .center
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        let messageLabel = UILabel()
        messageLabel.text = NSLocalizedString("Achievements will appear here when you start playing games", comment: "Empty state message")
        messageLabel.font = UIFont.systemFont(ofSize: 16)
        messageLabel.textColor = .appDisable
        messageLabel.textAlignment = .center
        messageLabel.numberOfLines = 0
        messageLabel.translatesAutoresizingMaskIntoConstraints = false
        
        view.addSubview(imageView)
        view.addSubview(titleLabel)
        view.addSubview(messageLabel)
        
        NSLayoutConstraint.activate([
            imageView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            imageView.centerYAnchor.constraint(equalTo: view.centerYAnchor, constant: -60),
            imageView.widthAnchor.constraint(equalToConstant: 80),
            imageView.heightAnchor.constraint(equalToConstant: 80),
            
            titleLabel.topAnchor.constraint(equalTo: imageView.bottomAnchor, constant: 20),
            titleLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            titleLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20),
            
            messageLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 8),
            messageLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            messageLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20)
        ])
        
        view.isHidden = true
        return view
    }()
    
    // MARK: - Lifecycle
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupNavigationBar()
        setupGameController()
        loadHardcoreModeState()
        loadAchievements()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        setupGameController()
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        removeGameControllerObserver()
        
        // Unpause game when leaving the achievement list
        if let gameMainVC = self.presentingViewController as? GameMainViewController {
            gameMainVC.gameVC?.isPaused = false
        }
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.backgroundColor = .defaultBackground
        
        view.addSubview(hardcoreModeContainer)
        view.addSubview(segmentedControl)
        view.addSubview(collectionView)
        view.addSubview(emptyStateView)
        
        NSLayoutConstraint.activate([
            hardcoreModeContainer.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
            hardcoreModeContainer.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            hardcoreModeContainer.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            
            segmentedControl.topAnchor.constraint(equalTo: hardcoreModeContainer.bottomAnchor, constant: 16),
            segmentedControl.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            segmentedControl.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            
            collectionView.topAnchor.constraint(equalTo: segmentedControl.bottomAnchor, constant: 16),
            collectionView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            collectionView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            collectionView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            
            emptyStateView.topAnchor.constraint(equalTo: segmentedControl.bottomAnchor),
            emptyStateView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            emptyStateView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            emptyStateView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
    }
    
    private func setupNavigationBar() {
        // Title will be set dynamically with progress info in updateUI()
        // Disable large titles for cleaner look with progress info
        navigationController?.navigationBar.prefersLargeTitles = false
        
        // Set title text attributes for better visibility
        navigationController?.navigationBar.titleTextAttributes = [
            .font: UIFont.systemFont(ofSize: 18, weight: .semibold),
            .foregroundColor: UIColor.adaptiveTextColor
        ]
        
        // Add back/close button
        if let navigationController = navigationController,
           navigationController.viewControllers.first == self {
            // Modal presentation - use close button
            navigationItem.leftBarButtonItem = UIBarButtonItem(
                barButtonSystemItem: .close,
                target: self,
                action: #selector(closeButtonTapped)
            )
        } else {
            // Push navigation - use back button with chevron.left icon
            navigationItem.leftBarButtonItem = UIBarButtonItem(
                image: UIImage(systemName: "chevron.left"),
                style: .plain,
                target: self,
                action: #selector(backButtonTapped)
            )
        }
        
        // Search controller - temporarily hidden (may be restored later)
        // navigationItem.searchController = searchController
        // navigationItem.hidesSearchBarWhenScrolling = false
        
        // Sort button - temporarily hidden (may be restored later)
        // let sortButton = UIBarButtonItem(
        //     image: UIImage(systemName: "arrow.up.arrow.down"),
        //     style: .plain,
        //     target: self,
        //     action: #selector(sortButtonTapped)
        // )
        // navigationItem.rightBarButtonItem = sortButton
    }
    
    private func createLayout() -> UICollectionViewLayout {
        let itemSize = NSCollectionLayoutSize(
            widthDimension: .fractionalWidth(1.0),
            heightDimension: .estimated(80)  // Reduced estimated height for more compact cells
        )
        let item = NSCollectionLayoutItem(layoutSize: itemSize)
        
        let groupSize = NSCollectionLayoutSize(
            widthDimension: .fractionalWidth(1.0),
            heightDimension: .estimated(80)  // Reduced estimated height
        )
        let group = NSCollectionLayoutGroup.horizontal(layoutSize: groupSize, subitems: [item])
        
        let section = NSCollectionLayoutSection(group: group)
        section.interGroupSpacing = 6
        section.contentInsets = NSDirectionalEdgeInsets(top: 8, leading: 16, bottom: 8, trailing: 16)
        
        // No header needed - using overall progress view instead
        
        return UICollectionViewCompositionalLayout(section: section)
    }
    
    // MARK: - Data Loading
    private func loadAchievements() {
        // Load achievements from rcheevos via RetroAchievements Manager
        guard let manager = RetroAchievementsManager.shared else {
            NSLog("RetroAchievementsManager not available, using mock data")
            achievements = createMockAchievements()
            applyFiltersAndSort()
            return
        }
        
        // Use RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL and RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS
        achievements = manager.createAchievementList(category: 1, grouping: 1)
        
        if achievements.isEmpty {
            NSLog("No achievements loaded from rcheevos, using mock data")
            achievements = createMockAchievements()
        } else {
            NSLog("Successfully loaded \(achievements.count) achievements from rcheevos")
        }
        
        applyFiltersAndSort()
    }
    
    private func createMockAchievements() -> [RAAchievement] {
        return [
            RAAchievement(
                id: 1,
                title: "First Steps",
                description: "Complete the first level of the game",
                badge: "",
                points: 5,
                type: 0,
                state: 1, // 1 = unlocked
                unlockTime: Date().addingTimeInterval(-86400),
                category: 0
            ),
            RAAchievement(
                id: 2, 
                title: "Speed Demon",
                description: "Complete a lap in under 2 minutes",
                badge: "",
                points: 10,
                type: 0,
                state: 1, // 1 = unlocked
                unlockTime: Date().addingTimeInterval(-3600),
                category: 0
            ),
            RAAchievement(
                id: 3,
                title: "Perfect Run",
                description: "Complete the game without losing a life",
                badge: "",
                points: 25,
                type: 0,
                state: 0, // 0 = locked
                unlockTime: nil,
                category: 0
            ),
            RAAchievement(
                id: 4,
                title: "Collector",
                description: "Collect all power-ups in a single level",
                badge: "",
                points: 15,
                type: 0,
                state: 0, // 0 = locked
                unlockTime: nil,
                category: 0
            ),
            RAAchievement(
                id: 5,
                title: "Master",
                description: "Achieve the highest score possible",
                badge: "",
                points: 50,
                type: 0,
                state: 0, // 0 = locked
                unlockTime: nil,
                category: 0
            )
        ]
    }
    
    // MARK: - Filtering and Sorting
    private func applyFiltersAndSort() {
        var filtered = achievements
        
        // Apply category filter
        switch currentCategory {
        case .unlocked:
            filtered = filtered.filter { $0.state == 2 }  // RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED
        case .locked:
            filtered = filtered.filter { $0.state != 2 }
        case .recent:
            filtered = filtered.filter { $0.state == 2 && $0.unlockTime != nil }
                .sorted { (lhs, rhs) in
                    guard let lhsDate = lhs.unlockTime,
                          let rhsDate = rhs.unlockTime else { return false }
                    return lhsDate > rhsDate
                }
        }
        
        // Apply search filter - temporarily disabled (may be restored later)
        // if let searchText = searchController.searchBar.text, !searchText.isEmpty {
        //     filtered = filtered.filter { achievement in
        //         achievement.title.localizedCaseInsensitiveContains(searchText) ||
        //         achievement.description.localizedCaseInsensitiveContains(searchText)
        //     }
        // }
        
        // Sorting disabled - always keep server order (may be restored later)
        // if currentCategory != .recent && currentSortOrder != .title {
        //     filtered.sort { (lhs, rhs) in
        //         switch currentSortOrder {
        //         case .title:
        //             return isAscending ? lhs.title < rhs.title : lhs.title > rhs.title
        //         case .points:
        //             return isAscending ? lhs.points < rhs.points : lhs.points > rhs.points
        //         case .dateUnlocked:
        //             let lhsDate = lhs.dateUnlocked ?? Date.distantPast
        //             let rhsDate = rhs.dateUnlocked ?? Date.distantPast
        //             return isAscending ? lhsDate < rhsDate : lhsDate > rhsDate
        //         }
        //     }
        // }
        
        filteredAchievements = filtered
        updateUI()
    }
    
    private func updateUI() {
        let isEmpty = filteredAchievements.isEmpty
        emptyStateView.isHidden = !isEmpty
        collectionView.isHidden = isEmpty
        
        // Update title bar with progress
        let unlockedCount = achievements.filter { $0.state == 2 }.count
        let totalCount = achievements.count
        let totalPoints = achievements.filter { $0.state == 2 }.reduce(0) { $0 + $1.points }
        
        // Check if hardcore mode is enabled
        let isHardcore = RetroAchievementsManager.shared?.getHardcoreEnabled() ?? false
        let modePrefix = isHardcore ? "[HC] " : ""
        
        // Set title with progress and trophy icon
        title = modePrefix + "🏆 " + String(format: "%d/%d Achievements %d points", unlockedCount, totalCount, totalPoints)
        
        if !isEmpty {
            collectionView.reloadData()
            // Reset gamepad selection when data changes
            selectedItemIndex = 0
            updateCollectionViewSelection()
        }
    }
    
    // MARK: - Hardcore Mode Management
    private func loadHardcoreModeState() {
        // Load hardcore mode state from RetroAchievementsManager
        guard let manager = RetroAchievementsManager.shared else {
            hardcoreModeSwitch.isOn = false
            updateHardcoreModeUI()
            return
        }
        hardcoreModeSwitch.isOn = manager.getHardcoreEnabled()
        updateHardcoreModeUI()
    }
    
    private func updateHardcoreModeUI() {
        // Update UI based on login state
        let authManager = RetroAchievementsAuthManager.shared
        let isLoggedIn = authManager.isRetroAchievementsLoggedIn
        
        hardcoreModeSwitch.isEnabled = isLoggedIn
        hardcoreModeLabel.textColor = isLoggedIn ? .adaptiveTextColor : .appDisable
        
        if !isLoggedIn {
            hardcoreModeSwitch.isOn = false
        }
    }
    
    // MARK: - Actions
    @objc private func closeButtonTapped() {
        // Dismiss modal presentation
        if let navigationController = navigationController {
            navigationController.dismiss(animated: true, completion: nil)
        } else {
            dismiss(animated: true, completion: nil)
        }
    }
    
    @objc private func backButtonTapped() {
        // Pop from navigation stack
        navigationController?.popViewController(animated: true)
    }
    
    @objc private func hardcoreModeSwitchChanged(_ sender: UISwitch) {
        guard let manager = RetroAchievementsManager.shared else { return }
        manager.setHardcoreEnabled(sender.isOn)
        
        // Reload achievements to reflect hardcore mode state changes
        // In hardcore mode, achievements may have different unlock states
        loadAchievements()
    }
    
    @objc private func categoryChanged(_ sender: UISegmentedControl) {
        currentCategory = AchievementCategory.allCases[sender.selectedSegmentIndex]
        applyFiltersAndSort()
    }
    
    @objc private func sortButtonTapped() {
        let alert = UIAlertController(
            title: NSLocalizedString("Sort Achievements", comment: "Sort dialog title"),
            message: nil,
            preferredStyle: .actionSheet
        )
        
        for sortOrder in AchievementSortOrder.allCases {
            let action = UIAlertAction(title: sortOrder.displayName, style: .default) { [weak self] _ in
                if self?.currentSortOrder == sortOrder {
                    self?.isAscending.toggle()
                } else {
                    self?.currentSortOrder = sortOrder
                    self?.isAscending = true
                }
                self?.applyFiltersAndSort()
            }
            
            if sortOrder == currentSortOrder {
                let prefix = isAscending ? "↑ " : "↓ "
                action.setValue(prefix + sortOrder.displayName, forKey: "title")
            }
            
            alert.addAction(action)
        }
        
        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel))
        
        // For iPad
        if let popover = alert.popoverPresentationController {
            popover.barButtonItem = navigationItem.rightBarButtonItem
        }
        
        present(alert, animated: true)
    }
    
    // MARK: - GameController Support
    
    private func setupGameController() {
        removeGameControllerObserver()
        
        // Register for controller connections
        gameControllerObserver = NotificationCenter.default.addObserver(
            forName: .GCControllerDidConnect,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            self?.configureGameController()
        }
        
        // Configure any already connected controllers
        configureGameController()
    }
    
    private func removeGameControllerObserver() {
        if let observer = gameControllerObserver {
            NotificationCenter.default.removeObserver(observer)
            gameControllerObserver = nil
        }
    }
    
    private func configureGameController() {
        guard let controller = GCController.controllers().first else { return }
        
        // Configure extended gamepad if available
        if let extendedGamepad = controller.extendedGamepad {
            configureExtendedGamepad(extendedGamepad)
        } else if let gamepad = controller.gamepad {
            configureGamepad(gamepad)
        }
    }
    
    private func configureExtendedGamepad(_ gamepad: GCExtendedGamepad) {
        // D-pad for list navigation
        gamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.moveSelection(up: true)
            }
        }
        
        gamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.moveSelection(up: false)
            }
        }
        
        // Left/Right triggers for category switching
        gamepad.leftTrigger.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.switchCategory(previous: true)
            }
        }
        
        gamepad.rightTrigger.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.switchCategory(previous: false)
            }
        }
        
        // Left/Right shoulder buttons as alternative for category switching
        gamepad.leftShoulder.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.switchCategory(previous: true)
            }
        }
        
        gamepad.rightShoulder.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.switchCategory(previous: false)
            }
        }
        
        // A button to show achievement detail
        gamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.showSelectedAchievementDetail()
            }
        }
        
        // B button to close
        gamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.dismiss(animated: true)
            }
        }
    }
    
    private func configureGamepad(_ gamepad: GCGamepad) {
        // D-pad for list navigation
        gamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.moveSelection(up: true)
            }
        }
        
        gamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.moveSelection(up: false)
            }
        }
        
        // Left/Right buttons for category switching
        gamepad.dpad.left.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.switchCategory(previous: true)
            }
        }
        
        gamepad.dpad.right.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.switchCategory(previous: false)
            }
        }
        
        // A button to show achievement detail
        gamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.showSelectedAchievementDetail()
            }
        }
        
        // B button to close
        gamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.dismiss(animated: true)
            }
        }
    }
    
    private func moveSelection(up: Bool) {
        guard !filteredAchievements.isEmpty else { return }
        
        let maxIndex = filteredAchievements.count - 1
        
        if up {
            selectedItemIndex = max(0, selectedItemIndex - 1)
        } else {
            selectedItemIndex = min(maxIndex, selectedItemIndex + 1)
        }
        
        updateCollectionViewSelection()
    }
    
    private func switchCategory(previous: Bool) {
        let categories = AchievementCategory.allCases
        let currentIndex = categories.firstIndex(of: currentCategory) ?? 0
        let maxIndex = categories.count - 1
        
        let newIndex: Int
        if previous {
            newIndex = currentIndex > 0 ? currentIndex - 1 : maxIndex
        } else {
            newIndex = currentIndex < maxIndex ? currentIndex + 1 : 0
        }
        
        currentCategory = categories[newIndex]
        segmentedControl.selectedSegmentIndex = newIndex
        selectedItemIndex = 0 // Reset selection to top
        applyFiltersAndSort()
    }
    
    private func updateCollectionViewSelection() {
        guard !filteredAchievements.isEmpty else { return }
        guard selectedItemIndex >= 0 && selectedItemIndex < filteredAchievements.count else { return }
        
        let indexPath = IndexPath(item: selectedItemIndex, section: 0)
        collectionView.selectItem(at: indexPath, animated: true, scrollPosition: [.centeredVertically])
    }
    
    private func showSelectedAchievementDetail() {
        guard selectedItemIndex >= 0 && selectedItemIndex < filteredAchievements.count else { return }
        
        let achievement = filteredAchievements[selectedItemIndex]
        showAchievementDetail(achievement)
    }
}

// MARK: - Collection View Data Source
extension AchievementListViewController: UICollectionViewDataSource {
    func numberOfSections(in collectionView: UICollectionView) -> Int {
        return 1
    }
    
    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return filteredAchievements.count
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueReusableCell(withReuseIdentifier: AchievementCollectionViewCell.identifier, for: indexPath) as! AchievementCollectionViewCell
        
        let achievement = filteredAchievements[indexPath.item]
        cell.configure(with: achievement)
        
        return cell
    }
    
    // No longer needed - using overall progress view instead of headers
}

// MARK: - Collection View Delegate
extension AchievementListViewController: UICollectionViewDelegate {
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        let achievement = filteredAchievements[indexPath.item]
        showAchievementDetail(achievement)
    }
    
    private func showAchievementDetail(_ achievement: RAAchievement) {
        let alert = UIAlertController(
            title: achievement.title,
            message: achievement.description,
            preferredStyle: .alert
        )
        
        if achievement.state == 2 {
            if let dateUnlocked = achievement.unlockTime {
                let formatter = DateFormatter()
                formatter.dateStyle = .medium
                formatter.timeStyle = .short
                
                let unlockedMessage = String(format: NSLocalizedString("Unlocked on %@", comment: "Achievement unlock date"), formatter.string(from: dateUnlocked))
                alert.message = (alert.message ?? "") + "\n\n" + unlockedMessage
            }
        }
        
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
}

// MARK: - Search Results Updating
extension AchievementListViewController: UISearchResultsUpdating {
    func updateSearchResults(for searchController: UISearchController) {
        applyFiltersAndSort()
    }
}

// MARK: - Achievement Collection View Cell
class AchievementCollectionViewCell: UICollectionViewCell {
    static let identifier = "AchievementCollectionViewCell"
    
    private let badgeImageView = UIImageView()
    private let titleLabel = UILabel()
    private let descriptionLabel = UILabel()
    private let pointsLabel = UILabel()
    private let unlockedIndicator = UIImageView()
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        setupUI()
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override var isSelected: Bool {
        didSet {
            updateSelectionVisuals()
        }
    }
    
    override var isHighlighted: Bool {
        didSet {
            updateSelectionVisuals()
        }
    }
    
    private func setupUI() {
        backgroundColor = .colorPrimary
        layer.cornerRadius = 12
        layer.masksToBounds = true
        
        badgeImageView.contentMode = .scaleAspectFit
        badgeImageView.backgroundColor = .colorAccent
        badgeImageView.translatesAutoresizingMaskIntoConstraints = false
        
        titleLabel.font = UIFont.boldSystemFont(ofSize: 17)
        titleLabel.textColor = .adaptiveTextColor
        titleLabel.numberOfLines = 1
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        descriptionLabel.font = UIFont.systemFont(ofSize: 13)
        descriptionLabel.textColor = .appDisable
        descriptionLabel.numberOfLines = 2
        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        
        pointsLabel.font = UIFont.boldSystemFont(ofSize: 12)
        pointsLabel.textColor = .headerTextColor
        pointsLabel.textAlignment = .center
        pointsLabel.backgroundColor = .defaultBackground
        pointsLabel.layer.cornerRadius = 5
        pointsLabel.layer.masksToBounds = true
        pointsLabel.translatesAutoresizingMaskIntoConstraints = false
        pointsLabel.numberOfLines = 1
        pointsLabel.setContentHuggingPriority(.required, for: .horizontal)
        pointsLabel.setContentCompressionResistancePriority(.required, for: .horizontal)
        
        unlockedIndicator.image = UIImage(systemName: "checkmark.circle.fill")
        unlockedIndicator.tintColor = .appYellow
        unlockedIndicator.translatesAutoresizingMaskIntoConstraints = false
        
        addSubview(badgeImageView)
        addSubview(titleLabel)
        addSubview(descriptionLabel)
        addSubview(pointsLabel)
        addSubview(unlockedIndicator)
        
        NSLayoutConstraint.activate([
            badgeImageView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 10),
            badgeImageView.topAnchor.constraint(equalTo: topAnchor, constant: 10),
            badgeImageView.widthAnchor.constraint(equalToConstant: 40),
            badgeImageView.heightAnchor.constraint(equalToConstant: 40),
            
            titleLabel.leadingAnchor.constraint(equalTo: badgeImageView.trailingAnchor, constant: 10),
            titleLabel.topAnchor.constraint(equalTo: topAnchor, constant: 10),
            titleLabel.trailingAnchor.constraint(equalTo: pointsLabel.leadingAnchor, constant: -10),
            
            descriptionLabel.leadingAnchor.constraint(equalTo: titleLabel.leadingAnchor),
            descriptionLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            descriptionLabel.trailingAnchor.constraint(equalTo: titleLabel.trailingAnchor),
            descriptionLabel.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -10),
            
            pointsLabel.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -10),
            pointsLabel.topAnchor.constraint(equalTo: topAnchor, constant: 10),
            pointsLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 20),
            pointsLabel.heightAnchor.constraint(greaterThanOrEqualToConstant: 20),
            
            unlockedIndicator.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -10),
            unlockedIndicator.topAnchor.constraint(equalTo: pointsLabel.bottomAnchor, constant: 5),
            unlockedIndicator.widthAnchor.constraint(equalToConstant: 20),
            unlockedIndicator.heightAnchor.constraint(equalToConstant: 20),
            
            // Ensure badge is at least as tall as the content
            badgeImageView.bottomAnchor.constraint(lessThanOrEqualTo: bottomAnchor, constant: -10)
        ])
    }
    
    func configure(with achievement: RAAchievement) {
        titleLabel.text = achievement.title
        descriptionLabel.text = achievement.description
        pointsLabel.text = " \(achievement.points) "
        
        unlockedIndicator.isHidden = (achievement.state != 2)
        
        if achievement.state == 2 {
            alpha = 1.0
            titleLabel.textColor = .adaptiveTextColor
            descriptionLabel.textColor = .appDisable
        } else {
            alpha = 0.6
            titleLabel.textColor = .appDisable
            descriptionLabel.textColor = .appDisable
        }
        
        // Set badge image using rcheevos API
        loadBadgeImage(for: achievement)
    }
    
    private func loadBadgeImage(for achievement: RAAchievement) {
        let state = achievement.state
        
        // Debug logging
        NSLog("Loading badge for achievement \(achievement.id): state=\(state), title='\(achievement.title)'")
        
        // Use the image cache system
        AchievementImageCache.shared.loadImage(for: String(achievement.id), state: state) { [weak self] image in
            if let image = image {
                NSLog("Successfully loaded badge image for achievement \(achievement.id)")
                self?.badgeImageView.image = image
                self?.badgeImageView.tintColor = nil
            } else {
                NSLog("Failed to load badge image for achievement \(achievement.id), using fallback system icon")
                // Fallback to system icon
                self?.setBadgeSystemIcon(for: achievement)
            }
        }
    }
    
    private func setBadgeSystemIcon(for achievement: RAAchievement) {
        let isUnlocked = (achievement.state == 2)
        NSLog("Setting system badge icon for achievement \(achievement.id): state=\(achievement.state), isUnlocked=\(isUnlocked), title='\(achievement.title)'")
        
        badgeImageView.image = UIImage(systemName: isUnlocked ? "trophy.fill" : "trophy")
        badgeImageView.tintColor = isUnlocked ? .appYellow : .appDisable
    }
    
    private func updateSelectionVisuals() {
        let isActiveSelection = isSelected || isHighlighted
        
        UIView.animate(withDuration: 0.2, delay: 0, options: [.curveEaseInOut]) {
            if isActiveSelection {
                // Selected/highlighted state
                self.backgroundColor = .tint.withAlphaComponent(0.3)
                self.layer.borderWidth = 2
                self.layer.borderColor = UIColor.tint.cgColor
                self.transform = CGAffineTransform(scaleX: 1.02, y: 1.02)
            } else {
                // Normal state
                self.backgroundColor = .colorPrimary
                self.layer.borderWidth = 0
                self.layer.borderColor = UIColor.clear.cgColor
                self.transform = CGAffineTransform.identity
            }
        }
    }
}

// Header view class removed - using overall progress view instead
