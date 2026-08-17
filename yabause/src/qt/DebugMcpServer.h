/*  DebugMcpServer.h
    In-process MCP (Model Context Protocol) server over Streamable HTTP.
    Lets an MCP client (e.g. Claude Code) drive the SH2 debugger:
    breakpoints, stepping, memory dump, execution history.

    Threading model: httplib runs on its own thread; every operation that
    touches the emulation core is marshalled to the UI thread (which is
    also the emulation thread, driven by YabauseThread's zero-interval
    timer) via QMetaObject::invokeMethod with BlockingQueuedConnection.
*/
#ifndef DEBUGMCPSERVER_H
#define DEBUGMCPSERVER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

extern "C" {
#include "sh2core.h"
}

class YabauseThread;
namespace httplib { class Server; }

class DebugMcpServer : public QObject {
  Q_OBJECT

public:
  explicit DebugMcpServer(YabauseThread* yabThread, QObject* parent = nullptr);
  ~DebugMcpServer();

  bool start(int port);
  void stop();
  bool isRunning() const { return mRunning.load(); }
  int port() const { return mPort; }

private:
  // --- JSON-RPC layer (runs on the HTTP thread) ---
  QByteArray handleRequest(const QByteArray& body, int* httpStatus);
  QJsonObject dispatch(const QJsonObject& req);
  QJsonArray toolDefinitions() const;

  // Marshal a functor onto the UI thread and wait for its result.
  QJsonObject runOnUiThread(const std::function<QJsonObject()>& fn);

  // --- Tool layer (runs on the UI thread unless noted) ---
  QJsonObject callTool(const QString& name, const QJsonObject& args);
  // Executed on the HTTP thread: blocks on mStopCv.
  QJsonObject toolWaitForStop(const QJsonObject& args);

  static QJsonObject textResult(const QString& text);
  static QJsonObject errorResult(const QString& text);

  // Resolve "master"/"slave" to a context; returns nullptr + error text.
  SH2_struct* resolveCpu(const QJsonObject& args, QString* error) const;

  // Breakpoint callback (called on the UI thread from inside SH2 exec).
  static void breakpointHandler(void* context, u32 addr, void* userdata);
  void onBreakpointHit(SH2_struct* context, u32 addr);
  void ensureCallbackRegistered();

  // Stop-state helpers, guarded by mStopMutex (see members below).
  bool isStopped();
  void setStopState(const QString& reason, const QString& cpu, quint32 addr);
  void clearStopState();

  // Sets the "step" stop state after step/step_over/step_out, unless a
  // breakpoint fired during the operation (mStopReason == "breakpoint" and
  // mStopSerial advanced past entrySerial), in which case the breakpoint's
  // stop state is preserved instead of being overwritten. Returns a note
  // to append to the tool result text when a breakpoint took precedence.
  QString finishStepStop(quint64 entrySerial, SH2_struct* ctx);

  // Formats R0-R15/SR/GBR/VBR/MACH/MACL/PR/PC plus a one-line disassembly
  // of the current PC. Shared with the Task 7 registers_get tool.
  static QString formatRegsAndDisasm(SH2_struct* ctx);

  // Disassembles count instructions starting at addr, marking the line
  // at markPc with ">". Used by the disassemble tool.
  QString disasmRange(quint32 addr, int count, quint32 markPc);

  YabauseThread* mYabauseThread;
  std::unique_ptr<httplib::Server> mServer;
  std::thread mHttpThread;
  std::atomic<bool> mRunning{false};
  // Set at the top of stop(); dispatch() checks this before queuing a
  // BlockingQueuedConnection call onto the UI thread, to avoid deadlocking
  // against stop()'s own join of the HTTP thread pool. See stop().
  std::atomic<bool> mStopping{false};
  int mPort = 9640;

  // Stop state, guarded by mStopMutex. Written on the UI thread,
  // read from the HTTP thread (wait_for_stop).
  std::mutex mStopMutex;
  std::condition_variable mStopCv;
  bool mStopped = false;
  QString mStopReason;   // "breakpoint" / "pause" / "step"
  QString mStopCpu;      // "master" / "slave"
  quint32 mStopAddr = 0;
  quint64 mStopSerial = 0;  // incremented on every stop event
};

#endif  // DEBUGMCPSERVER_H
