import os
import subprocess
import sys
import time
import glob

# --- 設定 ---
# Android NDKのパス (環境変数から取得)
ANDROID_NDK_ROOT = os.environ.get('ANDROID_NDK_ROOT',"E:\\Android\\ndk\\29.0.14033849")
if not ANDROID_NDK_ROOT:
    print("エラー: 環境変数 ANDROID_NDK_ROOT が設定されていません。")
    print("NDKのルートディレクトリへのパスを設定してください。例: export ANDROID_NDK_ROOT=/path/to/android-ndk-r23b")
    sys.exit(1)

# デバイスのABI (例: arm64-v8a, armeabi-v7a)
# 実際のデバイスのABIに合わせて変更してください
DEVICE_ABI = "arm64-v8a" 

# lldb-serverのホスト上のパス
# NDKのバージョンやOSによってパスが異なる場合があります。
# 例: r23bの場合 -> toolchains/llvm/prebuilt/linux-x86_64/lib64/lldb/bin/lldb-server
# 例: r25bの場合 -> toolchains/llvm/prebuilt/linux-x86_64/lib64/lldb/bin/lldb-server
# Windowsの場合 -> toolchains/llvm/prebuilt/windows-x86_64/lib64/lldb/bin/lldb-server.exe
LLDB_SERVER_HOST_PATH = os.path.join(
    ANDROID_NDK_ROOT,
    "toolchains", "llvm", "prebuilt", "windows-x86_64", "lib", "clang", "21", "lib", "linux", "aarch64", "lldb-server"
)

# lldb-serverのデバイス上のパス
LLDB_SERVER_DEVICE_PATH = "/data/local/tmp/lldb-server"

# アプリのパッケージ名 (debugビルドのapplicationId)
PACKAGE_NAME = "org.devmiyax.yabasanshioro2.debug"

# ポート番号
PORT = 5044

# --- 関数 ---
def run_command(command, check_error=True):
    """シェルコマンドを実行し、出力を表示するヘルパー関数"""
    print(f"実行中: {"".join(command)}")
    process = subprocess.run(command, capture_output=True, text=True, shell=True)
    if process.stdout:
        print(f"STDOUT:\n{process.stdout}")
    if process.stderr:
        print(f"STDERR:\n{process.stderr}")
    if check_error and process.returncode != 0:
        print(f"エラー: コマンドが失敗しました (終了コード: {process.returncode})")
        sys.exit(1)
    return process

def find_latest_apk():
    """最新のdebug APKのパスを検索して返す"""
    apk_dir = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "app", "build", "outputs", "apk", "debug"
    )
    apk_pattern = os.path.join(apk_dir, "*.apk")
    apks = glob.glob(apk_pattern)
    if not apks:
        print(f"エラー: APKファイルが見つかりません。ビルドディレクトリを確認してください: {apk_dir}")
        sys.exit(1)
    # 最終更新日時でソートして最新のものを選択
    latest_apk = max(apks, key=os.path.getmtime)
    return latest_apk

def main():
    print("--- Android LLDB デバッグセットアップスクリプト ---")

    # 1. lldb-serverをデバイスにプッシュ
    print("\n1. lldb-serverをデバイスにプッシュ中...")
    run_command(f"adb push \"{LLDB_SERVER_HOST_PATH}\" {LLDB_SERVER_DEVICE_PATH}")

    # 2. lldb-serverに実行権限を付与
    print("\n2. lldb-serverに実行権限を付与中...")
    run_command(f"adb shell chmod +x {LLDB_SERVER_DEVICE_PATH}")

    # 3. APKを検索してインストール
    print("\n3. APKを検索してインストール中...")
    apk_path = find_latest_apk()
    print(f"見つかったAPK: {apk_path}")
    # 既存のアプリをアンインストールしてからインストール
    print(f"既存のアプリ ({PACKAGE_NAME}) をアンインストール中...")
    run_command(f"adb uninstall {PACKAGE_NAME}", check_error=False) # アンインストール失敗はエラーとしない
    run_command(f"adb install -r \"{apk_path}\"")

    # 4. アプリを起動
    print(f"\n4. アプリ '{PACKAGE_NAME}' を起動中...")
    run_command(f"adb shell am start -n {PACKAGE_NAME}/org.devmiyax.yabasanshiro.StartupActivity")

    # 5. lldb-serverをアプリの権限で起動 (バックグラウンドで実行)
    print(f"\n5. lldb-serverをアプリの権限で起動中 (ポート: {PORT})...")
    run_command(f"adb shell run-as {PACKAGE_NAME} \"{LLDB_SERVER_DEVICE_PATH} platform --listen '*:{PORT}' --server\"", check_error=False)
    print("lldb-serverがバックグラウンドで起動しました。")
    time.sleep(2) # 起動を待つ

    # 6. ポート転送を設定
    print(f"\n6. ポート転送を設定中 (tcp:{PORT} -> tcp:{PORT})...")
    run_command(f"adb forward tcp:{PORT} tcp:{PORT}")

    print("\n---セットアップ完了 ---")
    print(f"VS Codeでデバッグを開始する準備ができました。")

    # 起動したアプリのPIDを取得して出力
    pid_command = f"adb shell pidof {PACKAGE_NAME}"
    print(f"アプリのPIDを取得中: {pid_command}")
    process = subprocess.run(pid_command, capture_output=True, text=True, shell=True)
    if process.stdout:
        pid = process.stdout.strip()
        print(f"PID: {pid}")
        print(f"↑このPIDをコピーして、VS Codeのデバッグプロンプトに貼り付けてください。")
    else:
        print("PIDの取得に失敗しました。アプリが起動しているか確認してください。")

if __name__ == "__main__":
    main()
