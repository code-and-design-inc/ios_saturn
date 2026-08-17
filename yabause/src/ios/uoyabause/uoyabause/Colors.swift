//
//  Colors.swift
//  YabaSnashiro
//
//  Created by Shinya Miyamoto on 2024/08/01.
//  Copyright © 2024 devMiyax. All rights reserved.
//

import UIKit

extension UIColor {
    // MARK: - App Colors

    // 基本色
    static var backgroundGradientStart: UIColor {
        return UIColor(named: "backgroundGradientStart") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x000000) : UIColor(hex: 0x333333)
        }
    }

    static var backgroundGradientEnd: UIColor {
        return UIColor(named: "backgroundGradientEnd") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xDDDDDD) : UIColor(hex: 0xFFFFFF)
        }
    }

    static var fastlaneBackground: UIColor {
        return UIColor(named: "fastlaneBackground") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x1c1f24) : UIColor(hex: 0x2c3034)
        }
    }

    static var searchOpaque: UIColor {
        return UIColor(named: "searchOpaque") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x4444DD) : UIColor(hex: 0x5555EE)
        }
    }
    
    static var selectedBackground: UIColor {
        return UIColor(named: "selectedBackground") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x6f7c91) : UIColor(hex: 0x8f9cb1)
        }
    }

    static var tint: UIColor {
        return UIColor(named: "tint") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x8888d5) : UIColor(hex: 0x444499)
        }
    }

    static var detailBackground: UIColor {
        return UIColor(named: "detailBackground") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x0096a6) : UIColor(hex: 0x00b6c6)
        }
    }

    static var softOpaque: UIColor {
        return UIColor(named: "softOpaque") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x000000, alpha: 0.3) : UIColor(hex: 0x000000, alpha: 0.2)
        }
    }

    static var imgSoftOpaque: UIColor {
        return UIColor(named: "imgSoftOpaque") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xFF0000, alpha: 0.3) : UIColor(hex: 0xFF0000, alpha: 0.2)
        }
    }

    static var imgFullOpaque: UIColor {
        return UIColor(named: "imgFullOpaque") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x000000, alpha: 0.0) : UIColor(hex: 0xFFFFFF, alpha: 0.0)
        }
    }

    static var blackOpaque: UIColor {
        return UIColor(named: "blackOpaque") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x000000, alpha: 0.67) : UIColor(hex: 0x000000, alpha: 0.5)
        }
    }

    // 基本色（ダークモード対応）
    static var appBlack: UIColor {
        return UIColor(named: "appBlack") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x000000) : UIColor(hex: 0x222222)
        }
    }

    static var appWhite: UIColor {
        return UIColor(named: "appWhite") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xFFFFFF) : UIColor(hex: 0xF5F5F5)
        }
    }

    static var appDisable: UIColor {
        return UIColor(named: "appDisable") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x808080) : UIColor(hex: 0xA0A0A0)
        }
    }

    static var orangeTransparent: UIColor {
        return UIColor(named: "orangeTransparent") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xFADCA7, alpha: 0.67) : UIColor(hex: 0xFADCA7, alpha: 0.5)
        }
    }

    static var appOrange: UIColor {
        return UIColor(named: "appOrange") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xFADCA7) : UIColor(hex: 0xF8C987)
        }
    }

    static var appYellow: UIColor {
        return UIColor(named: "appYellow") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xEEFF41) : UIColor(hex: 0xDDEE30)
        }
    }

    // ヘッダーテキスト色（ダークモード/ライトモード対応）
    static var headerTextColor: UIColor {
        return UIColor { traitCollection in
            return .tint
        }
    }

    static var defaultBackground: UIColor {
        return UIColor(named: "defaultBackground") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x121212) : UIColor(hex: 0xFFFFFF)
        }
    }

    static var colorPrimary: UIColor {
        return UIColor(named: "colorPrimary") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x373E48) : UIColor(hex: 0xD7DEE8)
        }
    }

    //static var colorPrimaryDark: UIColor {
    //    return UIColor(named: "colorPrimaryDark") ?? UIColor { traitCollection in
    //        return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x373E48) //: UIColor(hex: 0x475058)
        //}
    //}

    static var colorAccent: UIColor {
        return UIColor(named: "colorAccent") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xB7BEC8) : UIColor(hex: 0xD7DEE8)
        }
    }

    static var halfTransparent: UIColor {
        return UIColor(named: "halfTransparent") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x000000, alpha: 0.8) : UIColor(hex: 0x000000, alpha: 0.6)
        }
    }

    static var cardviewInitialBackground: UIColor {
        return UIColor(named: "cardviewInitialBackground") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x6f7c91) : UIColor(hex: 0x8f9cb1)
        }
    }

    static var titleBack: UIColor {
        return UIColor(named: "titleBack") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xdadee2) : UIColor(hex: 0xeaeef2)
        }
    }

    static var appError: UIColor {
        return UIColor(named: "appError") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0xCF6679) : UIColor(hex: 0xEF7689)
        }
    }

    static var secondary: UIColor {
        return UIColor(named: "secondary") ?? UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? UIColor(hex: 0x4444DD) : UIColor(hex: 0x5555EE)
        }
    }

    // テキスト色（ダークモード/ライトモード対応）
    static var adaptiveTextColor: UIColor {
        return UIColor { traitCollection in
            return traitCollection.userInterfaceStyle == .dark ? .appWhite : .appBlack
        }
    }

    // MARK: - Hex Initializer

    convenience init(hex: Int, alpha: CGFloat = 1.0) {
        let red = CGFloat((hex & 0xFF0000) >> 16) / 255.0
        let green = CGFloat((hex & 0x00FF00) >> 8) / 255.0
        let blue = CGFloat(hex & 0x0000FF) / 255.0

        self.init(red: red, green: green, blue: blue, alpha: alpha)
    }
}
