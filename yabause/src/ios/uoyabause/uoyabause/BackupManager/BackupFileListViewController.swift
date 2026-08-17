import UIKit
import FirebaseCore
import FirebaseAuth
import FirebaseDatabase
import FirebaseStorage

struct BackupItem {
    var index: Int32 = 0
    var filename: String?
    var comment: String?
    var language: Int32 = 0
    var savedate: String?
    var datasize: Int32 = 0
    var blocksize: Int32 = 0
    var url: String = ""
    var key: String?
    
    // DATE_PATTERN equivalent
    static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy/MM/dd HH:mm"
        return formatter
    }()
}

struct BackupDevice {
    var name: String
    var id: Int
    
    static let DEVICE_CLOUD = 9999 // Assuming this is the correct value for cloud device
}

class BackupFileListViewController: UIViewController {

    private var activityIndicator: UIActivityIndicatorView!
    private var backupManagerTabBarController: BackupManagerTabBarController!
    private var backupDevices: [BackupDevice] = []

    var completionHandler: ((_ selectedBackup: BackupItem?) -> Void)?

    init(completionHandler: ((_ selectedBackup: BackupItem?) -> Void)? = nil) {
        self.completionHandler = completionHandler
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        setupNavigationBar()
        
        activityIndicator = UIActivityIndicatorView(style: .large)
        activityIndicator.hidesWhenStopped = true
        activityIndicator.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(activityIndicator)
        
        NSLayoutConstraint.activate([
            activityIndicator.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            activityIndicator.centerYAnchor.constraint(equalTo: view.centerYAnchor)
        ])
        
        fetchDeviceList()
        setupBackupManagerTabBarController()
        
        // Ensure activityIndicator is always on top
        view.bringSubviewToFront(activityIndicator)
    }

    func startLoading() {
        DispatchQueue.main.async {
            self.activityIndicator.startAnimating()
        }
    }

    func stopLoading() {
        DispatchQueue.main.async {
            self.activityIndicator.stopAnimating()
        }
    }
    
    private func setupBackupManagerTabBarController() {
        backupManagerTabBarController = BackupManagerTabBarController(backupDevices: backupDevices, backupListVC: self)
        addChild(backupManagerTabBarController)
        view.addSubview(backupManagerTabBarController.view)
        backupManagerTabBarController.view.frame = view.bounds
        //backupManagerTabBarController.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        backupManagerTabBarController.didMove(toParent: self)
    }
    
    private func setupNavigationBar() {
        let closeButton = UIBarButtonItem(title: NSLocalizedString("Close", comment: "Close Dialoig"), style: .plain, target: self, action: #selector(closeButtonTapped))
        navigationItem.leftBarButtonItem = closeButton
        title = NSLocalizedString("Backup Manager", comment: "Backup Manager Settings")
    }
    
    @objc private func closeButtonTapped() {
        dismiss(animated: true) {
            self.completionHandler?(nil)
        }
    }
    
    private func fetchDeviceList() {
        guard let jsonStr = YSGetBackupDevicelist() else {
            print("Failed to get device list")
            return
        }
        do {
            if let jsonData = jsonStr.data(using: .utf8) {
                if let json = try JSONSerialization.jsonObject(with: jsonData, options: []) as? [String: Any] {
                    if let devices = json["devices"] as? [[String: Any]] {
                        for device in devices {
                            if let name = device["name"] as? String,
                               let id = device["id"] as? Int {
                                let backupDevice = BackupDevice(name: name, id: id)
                                backupDevices.append(backupDevice)
                            }
                        }
                    }
                }
            }
            
            // Add cloud device if user is logged in
            //if let currentUser = Auth.auth().currentUser {
                let cloudDevice = BackupDevice(name: "Cloud", id: BackupDevice.DEVICE_CLOUD)
                backupDevices.append(cloudDevice)
            //}
            
        } catch {
            print("Failed to parse JSON: \(error.localizedDescription)")
        }
        
        if backupDevices.isEmpty {
            print("Can't find any devices")
        }
        
        for device in backupDevices {
            print("Device: \(device.name), ID: \(device.id)")
        }
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        if self.isBeingDismissed {
            completionHandler?(nil)
        }
    }
    
    @objc private func cancel() {
        completionHandler?(nil)
        dismiss(animated: true) {
            self.completionHandler?(nil)
        }
    }
    
    func presentationControllerDidDismiss(_ presentationController: UIPresentationController) {
        completionHandler?(nil)
    }
}

class BackupManagerTabBarController: UITabBarController {
    var backupDevices: [BackupDevice] = []
    var backupList: [BackupDeviceViewController] = []
    weak var backupListVC: BackupFileListViewController?
    
    init(backupDevices: [BackupDevice], backupListVC: BackupFileListViewController) {
        self.backupDevices = backupDevices
        self.backupListVC = backupListVC
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupViewControllers()
        
        // タブバーの見た目を設定
        let appearance = UITabBarAppearance()
        appearance.stackedLayoutAppearance.normal.titlePositionAdjustment = UIOffset(horizontal: 0, vertical: 0)
        tabBar.standardAppearance = appearance
        if #available(iOS 15.0, *) {
            tabBar.scrollEdgeAppearance = appearance
        }
        
        // タブバーの幅を調整
        tabBar.itemPositioning = .fill
        tabBar.itemWidth = view.frame.width / CGFloat(backupDevices.count)
    }
    
    private func setupViewControllers() {
        var viewControllers: [UIViewController] = []
        
        self.view.backgroundColor = .systemBackground
        
        for (index, device) in backupDevices.enumerated() {
            let deviceVC = BackupDeviceViewController(device: device, deviceIndex: Int32(index)) { selectedBackup in
                self.showBackupOptions(device: device, for: selectedBackup)
            }
            deviceVC.tabBarItem = UITabBarItem(title: device.name, image: nil, tag: index)
            backupList.append(deviceVC)
            viewControllers.append(deviceVC)
        }
        
        self.viewControllers = viewControllers
    }
    
    private func showBackupOptions(device: BackupDevice, for backupItem: BackupItem) {
        let alertController = UIAlertController(title: nil, message: nil, preferredStyle: .actionSheet)
        
        if device.id == BackupDevice.DEVICE_CLOUD {
            // クラウドバックアップの場合の処理
            alertController.addAction(UIAlertAction(title: NSLocalizedString("Download to Internal memory", comment: "Download backup from cloud"), style: .default) { _ in
                self.downloadFromCloud(backupItem: backupItem)
            })
        } else if backupDevices.count > 1 {
            if device.id == 0 {
               
                if( backupDevices[1].id != BackupDevice.DEVICE_CLOUD ){
                    let message = String(format: NSLocalizedString("Copy to [%@]", comment: "copy a backup file to external device"), backupDevices[1].name)
                    alertController.addAction(UIAlertAction(title: message, style: .default) { _ in
                        YSCopy(Int32(self.backupDevices[1].id), backupItem.index)
                    })
                }
                
                // クラウドへのアップロード
                if let cloudDevice = backupDevices.first(where: { $0.id == BackupDevice.DEVICE_CLOUD }) {
                    alertController.addAction(UIAlertAction(title: NSLocalizedString("Upload to Cloud", comment: "Upload backup to cloud"), style: .default) { _ in
                        self.uploadToCloud(backupItem: backupItem)
                    })
                }
            } else {
                alertController.addAction(UIAlertAction(title: NSLocalizedString("Copy to Internal memory", comment: "copy a backup file to internal memory"), style: .default) { _ in
                    YSCopy(Int32(self.backupDevices[0].id), backupItem.index)
                })
            }
        }
        
        alertController.addAction(UIAlertAction(title: NSLocalizedString("Delete", comment: "Delete file"), style: .destructive) { _ in
            let message = String(format: NSLocalizedString("Are you sure you want to delete [%@]", comment: "Confirm deletion message"), backupItem.filename ?? "")
            
            let confirmAlert = UIAlertController(title: NSLocalizedString("Confirm Deletion", comment: "Confirm deletion alert title"),
                                               message: message,
                                               preferredStyle: .alert)
            
            confirmAlert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel deletion"), style: .cancel, handler: nil))
            
            confirmAlert.addAction(UIAlertAction(title: NSLocalizedString("Delete", comment: "Confirm deletion"), style: .destructive) { _ in
                if device.id == BackupDevice.DEVICE_CLOUD {
                    self.deleteFromCloud(backupItem: backupItem)
                } else {
                    YSDeleteBackupFile(backupItem.index)
                    if device.id == 0 {
                        self.backupList[0].updateSaveList()
                    } else {
                        self.backupList[1].updateSaveList()
                    }
                }
            })
            
            self.present(confirmAlert, animated: true, completion: nil)
        })
        
        alertController.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: "cancel"), style: .cancel, handler: nil))
        
        if let popoverController = alertController.popoverPresentationController {
            popoverController.sourceView = view
            popoverController.sourceRect = CGRect(x: view.bounds.midX, y: view.bounds.midY, width: 0, height: 0)
            popoverController.permittedArrowDirections = []
        }
        
        present(alertController, animated: true)
    }
    
    private func uploadToCloud(backupItem: BackupItem) {
        backupListVC?.startLoading()
        // Get file data
        guard let jsonStr = YSGetBackupFile(backupItem.index) else {
            backupListVC?.stopLoading()
            return
        }
        
        // Check Firebase Auth
        guard let currentUser = Auth.auth().currentUser else {
            backupListVC?.stopLoading()
            return
        }
        
        // Setup database reference
        let baseRef = Database.database().reference()
        let userRef = "/user-posts/\(currentUser.uid)/backup/"
        
        // Check if we have an existing key
        if let key = backupItem.key {
            // Update existing backup
            let backupRef = baseRef.child(userRef).child(key)
            let backupData: [String: Any] = [
                "filename": backupItem.filename ?? "",
                "comment": backupItem.comment ?? "",
                "savedate": backupItem.savedate ?? "",
                "datasize": backupItem.datasize,
                "blocksize": backupItem.blocksize
            ]
            
            backupRef.setValue(backupData) { [weak self] error, _ in
                guard let self = self else { return }
                if let error = error {
                    print("Error updating backup: \(error)")
                    self.backupListVC?.stopLoading()
                    self.showAlert(title: "Error", message: "Failed to update backup: \(error.localizedDescription)")
                    return
                }
                
                // Upload file data to Storage
                let dbRef = userRef + key
                self.uploadData(backupItem: backupItem, jsonStr: jsonStr, userId: currentUser.uid, dbRef: dbRef)
            }
        } else {
            // Check current record count
            baseRef.child(userRef).observeSingleEvent(of: .value) { [weak self] snapshot in
                guard let self = self else { return }
                let count = snapshot.childrenCount
                
                // Get max backup count
                let countUrl = "/user-posts/\(currentUser.uid)/max_backup_count"
                baseRef.child(countUrl).observeSingleEvent(of: .value) { snapshot in
                    guard let maxCount = snapshot.value as? Int64 else { return }
                    
                    if count < maxCount {
                        // Create new backup
                        let newBackupRef = baseRef.child(userRef).childByAutoId()
                        guard let newKey = newBackupRef.key else { return }
                        
                        var updatedBackupItem = backupItem
                        updatedBackupItem.key = newKey
                        
                        let backupData: [String: Any] = [
                            "filename": backupItem.filename ?? "",
                            "comment": backupItem.comment ?? "",
                            "savedate": backupItem.savedate ?? "",
                            "datasize": backupItem.datasize,
                            "blocksize": backupItem.blocksize
                        ]
                        
                        newBackupRef.setValue(backupData) { [weak self] error, _ in
                            guard let self = self else { return }
                            if let error = error {
                                print("Error creating backup: \(error)")
                                self.backupListVC?.stopLoading()
                                self.showAlert(title: "Error", message: "Failed to create backup: \(error.localizedDescription)")
                                return
                            }
                            
                            // Upload file data to Storage
                            let dbRef = userRef + newKey
                            self.uploadData(backupItem: updatedBackupItem, jsonStr: jsonStr, userId: currentUser.uid, dbRef: dbRef)
                        }
                    } else {
                        // Show max slot count reached message
                        DispatchQueue.main.async {
                            let alert = UIAlertController(
                                title: "Max Slots Reached",
                                message: "You have reached the max slot count. To expand slot count, get pro version.",
                                preferredStyle: .alert
                            )
                            
                            alert.addAction(UIAlertAction(title: "Get Pro Version", style: .default) { _ in
                                if let url = URL(string: "https://apps.apple.com/app/yabasanshiro-pro/id123456789") {
                                    UIApplication.shared.open(url)
                                }
                            })
                            
                            alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
                            
                            self.backupListVC?.stopLoading()
                            if let presentingVC = self.view?.window?.rootViewController {
                                presentingVC.present(alert, animated: true)
                            }
                        }
                    }
                }
            }
        }
    }
    
    private func uploadData(backupItem: BackupItem, jsonStr: String, userId: String, dbRef: String) {
        // Create a reference to Firebase Storage
        let storage = Storage.storage()
        let storageRef = storage.reference()
        let backupRef = storageRef.child("backups/\(userId)/\(backupItem.key ?? UUID().uuidString).json")
        
        // Convert string to Data
        guard let data = jsonStr.data(using: .utf8) else {
            print("Error converting string to data")
            return
        }
        
        // Upload the file
        let metadata = StorageMetadata()
        metadata.contentType = "application/json"
        
        backupRef.putData(data, metadata: metadata) { [weak self] metadata, error in
            guard let self = self else { return }
            if let error = error {
                print("Error uploading file: \(error)")
                self.backupListVC?.stopLoading()
                self.showAlert(title: "Error", message: "Failed to upload file: \(error.localizedDescription)")
                return
            }
            
            // Get the download URL
            backupRef.downloadURL { [weak self] url, error in
                guard let self = self else { return }
                if let error = error {
                    print("Error getting download URL: \(error)")
                    self.backupListVC?.stopLoading()
                    self.showAlert(title: "Error", message: "Failed to get download URL: \(error.localizedDescription)")
                    return
                }
                
                guard let downloadURL = url else {
                    print("Error: Download URL is nil")
                    return
                }
                
                // Update the database record with the file URL
                let baseRef = Database.database().reference()
                baseRef.child(dbRef).updateChildValues(["url": downloadURL.absoluteString]) { [weak self] error, _ in
                    guard let self = self else { return }
                    self.backupListVC?.stopLoading()
                    if let error = error {
                        print("Error updating database with URL: \(error)")
                        self.showAlert(title: "Error", message: "Failed to update database with URL: \(error.localizedDescription)")
                    } else {
                        print("Backup uploaded successfully")
                        DispatchQueue.main.async { [weak self] in
                            guard let self = self else { return }
                            // Delay the update slightly to ensure Firebase data is consistent
                            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                                if let cloudVC = self.backupList.first(where: { $0.device.id == BackupDevice.DEVICE_CLOUD }) {
                                    cloudVC.updateSaveList()
                                }
                            }
                            self.showToast(message: NSLocalizedString("Backup uploaded successfully", comment: "Backup uploaded successfully"))
                        }
                    }
                }
            }
        }
    }
    
    private func downloadFromCloud(backupItem: BackupItem) {
        guard !backupItem.url.isEmpty else { return }
        backupListVC?.startLoading()
        
        let storage = Storage.storage()
        let httpsReference = storage.reference(forURL: backupItem.url)
        let ONE_MEGABYTE: Int64 = 1024 * 1024
        
        httpsReference.getData(maxSize: ONE_MEGABYTE) { [weak self] data, error in
            guard let self = self else { return }
            if let error = error {
                self.backupListVC?.stopLoading()
                self.showAlert(title: "Error", message: "Failed to download from cloud: \(error.localizedDescription)")
                return
            }
            
            guard let data = data,
                  let jsonStr = String(data: data, encoding: .utf8),
                  let deviceVC = self.backupList.first(where: { $0.device.id == 0 }) else {
                return
            }
            
            // Get free space from device
            if let fileListStr = YSGetBackupFilelist(0),
               let fileListData = fileListStr.data(using: .utf8),
               let json = try? JSONSerialization.jsonObject(with: fileListData) as? [String: Any],
               let status = json["status"] as? [String: Int32],
               let freeSize = status["freesize"] {
                
                if backupItem.datasize >= freeSize {
                    self.backupListVC?.stopLoading()
                    self.showAlert(title: "Error", message: "Not enough free space in the target device")
                    return
                }
            }
            
            // Save the file
            YSPutFile(jsonStr)
            
            self.backupListVC?.stopLoading()
            self.showToast(message: NSLocalizedString("Download completed", comment: "Download completed"))
        }
    }
    
    private func showAlert(title: String, message: String, completion: (() -> Void)? = nil) {
        DispatchQueue.main.async {
            let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in
                completion?()
            })
            self.present(alert, animated: true)
        }
    }
    
    private func deleteFromCloud(backupItem: BackupItem) {
        guard let currentUser = Auth.auth().currentUser,
              let key = backupItem.key else {
            return
        }
        backupListVC?.startLoading()
        
        let baseRef = Database.database().reference()
        let backupRef = baseRef.child("/user-posts/\(currentUser.uid)/backup/\(key)")
        
        backupRef.removeValue { [weak self] error, _ in
            guard let self = self else { return }
            self.backupListVC?.stopLoading()
            if let error = error {
                print("Error deleting backup: \(error)")
                self.showAlert(title: "Error", message: "Failed to delete backup: \(error.localizedDescription)")
            } else {
                print("Backup deleted successfully")
                if let cloudVC = self.backupList.first(where: { $0.device.id == BackupDevice.DEVICE_CLOUD }) {
                    cloudVC.updateSaveList()
                }
            }
        }
    }
    
    private func showToast(message: String) {
        let toastLabel = UILabel(frame: CGRect(x: self.view.frame.size.width/2 - 150, y: self.view.frame.size.height-100, width: 300, height: 35))
        toastLabel.backgroundColor = UIColor.black.withAlphaComponent(0.6)
        toastLabel.textColor = UIColor.white
        toastLabel.textAlignment = .center;
        toastLabel.font = UIFont(name: "Helvetica Neue", size: 12.0)
        toastLabel.text = message
        toastLabel.alpha = 1.0
        toastLabel.layer.cornerRadius = 10;
        toastLabel.clipsToBounds  =  true
        self.view.addSubview(toastLabel)
        UIView.animate(withDuration: 4.0, delay: 0.1, options: .curveEaseOut, animations: {
            toastLabel.alpha = 0.0
        }, completion: {(isCompleted) in
            toastLabel.removeFromSuperview()
        })
    }
}

class BackupDeviceViewController: UIViewController, UITableViewDataSource, UITableViewDelegate {
    
    private var tableView: UITableView!
    
    public var device: BackupDevice
    private var deviceIndex: Int32
    
    private var backupItems: [BackupItem] = []
    private var currentPage: Int = 0
    private var totalSize: Int32 = 0
    private var freeSize: Int32 = 0
    
    private var sumLabel: UILabel!
    
    var showBackupOption: ((_ selectedBackup: BackupItem) -> Void)?
    
    init(device: BackupDevice, deviceIndex: Int32, showBackupOption: ((_ selectedBackup: BackupItem) -> Void)? = nil) {
        self.device = device
        self.deviceIndex = deviceIndex
        self.showBackupOption = showBackupOption
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupViews()
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        updateSaveList()
    }
    
    private func setupViews() {
        tableView = UITableView(frame: .zero, style: .plain)
        tableView.translatesAutoresizingMaskIntoConstraints = false
        tableView.dataSource = self
        tableView.delegate = self
        tableView.backgroundColor = .systemBackground
        tableView.register(BackupItemCell.self, forCellReuseIdentifier: "BackupItemCell")
        view.addSubview(tableView)
        
        sumLabel = UILabel()
        sumLabel.translatesAutoresizingMaskIntoConstraints = false
        sumLabel.textAlignment = .center
        sumLabel.font = UIFont.systemFont(ofSize: 14)
        sumLabel.textColor = .secondaryLabel
        sumLabel.backgroundColor = .systemBackground
        view.addSubview(sumLabel)
        
        NSLayoutConstraint.activate([
            tableView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            tableView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            tableView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tableView.bottomAnchor.constraint(equalTo: sumLabel.topAnchor),
            
            sumLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            sumLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            sumLabel.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
            sumLabel.heightAnchor.constraint(equalToConstant: 44)
        ])
    }
    
    private func updateSumLabel() {
        guard let label = sumLabel else { return }
        
        DispatchQueue.main.async {
            let numberFormatter = NumberFormatter()
            numberFormatter.numberStyle = .decimal
            numberFormatter.groupingSeparator = ","
            numberFormatter.groupingSize = 3
            
            let usesize = self.totalSize - self.freeSize
            
            let formattedFreeSize = numberFormatter.string(from: NSNumber(value: usesize)) ?? "\(usesize)"
            let formattedTotalSize = numberFormatter.string(from: NSNumber(value: self.totalSize)) ?? "\(self.totalSize)"
            
            let localizedFormat = NSLocalizedString("%@ Byte used / %@ Byte total", comment: "Format string for storage info")
            label.text = String(format: localizedFormat, formattedFreeSize, formattedTotalSize)
        }
    }
    
    func updateSaveList() {
        if device.id == BackupDevice.DEVICE_CLOUD {
            updateCloudSaveList()
        } else {
            updateLocalSaveList()
        }
    }
    
    private func updateLocalSaveList() {
        guard let jsonStr = YSGetBackupFilelist(deviceIndex) else {
            print("Failed to get file list")
            return
        }
        
        backupItems.removeAll()
        
        do {
            if let jsonData = jsonStr.data(using: .utf8),
               let json = try JSONSerialization.jsonObject(with: jsonData, options: []) as? [String: Any] {
                
                if let status = json["status"] as? [String: Int32] {
                    totalSize = status["totalsize"] ?? 0
                    freeSize = status["freesize"] ?? 0
                }
                
                if let saves = json["saves"] as? [[String: Any]] {
                    for save in saves {
                        var backupItem = BackupItem()
                        backupItem.index = save["index"] as? Int32 ?? 0
                        
                        if let filenameBase64 = save["filename"] as? String,
                           let filenameData = Data(base64Encoded: filenameBase64) {
                            backupItem.filename = String(data: filenameData, encoding: .windowsCP932) ?? filenameBase64
                        }
                        
                        if let commentBase64 = save["comment"] as? String,
                           let commentData = Data(base64Encoded: commentBase64) {
                            backupItem.comment = String(data: commentData, encoding: .windowsCP932) ?? commentBase64
                        }
                        
                        backupItem.datasize = save["datasize"] as? Int32 ?? 0
                        backupItem.blocksize = save["blocksize"] as? Int32 ?? 0
                        
                        let year = (save["year"] as? Int32 ?? 0) + 1980
                        let month = save["month"] as? Int32 ?? 1
                        let day = save["day"] as? Int32 ?? 1
                        let hour = save["hour"] as? Int32 ?? 0
                        let minute = save["minute"] as? Int32 ?? 0
                        
                        let dateFormatter = DateFormatter()
                        dateFormatter.dateFormat = "yyyy/MM/dd HH:mm:ss"
                        if let date = dateFormatter.date(from: String(format: "%04d/%02d/%02d %02d:%02d:00", year, month, day, hour, minute)) {
                            backupItem.savedate = dateFormatter.string(from: date)
                        }
                        
                        backupItems.append(backupItem)
                    }
                }
            } else {
                totalSize = 0
                freeSize = 0
            }
            
            tableView.reloadData()
            updateSumLabel()
        } catch {
            print("Failed to parse JSON: \(error.localizedDescription)")
        }
    }
    
    private func updateCloudSaveList() {
        
        guard let currentUser = Auth.auth().currentUser else {
            let loginVC = LoginViewController()
            let navController = UINavigationController(rootViewController: loginVC)
            navController.modalPresentationStyle = .pageSheet
            loginVC.modalPresentationStyle = .fullScreen
            loginVC.completionHandler = { [weak self] success in
                if success {
                    self?.updateCloudSaveList()
                }
            }
            present(navController, animated: true, completion: nil)
            return
        }
        
        if let parentVC = parent as? BackupManagerTabBarController {
            parentVC.backupListVC?.startLoading()
        }
        
        let baseRef = Database.database().reference()
        let userRef = "/user-posts/\(currentUser.uid)"
        
        // Get backup list first
        let backupRef = baseRef.child("\(userRef)/backup")
        backupRef.observeSingleEvent(of: .value) { [weak self] snapshot in
            guard let self = self else { return }
            
            DispatchQueue.main.async {
                
                if let parentVC = self.parent as? BackupManagerTabBarController {
                    parentVC.backupListVC?.stopLoading()
                }
                
                self.backupItems.removeAll()
                
                for child in snapshot.children {
                    guard let snapshot = child as? DataSnapshot,
                          let dict = snapshot.value as? [String: Any] else { continue }
                    
                    var backupItem = BackupItem()
                    backupItem.key = snapshot.key
                    backupItem.filename = dict["filename"] as? String
                    backupItem.comment = dict["comment"] as? String
                    backupItem.savedate = dict["savedate"] as? String
                    backupItem.url = dict["url"] as? String ?? ""
                    backupItem.datasize = dict["datasize"] as? Int32 ?? 0
                    backupItem.blocksize = dict["blocksize"] as? Int32 ?? 0
                    
                    self.backupItems.append(backupItem)
                }
                
                if( self.tableView != nil ){
                    self.tableView.reloadData()
                }
                    
                
                // Get max backup count after loading the list
                baseRef.child("\(userRef)/max_backup_count").observeSingleEvent(of: .value) { [weak self] snapshot in
                    guard let self = self else { return }
                                        DispatchQueue.main.async {
                        if let maxCount = snapshot.value as? Int32 {
                            self.totalSize = maxCount
                            self.freeSize = maxCount - Int32(self.backupItems.count)
                            self.updateSumLabel()
                        }
                    }
                }
            }
        }

    }
    
    // MARK: - UITableViewDataSource
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return backupItems.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "BackupItemCell", for: indexPath) as! BackupItemCell
        let backupItem = backupItems[indexPath.row]
        cell.configure(with: backupItem)
        return cell
    }
    
    // MARK: - UITableViewDelegate
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        let backupItem = backupItems[indexPath.row]
        self.showBackupOption?(backupItem)
    }
}

class BackupItemCell: UITableViewCell {
    private let filenameLabel = UILabel()
    private let commentLabel = UILabel()
    private let savedateLabel = UILabel()
    private let datasizeLabel = UILabel()
    
    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: style, reuseIdentifier: reuseIdentifier)
        setupViews()
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    private func setupViews() {
        filenameLabel.font = UIFont.systemFont(ofSize: 16, weight: .medium)
        commentLabel.font = UIFont.systemFont(ofSize: 14)
        commentLabel.textColor = .label
        commentLabel.numberOfLines = 0
        savedateLabel.font = UIFont.systemFont(ofSize: 12)
        savedateLabel.textColor = .secondaryLabel
        datasizeLabel.font = UIFont.systemFont(ofSize: 12)
        datasizeLabel.textColor = .secondaryLabel
        
        [filenameLabel, commentLabel, savedateLabel, datasizeLabel].forEach {
            $0.translatesAutoresizingMaskIntoConstraints = false
            contentView.addSubview($0)
        }
        
        let dateAndSizeStack = UIStackView(arrangedSubviews: [savedateLabel, datasizeLabel])
        dateAndSizeStack.axis = .vertical
        dateAndSizeStack.alignment = .trailing
        dateAndSizeStack.spacing = 2
        dateAndSizeStack.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(dateAndSizeStack)
        
        NSLayoutConstraint.activate([
            filenameLabel.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 8),
            filenameLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            filenameLabel.trailingAnchor.constraint(equalTo: dateAndSizeStack.leadingAnchor, constant: -8),
            
            commentLabel.topAnchor.constraint(equalTo: filenameLabel.bottomAnchor, constant: 4),
            commentLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            commentLabel.trailingAnchor.constraint(equalTo: dateAndSizeStack.leadingAnchor, constant: -8),
            commentLabel.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -8),
            
            dateAndSizeStack.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 8),
            dateAndSizeStack.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -16),
            dateAndSizeStack.widthAnchor.constraint(lessThanOrEqualTo: contentView.widthAnchor, multiplier: 0.4),
            
            dateAndSizeStack.leadingAnchor.constraint(greaterThanOrEqualTo: contentView.leadingAnchor, constant: 100)
        ])
    }
    
    func configure(with backupItem: BackupItem) {
        filenameLabel.text = backupItem.filename
        commentLabel.text = backupItem.comment
        savedateLabel.text = String(format: NSLocalizedString("Date: %@", comment: "last update date time"), backupItem.savedate ?? "")
        datasizeLabel.text = String(format: NSLocalizedString("Size: %@", comment: "File isze"), formatFileSize(Int(backupItem.datasize)))
    }
    
    private func formatFileSize(_ size: Int) -> String {
        let formatter = ByteCountFormatter()
        formatter.allowedUnits = [.useAll]
        formatter.countStyle = .file
        return formatter.string(fromByteCount: Int64(size))
    }
}

extension String.Encoding {
    static let windowsCP932 = String.Encoding(rawValue: CFStringConvertEncodingToNSStringEncoding(CFStringEncoding(CFStringEncodings.dosJapanese.rawValue)))
}
