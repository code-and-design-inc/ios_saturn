/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

import UIKit

class TabCheatViewController: UIViewController {
    
    private var gameId: String?
    private var activeCheatCodes: Set<String> = []
    private var cheatTabBarController: UITabBarController!
    
    // MARK: - Lifecycle
    
    init(gameId: String?) {
        self.gameId = gameId
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
    }
    
    static func newInstance(gameId: String, currentCheatCodes: [String]?) -> TabCheatViewController {
        let viewController = TabCheatViewController(gameId: gameId)
        viewController.gameId = gameId
        // YabauseManagerから現在のチートコード状態を取得
        viewController.activeCheatCodes = Set(YabauseManager.shared.getCurrentCheatCodes())
        return viewController
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupNavigationBar()
        setupViewControllers()
/*
        // タブバーのアピアランスをカスタマイズ
        let appearance = UITabBarAppearance()
        appearance.configureWithOpaqueBackground()
        appearance.backgroundColor = .systemBackground
        
        // フォントサイズを大きくする
        let fontSize: CGFloat = 18.0
        let attributes = [NSAttributedString.Key.font: UIFont.systemFont(ofSize: fontSize, weight: .medium)]
        appearance.stackedLayoutAppearance.normal.titleTextAttributes = attributes
        appearance.stackedLayoutAppearance.selected.titleTextAttributes = attributes
        
        self.tabBar.standardAppearance = appearance
        if #available(iOS 15.0, *) {
            self.tabBar.scrollEdgeAppearance = appearance
        }
 */
    }
    
    private func setupNavigationBar() {
        let closeButton = UIBarButtonItem(title: NSLocalizedString("Close", comment: "Close Dialog"), style: .plain, target: self, action: #selector(closeButtonTapped))
        navigationItem.leftBarButtonItem = closeButton
        title = NSLocalizedString("Action Replay Code Manager", comment: "Action Replay Code Manager Settings")
    }
    
    var completionHandler: (() -> Void)?

    @objc private func closeButtonTapped() {
        dismiss(animated: true) {
            self.completionHandler?()
        }
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        sendCheatListToYabause()
    }
    
    
    // MARK: - Setup
    
    private func setupViewControllers() {
        cheatTabBarController = UITabBarController()
        
        let localVC = LocalCheatViewController.newInstance(gameId: gameId ?? "", delegate: self)
        localVC.tabBarItem = UITabBarItem(title: "Local", image: UIImage(systemName: "iphone"), tag: 0)
        
        let cloudVC = CloudCheatViewController.newInstance(gameId: gameId ?? "", delegate: self)
        cloudVC.tabBarItem = UITabBarItem(title: "Shared", image: UIImage(systemName: "arrow.trianglehead.counterclockwise.icloud"), tag: 1)
        
        cheatTabBarController.viewControllers = [localVC, cloudVC]
        
        addChild(cheatTabBarController)
        view.addSubview(cheatTabBarController.view)
        cheatTabBarController.view.frame = view.bounds
        cheatTabBarController.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        cheatTabBarController.didMove(toParent: self)
    }
    
    // MARK: - Cheat Management
    
    func addActiveCheat(_ code: String) {
        activeCheatCodes.insert(code)
    }
    
    func removeActiveCheat(_ code: String) {
        activeCheatCodes.remove(code)
    }
    
    func isActive(_ code: String) -> Bool {
        return activeCheatCodes.contains(code)
    }
    
    private func sendCheatListToYabause() {
        if activeCheatCodes.isEmpty {
            YabauseManager.shared.updateCheatCode(nil)
        } else {
            let codes = Array(activeCheatCodes)
            YabauseManager.shared.updateCheatCode(codes)
        }
    }
}

// MARK: - CheatViewControllerDelegate

protocol CheatViewControllerDelegate: AnyObject {
    func addActiveCheat(_ code: String)
    func removeActiveCheat(_ code: String)
    func isActive(_ code: String) -> Bool
}

extension TabCheatViewController: CheatViewControllerDelegate {
    // Protocol implementation is already covered by the class methods
}
