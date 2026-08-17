import Foundation

class YabauseManager {
    static let shared = YabauseManager()
    
    private init() {}
    
    // MARK: - Cheat Management
    
    private var activeCheatCodes: [String] = []
    
    func updateCheatCode(_ codes: [String]?) {
        if let codes = codes {
            // チートコードが指定されている場合
            activeCheatCodes = codes
            YSUpdateCheat(codes)
        } else {
            // チートコードが指定されていない場合（無効化）
            activeCheatCodes = []
            YSUpdateCheat(nil)
        }
    }
    
    func getCurrentCheatCodes() -> [String] {
        return activeCheatCodes
    }
}

// MARK: - C Functions

// これらの関数はYabauseエミュレータ側で実装される必要があります
//@_silgen_name("YabauseUpdateCheatCode")
//func YabauseUpdateCheatCode(_ codes: UnsafeMutablePointer<UnsafeMutablePointer<Int8>?>?, _ count: Int32)
