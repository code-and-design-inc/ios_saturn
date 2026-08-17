import UIKit
import FirebaseAuth
import FirebaseFirestore
import FirebaseCore
import FirebaseDatabase
import FirebaseStorage
import NotificationCenter
import GoogleSignIn
import GameController

// DiscordOAuthRedirectHandlerからの通知を使用するためのimport
@_exported import class UIKit.UIViewController
@_exported import struct Foundation.Notification

class UserProfileViewController: UIViewController {
    
    // MARK: - GameController Support
    private var gameController: GCController?

    // MARK: - UI Components
    private let scrollView: UIScrollView = {
        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        scroll.showsVerticalScrollIndicator = true
        return scroll
    }()
    
    private let stackView: UIStackView = {
        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 24
        stack.alignment = .fill
        stack.distribution = .fill
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }()

    private let userImageView: UIImageView = {
        let imageView = UIImageView()
        imageView.contentMode = .scaleAspectFill
        imageView.clipsToBounds = true
        imageView.layer.cornerRadius = 60
        imageView.backgroundColor = .systemGray6
        imageView.layer.borderWidth = 3
        imageView.layer.borderColor = UIColor.systemGray4.cgColor
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
    }()

    private let userNameLabel: UILabel = {
        let label = UILabel()
        label.font = .systemFont(ofSize: 28, weight: .bold)
        label.textAlignment = .center
        label.textColor = .label
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private let userEmailLabel: UILabel = {
        let label = UILabel()
        label.font = .systemFont(ofSize: 16, weight: .regular)
        label.textAlignment = .center
        label.textColor = .secondaryLabel
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()

    private let logoutButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Sign out", comment: "Button title for sign out"), for: .normal)
        button.backgroundColor = .systemGray5
        button.setTitleColor(.label, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 17, weight: .medium)
        button.layer.cornerRadius = 12
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()

    private let deleteAccountButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Delete Account", comment: "Button title for account deletion"), for: .normal)
        button.backgroundColor = .systemRed.withAlphaComponent(0.1)
        button.setTitleColor(.systemRed, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 17, weight: .medium)
        button.layer.cornerRadius = 12
        button.layer.borderWidth = 1
        button.layer.borderColor = UIColor.systemRed.cgColor
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()

    private let discordStatusLabel: UILabel = {
        let label = UILabel()
        label.font = .systemFont(ofSize: 16, weight: .medium)
        label.textAlignment = .left
        label.textColor = .label
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()

    private let discordIconView: UIImageView = {
        let imageView = UIImageView()
        imageView.image = UIImage(named: "Discord-Symbol-Blurple")
        imageView.contentMode = .scaleAspectFit
        imageView.tintColor = UIColor(red: 88/255.0, green: 101/255.0, blue: 242/255.0, alpha: 1.0)
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
    }()

    private let discordLinkButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Connect Discord", comment: "Button title for Discord linking"), for: .normal)
        button.backgroundColor = UIColor(red: 88/255.0, green: 101/255.0, blue: 242/255.0, alpha: 1.0)
        button.setTitleColor(.white, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 16, weight: .semibold)
        button.layer.cornerRadius = 12
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()

    private let discordUnlinkButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Disconnect", comment: "Button title for Discord unlinking"), for: .normal)
        button.backgroundColor = .systemGray5
        button.setTitleColor(.label, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 16, weight: .medium)
        button.layer.cornerRadius = 12
        button.translatesAutoresizingMaskIntoConstraints = false
        button.isHidden = true
        return button
    }()

    // Section containers for better organization
    private let profileSectionView: UIView = {
        let view = UIView()
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }()
    
    private let connectionsSectionView: UIView = {
        let view = UIView()
        view.backgroundColor = .secondarySystemBackground
        view.layer.cornerRadius = 16
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }()
    
    private let accountSectionView: UIView = {
        let view = UIView()
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }()
    
    private let sectionTitleLabel: UILabel = {
        let label = UILabel()
        label.text = NSLocalizedString("Connected Accounts", comment: "Section title for connected accounts")
        label.font = .systemFont(ofSize: 20, weight: .semibold)
        label.textColor = .label
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private lazy var discordContainerView: UIView = {
        let containerView = UIView()
        containerView.translatesAutoresizingMaskIntoConstraints = false
        containerView.backgroundColor = .tertiarySystemBackground
        containerView.layer.cornerRadius = 12
        
        // Create horizontal stack for icon and status
        let hStack = UIStackView()
        hStack.axis = .horizontal
        hStack.spacing = 12
        hStack.alignment = .center
        hStack.translatesAutoresizingMaskIntoConstraints = false
        
        // Add icon and status label to stack
        hStack.addArrangedSubview(discordIconView)
        hStack.addArrangedSubview(discordStatusLabel)
        
        // Add buttons to container
        containerView.addSubview(hStack)
        containerView.addSubview(discordLinkButton)
        containerView.addSubview(discordUnlinkButton)

        NSLayoutConstraint.activate([
            hStack.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 16),
            hStack.topAnchor.constraint(equalTo: containerView.topAnchor, constant: 16),
            hStack.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor, constant: -16),
            
            discordIconView.widthAnchor.constraint(equalToConstant: 32),
            discordIconView.heightAnchor.constraint(equalToConstant: 32),
            
            discordLinkButton.topAnchor.constraint(equalTo: hStack.bottomAnchor, constant: 12),
            discordLinkButton.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 16),
            discordLinkButton.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -16),
            discordLinkButton.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -16),
            discordLinkButton.heightAnchor.constraint(equalToConstant: 44),
            
            discordUnlinkButton.topAnchor.constraint(equalTo: hStack.bottomAnchor, constant: 12),
            discordUnlinkButton.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 16),
            discordUnlinkButton.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -16),
            discordUnlinkButton.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -16),
            discordUnlinkButton.heightAnchor.constraint(equalToConstant: 44)
        ])

        return containerView
    }()

    // MARK: - RetroAchievements UI Components
    private let retroAchievementsStatusLabel: UILabel = {
        let label = UILabel()
        label.font = .systemFont(ofSize: 16, weight: .medium)
        label.textAlignment = .left
        label.textColor = .label
        label.numberOfLines = 0
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()

    private let retroAchievementsIconView: UIImageView = {
        let imageView = UIImageView()
        imageView.contentMode = .scaleAspectFit
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
    }()

    private let retroAchievementsLoginButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Connect", comment: "Button title for RetroAchievements linking"), for: .normal)
        button.backgroundColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
        button.setTitleColor(.white, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 16, weight: .semibold)
        button.layer.cornerRadius = 12
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()

    private let retroAchievementsLogoutButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Disconnect", comment: "Button title for RetroAchievements unlinking"), for: .normal)
        button.backgroundColor = .systemGray5
        button.setTitleColor(.label, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 16, weight: .medium)
        button.layer.cornerRadius = 12
        button.translatesAutoresizingMaskIntoConstraints = false
        button.isHidden = true
        return button
    }()

    // RA Settings button removed as requested

    private lazy var retroAchievementsContainerView: UIView = {
        let containerView = UIView()
        containerView.translatesAutoresizingMaskIntoConstraints = false
        containerView.backgroundColor = .tertiarySystemBackground
        containerView.layer.cornerRadius = 12
        
        // Create horizontal stack for icon and status
        let hStack = UIStackView()
        hStack.axis = .horizontal
        hStack.spacing = 12
        hStack.alignment = .center
        hStack.translatesAutoresizingMaskIntoConstraints = false
        
        // Add icon and status label to stack
        hStack.addArrangedSubview(retroAchievementsIconView)
        hStack.addArrangedSubview(retroAchievementsStatusLabel)
        
        // Add stack and buttons to container
        containerView.addSubview(hStack)
        containerView.addSubview(retroAchievementsLoginButton)
        containerView.addSubview(retroAchievementsLogoutButton)

        NSLayoutConstraint.activate([
            hStack.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 16),
            hStack.topAnchor.constraint(equalTo: containerView.topAnchor, constant: 16),
            hStack.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor, constant: -16),
            
            retroAchievementsIconView.widthAnchor.constraint(equalToConstant: 32),
            retroAchievementsIconView.heightAnchor.constraint(equalToConstant: 32),
            
            retroAchievementsLoginButton.topAnchor.constraint(equalTo: hStack.bottomAnchor, constant: 12),
            retroAchievementsLoginButton.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 16),
            retroAchievementsLoginButton.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -16),
            retroAchievementsLoginButton.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -16),
            retroAchievementsLoginButton.heightAnchor.constraint(equalToConstant: 44),
            
            retroAchievementsLogoutButton.topAnchor.constraint(equalTo: hStack.bottomAnchor, constant: 12),
            retroAchievementsLogoutButton.leadingAnchor.constraint(equalTo: containerView.leadingAnchor, constant: 16),
            retroAchievementsLogoutButton.trailingAnchor.constraint(equalTo: containerView.trailingAnchor, constant: -16),
            retroAchievementsLogoutButton.bottomAnchor.constraint(equalTo: containerView.bottomAnchor, constant: -16),
            retroAchievementsLogoutButton.heightAnchor.constraint(equalToConstant: 44)
        ])

        return containerView
    }()

    // MARK: - Lifecycle
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupNavigationBar()
        updateUserInfo()

        // Discord連携の通知を購読
        setupNotifications()
        
        // GameController設定
        setupGameController()
    }

    deinit {
        // 通知の購読を解除
        NotificationCenter.default.removeObserver(self)
    }

    // MARK: - Notifications
    private func setupNotifications() {
        // Discord連携成功の通知を購読
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleDiscordLinkSuccess),
            name: Notification.Name("discordLinkSuccess"),
            object: nil
        )

        // Discord連携失敗の通知を購読
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleDiscordLinkFailure),
            name: Notification.Name("discordLinkFailure"),
            object: nil
        )
    }

    @objc private func handleDiscordLinkSuccess() {
        // ユーザー情報を更新して、Discord連携状態を反映
        if let userId = Auth.auth().currentUser?.uid {
            checkDiscordLinkStatus(userId: userId)
        }
    }

    @objc private func handleDiscordLinkFailure() {
        // 失敗時は特に何もしない（アラートは既に表示されている）
        // 必要に応じて追加の処理を実装
    }

    // MARK: - UI Setup
    private func setupUI() {
        view.backgroundColor = .systemBackground
        title = NSLocalizedString("Profile", comment: "Title for profile screen")

        // Add scroll view
        view.addSubview(scrollView)
        scrollView.addSubview(stackView)
        
        // Setup profile section
        profileSectionView.addSubview(userImageView)
        profileSectionView.addSubview(userNameLabel)
        profileSectionView.addSubview(userEmailLabel)
        
        // Setup connections section
        connectionsSectionView.addSubview(sectionTitleLabel)
        connectionsSectionView.addSubview(discordContainerView)
        connectionsSectionView.addSubview(retroAchievementsContainerView)
        
        // Setup account section
        accountSectionView.addSubview(logoutButton)
        accountSectionView.addSubview(deleteAccountButton)
        
        // Add sections to stack view
        stackView.addArrangedSubview(profileSectionView)
        stackView.addArrangedSubview(connectionsSectionView)
        stackView.addArrangedSubview(accountSectionView)

        NSLayoutConstraint.activate([
            // Scroll view constraints
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            
            // Stack view constraints
            stackView.topAnchor.constraint(equalTo: scrollView.topAnchor, constant: 20),
            stackView.leadingAnchor.constraint(equalTo: scrollView.leadingAnchor, constant: 20),
            stackView.trailingAnchor.constraint(equalTo: scrollView.trailingAnchor, constant: -20),
            stackView.bottomAnchor.constraint(equalTo: scrollView.bottomAnchor, constant: -20),
            stackView.widthAnchor.constraint(equalTo: scrollView.widthAnchor, constant: -40),
            
            // Profile section
            profileSectionView.heightAnchor.constraint(greaterThanOrEqualToConstant: 200),
            userImageView.centerXAnchor.constraint(equalTo: profileSectionView.centerXAnchor),
            userImageView.topAnchor.constraint(equalTo: profileSectionView.topAnchor),
            userImageView.widthAnchor.constraint(equalToConstant: 120),
            userImageView.heightAnchor.constraint(equalToConstant: 120),
            
            userNameLabel.topAnchor.constraint(equalTo: userImageView.bottomAnchor, constant: 16),
            userNameLabel.leadingAnchor.constraint(equalTo: profileSectionView.leadingAnchor),
            userNameLabel.trailingAnchor.constraint(equalTo: profileSectionView.trailingAnchor),
            
            userEmailLabel.topAnchor.constraint(equalTo: userNameLabel.bottomAnchor, constant: 4),
            userEmailLabel.leadingAnchor.constraint(equalTo: profileSectionView.leadingAnchor),
            userEmailLabel.trailingAnchor.constraint(equalTo: profileSectionView.trailingAnchor),
            userEmailLabel.bottomAnchor.constraint(equalTo: profileSectionView.bottomAnchor),
            
            // Connections section
            sectionTitleLabel.topAnchor.constraint(equalTo: connectionsSectionView.topAnchor, constant: 20),
            sectionTitleLabel.leadingAnchor.constraint(equalTo: connectionsSectionView.leadingAnchor, constant: 20),
            sectionTitleLabel.trailingAnchor.constraint(equalTo: connectionsSectionView.trailingAnchor, constant: -20),
            
            discordContainerView.topAnchor.constraint(equalTo: sectionTitleLabel.bottomAnchor, constant: 16),
            discordContainerView.leadingAnchor.constraint(equalTo: connectionsSectionView.leadingAnchor, constant: 16),
            discordContainerView.trailingAnchor.constraint(equalTo: connectionsSectionView.trailingAnchor, constant: -16),
            
            retroAchievementsContainerView.topAnchor.constraint(equalTo: discordContainerView.bottomAnchor, constant: 12),
            retroAchievementsContainerView.leadingAnchor.constraint(equalTo: connectionsSectionView.leadingAnchor, constant: 16),
            retroAchievementsContainerView.trailingAnchor.constraint(equalTo: connectionsSectionView.trailingAnchor, constant: -16),
            retroAchievementsContainerView.bottomAnchor.constraint(equalTo: connectionsSectionView.bottomAnchor, constant: -20),
            
            // Account section
            accountSectionView.heightAnchor.constraint(greaterThanOrEqualToConstant: 120),
            logoutButton.topAnchor.constraint(equalTo: accountSectionView.topAnchor, constant: 20),
            logoutButton.leadingAnchor.constraint(equalTo: accountSectionView.leadingAnchor),
            logoutButton.trailingAnchor.constraint(equalTo: accountSectionView.trailingAnchor),
            logoutButton.heightAnchor.constraint(equalToConstant: 50),
            
            deleteAccountButton.topAnchor.constraint(equalTo: logoutButton.bottomAnchor, constant: 12),
            deleteAccountButton.leadingAnchor.constraint(equalTo: accountSectionView.leadingAnchor),
            deleteAccountButton.trailingAnchor.constraint(equalTo: accountSectionView.trailingAnchor),
            deleteAccountButton.heightAnchor.constraint(equalToConstant: 50),
            deleteAccountButton.bottomAnchor.constraint(equalTo: accountSectionView.bottomAnchor)
        ])

        logoutButton.addTarget(self, action: #selector(logoutButtonTapped), for: .touchUpInside)
        discordLinkButton.addTarget(self, action: #selector(discordLinkButtonTapped), for: .touchUpInside)
        discordUnlinkButton.addTarget(self, action: #selector(discordUnlinkButtonTapped), for: .touchUpInside)
        retroAchievementsLoginButton.addTarget(self, action: #selector(retroAchievementsLoginButtonTapped), for: .touchUpInside)
        retroAchievementsLogoutButton.addTarget(self, action: #selector(retroAchievementsLogoutButtonTapped), for: .touchUpInside)
        deleteAccountButton.addTarget(self, action: #selector(deleteAccountButtonTapped), for: .touchUpInside)
    }

    private func setupNavigationBar() {
        let closeButton = UIBarButtonItem(title: NSLocalizedString("Close", comment: "Close Dialog"), style: .plain, target: self, action: #selector(closeButtonTapped))
        navigationItem.leftBarButtonItem = closeButton
    }

    // MARK: - User Info
    private func updateUserInfo() {
        // Firebaseが初期化されているか確認
        if FirebaseApp.app() == nil {
            // GoogleService-Info.plistからFirebaseを初期化
            var filePath: String?

            //#if DEBUG
            //filePath = Bundle.main.path(forResource: "GoogleService-Info-Debug", ofType: "plist")
            //#else
            filePath = Bundle.main.path(forResource: "GoogleService-Info", ofType: "plist")
            //#endif

            if let filePath = filePath, let options = FirebaseOptions(contentsOfFile: filePath) {
                FirebaseApp.configure(options: options)
            } else {
                print("Error: Couldn't find correct GoogleService-Info.plist file.")
                dismiss(animated: true)
                return
            }
        }

        guard let user = Auth.auth().currentUser else {
            dismiss(animated: true)
            return
        }

        // ユーザー名の設定
        userNameLabel.text = user.displayName ?? NSLocalizedString("Anonymous User", comment: "Default name for users without a display name")
        
        // メールアドレスの設定
        userEmailLabel.text = user.email ?? ""

        // プロフィール画像の設定
        if let photoURL = user.photoURL {
            // URLから画像を読み込む（実際のアプリではKingfisherなどのライブラリを使うことが推奨されます）
            DispatchQueue.global().async {
                if let data = try? Data(contentsOf: photoURL), let image = UIImage(data: data) {
                    DispatchQueue.main.async {
                        self.userImageView.image = image
                    }
                } else {
                    // 画像の読み込みに失敗した場合はデフォルト画像を設定
                    DispatchQueue.main.async {
                        self.userImageView.image = UIImage(systemName: "person.circle.fill")
                        self.userImageView.tintColor = .systemBlue
                    }
                }
            }
        } else {
            // プロフィール画像がない場合はデフォルト画像を設定
            userImageView.image = UIImage(systemName: "person.circle.fill")
            userImageView.tintColor = .systemBlue
        }

        // Discordアカウント連携状態の確認
        checkDiscordLinkStatus(userId: user.uid)
        
        // RetroAchievementsアカウント連携状態の確認
        checkRetroAchievementsStatus()
    }

    private func checkDiscordLinkStatus(userId: String) {
        // Firestoreからユーザーのディスコード連携情報を取得
        let db = Firestore.firestore()
        db.collection("discord_links").document(userId).getDocument { [weak self] (document, error) in
            guard let self = self else { return }

            if let document = document, document.exists, let _ = document.data()?["discord_id"] as? String {
                // ディスコードアカウントが連携されている場合
                DispatchQueue.main.async {
                    self.discordStatusLabel.text = NSLocalizedString("Linked", comment: "Status for linked Discord account")
                    self.discordLinkButton.isHidden = true
                    self.discordUnlinkButton.isHidden = false
                }
            } else {
                // ディスコードアカウントが連携されていない場合
                DispatchQueue.main.async {
                    self.discordStatusLabel.text = NSLocalizedString("Not Linked", comment: "Status for unlinked Discord account")
                    self.discordLinkButton.isHidden = false
                    self.discordUnlinkButton.isHidden = true
                }
            }
        }
    }
    
    private func checkRetroAchievementsStatus() {
        // Ensure RetroAchievementsManager is initialized
        RetroAchievementsManager.initialize()
        
        // RetroAchievementsAuthManagerから連携状態を確認
        let authManager = RetroAchievementsAuthManager.shared
        let isLoggedIn = authManager.isRetroAchievementsLoggedIn
        
        if isLoggedIn, let username = authManager.currentUsername {
            // ログイン済みの場合
            retroAchievementsStatusLabel.text = username
            retroAchievementsLoginButton.isHidden = true
            retroAchievementsLogoutButton.isHidden = false
        } else {
            // 未ログインの場合
            retroAchievementsStatusLabel.text = NSLocalizedString("Not Connected", comment: "Status for unconnected RetroAchievements account")
            retroAchievementsLoginButton.isHidden = false
            retroAchievementsLogoutButton.isHidden = true
        }
        
        // 公式アイコンを読み込み
        loadRetroAchievementsIcon()
    }
    
    private func loadRetroAchievementsIcon() {
        // 公式アイコンURLから画像を読み込む
        guard let iconURL = URL(string: "https://docs.retroachievements.org/ra-logo-big-shadow.png") else {
            // フォールバック: システムアイコンを使用
            retroAchievementsIconView.image = UIImage(systemName: "gamecontroller.fill")
            retroAchievementsIconView.tintColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
            return
        }
        
        DispatchQueue.global().async {
            if let data = try? Data(contentsOf: iconURL), let image = UIImage(data: data) {
                DispatchQueue.main.async {
                    self.retroAchievementsIconView.image = image
                }
            } else {
                // 画像の読み込みに失敗した場合はフォールバック
                DispatchQueue.main.async {
                    self.retroAchievementsIconView.image = UIImage(systemName: "gamecontroller.fill")
                    self.retroAchievementsIconView.tintColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
                }
            }
        }
    }

    // MARK: - Actions
    @objc private func closeButtonTapped() {
        dismiss(animated: true)
    }

    @objc private func logoutButtonTapped() {
        // サインアウト処理
        do {
            try Auth.auth().signOut()
            let alert = UIAlertController(
                title: NSLocalizedString("Logout Successful", comment: "Title for successful logout"),
                message: NSLocalizedString("You have been logged out", comment: "Message for successful logout"),
                preferredStyle: .alert
            )
            alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default) { [weak self] _ in
                self?.dismiss(animated: true)
            })
            present(alert, animated: true)
        } catch {
            print("Error signing out: \(error.localizedDescription)")

            // エラー時のアラート表示
            let alert = UIAlertController(
                title: NSLocalizedString("Logout Failed", comment: "Title for logout error"),
                message: error.localizedDescription,
                preferredStyle: .alert
            )
            alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
            present(alert, animated: true)
        }
    }

    @objc private func discordLinkButtonTapped() {
        // Discordアカウント連携処理
        let discordAuthManager = DiscordAuthManager()
        discordAuthManager.startDiscordLogin()
    }

    @objc private func discordUnlinkButtonTapped() {
        // Discordアカウント連携解除の確認ダイアログ
        let alert = UIAlertController(
            title: NSLocalizedString("Discord Unlink", comment: "Title for Discord unlink confirmation"),
            message: NSLocalizedString("Are you sure you want to unlink your Discord account?", comment: "Message for Discord unlink confirmation"),
            preferredStyle: .alert
        )

        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel))
        alert.addAction(UIAlertAction(title: NSLocalizedString("Unlink", comment: "Confirm unlink button"), style: .destructive) { [weak self] _ in
            self?.unlinkDiscordAccount()
        })

        present(alert, animated: true)
    }

    private func unlinkDiscordAccount() {
        guard let userId = Auth.auth().currentUser?.uid else { return }

        // Firestoreからディスコード連携情報を削除
        let db = Firestore.firestore()
        db.collection("discord_links").document(userId).delete { [weak self] error in
            guard let self = self else { return }

            if let error = error {
                print("Error unlinking Discord account: \(error.localizedDescription)")

                // エラー時のアラート表示
                let alert = UIAlertController(
                    title: NSLocalizedString("Unlink Failed", comment: "Title for Discord unlink error"),
                    message: error.localizedDescription,
                    preferredStyle: .alert
                )
                alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
                self.present(alert, animated: true)
            } else {
                // 成功時のアラート表示
                let alert = UIAlertController(
                    title: NSLocalizedString("Unlink Successful", comment: "Title for successful Discord unlink"),
                    message: NSLocalizedString("Discord account unlinked", comment: "Message for successful Discord unlink"),
                    preferredStyle: .alert
                )
                alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
                self.present(alert, animated: true)

                // UI更新
                self.discordStatusLabel.text = NSLocalizedString("Not Linked", comment: "Status for unlinked Discord account")
                self.discordLinkButton.isHidden = false
                self.discordUnlinkButton.isHidden = true
            }
        }
    }
    
    @objc private func retroAchievementsLoginButtonTapped() {
        // RetroAchievementsログインダイアログを表示
        let loginViewController = RetroAchievementsLoginViewController()
        
        loginViewController.onLoginSuccess = { [weak self] (username: String) in
            // ログイン成功時の処理
            DispatchQueue.main.async {
                self?.checkRetroAchievementsStatus()
                
                // 成功通知を表示
                let alert = UIAlertController(
                    title: NSLocalizedString("Login Successful", comment: "Title for successful RetroAchievements login"),
                    message: NSLocalizedString("Welcome back", comment: "Message for successful RetroAchievements login") + ", \(username)!",
                    preferredStyle: .alert
                )
                alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
                self?.present(alert, animated: true)
            }
        }
        
        loginViewController.onLoginCancel = {
            // キャンセル時は特に処理なし
        }
        
        loginViewController.modalPresentationStyle = UIModalPresentationStyle.formSheet
        present(loginViewController, animated: true)
    }
    
    @objc private func retroAchievementsLogoutButtonTapped() {
        // RetroAchievementsアカウント連携解除の確認ダイアログ
        let alert = UIAlertController(
            title: NSLocalizedString("RetroAchievements Logout", comment: "Title for RetroAchievements logout confirmation"),
            message: NSLocalizedString("Are you sure you want to disconnect your RetroAchievements account?", comment: "Message for RetroAchievements logout confirmation"),
            preferredStyle: .alert
        )
        
        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel))
        alert.addAction(UIAlertAction(title: NSLocalizedString("Disconnect", comment: "Confirm logout button"), style: .destructive) { [weak self] _ in
            self?.logoutRetroAchievements()
        })
        
        present(alert, animated: true)
    }
    
    private func logoutRetroAchievements() {
        let authManager = RetroAchievementsAuthManager.shared
        authManager.logoutRetroAchievements()
        
        // UI更新
        checkRetroAchievementsStatus()
        
        // 成功時のアラート表示
        let alert = UIAlertController(
            title: NSLocalizedString("Logout Successful", comment: "Title for successful RetroAchievements logout"),
            message: NSLocalizedString("Your RetroAchievements account has been disconnected.", comment: "Message for successful RetroAchievements logout"),
            preferredStyle: .alert
        )
        alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
        present(alert, animated: true)
    }

    // MARK: - Account Deletion
    @objc private func deleteAccountButtonTapped() {
        // アカウント削除の確認ダイアログ
        let alert = UIAlertController(
            title: NSLocalizedString("Delete Account Confirmation", comment: "Title for account deletion confirmation"),
            message: NSLocalizedString("This will delete your account and all related data. This action cannot be undone. Continue?", comment: "Message for account deletion confirmation"),
            preferredStyle: .alert
        )

        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel))
        alert.addAction(UIAlertAction(title: NSLocalizedString("Delete", comment: "Confirm deletion button"), style: .destructive) { [weak self] _ in
            self?.promptForReauthentication()
        })

        present(alert, animated: true)
    }

    private func promptForReauthentication() {
        guard let currentUser = Auth.auth().currentUser else { return }

        // ユーザーの認証プロバイダを確認
        let providers = currentUser.providerData.map { $0.providerID }

        if providers.contains("google.com") {
            // Googleでの再認証
            reauthenticateWithGoogle()
        } else if providers.contains("password") {
            // メール/パスワードでの再認証
            promptForEmailPassword()
        } else {
            // その他のプロバイダまたは匿名認証の場合
            let alert = UIAlertController(
                title: NSLocalizedString("Reauthentication Required", comment: "Title for reauthentication required"),
                message: NSLocalizedString("For security reasons, you need to sign in again to delete your account. Please sign out and sign in again, then retry.", comment: "Message for reauthentication required"),
                preferredStyle: .alert
            )
            alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
            present(alert, animated: true)
        }
    }

    private func reauthenticateWithGoogle() {
        // GoogleSignInの設定
        guard let clientID = FirebaseApp.app()?.options.clientID else { return }
        let config = GIDConfiguration(clientID: clientID)
        GIDSignIn.sharedInstance.configuration = config

        // 現在のViewControllerからGoogleサインインを開始
        GIDSignIn.sharedInstance.signIn(withPresenting: self) { [weak self] result, error in
            guard let self = self else { return }

            if let error = error {
                print("Google Sign-In error: \(error.localizedDescription)")
                self.showReauthenticationError(error)
                return
            }

            guard let user = result?.user,
                  let idToken = user.idToken?.tokenString else {
                self.showReauthenticationError(NSError(domain: "GoogleSignIn", code: -1, userInfo: [NSLocalizedDescriptionKey: "ID token missing"]))
                return
            }

            // Googleの認証情報を作成
            let credential = GoogleAuthProvider.credential(withIDToken: idToken,
                                                         accessToken: user.accessToken.tokenString)

            // 再認証を実行
            self.reauthenticateAndDeleteAccount(with: credential)
        }
    }

    private func promptForEmailPassword() {
        let alert = UIAlertController(
            title: NSLocalizedString("Reauthentication", comment: "Title for reauthentication"),
            message: NSLocalizedString("For security reasons, please re-enter your password to delete your account.", comment: "Message for reauthentication"),
            preferredStyle: .alert
        )

        alert.addTextField { textField in
            textField.placeholder = NSLocalizedString("Password", comment: "Password placeholder")
            textField.isSecureTextEntry = true
        }

        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel))
        alert.addAction(UIAlertAction(title: NSLocalizedString("Confirm", comment: "Confirm button"), style: .default) { [weak self, weak alert] _ in
            guard let self = self,
                  let textFields = alert?.textFields,
                  let passwordField = textFields.first,
                  let password = passwordField.text,
                  !password.isEmpty,
                  let email = Auth.auth().currentUser?.email else {
                self?.showReauthenticationError(NSError(domain: "Reauthentication", code: -1, userInfo: [NSLocalizedDescriptionKey: "パスワードが入力されていません"]))
                return
            }

            // メール/パスワードの認証情報を作成
            let credential = EmailAuthProvider.credential(withEmail: email, password: password)

            // 再認証を実行
            self.reauthenticateAndDeleteAccount(with: credential)
        })

        present(alert, animated: true)
    }

    private func reauthenticateAndDeleteAccount(with credential: AuthCredential) {
        guard let currentUser = Auth.auth().currentUser else { return }

        // 再認証を実行
        currentUser.reauthenticate(with: credential) { [weak self] _, error in
            guard let self = self else { return }

            if let error = error {
                print("Reauthentication error: \(error.localizedDescription)")
                self.showReauthenticationError(error)
                return
            }

            // 再認証成功後、アカウント削除処理を実行
            self.deleteUserAccount()
        }
    }

    private func showReauthenticationError(_ error: Error) {
        let alert = UIAlertController(
            title: NSLocalizedString("Reauthentication Failed", comment: "Title for reauthentication failure"),
            message: error.localizedDescription,
            preferredStyle: .alert
        )
        alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
        present(alert, animated: true)
    }

    private func deleteUserAccount() {
        guard let currentUser = Auth.auth().currentUser else { return }
        let userId = currentUser.uid

        // ローディングインジケータを表示するなどの処理があれば追加

        // 非同期処理を開始
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                // 1. Realtime Databaseからユーザーデータを削除
                try self?.deleteUserDataFromDatabase(userId: userId)

                // 2. Firestoreからユーザードキュメントを削除
                let db = Firestore.firestore()
                let userDocRef = db.collection("users").document(userId)
                let semaphoreUser = DispatchSemaphore(value: 0)
                var userError: Error?

                userDocRef.delete { error in
                    userError = error
                    semaphoreUser.signal()
                }
                semaphoreUser.wait()

                if let error = userError {
                    throw error
                }
                print("Attempted Firestore document deletion: users/\(userId)")

                // 3. Firestoreからdiscord_linksドキュメントを削除
                let discordLinkDocRef = db.collection("discord_links").document(userId)
                let semaphoreDiscord = DispatchSemaphore(value: 0)
                var discordError: Error?

                discordLinkDocRef.delete { error in
                    discordError = error
                    semaphoreDiscord.signal()
                }
                semaphoreDiscord.wait()

                if let error = discordError {
                    throw error
                }
                print("Attempted Firestore document deletion: discord_links/\(userId)")

                // 4. Firebase Storageからユーザーファイルを削除
                try self?.deleteUserFilesFromStorage(userId: userId)

                // 5. ユーザーアカウントを削除
                let semaphoreAccount = DispatchSemaphore(value: 0)
                var accountError: Error?

                currentUser.delete { error in
                    accountError = error
                    semaphoreAccount.signal()
                }
                semaphoreAccount.wait()

                if let error = accountError {
                    throw error
                }

                // 6. 成功時の処理をメインスレッドで実行
                DispatchQueue.main.async {
                    // 成功時のアラート表示
                    let alert = UIAlertController(
                        title: NSLocalizedString("Account Deleted Successfully", comment: "Title for successful account deletion"),
                        message: NSLocalizedString("Your account and all related data have been deleted", comment: "Message for successful account deletion"),
                        preferredStyle: .alert
                    )
                    alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default) { _ in
                        // ViewControllerを閉じる
                        self?.dismiss(animated: true)
                    })
                    self?.present(alert, animated: true)
                }
            } catch {
                // エラー時の処理をメインスレッドで実行
                DispatchQueue.main.async {
                    print("Error deleting user account: \(error.localizedDescription)")

                    // エラー時のアラート表示
                    let alert = UIAlertController(
                        title: NSLocalizedString("Account Deletion Failed", comment: "Title for account deletion error"),
                        message: "\(NSLocalizedString("Failed to delete account", comment: "Message for account deletion error")): \(error.localizedDescription)",
                        preferredStyle: .alert
                    )
                    alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default))
                    self?.present(alert, animated: true)
                }
            }
        }
    }

    private func deleteUserDataFromDatabase(userId: String) throws {
        // Realtime Databaseにある "/user-posts/{userId}" にある全データを削除
        let baseurl = "/user-posts/\(userId)"
        let database = Database.database()
        let userPostsRef = database.reference(withPath: baseurl)

        // 同期的に処理するためのセマフォを作成
        let semaphore = DispatchSemaphore(value: 0)
        var databaseError: Error?

        userPostsRef.removeValue { error, _ in
            databaseError = error
            semaphore.signal()
        }

        // 処理が完了するまで待機
        semaphore.wait()

        // エラーがあれば例外をスロー
        if let error = databaseError {
            print("Error deleting data for user: \(userId) at path \(baseurl): \(error.localizedDescription)")
            throw error
        }

        print("Successfully deleted data for user: \(userId) at path \(baseurl)")
    }

    private func deleteUserFilesFromStorage(userId: String) throws {
        let storage = Storage.storage()
        let storageRef = storage.reference()
        let userRef = storageRef.child(userId)

        // 同期的に処理するためのセマフォを作成
        let semaphore = DispatchSemaphore(value: 0)
        var storageError: Error?

        // ユーザーディレクトリ内のすべてのファイルを取得
        userRef.listAll { result, error in
            // ユーザーデータが存在しない場合（オブジェクトが見つからないエラー）は正常終了とする
            if let error = error as NSError?, error.domain == StorageErrorDomain,
               error.code == StorageErrorCode.objectNotFound.rawValue {
                print("No user data found in Storage for user: \(userId). Skipping deletion.")
                semaphore.signal()
                return
            } else if let error = error {
                // その他のエラーは通常通り処理
                storageError = error
                semaphore.signal()
                return
            }

            let group = DispatchGroup()

            // 各ファイルを削除
            if let items = result?.items {
                if items.isEmpty {
                    print("No files found in Storage for user: \(userId)")
                }

                for item in items {
                    group.enter()
                    item.delete { error in
                        if let error = error {
                            print("Error deleting file \(item.name): \(error.localizedDescription)")
                        } else {
                            print("Successfully deleted file: \(item.name)")
                        }
                        group.leave()
                    }
                }
            }

            // 各サブディレクトリを再帰的に削除
            if let prefixes = result?.prefixes {
                if prefixes.isEmpty {
                    print("No directories found in Storage for user: \(userId)")
                }

                for prefix in prefixes {
                    group.enter()
                    self.deleteStorageDirectory(prefix) { error in
                        if let error = error {
                            // エラーがあっても処理を続行する（ベストエフォート）
                            print("Error deleting directory \(prefix.name): \(error.localizedDescription)")
                        } else {
                            print("Successfully deleted directory: \(prefix.name)")
                        }
                        group.leave()
                    }
                }
            }

            // すべての削除処理が完了するのを待つ
            group.notify(queue: .global()) {
                semaphore.signal()
            }
        }

        // 処理が完了するまで待機
        semaphore.wait()

        // エラーがあれば例外をスロー（ただしオブジェクトが見つからないエラーは除く）
        if let error = storageError as NSError?,
           !(error.domain == StorageErrorDomain && error.code == StorageErrorCode.objectNotFound.rawValue) {
            throw error
        }

        print("Storage cleanup for user \(userId) completed")
    }

    private func deleteStorageDirectory(_ reference: StorageReference, completion: @escaping (Error?) -> Void) {
        reference.listAll { result, error in
            // ディレクトリが存在しない場合は正常終了とする
            if let error = error as NSError?, error.domain == StorageErrorDomain,
               error.code == StorageErrorCode.objectNotFound.rawValue {
                print("Directory not found: \(reference.fullPath). Skipping deletion.")
                completion(nil)
                return
            } else if let error = error {
                // その他のエラーは通常通り処理
                print("Error listing directory \(reference.fullPath): \(error.localizedDescription)")
                completion(error)
                return
            }

            let group = DispatchGroup()
            var lastError: Error?

            // 各ファイルを削除
            if let items = result?.items {
                if items.isEmpty {
                    print("No files found in directory: \(reference.fullPath)")
                }

                for item in items {
                    group.enter()
                    item.delete { error in
                        if let error = error {
                            // エラーを記録するが、処理は続行する
                            lastError = error
                            print("Error deleting file \(item.name): \(error.localizedDescription)")
                        } else {
                            print("Successfully deleted file: \(item.fullPath)")
                        }
                        group.leave()
                    }
                }
            }

            // 各サブディレクトリを再帰的に削除
            if let prefixes = result?.prefixes {
                if prefixes.isEmpty {
                    print("No subdirectories found in directory: \(reference.fullPath)")
                }

                for prefix in prefixes {
                    group.enter()
                    self.deleteStorageDirectory(prefix) { error in
                        if let error = error {
                            // エラーを記録するが、処理は続行する
                            lastError = error
                            print("Error in subdirectory deletion: \(error.localizedDescription)")
                        } else {
                            print("Successfully deleted subdirectory: \(prefix.fullPath)")
                        }
                        group.leave()
                    }
                }
            }

            // すべての削除処理が完了したらコールバックを呼び出す
            group.notify(queue: .global()) {
                // オブジェクトが見つからないエラーは無視する
                if let error = lastError as NSError?,
                   error.domain == StorageErrorDomain && error.code == StorageErrorCode.objectNotFound.rawValue {
                    completion(nil)
                } else {
                    completion(lastError)
                }
            }
        }
    }
    
    // MARK: - GameController Support
    
    private func setupGameController() {
        // 既に接続されているコントローラーがあるかチェック
        if let controller = GCController.controllers().first {
            gameController = controller
            setupControllerInputHandlers(controller)
        }
        
        // コントローラー接続の監視
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(controllerDidConnect),
            name: .GCControllerDidConnect,
            object: nil
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(controllerDidDisconnect),
            name: .GCControllerDidDisconnect,
            object: nil
        )
    }
    
    @objc private func controllerDidConnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController else { return }
        gameController = controller
        setupControllerInputHandlers(controller)
    }
    
    @objc private func controllerDidDisconnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController,
              controller == gameController else { return }
        gameController = nil
    }
    
    private func setupControllerInputHandlers(_ controller: GCController) {
        // Extended gamepadの場合
        if let extendedGamepad = controller.extendedGamepad {
            // B button (キャンセル/戻る)
            extendedGamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.dismiss(animated: true, completion: nil)
                    }
                }
            }
            
            // Menu button (設定画面を閉じる)
            extendedGamepad.buttonMenu.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.dismiss(animated: true, completion: nil)
                    }
                }
            }
        }
        // Micro gamepadの場合
        else if let microGamepad = controller.microGamepad {
            // X button (キャンセル/戻る)
            microGamepad.buttonX.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.dismiss(animated: true, completion: nil)
                    }
                }
            }
        }
    }
}
