//
//  MainScreenController.swift
//  uoyabause
//
//  Created by MiyamotoShinya on 2016/09/04.
//  Copyright © 2016年 devMiyax. All rights reserved.
//

import Foundation
import UIKit
import UniformTypeIdentifiers
import FirebaseAuth
import GameController

// Focus management types
enum FocusableElement {
    case fileList
    case settingsButton
    case authButton
    case addButton
}

class MainScreenController :UIViewController, UIDocumentPickerDelegate  {

    var activityIndicator: UIActivityIndicatorView!
    var blurEffectView: UIVisualEffectView!
    var selected_file_path: String = ""
    @IBOutlet weak var settingButton: UIButton!
    private var authButton: UIBarButtonItem!
    private var authIconView: UIImageView!
    
    // GameController support
    private var gameController: GCController?
    private var keyMapper: KeyMapper!
    
    // Focus management
    private var focusedElement: FocusableElement = .fileList
    private var navigationBarButtons: [UIBarButtonItem] = []
    
    // Focus animation settings
    private var focusAnimationDuration: TimeInterval = 0.2 // デフォルトは0.2秒

    override func viewDidLoad() {
        super.viewDidLoad()

        // 背景色を設定（システムのモードに応じて）
        view.backgroundColor = .defaultBackground

        // Blur Effect Viewの設定
        let blurEffect = UIBlurEffect(style: .dark)
        blurEffectView = UIVisualEffectView(effect: blurEffect)
        blurEffectView.frame = self.view.bounds
        blurEffectView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        blurEffectView.isHidden = true
        self.view.addSubview(blurEffectView)

        // Activity Indicatorの設定
        activityIndicator = UIActivityIndicatorView(style: .large)
        activityIndicator.center = self.view.center
        activityIndicator.hidesWhenStopped = true
        self.view.addSubview(activityIndicator)

        // Activity Indicatorをビュー階層の一番上に持ってくる
        self.view.bringSubviewToFront(activityIndicator)
        #if FREE_VERSION
        self.navigationItem.title = NSLocalizedString("Yaba Sanshiro 2 Lite", comment: "App title for lite version")
        #endif

        settingButton.accessibilityIdentifier = "settingButton"
        
        // Remove all existing actions and add programmatic action for settings button
        settingButton.removeTarget(nil, action: nil, for: .allEvents)
        settingButton.addTarget(self, action: #selector(settingButtonTapped), for: .touchUpInside)

        // Auth buttonの追加
        setupAuthButton()

        // Auth状態の監視を開始
        observeAuthState()
        
        // GameController初期化
        setupGameController()
        observeControllerConnections()
        
        // Focus animation設定のロード
        loadFocusAnimationSettings()
    }

    private func setupAuthButton() {
        // コンテナビューの作成（固定サイズ）
        let containerView = UIView(frame: CGRect(x: 0, y: 0, width: 30, height: 30))

        // アイコンビューの作成
        authIconView = UIImageView(frame: containerView.bounds)
        authIconView.contentMode = .scaleAspectFill
        authIconView.clipsToBounds = true
        authIconView.layer.cornerRadius = 15
        authIconView.layer.masksToBounds = true
        authIconView.isUserInteractionEnabled = true
        authIconView.backgroundColor = .clear
        authIconView.autoresizingMask = [.flexibleWidth, .flexibleHeight]

        // コンテナビューにアイコンビューを追加
        containerView.addSubview(authIconView)

        // タップジェスチャーの追加
        let tapGesture = UITapGestureRecognizer(target: self, action: #selector(authButtonTapped))
        containerView.addGestureRecognizer(tapGesture)
        containerView.isUserInteractionEnabled = true

        // UIBarButtonItemの作成
        authButton = UIBarButtonItem(customView: containerView)
        navigationItem.rightBarButtonItems = [navigationItem.rightBarButtonItem, authButton].compactMap { $0 }
        
        // Navigation bar buttons配列を更新
        updateNavigationBarButtons()

        // 初期状態の更新
        updateAuthButtonState()
    }

    private func observeAuthState() {
        Auth.auth().addStateDidChangeListener { [weak self] (auth, user) in
            self?.updateAuthButtonState()
        }
    }

    private func updateAuthButtonState() {
        // アイコンの丸みを確保（念のため）
        authIconView.layer.cornerRadius = 15
        authIconView.clipsToBounds = true
        authIconView.layer.masksToBounds = true

        if let user = Auth.auth().currentUser {
            // ログイン済みの場合
            if let photoURL = user.photoURL {
                // ユーザーのプロフィール画像がある場合は読み込む
                DispatchQueue.global().async {
                    if let data = try? Data(contentsOf: photoURL), let image = UIImage(data: data) {
                        DispatchQueue.main.async {
                            // 画像を設定
                            self.authIconView.image = image

                            // 画像設定後も丸みを確保（念のため）
                            self.authIconView.layer.cornerRadius = 15
                            self.authIconView.clipsToBounds = true
                            self.authIconView.layer.masksToBounds = true
                        }
                    } else {
                        // 画像の読み込みに失敗した場合はデフォルト画像を設定
                        DispatchQueue.main.async {
                            self.authIconView.image = UIImage(systemName: "person.circle.fill")
                            self.authIconView.tintColor = .tint
                        }
                    }
                }
            } else {
                // プロフィール画像がない場合はデフォルト画像を設定
                authIconView.image = UIImage(systemName: "person.circle.fill")
                authIconView.tintColor = .tint
            }
        } else {
            // 未ログインの場合
            authIconView.image = UIImage(systemName: "person.circle")
            authIconView.tintColor = .appDisable
        }
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)

        // 背景色を設定
        view.backgroundColor = .defaultBackground

        // 子ビューコントローラーの背景色も設定（ビューがロードされている場合のみ）
        for child in children {
            if let fileSelectController = child as? FileSelectController, fileSelectController.isViewLoaded {
                fileSelectController.view.backgroundColor = .defaultBackground
            }
        }

        // アイコンの丸みを確保（念のため）
        authIconView.layer.cornerRadius = 15
        authIconView.clipsToBounds = true
        authIconView.layer.masksToBounds = true

        // 認証状態を更新（アイコンの表示を更新）
        updateAuthButtonState()
        
        // GameControllerハンドラーを再設定（設定画面から戻った時など）
        if let controller = gameController {
            print("MainScreenController: Re-setting up GameController handlers")
            setupControllerInputHandlers(controller)
        }
        
        // GamePadフォーカスを復元（設定画面から戻った時など）
        restoreGamePadFocus()
    }

    @objc private func authButtonTapped() {
        if Auth.auth().currentUser != nil {
            // ログイン済みの場合はユーザープロフィール画面を表示
            let profileVC = UserProfileViewController()
            let navController = UINavigationController(rootViewController: profileVC)
            setupModalGamePadSupport(for: navController)
            present(navController, animated: true)
        } else {
            // 未ログインの場合はサインイン画面を表示
            let loginVC = LoginViewController()
            let navController = UINavigationController(rootViewController: loginVC)
            setupModalGamePadSupport(for: navController)
            present(navController, animated: true)
        }
    }

    @IBAction func onAddFile(_ sender: Any) {

        for child in self.children {
            if let fc = child as? FileSelectController {
                if fc.checkLimitation() == false {
                    return
                }
            }
        }

        var documentPicker: UIDocumentPickerViewController!
            // iOS 14 & later
            let supportedTypes: [UTType] = [
                UTType(filenameExtension: "bin")!,
                UTType(filenameExtension: "cue")!,
                UTType(filenameExtension: "chd")!,
                UTType(filenameExtension: "ccd")!,
                UTType(filenameExtension: "img")!,
                UTType(filenameExtension: "mds")!,
                UTType(filenameExtension: "mdf")!,
            ]

        documentPicker = UIDocumentPickerViewController(forOpeningContentTypes: supportedTypes)
        documentPicker.delegate = self
        documentPicker.allowsMultipleSelection = true
        
        // GamePad support for document picker
        setupModalGamePadSupport(for: documentPicker)
        
        self.present(documentPicker, animated:true, completion: nil)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]){

        self.view.bringSubviewToFront(activityIndicator)
        blurEffectView.isHidden = false
        activityIndicator.startAnimating()

        DispatchQueue.global(qos: .userInitiated).async {
                // 選択されたファイルのURLを処理
            for url in urls {
                // ここで各ファイルのURLを処理します
                print("Selected file URL: \(url)")
                // 例: ファイルを解凍して処理する
                self.processFile(at: url)
            }

            DispatchQueue.main.async {
                self.children.forEach{
                    let fc = $0 as? FileSelectController
                    if fc != nil {
                        fc?.updateDoc()
                    }
                }
                self.blurEffectView.isHidden = true
                self.activityIndicator.stopAnimating()
                
                // ファイルリスト更新後にGameControllerハンドラーを確実に復元
                if let controller = self.gameController {
                    print("MainScreenController: Re-setting up GameController handlers after file list update")
                    self.setupControllerInputHandlers(controller)
                }
                
                // GamePadフォーカスも復元
                self.restoreGamePadFocus()
            }
        }

    }

    func processFile(at fileURL: URL) {
        print(fileURL)

        guard fileURL.startAccessingSecurityScopedResource() else {
            // エラー処理
            return
        }

        var documentsUrl = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]


        let theFileName = fileURL.lastPathComponent

        if theFileName.lowercased().contains(".cue") ||
            theFileName.lowercased().contains(".bin") ||
            theFileName.lowercased().contains(".chd") ||
            theFileName.lowercased().contains(".ccd") ||
            theFileName.lowercased().contains(".img") ||
            theFileName.lowercased().contains(".mdf") ||
            theFileName.lowercased().contains(".mds")
        {
            documentsUrl.appendPathComponent(theFileName)

            if( documentsUrl != fileURL ){

                let fileManager = FileManager.default
                do {
                    if fileManager.fileExists(atPath: documentsUrl.path) {
                        try fileManager.removeItem(at: documentsUrl)
                    }
                    try fileManager.copyItem(at: fileURL, to: documentsUrl)
                } catch let error as NSError {
                    print("Fail to copy \(error.localizedDescription)")
                    return
                }
            }

        } else{
            let alert: UIAlertController = UIAlertController(
                title: NSLocalizedString("Fail to open", comment: "Title for the alert when a file fails to open"),
                message: NSLocalizedString("You can select chd or bin or cue", comment: "Message indicating the supported file formats"),
                preferredStyle: UIAlertController.Style.alert
            )

            let defaultAction: UIAlertAction = UIAlertAction(
                title: NSLocalizedString("OK", comment: "Default action button title"),
                style: UIAlertAction.Style.default,
                handler: { (action: UIAlertAction!) -> Void in
                    print("OK")
                }
            )

            alert.addAction(defaultAction)
            present(alert, animated: true, completion: nil)
            return
        }

        fileURL.stopAccessingSecurityScopedResource()
    }

    // ダークモード変更時の処理
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)

        if #available(iOS 13.0, *) {
            if traitCollection.hasDifferentColorAppearance(comparedTo: previousTraitCollection) {
                // 背景色を更新
                view.backgroundColor = .defaultBackground

                // 子ビューコントローラーにも通知（ビューがロードされている場合のみ）
                for child in children {
                    if let fileSelectController = child as? FileSelectController, fileSelectController.isViewLoaded {
                        fileSelectController.view.backgroundColor = .defaultBackground
                    }
                }
            }
        }
    }
    
    // MARK: - GameController Support
    
    private func loadFocusAnimationSettings() {
        // UserDefaultsから設定を読み込み
        let userDefaults = UserDefaults.standard
        
        // アニメーション速度設定（0: Fast, 1: Normal, 2: Slow）
        let animationSpeed = userDefaults.integer(forKey: "mainScreenFocusAnimationSpeed")
        switch animationSpeed {
        case 0: // Fast
            focusAnimationDuration = 0.1
        case 2: // Slow
            focusAnimationDuration = 0.3
        default: // Normal (1 or default)
            focusAnimationDuration = 0.2
        }
    }
    
    private func setupGameController() {
        // KeyMapperの初期化
        keyMapper = KeyMapper()
        
        // 既に接続されているコントローラーがあるかチェック
        if let controller = GCController.controllers().first {
            gameController = controller
            setupControllerInputHandlers(controller)
        }
        
        // ワイヤレスコントローラーの検索を開始
        GCController.startWirelessControllerDiscovery(completionHandler: nil)
    }
    
    private func observeControllerConnections() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(foundController),
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
    
    @objc private func foundController(_ notification: Notification) {
        guard let controller = notification.object as? GCController else { return }
        
        gameController = controller
        setupControllerInputHandlers(controller)
        
        print("GameController connected: \(controller.vendorName ?? "Unknown")")
    }
    
    @objc private func controllerDidDisconnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController,
              controller == gameController else { return }
        
        gameController = nil
        print("GameController disconnected")
    }
    
    private func setupControllerInputHandlers(_ controller: GCController) {
        // Extended gamepadの場合
        if let extendedGamepad = controller.extendedGamepad {
            setupExtendedGamepadHandlers(extendedGamepad)
        }
        // Micro gamepadの場合
        else if let microGamepad = controller.microGamepad {
            setupMicroGamepadHandlers(microGamepad)
        }
    }
    
    private func setupExtendedGamepadHandlers(_ gamepad: GCExtendedGamepad) {
        // A button (選択/決定)
        gamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_A)
            }
        }
        
        // B button (キャンセル/戻る)
        gamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_B)
            }
        }
        
        // Menu button (設定画面)
        gamepad.buttonMenu.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_MENU)
            }
        }
        
        // Options button (ファイル追加)
        if let optionsButton = gamepad.buttonOptions {
            optionsButton.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    self?.handleGameControllerInput(.MFI_BUTTON_OPTION)
                }
            }
        }
        
        // D-Pad (ナビゲーション)
        gamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_UP)
            }
        }
        
        gamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_DOWN)
            }
        }
        
        gamepad.dpad.left.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_LEFT)
            }
        }
        
        gamepad.dpad.right.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_RIGHT)
            }
        }
        
        // Left Shoulder (Page Up)
        gamepad.leftShoulder.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_LS)
            }
        }
        
        // Right Shoulder (Page Down)
        gamepad.rightShoulder.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_RS)
            }
        }
        
        // Left Trigger (Page Up Alternative)
        gamepad.leftTrigger.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_LT)
            }
        }
        
        // Right Trigger (Page Down Alternative)
        gamepad.rightTrigger.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_RT)
            }
        }
    }
    
    private func setupMicroGamepadHandlers(_ gamepad: GCMicroGamepad) {
        // Micro gamepad用の基本ハンドラー
        gamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_A)
            }
        }
        
        gamepad.buttonX.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_BUTTON_B)
            }
        }
        
        gamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_UP)
            }
        }
        
        gamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_DOWN)
            }
        }
        
        gamepad.dpad.left.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_LEFT)
            }
        }
        
        gamepad.dpad.right.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                self?.handleGameControllerInput(.MFI_DPAD_RIGHT)
            }
        }
    }
    
    private func handleGameControllerInput(_ button: KeyMapMappableButton) {
        DispatchQueue.main.async { [weak self] in
            print("MainScreenController: handleGameControllerInput - \(button), focusedElement: \(self?.focusedElement ?? .fileList)")
            
            switch button {
            case .MFI_BUTTON_A:
                // A button: 現在フォーカスされた要素を選択/決定
                self?.activateCurrentFocus()
                
            case .MFI_BUTTON_B:
                // B button: キャンセル/戻る（ナビゲーション戻る）
                self?.navigationController?.popViewController(animated: true)
                
            case .MFI_BUTTON_MENU:
                // Menu button: 設定画面を表示
                self?.focusedElement = .settingsButton
                self?.activateCurrentFocus()
                
            case .MFI_BUTTON_OPTION:
                // Option button: ファイル追加
                self?.focusedElement = .addButton
                self?.activateCurrentFocus()
                
            case .MFI_DPAD_UP:
                print("MainScreenController: D-PAD UP pressed")
                self?.moveFocus(.up)
                
            case .MFI_DPAD_DOWN:
                print("MainScreenController: D-PAD DOWN pressed")
                self?.moveFocus(.down)
                
            case .MFI_DPAD_LEFT:
                print("MainScreenController: D-PAD LEFT pressed")
                self?.moveFocus(.left)
                
            case .MFI_DPAD_RIGHT:
                print("MainScreenController: D-PAD RIGHT pressed")
                self?.moveFocus(.right)
                
            case .MFI_BUTTON_LT, .MFI_BUTTON_LS:
                // Left Trigger/Shoulder: Page Up
                self?.handlePageNavigation(.up)
                
            case .MFI_BUTTON_RT, .MFI_BUTTON_RS:
                // Right Trigger/Shoulder: Page Down
                self?.handlePageNavigation(.down)
                
            default:
                print("GameController: Unhandled button \(button)")
            }
        }
    }
    
    private func handlePageNavigation(_ direction: FocusDirection) {
        // ページスクロール機能
        if focusedElement == .fileList {
            // FileSelectControllerにページナビゲーションを転送
            forwardPageNavigationToFileSelector(direction)
        }
    }
    
    private func forwardPageNavigationToFileSelector(_ direction: FocusDirection) {
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                fileSelectController.handlePageNavigation(direction)
                break
            }
        }
    }
    
    // MARK: - Focus Management
    
    private func updateNavigationBarButtons() {
        navigationBarButtons = []
        
        // Add buttonを左側に追加
        if let leftButton = navigationItem.leftBarButtonItem {
            navigationBarButtons.append(leftButton)
        }
        
        // 右側のボタンを追加（設定ボタン、認証ボタンなど）
        if let rightButtons = navigationItem.rightBarButtonItems {
            navigationBarButtons.append(contentsOf: rightButtons)
        }
    }
    
    private func moveFocus(_ direction: FocusDirection) {
        let previousElement = focusedElement
        
        switch direction {
        case .up:
            // ファイルリスト内での上移動またはNavigation barへの移動
            if focusedElement == .fileList {
                // まずFileSelectController内で上移動を試行
                let didMove = forwardNavigationToFileSelector(.up)
                // FileSelectControllerで移動できなかった場合、Navigation barに移動
                if !didMove {
                    focusedElement = .authButton // 中央のボタンから開始
                    updateFocusVisuals()
                }
            }
            
        case .down:
            // Navigation barから下に移動する場合、ファイルリストに移動
            if focusedElement != .fileList {
                focusedElement = .fileList
                updateFocusVisuals()
                // FileSelectControllerに初期フォーカスを設定（一行目の最初の項目）
                initializeFileListFocusToFirstItem()
            } else {
                // 既にファイルリストにフォーカスがある場合は、下移動を転送
                _ = forwardNavigationToFileSelector(.down)
            }
            
        case .left:
            // Navigation bar内での左右移動
            if focusedElement != .fileList {
                navigateWithinNavigationBar(.left)
            } else {
                // ファイルリスト内での左移動
                _ = forwardNavigationToFileSelector(.left)
            }
            
        case .right:
            // Navigation bar内での左右移動
            if focusedElement != .fileList {
                navigateWithinNavigationBar(.right)
            } else {
                // ファイルリスト内での右移動
                _ = forwardNavigationToFileSelector(.right)
            }
        }
        
        // フォーカスが実際に移動した場合はHaptic feedbackを提供
        if previousElement != focusedElement {
            provideFocusHapticFeedback()
        }
    }
    
    private func navigateWithinNavigationBar(_ direction: FocusDirection) {
        // Navigation bar内の要素間移動
        // 左から右への順序: addButton → authButton → settingsButton
        switch focusedElement {
        case .addButton:
            if direction == .right {
                focusedElement = .authButton
            }
        case .authButton:
            if direction == .left {
                focusedElement = .addButton
            } else if direction == .right {
                focusedElement = .settingsButton
            }
        case .settingsButton:
            if direction == .left {
                focusedElement = .authButton
            }
        default:
            break
        }
        updateFocusVisuals()
    }
    
    private func activateCurrentFocus() {
        // 選択時のHaptic feedback
        provideSelectionHapticFeedback()
        
        switch focusedElement {
        case .fileList:
            // ファイルリストでの選択をFileSelectControllerに転送
            forwardSelectionToFileSelector()
            
        case .settingsButton:
            // 設定ボタンを実行
            // settingButtonのタップイベントを直接送信
            settingButton.sendActions(for: .touchUpInside)
            
        case .authButton:
            // 認証ボタンを実行
            authButtonTapped()
            
        case .addButton:
            // ファイル追加を実行
            onAddFile(self)
        }
    }
    
    private func updateFocusVisuals() {
        // 全てのNavigation bar要素からフォーカス表示を削除
        removeAllNavigationBarFocus()
        
        // デバッグ: Navigation bar構造を確認
        if let rightButtons = navigationItem.rightBarButtonItems {
            print("Right buttons count: \(rightButtons.count)")
            for (index, button) in rightButtons.enumerated() {
                print("Right button \(index): \(button)")
            }
        }
        
        // FileSelectControllerのフォーカス表示を管理
        if focusedElement == .fileList {
            // ファイルリストにフォーカスがある場合、FileSelectControllerのフォーカス表示を有効
            setFileListFocusVisibility(true)
        } else {
            // Navigation barにフォーカスがある場合、FileSelectControllerのフォーカス表示を無効
            setFileListFocusVisibility(false)
        }
        
        // 現在フォーカスされた要素にハイライトを追加
        switch focusedElement {
        case .settingsButton:
            print("Highlighting settings button at index 0")
            highlightNavigationBarButton(at: 0) // 設定ボタン（右側の1番目）
        case .authButton:
            print("Highlighting auth button at index 1")
            highlightNavigationBarButton(at: 1) // 認証ボタン（右側の2番目）
        case .addButton:
            print("Highlighting add button on left side")
            highlightNavigationBarButton(leftSide: true) // 追加ボタン（左側）
        case .fileList:
            // ファイルリストは FileSelectController で管理
            break
        }
        
        print("Focus moved to: \(focusedElement)")
    }
    
    private func setFileListFocusVisibility(_ visible: Bool) {
        // FileSelectControllerのフォーカス表示を制御
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                fileSelectController.setFocusVisibility(visible)
                break
            }
        }
    }
    
    private func removeAllNavigationBarFocus() {
        // 右側ボタンのフォーカス削除
        if let rightButtons = navigationItem.rightBarButtonItems {
            for button in rightButtons {
                removeNavigationButtonHighlight(button)
            }
        }
        
        // 左側ボタンのフォーカス削除
        if let leftButton = navigationItem.leftBarButtonItem {
            removeNavigationButtonHighlight(leftButton)
        }
    }
    
    private func highlightNavigationBarButton(at index: Int) {
        // 右側ボタンのハイライト
        if let rightButtons = navigationItem.rightBarButtonItems,
           index < rightButtons.count {
            print("Actually highlighting right button at index \(index)")
            addNavigationButtonHighlight(rightButtons[index])
        } else {
            print("Failed to highlight button at index \(index) - buttons count: \(navigationItem.rightBarButtonItems?.count ?? 0)")
        }
    }
    
    private func highlightNavigationBarButton(leftSide: Bool) {
        // 左側ボタンのハイライト
        if leftSide {
            if let leftButton = navigationItem.leftBarButtonItem {
                print("Highlighting left button: \(leftButton)")
                addNavigationButtonHighlight(leftButton)
            } else {
                print("No left button found in navigationItem.leftBarButtonItem")
            }
        }
    }
    
    private func addNavigationButtonHighlight(_ barButtonItem: UIBarButtonItem) {
        // UIBarButtonItem のカスタムビューにハイライトを追加
        if let customView = barButtonItem.customView {
            print("Adding highlight to custom view: \(customView)")
            customView.layer.borderWidth = 2.0
            customView.layer.borderColor = UIColor.tint.cgColor
            customView.layer.cornerRadius = 8.0
            customView.backgroundColor = UIColor.tint.withAlphaComponent(0.2)
        } else {
            print("No custom view found - this is likely a system button")
            // System buttonの場合は、オレンジ色でフォーカス状態を表現（tintとは違う色で視認性を高める）
            barButtonItem.tintColor = UIColor.appOrange
            
            // さらに、可能であればimageを一時的に変更
            if let _ = barButtonItem.image {
                print("System button has image - orange tintColor applied for focus")
            } else {
                print("System button - orange tintColor applied for focus")
            }
        }
    }
    
    private func removeNavigationButtonHighlight(_ barButtonItem: UIBarButtonItem) {
        // UIBarButtonItem のカスタムビューからハイライトを削除
        if let customView = barButtonItem.customView {
            customView.layer.borderWidth = 0.0
            customView.layer.borderColor = UIColor.clear.cgColor
            customView.backgroundColor = UIColor.clear
        } else {
            // System buttonの場合は、tintColorをデフォルト（システムデフォルト）に戻す
            barButtonItem.tintColor = nil // nilにするとシステムデフォルトに戻る
        }
    }
    
    private func forwardNavigationToFileSelector(_ direction: FocusDirection) -> Bool {
        // FileSelectControllerへのナビゲーション転送
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                return fileSelectController.handleGamePadNavigation(direction)
            }
        }
        return false
    }
    
    private func forwardSelectionToFileSelector() {
        // FileSelectControllerでの選択処理
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                fileSelectController.handleGamePadSelection()
                break
            }
        }
    }
    
    private func initializeFileListFocus() {
        // FileSelectControllerに初期フォーカスを設定
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                // フォーカスが初期化されていない場合は、初期化のためのナビゲーションを送る
                _ = fileSelectController.handleGamePadNavigation(.down) // 初期化を促す
                break
            }
        }
    }
    
    private func initializeFileListFocusToFirstItem() {
        // FileSelectControllerに初期フォーカスを設定（一行目の最初の項目）
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                fileSelectController.resetFocusToFirstItem()
                break
            }
        }
    }
    
    private func restoreGamePadFocus() {
        // GamePadフォーカスを適切に復元
        print("MainScreenController: Restoring GamePad focus")
        
        // フォーカスをファイルリストに設定
        focusedElement = .fileList
        
        // FileSelectControllerのフォーカス表示を有効にし、最初の項目にフォーカス
        for child in children {
            if let fileSelectController = child as? FileSelectController {
                // フォーカス表示を有効にする
                fileSelectController.setFocusVisibility(true)
                
                // 少し遅延させてリストが完全に更新された後にフォーカスをリセット
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                    fileSelectController.resetFocusToFirstItem()
                    print("MainScreenController: FileSelectController focus reset to first item")
                }
                break
            }
        }
        
        // ビジュアルフォーカスを更新
        updateFocusVisuals()
    }
    
    func restoreGameControllerAfterDocumentUpdate() {
        // FileSelectControllerからの通知でGameControllerハンドラーを復元
        print("MainScreenController: Restoring GameController handlers after document update")
        
        if let controller = gameController {
            setupControllerInputHandlers(controller)
        }
        
        // 少し遅延させてから GamePadフォーカスも復元
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
            self.restoreGamePadFocus()
        }
    }
    
    // UIFocusEnvironment Protocol
    override var preferredFocusEnvironments: [UIFocusEnvironment] {
        // 子ビューコントローラーを優先フォーカス環境として返す
        return children
    }
    
    override func updateFocusIfNeeded() {
        super.updateFocusIfNeeded()
        updateFocusVisuals()
    }
    
    // MARK: - Modal Dialog GamePad Support
    
    private func setupModalGamePadSupport(for viewController: UIViewController) {
        // Document Picker や他のModalでゲームパッド操作を有効にする
        // iOS 14以降では、自動的にフォーカスエンジンが有効になるが、
        // カスタマイズが必要な場合はここで設定
    }
    
    private func presentAlert(title: String?, message: String?, actions: [UIAlertAction]) {
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        
        for action in actions {
            alert.addAction(action)
        }
        
        // Alert Dialogのゲームパッド対応
        // iOS 14以降、UIAlertControllerは自動的にフォーカスエンジンをサポート
        // preferredActionを設定することで、デフォルトでフォーカスされるボタンを指定可能
        if let firstAction = actions.first {
            alert.preferredAction = firstAction
        }
        
        present(alert, animated: true)
    }
    
    // MARK: - Haptic Feedback Support
    
    private func provideHapticFeedback(_ style: UIImpactFeedbackGenerator.FeedbackStyle = .light) {
        if gameController != nil {
            // GameControllerが接続されている場合のみHaptic feedbackを提供
            let generator = UIImpactFeedbackGenerator(style: style)
            generator.prepare()
            generator.impactOccurred()
        }
    }
    
    private func provideFocusHapticFeedback() {
        // フォーカス移動時の軽いフィードバック
        provideHapticFeedback(.light)
    }
    
    private func provideSelectionHapticFeedback() {
        // 選択/決定時の中程度のフィードバック
        provideHapticFeedback(.medium)
    }
    
    // MARK: - Segue Support
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "toGameViewFromMain" {
            if let gameMainVC = segue.destination as? GameMainViewController {
                // FileSelectControllerから選択されたファイル情報を取得
                if let fileSelectController = sender as? FileSelectController {
                    gameMainVC.selectedFile = fileSelectController.selected_file_path
                    gameMainVC.productNumber = fileSelectController.productNumber
                }
            }
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    // MARK: - Settings Button Action
    @objc private func settingButtonTapped() {
        let settingsVC = SettingsViewController()
        let navigationController = UINavigationController(rootViewController: settingsVC)
        navigationController.modalPresentationStyle = .fullScreen
        present(navigationController, animated: true, completion: nil)
    }
}

