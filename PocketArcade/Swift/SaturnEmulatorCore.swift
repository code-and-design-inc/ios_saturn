import AVFoundation
import Foundation
import os
import UIKit

/// Compile-time gate and the only registry hook for the Sega Saturn core.
/// Removing ENABLE_SATURN_CORE leaves the native ABI dormant; the enum case
/// stays so an existing library never becomes undecodable.
enum SaturnFeature {
    #if ENABLE_SATURN_CORE
    static let isEnabled = true

    static func register(in registry: CoreRegistry) {
        registry.register(.saturn) { SaturnEmulatorCore() }
    }
    #else
    static let isEnabled = false

    static func register(in registry: CoreRegistry) {}
    #endif
}

#if ENABLE_SATURN_CORE
enum SaturnCoreError: LocalizedError {
    case unavailable(String)
    case missingBIOS
    case bootFailed(String)
    case saveStateFailed(String)
    case loadStateFailed(String)

    var errorDescription: String? {
        switch self {
        case .unavailable(let detail):
            String(localized: "The Sega Saturn core is unavailable (\(detail))")
        case .missingBIOS:
            String(localized: "Import a Sega Saturn BIOS in Settings before playing Saturn games")
        case .bootFailed(let detail):
            String(localized: "Could not start the Sega Saturn game (\(detail))")
        case .saveStateFailed(let detail):
            String(localized: "Could not save the Sega Saturn state (\(detail))")
        case .loadStateFailed(let detail):
            String(localized: "Could not load the Sega Saturn state (\(detail))")
        }
    }
}

/// Thin adapter over the isolated Yaba Sanshiro bridge in Native/SaturnCore.
/// The core is a software renderer driven one frame at a time from a 60 Hz
/// timer, exactly like the ares-backed systems: frames arrive as RGBA buffers
/// through `frameHandler`, and audio is pulled from the core's ring buffer
/// into an AVAudioEngine player. The SH-2 core is the interpreter: no
/// executable memory is ever requested.
///
/// Netplay taps: the core's own RGBA buffer is handed to `streamVideoHandler`
/// before it is copied for the local display, and the 44.1 kHz int16 ring is
/// resampled to the session's 48 kHz float format for `streamAudioHandler`.
final class SaturnEmulatorCore: EmulatorCore, NetplayStreamingCore {
    let system: ConsoleSystem = .saturn
    var frameHandler: ((EmulatorVideoFrame) -> Void)?
    let supportsRewind = false
    /// Read on the emulation thread inside `runFrame`; assigned from the main
    /// thread when a session starts or stops (see AresEmulatorCore).
    var streamVideoHandler: ((UnsafeRawPointer, Int, Int) -> Void)?
    var streamAudioHandler: ((UnsafePointer<Float>, Int) -> Void)?

    private let emulationQueue = DispatchQueue(label: "com.cad.emu.saturn", qos: .userInteractive)
    private var timer: DispatchSourceTimer?
    private var session: UnsafeMutableRawPointer?
    private var romURL: URL?
    private var lastPersistentSave = DispatchTime.now().uptimeNanoseconds
    private var lastFrameSequence: UInt64 = 0
    private let audioEngine = AVAudioEngine()
    private let audioPlayer = AVAudioPlayerNode()
    private lazy var audioFormat = AVAudioFormat(standardFormatWithSampleRate: 44_100, channels: 2)!
    private lazy var streamSourceFormat = AVAudioFormat(
        commonFormat: .pcmFormatInt16, sampleRate: 44_100, channels: 2, interleaved: true
    )!
    private var audioScratch = [Int16](repeating: 0, count: 4_096 * 2)
    private let streamAudioTap = NetplayAudioResamplingTap()
    private let perf = SaturnPerfMonitor()

    deinit {
        stop()
    }

    func loadROM(at url: URL) throws {
        guard PASaturnIsAvailable() else {
            throw SaturnCoreError.unavailable("the native core is not built for this platform")
        }
        guard let biosURL = EmulatorFirmware.url(for: .saturn) else {
            throw SaturnCoreError.missingBIOS
        }
        romURL = url
        let backupURL = try Self.backupRAMURL(for: url)
        let created: UnsafeMutableRawPointer? = emulationQueue.sync {
            PASaturnSessionCreate(biosURL.path, url.path, backupURL.path)
        }
        guard let created else {
            throw SaturnCoreError.bootFailed(Self.lastError())
        }
        session = created
        lastPersistentSave = DispatchTime.now().uptimeNanoseconds
    }

    func start() throws {
        guard session != nil else { throw SaturnCoreError.unavailable("no active session") }
        guard timer == nil else { return }
        startAudio()

        let source = DispatchSource.makeTimerSource(queue: emulationQueue)
        source.schedule(deadline: .now(), repeating: .nanoseconds(16_666_667), leeway: .milliseconds(1))
        source.setEventHandler { [weak self] in self?.runFrame() }
        timer = source
        source.resume()
    }

    func pause() {
        timer?.cancel()
        timer = nil
        audioPlayer.pause()
    }

    func stop() {
        timer?.cancel()
        timer = nil
        audioPlayer.stop()
        audioEngine.stop()

        guard let oldSession = session else { return }
        session = nil
        emulationQueue.sync {
            PASaturnSessionFlushPersistentSaves(oldSession)
            PASaturnSessionDestroy(oldSession)
        }
    }

    func setButton(_ button: GamepadButton, pressed: Bool, player: Int) {
        guard (0...1).contains(player), let control = button.saturnButton else { return }
        emulationQueue.async { [weak self] in
            guard let session = self?.session else { return }
            PASaturnSessionSetButton(session, Int32(player), control, pressed)
        }
    }

    func setAnalogSticks(_ sticks: AnalogStickState, player: Int) {
        // The standard control pad is digital; the 3D pad is not wired up yet.
    }

    func saveState() throws -> Data {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("PocketArcade-Saturn-save-\(UUID().uuidString).yss")
        defer { try? FileManager.default.removeItem(at: url) }
        let saved: Bool = emulationQueue.sync {
            guard let session else { return false }
            return PASaturnSessionSaveState(session, url.path)
        }
        guard saved else { throw SaturnCoreError.saveStateFailed(Self.lastError()) }
        do {
            return try Data(contentsOf: url)
        } catch {
            throw SaturnCoreError.saveStateFailed(error.localizedDescription)
        }
    }

    func loadState(_ data: Data) throws {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("PocketArcade-Saturn-load-\(UUID().uuidString).yss")
        defer { try? FileManager.default.removeItem(at: url) }
        do {
            try data.write(to: url, options: .atomic)
        } catch {
            throw SaturnCoreError.loadStateFailed(error.localizedDescription)
        }
        let loaded: Bool = emulationQueue.sync {
            guard let session else { return false }
            return PASaturnSessionLoadState(session, url.path)
        }
        guard loaded else { throw SaturnCoreError.loadStateFailed(Self.lastError()) }
    }

    func flushPersistentSaves() {
        emulationQueue.sync {
            guard let session else { return }
            PASaturnSessionFlushPersistentSaves(session)
            lastPersistentSave = DispatchTime.now().uptimeNanoseconds
        }
    }

    func setAudioMuted(_ muted: Bool) {
        audioPlayer.volume = muted ? 0 : 1
    }

    /// Resolves the bridge without starting a guest so packaging regressions
    /// surface in tests that run without a disc.
    static func validateNativeRuntime() throws {
        guard let version = PASaturnBridgeVersion(), !String(cString: version).isEmpty else {
            throw SaturnCoreError.unavailable("the native bridge has no version")
        }
        guard PASaturnUsesInterpreter() else {
            throw SaturnCoreError.unavailable("JIT must be disabled")
        }
        guard PASaturnIsAvailable() else {
            throw SaturnCoreError.unavailable("the native core is not built for this platform")
        }
    }

    // MARK: - Emulation loop

    private func runFrame() {
        guard let session else { return }
        let signpost = perf.beginFrame()
        let rendered = PASaturnSessionRunFrame(session)
        if rendered {
            var frame = PASaturnVideoFrame()
            if PASaturnSessionGetVideo(session, &frame),
               let pixels = frame.pixels, frame.width > 0, frame.height > 0,
               frame.sequence != lastFrameSequence {
                lastFrameSequence = frame.sequence
                // Hand the streaming encoder the core's own buffer before
                // copying for the local display; the pointer is only valid
                // until the next core call.
                streamVideoHandler?(UnsafeRawPointer(pixels), Int(frame.width), Int(frame.height))
                let byteCount = Int(frame.width) * Int(frame.height) * 4
                let video = EmulatorVideoFrame(
                    width: Int(frame.width),
                    height: Int(frame.height),
                    rgba: Data(bytes: pixels, count: byteCount)
                )
                DispatchQueue.main.async { [weak self] in self?.frameHandler?(video) }
            }
        }
        pumpAudio(session)
        perf.endFrame(signpost, session: session)

        let now = DispatchTime.now().uptimeNanoseconds
        if now &- lastPersistentSave >= 30_000_000_000 {
            PASaturnSessionFlushPersistentSaves(session)
            lastPersistentSave = now
        }
    }

    private func startAudio() {
        guard UserDefaults.standard.object(forKey: "soundEnabled") as? Bool ?? true else { return }
        if !audioEngine.attachedNodes.contains(audioPlayer) {
            audioEngine.attach(audioPlayer)
            audioEngine.connect(audioPlayer, to: audioEngine.mainMixerNode, format: audioFormat)
        }
        if !audioEngine.isRunning { try? audioEngine.start() }
        if !audioPlayer.isPlaying { audioPlayer.play() }
    }

    private func pumpAudio(_ session: UnsafeMutableRawPointer) {
        // Always drain the ring so a muted session does not accumulate stale
        // audio that would play back late after unmuting, and so a muted host
        // keeps streaming sound to its guest.
        let available = min(Int(PASaturnSessionAudioAvailable(session)), 4_096)
        guard available > 0 else { return }
        let read = audioScratch.withUnsafeMutableBufferPointer {
            PASaturnSessionReadAudio(session, $0.baseAddress, Int32(available))
        }
        let frames = Int(read)
        guard frames > 0 else { return }

        if let streamHandler = streamAudioHandler {
            audioScratch.withUnsafeBufferPointer { buffer in
                guard let base = buffer.baseAddress else { return }
                streamAudioTap.forward(
                    UnsafeRawPointer(base),
                    byteCount: frames * 4,
                    from: streamSourceFormat,
                    to: streamHandler
                )
            }
        }

        guard audioEngine.isRunning,
              let buffer = AVAudioPCMBuffer(
                  pcmFormat: audioFormat,
                  frameCapacity: AVAudioFrameCount(frames)
              ),
              let channels = buffer.floatChannelData else { return }
        buffer.frameLength = AVAudioFrameCount(frames)
        let scale: Float = 1.0 / 32_768.0
        for index in 0..<frames {
            channels[0][index] = Float(audioScratch[index * 2]) * scale
            channels[1][index] = Float(audioScratch[index * 2 + 1]) * scale
        }
        audioPlayer.scheduleBuffer(buffer)
    }

    // MARK: - Paths

    /// Internal backup RAM lives next to the other cores' saves, one image per
    /// game folder so titles never overwrite each other's blocks.
    private static func backupRAMURL(for romURL: URL) throws -> URL {
        let base = try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        )
        let directory = base
            .appendingPathComponent("PocketArcade", isDirectory: true)
            .appendingPathComponent("Saturn/Saves", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        // Games are stored as ROMs/saturn/<UUID>/<name>; key on the folder.
        let key = romURL.deletingLastPathComponent().lastPathComponent
        return directory.appendingPathComponent("\(key).bup")
    }

    private static func lastError() -> String {
        guard let message = PASaturnLastError(), !String(cString: message).isEmpty else {
            return "unknown"
        }
        return String(cString: message)
    }
}

/// Per-frame signposts for Instruments plus a periodic summary line in the
/// unified log (subsystem com.cad.emu, category SaturnPerf), so device
/// performance can be read from Xcode's console or Console.app without
/// attaching a profiler:
///
///     saturn perf: emu 60.0 fps | frame avg 8.9 ms p95 11.2 ms max 14.0 ms |
///     vdp1 2.1 vdp2 3.0 present 0.8 cpu 3.0 ms | audio ring 2205 frames, overruns 0
///
/// "cpu" is SH-2 + SCU + SCSP + everything else (frame minus the three
/// renderer buckets measured inside the bridge).
final class SaturnPerfMonitor {
    private let logger = Logger(subsystem: "com.cad.emu", category: "SaturnPerf")
    private let signposter = OSSignposter(subsystem: "com.cad.emu", category: "SaturnPerf")
    private let reportInterval: UInt64 = 5_000_000_000
    private var lastReportTime: UInt64 = 0
    private var lastStats = PASaturnStats()

    func beginFrame() -> OSSignpostIntervalState? {
        guard signposter.isEnabled else { return nil }
        return signposter.beginInterval("SaturnFrame")
    }

    func endFrame(_ state: OSSignpostIntervalState?, session: UnsafeMutableRawPointer) {
        if let state { signposter.endInterval("SaturnFrame", state) }
        let now = DispatchTime.now().uptimeNanoseconds
        if lastReportTime == 0 {
            lastReportTime = now
            PASaturnSessionGetStats(session, &lastStats)
            return
        }
        guard now &- lastReportTime >= reportInterval else { return }
        var stats = PASaturnStats()
        PASaturnSessionGetStats(session, &stats)
        let seconds = Double(now &- lastReportTime) / 1e9
        let frames = Double(stats.emulatedFrames &- lastStats.emulatedFrames)
        guard frames > 0 else { lastReportTime = now; lastStats = stats; return }
        let ms = { (delta: UInt64) -> Double in Double(delta) / frames / 1e6 }
        let frameMs = ms(stats.frameNanosTotal &- lastStats.frameNanosTotal)
        let vdp1Ms = ms(stats.vdp1Nanos &- lastStats.vdp1Nanos)
        let vdp2Ms = ms(stats.vdp2Nanos &- lastStats.vdp2Nanos)
        let presentMs = ms(stats.presentNanos &- lastStats.presentNanos)
        let cpuMs = max(0, frameMs - vdp1Ms - vdp2Ms - presentMs)
        let audioAvailable = PASaturnSessionAudioAvailable(session)
        logger.notice(
            "saturn perf: emu \(frames / seconds, format: .fixed(precision: 1)) fps | frame avg \(frameMs, format: .fixed(precision: 1)) ms p95 \(Double(stats.recentFrameNanosP95) / 1e6, format: .fixed(precision: 1)) ms max \(Double(stats.frameNanosMax) / 1e6, format: .fixed(precision: 1)) ms | vdp1 \(vdp1Ms, format: .fixed(precision: 1)) vdp2 \(vdp2Ms, format: .fixed(precision: 1)) present \(presentMs, format: .fixed(precision: 1)) cpu \(cpuMs, format: .fixed(precision: 1)) ms | audio ring \(audioAvailable) frames, overruns \(stats.audioOverruns)"
        )
        lastReportTime = now
        lastStats = stats
    }
}

private extension GamepadButton {
    var saturnButton: PASaturnButton? {
        switch self {
        // Pocket Arcade's face vocabulary is positional: B is south, A east,
        // Y west, and X north. On the on-screen Saturn pad (two rows of three)
        // the bottom row A B C is bound to B/A/right-trigger and the top row
        // X Y Z to Y/X/left-trigger, so a physical pad's south/east face
        // buttons are A/B, west/north are X/Y, and the triggers give C/Z.
        case .b: PASaturnButtonA
        case .a: PASaturnButtonB
        case .rightTrigger: PASaturnButtonC
        case .y: PASaturnButtonX
        case .x: PASaturnButtonY
        case .leftTrigger: PASaturnButtonZ
        case .leftShoulder: PASaturnButtonLeftTrigger
        case .rightShoulder: PASaturnButtonRightTrigger
        case .start: PASaturnButtonStart
        case .select: nil // The Saturn pad has no SELECT.
        case .up: PASaturnButtonUp
        case .down: PASaturnButtonDown
        case .left: PASaturnButtonLeft
        case .right: PASaturnButtonRight
        }
    }
}
#endif
