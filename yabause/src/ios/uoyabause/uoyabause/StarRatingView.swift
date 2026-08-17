/*  Copyright 2024 devMiyax(smiyaxdev@gmail.com)

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

class StarRatingView: UIView {

    // MARK: - Properties
    private let starCount = 5
    private var starButtons: [UIButton] = []
    private var starSize: CGFloat = 35 // 星のサイズを大きくする
    private var starSpacing: CGFloat = 8 // 間隔を広げる

    // Current rating (1-5)
    private(set) var rating: Int = 3 {
        didSet {
            updateStars()
            ratingDidChange?(rating)
        }
    }

    // Star colors
    var starColor: UIColor = .systemYellow {
        didSet {
            updateStars()
        }
    }

    // Callback when rating changes
    var ratingDidChange: ((Int) -> Void)?

    // MARK: - Initialization
    override init(frame: CGRect) {
        super.init(frame: frame)
        setupStars()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupStars()
    }

    // MARK: - Setup
    private func setupStars() {
        // Remove any existing stars
        starButtons.forEach { $0.removeFromSuperview() }
        starButtons.removeAll()

        // スターを配置するコンテナを作成
        let stackView = UIStackView()
        stackView.axis = .horizontal
        stackView.alignment = .center
        stackView.distribution = .fillEqually
        stackView.spacing = starSpacing
        stackView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stackView)

        // スタックビューの制約
        NSLayoutConstraint.activate([
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor),
            stackView.heightAnchor.constraint(equalToConstant: starSize + 20)
        ])

        // Create star buttons
        for i in 0..<starCount {
            let button = UIButton(type: .custom)
            // 大きめのアイコンを使用
            let config = UIImage.SymbolConfiguration(pointSize: starSize, weight: .regular)
            button.setImage(UIImage(systemName: "star", withConfiguration: config), for: .normal)
            button.setImage(UIImage(systemName: "star.fill", withConfiguration: config), for: .selected)
            button.tag = i + 1 // Tag represents the star's rating value (1-5)
            button.addTarget(self, action: #selector(starTapped(_:)), for: .touchUpInside)
            button.translatesAutoresizingMaskIntoConstraints = false
            button.tintColor = starColor

            // タップ領域を広げる
            button.contentEdgeInsets = UIEdgeInsets(top: 10, left: 5, bottom: 10, right: 5)

            // ボタンのサイズを固定
            NSLayoutConstraint.activate([
                button.heightAnchor.constraint(equalToConstant: starSize + 20),
                button.widthAnchor.constraint(equalToConstant: starSize + 10)
            ])

            stackView.addArrangedSubview(button)
            starButtons.append(button)
        }

        // Set initial rating
        updateStars()
    }

    // layoutStarsメソッドは不要になったので削除

    // MARK: - Actions
    @objc private func starTapped(_ sender: UIButton) {
        // デバッグ出力を追加
        print("Star tapped: \(sender.tag)")

        // タップされた星の評価を設定
        let newRating = sender.tag

        // 必ず評価を更新
        rating = newRating

        // フィードバックを追加
        UIView.animate(withDuration: 0.1, animations: {
            sender.transform = CGAffineTransform(scaleX: 1.2, y: 1.2)
        }) { _ in
            UIView.animate(withDuration: 0.1) {
                sender.transform = .identity
            }
        }

        // レイアウトを更新
        self.setNeedsLayout()
    }

    // MARK: - Update UI
    private func updateStars() {
        // デバッグ出力を追加
        print("Updating stars to rating: \(rating)")

        for (index, button) in starButtons.enumerated() {
            let starValue = index + 1

            // 星の選択状態を更新
            button.isSelected = starValue <= rating
            button.tintColor = starColor

            // アニメーションを追加
            UIView.animate(withDuration: 0.2, delay: Double(index) * 0.05, options: [], animations: {
                button.alpha = 0.5
                // 選択された星は少し大きくする
                if button.isSelected {
                    button.transform = CGAffineTransform(scaleX: 1.1, y: 1.1)
                } else {
                    button.transform = .identity
                }
            }) { _ in
                UIView.animate(withDuration: 0.2) {
                    button.alpha = 1.0
                }
            }

            // デバッグ出力
            print("Star \(starValue) selected: \(button.isSelected)")
        }
    }

    // MARK: - Public Methods
    func setRating(_ newRating: Int) {
        guard newRating >= 1 && newRating <= starCount else { return }
        rating = newRating
    }
}
