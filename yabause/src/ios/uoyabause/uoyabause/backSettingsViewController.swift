//
//  SettingsViewController.swift
//  uoyabause
//
//  Created by MiyamotoShinya on 2016/09/25.
//  Copyright © 2016年 devMiyax. All rights reserved.
//

import Foundation
import UIKit
import GameController


class OldSettingsViewController :UITableViewController,UIPickerViewDelegate, UIPickerViewDataSource {
    
    // MARK: - GameController Support
    private var gameController: GCController?



    let _DATEPICKER_CELL_HEIGHT:CGFloat = 128.0

    let _cart_group = 0
    let _cart_index = 0
    var _CartPickerIsShowing = false
    fileprivate let cartArray: [String] = [
        NSLocalizedString("None", comment: "No backup RAM"),
        NSLocalizedString("4Mbit BackupRam", comment: "4 megabit backup RAM"),
        NSLocalizedString("8Mbit BackupRam", comment: "8 megabit backup RAM"),
        NSLocalizedString("16Mbit BackupRam", comment: "16 megabit backup RAM"),
        NSLocalizedString("32Mbit BackupRam", comment: "32 megabit backup RAM"),
        NSLocalizedString("8Mbit DRAM", comment: "8 megabit DRAM"),
        NSLocalizedString("32Mbit DRAM", comment: "32 megabit DRAM")
    ]
    fileprivate let cartValues: NSArray = [ 0,2,3,4,5,6,7]
    @IBOutlet weak var _cart_sel_label: UILabel!
    @IBOutlet weak var _picker: UIPickerView!


    let _sound_group = 2
    let _sound_index = 0
    var _SoundPickerIsShowing = false
    fileprivate let soundArray: [String] = [
        NSLocalizedString("High Quality but slow", comment: "Sound option for high quality but slower performance"),
        NSLocalizedString("Low Quality but fast", comment: "Sound option for lower quality but faster performance")
    ]
    fileprivate let soundValues: NSArray = [ 1,0 ]
    //@IBOutlet weak var _sound_picker_label: UILabel!
    //@IBOutlet weak var _soundPicker: UIPickerView!

    let _resolution_group = 1
    let _resolution_index = 3
    var _ResolutionPickerIsShowing = false
    
    // Notification settings constants  
    let _notification_group = 2  // Add to sound group (section 2)
    let _notification_index = 1  // After the sound setting item
    fileprivate let resolutionArray: [String] = [
        NSLocalizedString("Native", comment: "Resolution option for native resolution"),
        NSLocalizedString("4x", comment: "Resolution option for 4 times the native resolution"),
        NSLocalizedString("2x", comment: "Resolution option for 2 times the native resolution"),
        NSLocalizedString("Original", comment: "Resolution option for original resolution")
    ]
    fileprivate let resolutionValues: NSArray = [ 0,1,2,3]
    @IBOutlet weak var _resolution_sel_label: UILabel!
    @IBOutlet weak var _resolution_picker: UIPickerView!

    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
    }

    @IBOutlet weak var _showFpsSwitch: UISwitch!
    @IBOutlet weak var _showFrameSkip: UISwitch!
    @IBOutlet weak var _keepAspectRate: UISwitch!
    @IBOutlet weak var _rotate_screen: UISwitch!
    @IBOutlet weak var _landscape: UISwitch!
    @IBOutlet weak var _analog_as_dpad: UISwitch!

    @IBAction func onChangeLandscapeMode(_ sender: Any) {
        let userDefaults = UserDefaults.standard
        userDefaults.set(_landscape.isOn, forKey: "landscape")
        userDefaults.synchronize()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)

        // テーブルビューを再読み込みして、すべてのセルのスタイルを適用
        tableView.reloadData()
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        // 背景色を設定
        view.backgroundColor = .defaultBackground
        tableView.backgroundColor = .defaultBackground
        
        // Debug: Original sections count
        print("Original storyboard sections count: \(super.numberOfSections(in: tableView))")

        // セルのスタイルを設定
        tableView.separatorColor = .colorAccent

        // スイッチのスタイルを設定
        setupSwitchStyles()

        // ラベルの色を設定（現在のモードに応じて）
        _cart_sel_label.textColor = .adaptiveTextColor
        _resolution_sel_label.textColor = .adaptiveTextColor

        _picker.isHidden = !_CartPickerIsShowing
        _picker.delegate = self
        _picker.dataSource = self
        _picker.backgroundColor = .defaultBackground

        _resolution_picker.isHidden = !_SoundPickerIsShowing
        _resolution_picker.delegate = self
        _resolution_picker.dataSource = self
        _resolution_picker.backgroundColor = .defaultBackground

        let userDefaults = UserDefaults.standard
        userDefaults.register(defaults: ["landscape": true])
        //userDefaults.set(true, forKey: "landscape")
        _landscape.isOn = userDefaults.bool(forKey: "landscape")

        //
        let plist = SettingsViewController.getSettingPlist()

        //_BultinBiosswitch.isOn = plist.value(forKey: "builtin bios") as! Bool
        _showFpsSwitch.isOn = plist.value(forKey: "show fps") as! Bool
        _showFrameSkip.isOn = plist.value(forKey: "frame skip") as! Bool
        _keepAspectRate.isOn = plist.value(forKey: "keep aspect rate") as! Bool
        _rotate_screen.isOn = plist.value(forKey: "rotate screen") as! Bool

        _analog_as_dpad.isOn = plist.value(forKey: "analog as dpad") as? Bool ?? false


        let cart_index = plist.value(forKey: "cartridge") as! Int

        var index : Int = 0
        for  i in cartValues {
            if( cart_index == i as! Int){
                _cart_sel_label.text = cartArray[index]
            }
            index += 1;
        }



        var resolution_index = plist.value(forKey: "rendering resolution") as? Int
        if( resolution_index == nil ){
            resolution_index = 0
        }

        index = 0
        for  i in resolutionValues {
            if( resolution_index == i as? Int){
                _resolution_sel_label.text = resolutionArray[index]
            }
            index += 1;
        }
        
        // GameController設定
        setupGameController()
    }

    static func getSettingFilname() -> String {
        let libraryPath = NSSearchPathForDirectoriesInDomains(.libraryDirectory, .userDomainMask, true)[0]
        let filename = "settings.plist"
        let filePath = libraryPath + "/" + filename
        return filePath
    }

    static func getSettingPlist() -> NSMutableDictionary {
        let filePath = getSettingFilname()
        let manager = FileManager()
        if( !manager.fileExists(atPath: filePath) ){
            let bundelfilePath = Bundle.main.path(forResource: "settings", ofType: "plist")
            do{

                try
                    FileManager.default.copyItem(atPath: bundelfilePath!, toPath: filePath)

            } catch let error as NSError {
                // Catch fires here, with an NSError being thrown
                print("error occurred, here are the details:\n \(error)")
            }
        }
        let plist = NSMutableDictionary(contentsOfFile:filePath)
        return plist!;
    }



    /*
     pickerに表示する列数を返すデータソースメソッド.
     (実装必須)
     */
    func numberOfComponents(in pickerView: UIPickerView) -> Int {
        return 1
    }

    /*
     pickerに表示する行数を返すデータソースメソッド.
     (実装必須)
     */
    func pickerView(_ pickerView: UIPickerView, numberOfRowsInComponent component: Int) -> Int{

        if( pickerView == _picker){
            return cartArray.count
        }
        else if( pickerView == _resolution_picker){
            return resolutionArray.count
        }
        return 0;


    }

    /*
     pickerに表示する値を返すデリゲートメソッド.
     */
    func pickerView(_ pickerView: UIPickerView, titleForRow row: Int, forComponent component: Int) -> String? {

        if( pickerView == _picker){
            return cartArray[row]
        }
        else if( pickerView == _resolution_picker){
            return resolutionArray[row]
        }
        return soundArray[row]
    }

    // ピッカービューのスタイルを設定
    func pickerView(_ pickerView: UIPickerView, viewForRow row: Int, forComponent component: Int, reusing view: UIView?) -> UIView {
        var label = view as? UILabel

        if label == nil {
            label = UILabel()
            label?.font = UIFont.systemFont(ofSize: 16)
            label?.textAlignment = .center
        }

        // テキスト色を設定（現在のモードに応じて）
        label?.textColor = .adaptiveTextColor

        if pickerView == _picker {
            label?.text = cartArray[row]
        } else if pickerView == _resolution_picker {
            label?.text = resolutionArray[row]
        } else {
            label?.text = soundArray[row]
        }

        return label!
    }

    /*
     pickerが選択された際に呼ばれるデリゲートメソッド.
     */
    func pickerView(_ pickerView: UIPickerView, didSelectRow row: Int, inComponent component: Int) {

        if( pickerView == _picker ){

            let plist = SettingsViewController.getSettingPlist();
            plist.setObject(cartValues[row], forKey: "cartridge" as NSCopying)
            plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)

            _cart_sel_label.text = cartArray[row]

            _CartPickerIsShowing = false;
            _picker.isHidden = !_CartPickerIsShowing;
            self.tableView.beginUpdates()
            self.tableView.endUpdates()
        }


        else if( pickerView == _resolution_picker ){

            let plist = SettingsViewController.getSettingPlist();
            plist.setObject(resolutionValues[row], forKey: "rendering resolution" as NSCopying)
            plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)


            _resolution_sel_label.text = resolutionArray[row]

            _ResolutionPickerIsShowing = false;
            _resolution_picker.isHidden = !_ResolutionPickerIsShowing;
            self.tableView.beginUpdates()
            self.tableView.endUpdates()
        }

    }

    @IBAction func RotateScreenChanged(_ sender: Any) {
        let plist = SettingsViewController.getSettingPlist();
        plist.setObject(_rotate_screen.isOn, forKey: "rotate screen" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)

    }


    @IBAction func ShowFPSChanged(_ sender: AnyObject) {

        let plist = SettingsViewController.getSettingPlist();
        plist.setObject(_showFpsSwitch.isOn, forKey: "show fps" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)
    }

    @IBAction func FrameSkipChanged(_ sender: AnyObject) {

        let plist = SettingsViewController.getSettingPlist();
        plist.setObject(_showFrameSkip.isOn, forKey: "frame skip" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)


    }

    @IBAction func AspectrateChnaged(_ sender: AnyObject) {
        let plist = SettingsViewController.getSettingPlist();
        plist.setObject(_keepAspectRate.isOn, forKey: "keep aspect rate" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)
    }

    @IBAction func AasDChanged(_ sender: AnyObject) {
        let plist = SettingsViewController.getSettingPlist();
        plist.setObject(_analog_as_dpad.isOn, forKey: "analog as dpad" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilname(), atomically: true)
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath){

        if( (indexPath as NSIndexPath).section == _cart_group && (indexPath as NSIndexPath).row == _cart_index){
            _CartPickerIsShowing = !_CartPickerIsShowing;
            _picker.isHidden = !_CartPickerIsShowing;

            self.tableView.beginUpdates()
            self.tableView.endUpdates()

            if( _picker.isHidden == false ){

                self._picker.alpha = 0
                UIView.animate(withDuration: 0.25, animations: { () -> Void in
                    self._picker.alpha = 1.0
                    }, completion: {(Bool) -> Void in

                })
            }
        }




        if( (indexPath as NSIndexPath).section == _resolution_group && (indexPath as NSIndexPath).row == _resolution_index){
            _ResolutionPickerIsShowing = !_ResolutionPickerIsShowing;
            _resolution_picker.isHidden = !_ResolutionPickerIsShowing;

            self.tableView.beginUpdates()
            self.tableView.endUpdates()

            if( _resolution_picker.isHidden == false ){

                self._resolution_picker.alpha = 0
                UIView.animate(withDuration: 0.25, animations: { () -> Void in
                    self._resolution_picker.alpha = 1.0
                    }, completion: {(Bool) -> Void in

                })
            }
        }
        
        // Handle notification settings navigation
        if (indexPath as NSIndexPath).section == _notification_group && (indexPath as NSIndexPath).row == _notification_index {
            showNotificationSettings()
        }
    }
    
    // MARK: - Table View Data Source Override
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        if section == _notification_group {
            // Add one more row to the sound section for notification settings
            return super.tableView(tableView, numberOfRowsInSection: section) + 1
        }
        return super.tableView(tableView, numberOfRowsInSection: section)
    }
    
    // MARK: - Notification Settings Navigation
    private func showNotificationSettings() {
        let storyboard = UIStoryboard(name: "Main", bundle: nil)
        
        // Create NotificationSettingsViewController programmatically since it's not in storyboard
        let notificationSettingsVC = NotificationSettingsViewController()
        notificationSettingsVC.title = NSLocalizedString("Notification Settings", comment: "Notification settings title")
        
        navigationController?.pushViewController(notificationSettingsVC, animated: true)
    }


    override func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        var height: CGFloat = self.tableView.rowHeight

        if ( (indexPath as NSIndexPath).section == _cart_group && (indexPath as NSIndexPath).row == _cart_index+1){
            height =  self._CartPickerIsShowing ? self._DATEPICKER_CELL_HEIGHT : CGFloat(0)
        }

        if ( (indexPath as NSIndexPath).section == _sound_group && (indexPath as NSIndexPath).row == _sound_index+1){
            height =  self._SoundPickerIsShowing ? self._DATEPICKER_CELL_HEIGHT : CGFloat(0)
        }

        if ( (indexPath as NSIndexPath).section == _resolution_group && (indexPath as NSIndexPath).row == _resolution_index+1){
            height =  self._ResolutionPickerIsShowing ? self._DATEPICKER_CELL_HEIGHT : CGFloat(0)
        }

        return height
    }

    // セルのスタイルを設定
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        // Handle notification settings cell separately
        if (indexPath as NSIndexPath).section == _notification_group && (indexPath as NSIndexPath).row == _notification_index {
            let cell = UITableViewCell(style: .value1, reuseIdentifier: "NotificationCell")
            cell.textLabel?.text = NSLocalizedString("Notification Settings", comment: "Notification settings menu item")
            cell.accessoryType = .disclosureIndicator
            cell.backgroundColor = .defaultBackground
            cell.textLabel?.textColor = .adaptiveTextColor
            cell.detailTextLabel?.textColor = .adaptiveTextColor
            
            // 選択時の背景色
            let selectedBackgroundView = UIView()
            selectedBackgroundView.backgroundColor = .selectedBackground
            cell.selectedBackgroundView = selectedBackgroundView
            
            return cell
        }
        
        // For all other cells, use super implementation
        let cell = super.tableView(tableView, cellForRowAt: indexPath)

        // 背景色を設定
        cell.backgroundColor = .defaultBackground

        // テキスト色を設定（現在のモードに応じて）
        cell.textLabel?.textColor = .adaptiveTextColor
        cell.detailTextLabel?.textColor = .adaptiveTextColor

        // セル内のすべてのラベルの色を設定
        for subview in cell.contentView.subviews {
            if let label = subview as? UILabel {
                label.textColor = .adaptiveTextColor
            }

            // サブビュー内のラベルも検索
            for innerSubview in subview.subviews {
                if let label = innerSubview as? UILabel {
                    label.textColor = .adaptiveTextColor
                }
            }
        }

        // 選択時の背景色
        let selectedBackgroundView = UIView()
        selectedBackgroundView.backgroundColor = .selectedBackground
        cell.selectedBackgroundView = selectedBackgroundView

        return cell
    }

    // セクションヘッダーのスタイルを設定
    override func tableView(_ tableView: UITableView, willDisplayHeaderView view: UIView, forSection section: Int) {
        if let headerView = view as? UITableViewHeaderFooterView {
            headerView.textLabel?.textColor = .adaptiveTextColor
            headerView.contentView.backgroundColor = .colorPrimary
        }
    }

    // スイッチのスタイルを設定するメソッド
    private func setupSwitchStyles() {
        // スイッチの色を設定
        _showFpsSwitch.onTintColor = .colorPrimary
        _showFrameSkip.onTintColor = .colorPrimary
        _keepAspectRate.onTintColor = .colorPrimary
        _rotate_screen.onTintColor = .colorPrimary
        _landscape.onTintColor = .colorPrimary
        _analog_as_dpad.onTintColor = .colorPrimary

        // iOS 13以降ではスイッチのサムの色も設定可能
        if #available(iOS 13.0, *) {
            let thumbColor: UIColor = isDarkMode ? .appWhite : .appBlack
            _showFpsSwitch.thumbTintColor = thumbColor
            _showFrameSkip.thumbTintColor = thumbColor
            _keepAspectRate.thumbTintColor = thumbColor
            _rotate_screen.thumbTintColor = thumbColor
            _landscape.thumbTintColor = thumbColor
            _analog_as_dpad.thumbTintColor = thumbColor
        }
    }

    // 現在のモードがダークモードかどうかを判定
    private var isDarkMode: Bool {
        if #available(iOS 13.0, *) {
            return traitCollection.userInterfaceStyle == .dark
        } else {
            return false
        }
    }

    // モード変更時に呼び出されるメソッド
    private func updateInterfaceStyle() {
        // スイッチのスタイルを更新
        setupSwitchStyles()

        // ラベルの色を更新
        _cart_sel_label.textColor = .adaptiveTextColor
        _resolution_sel_label.textColor = .adaptiveTextColor

        // テーブルビューを再読み込み
        tableView.reloadData()
    }

    // モード変更時にも呼び出される
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)

        if #available(iOS 13.0, *) {
            if traitCollection.hasDifferentColorAppearance(comparedTo: previousTraitCollection) {
                updateInterfaceStyle()
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
                        self?.navigationController?.popViewController(animated: true)
                    }
                }
            }
            
            // Menu button (設定画面を閉じる)
            extendedGamepad.buttonMenu.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigationController?.popViewController(animated: true)
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
                        self?.navigationController?.popViewController(animated: true)
                    }
                }
            }
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
}
