import UIKit
import FirebaseCore
import FirebaseMessaging
import FirebaseInAppMessaging
import GoogleSignIn
#if FREE_VERSION
import GoogleMobileAds
#endif
import FirebaseAuth
import UserNotifications

@UIApplicationMain  
class AppDelegate: UIResponder, UIApplicationDelegate, UNUserNotificationCenterDelegate, MessagingDelegate {

    var window: UIWindow?
    private let discordRedirectHandler = DiscordOAuthRedirectHandler()

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {

        // ダークモード対応の設定
        setupAppearance()

        var filePath: String?

        //#if DEBUG
        //filePath = Bundle.main.path(forResource: "GoogleService-Info-Debug", ofType: "plist")
        //#else
        filePath = Bundle.main.path(forResource: "GoogleService-Info", ofType: "plist")
        //#endif

        if let filePath = filePath, let options = FirebaseOptions(contentsOfFile: filePath) {
            FirebaseApp.configure(options: options)
        } else {
            fatalError("Couldn't find correct GoogleService-Info.plist file.")
        }

        // Initialize the Google Mobile Ads SDK.FirebaseCoreDiagnostics
#if FREE_VERSION
        GADMobileAds.sharedInstance().start(completionHandler: nil)
#endif
        
        // Configure Firebase Messaging
        configureFirebaseMessaging()
        
        // Auto-login to RetroAchievements on app startup
        RetroAchievementsAuthManager.shared.attemptAutoLogin { success, error in
            if success {
                NSLog("RetroAchievements: Auto-login successful on app startup")
            } else {
                NSLog("RetroAchievements: Auto-login failed on app startup: \(error ?? "Unknown error")")
            }
        }
        
        return true
    }

    // アプリ全体の外観設定
    private func setupAppearance() {
        
        window?.tintColor = .tint
        
        if #available(iOS 13.0, *) {
            // iOS 13以降はシステムのダークモード設定に従う
            window?.overrideUserInterfaceStyle = .unspecified

            // ナビゲーションバーの外観設定
            let navBarAppearance = UINavigationBarAppearance()
            navBarAppearance.configureWithOpaqueBackground()
            navBarAppearance.backgroundColor = UIColor.colorPrimary
            navBarAppearance.titleTextAttributes = [.foregroundColor: UIColor.tint]
            navBarAppearance.largeTitleTextAttributes = [.foregroundColor: UIColor.tint]

            UINavigationBar.appearance().standardAppearance = navBarAppearance
            UINavigationBar.appearance().scrollEdgeAppearance = navBarAppearance
            UINavigationBar.appearance().compactAppearance = navBarAppearance

            // タブバーの外観設定
            let tabBarAppearance = UITabBarAppearance()
            tabBarAppearance.configureWithOpaqueBackground()
            tabBarAppearance.backgroundColor = UIColor.colorPrimary

            UITabBar.appearance().standardAppearance = tabBarAppearance
            if #available(iOS 15.0, *) {
                UITabBar.appearance().scrollEdgeAppearance = tabBarAppearance
            }

            // テーブルビューの外観設定
            UITableView.appearance().backgroundColor = UIColor.defaultBackground

            // テーブルビューセルの外観設定
            UITableViewCell.appearance().backgroundColor = UIColor.defaultBackground

            // コレクションビューの外観設定
            UICollectionView.appearance().backgroundColor = UIColor.defaultBackground
        } else {
            // iOS 13未満の場合は独自のダークテーマを適用
            UINavigationBar.appearance().barTintColor = UIColor.colorPrimary
            UINavigationBar.appearance().tintColor = UIColor.appWhite
            UINavigationBar.appearance().titleTextAttributes = [.foregroundColor: UIColor.appWhite]

            UITabBar.appearance().barTintColor = UIColor.colorPrimary
            UITabBar.appearance().tintColor = UIColor.appWhite

            UITableView.appearance().backgroundColor = UIColor.defaultBackground
            UITableViewCell.appearance().backgroundColor = UIColor.defaultBackground
            UICollectionView.appearance().backgroundColor = UIColor.defaultBackground
        }
    }

    func applicationWillResignActive(_ application: UIApplication) {
        // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
        // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
    }

    func applicationDidEnterBackground(_ application: UIApplication) {
        // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
        // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    }

    func applicationWillEnterForeground(_ application: UIApplication) {
        // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
    }

    func applicationDidBecomeActive(_ application: UIApplication) {
        // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
        //StoreReviewHelper.shared.incrementAppLaunchCount()
        //StoreReviewHelper.shared.requestReviewIfAppropriate()
    }

    func applicationWillTerminate(_ application: UIApplication) {
        // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    }

    // URLハンドリング
    func application(_ app: UIApplication,
                    open url: URL,
                    options: [UIApplication.OpenURLOptionsKey: Any] = [:]) -> Bool {
        // Google SignInのURLハンドリング
        if GIDSignIn.sharedInstance.handle(url) {
            return true
        }

        // Discord OAuth リダイレクトのハンドリング
        if discordRedirectHandler.handleUrl(url) {
            return true
        }

        // 他のアプリからファイルを開く処理
        openMainScreenController(withFileAt: url)
        return true
    }

    // MainScreenControllerを起動してファイルを処理するためのカスタムメソッド
    func openMainScreenController(withFileAt url: URL) {
    }
    
    // MARK: - Firebase Messaging Configuration
    private func configureFirebaseMessaging() {
        // Register default notification category values before any
        // subscribeToEnabledTopics() call (fired from token callbacks)
        MessagingManager.shared.registerDefaultCategorySettings()

        // Set messaging delegate
        Messaging.messaging().delegate = self
        
        // Set UNUserNotificationCenter delegate
        UNUserNotificationCenter.current().delegate = self
        
        // Request notification permissions
        requestNotificationPermissions()
        
        // Register for remote notifications
        UIApplication.shared.registerForRemoteNotifications()
        
        // FCM token will be retrieved after APNs token is set in didRegisterForRemoteNotificationsWithDeviceToken
        // This avoids the "APNS device token not set" error
        print("Waiting for APNs token before retrieving FCM token...")
    }
    
    private func requestNotificationPermissions() {
        let authOptions: UNAuthorizationOptions = [.alert, .badge, .sound]
        UNUserNotificationCenter.current().requestAuthorization(
            options: authOptions,
            completionHandler: { granted, error in
                if let error = error {
                    print("Error requesting notification permissions: \(error)")
                } else {
                    print("Notification permissions granted: \(granted)")
                    // Store permission status in UserDefaults for settings screen
                    UserDefaults.standard.set(granted, forKey: "notificationPermissionGranted")
                }
            }
        )
    }
    
    private func saveFCMTokenToFirestore(token: String) {
        MessagingManager.shared.saveFCMTokenToFirestore(token: token)
    }
    
    // MARK: - MessagingDelegate
    func messaging(_ messaging: Messaging, didReceiveRegistrationToken fcmToken: String?) {
        print("Firebase registration token: \(String(describing: fcmToken))")
        
        if let token = fcmToken {
            saveFCMTokenToFirestore(token: token)
        }
        
        // Subscribe to enabled notification topics
        MessagingManager.shared.subscribeToEnabledTopics()
        
        // Note: This callback is fired at each app startup and whenever a new token is generated.
    }
    
    // MARK: - UNUserNotificationCenterDelegate
    // Handle notification when app is in foreground
    func userNotificationCenter(_ center: UNUserNotificationCenter,
                              willPresent notification: UNNotification,
                              withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void) {
        let userInfo = notification.request.content.userInfo
        
        // Print message ID if available
        if let messageID = userInfo["gcm.message_id"] {
            print("Message ID: \(messageID)")
        }
        
        // Show notification even when app is in foreground
        if #available(iOS 14.0, *) {
            completionHandler([[.alert, .sound, .badge, .list, .banner]])
        } else {
            completionHandler([[.alert, .sound, .badge]])
        }
    }
    
    // Handle notification tap
    func userNotificationCenter(_ center: UNUserNotificationCenter,
                              didReceive response: UNNotificationResponse,
                              withCompletionHandler completionHandler: @escaping () -> Void) {
        let userInfo = response.notification.request.content.userInfo
        
        // Print message ID if available
        if let messageID = userInfo["gcm.message_id"] {
            print("Message ID: \(messageID)")
        }
        
        // Handle notification tap actions
        handleNotificationTap(userInfo: userInfo)
        
        completionHandler()
    }
    
    private func handleNotificationTap(userInfo: [AnyHashable: Any]) {
        // TODO: Handle different notification types
        // - Achievement notifications -> open RetroAchievements
        // - Game updates -> open specific game
        // - System notifications -> open settings
        print("Notification tapped with userInfo: \(userInfo)")
        
        // Check for notification type
        if let notificationType = userInfo["type"] as? String {
            switch notificationType {
            case "achievement":
                // Open RetroAchievements section
                print("Would open RetroAchievements section")
                break
            case "game_update":
                // Open specific game if gameId is provided
                print("Would open game update section")
                break
            case "system":
                // Open settings or appropriate section
                print("Would open settings section")
                break
            default:
                break
            }
        }
    }
    
    // MARK: - Remote Notification Registration
    func application(_ application: UIApplication, didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data) {
        let tokenParts = deviceToken.map { data in String(format: "%02.2hhx", data) }
        let token = tokenParts.joined()
        print("APNs device token (hex): \(token)")
        
        // Set APNs token for Firebase Messaging
        Messaging.messaging().apnsToken = deviceToken
        
        // Now retrieve FCM token after APNs token is set
        Messaging.messaging().token { fcmToken, error in
            if let error = error {
                print("Error fetching FCM registration token after APNs: \(error)")
            } else if let fcmToken = fcmToken {
                print("FCM registration token (after APNs): \(fcmToken)")
                self.saveFCMTokenToFirestore(token: fcmToken)
                
                // Subscribe to enabled topics
                MessagingManager.shared.subscribeToEnabledTopics()
            }
        }
    }
    
    func application(_ application: UIApplication, didFailToRegisterForRemoteNotificationsWithError error: Error) {
        print("Failed to register for remote notifications: \(error)")
    }
}
