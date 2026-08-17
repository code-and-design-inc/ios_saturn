#include "DebugMcpServer.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>

#include <cctype>
#include <cstring>

#include "httplib.h"
#include "QtYabause.h"
#include "Settings.h"
#include "VolatileSettings.h"
#include "YabauseThread.h"

extern "C" {
#include "cs2.h"
#include "memory.h"
#include "sh2int.h"
#include "yabause.h"
}

static bool parseAddr(const QJsonValue& v, quint32* out) {
  QString s = v.toString();
  bool ok = false;
  *out = s.toUInt(&ok, 16);
  if (!ok) *out = s.toUInt(&ok, 10);
  return ok;
}

// MCP Streamable HTTP transport requires servers to validate the Origin
// header on every request to guard against DNS-rebinding / drive-by browser
// attacks against localhost-bound servers (MCP spec, Security
// Considerations: Origin validation for local servers). Native MCP clients
// (e.g. Claude Code) do not send an Origin header at all, so absence of the
// header is allowed; only a present-but-disallowed Origin is rejected.
static bool isAllowedOrigin(const std::string& origin) {
  static const char* kAllowedPrefixes[] = {
      "http://localhost", "http://127.0.0.1", "https://localhost",
      "https://127.0.0.1"};
  for (const char* prefix : kAllowedPrefixes) {
    size_t plen = strlen(prefix);
    if (origin.compare(0, plen, prefix) != 0) continue;
    if (origin.size() == plen) return true;  // no port suffix
    if (origin[plen] != ':') continue;
    bool allDigits = origin.size() > plen + 1;
    for (size_t i = plen + 1; allDigits && i < origin.size(); i++) {
      if (!isdigit(static_cast<unsigned char>(origin[i]))) allDigits = false;
    }
    if (allDigits) return true;
  }
  return false;
}

DebugMcpServer::DebugMcpServer(YabauseThread* yabThread, QObject* parent)
    : QObject(parent), mYabauseThread(yabThread) {}

DebugMcpServer::~DebugMcpServer() { stop(); }

bool DebugMcpServer::start(int port) {
  if (mRunning.load()) return true;
  mPort = port;
  mServer.reset(new httplib::Server());

  mServer->Post("/mcp", [this](const httplib::Request& req,
                               httplib::Response& res) {
    // Origin validation per the MCP Streamable HTTP transport spec (Security
    // Considerations): a browser page loaded from any origin can issue a
    // "simple" cross-origin POST that reaches this handler even though the
    // server only binds 127.0.0.1. Reject any request that carries a
    // disallowed Origin header; a missing Origin header (native MCP
    // clients, e.g. Claude Code, do not send one) is allowed through.
    if (req.has_header("Origin") &&
        !isAllowedOrigin(req.get_header_value("Origin"))) {
      res.status = 403;
      return;
    }
    int status = 200;
    QByteArray out = handleRequest(
        QByteArray(req.body.data(), (int)req.body.size()), &status);
    res.status = status;
    if (!out.isEmpty())
      res.set_content(out.constData(), out.size(), "application/json");
  });
  // No SSE stream support: reject GET as allowed by the MCP spec.
  mServer->Get("/mcp", [](const httplib::Request&, httplib::Response& res) {
    res.status = 405;
  });

  if (!mServer->bind_to_port("127.0.0.1", mPort)) {
    mServer.reset();
    return false;
  }
  mRunning.store(true);
  mHttpThread = std::thread([this] { mServer->listen_after_bind(); });
  return true;
}

void DebugMcpServer::stop() {
  if (!mRunning.load()) return;
  // Block any new tools/call dispatch from queuing a BlockingQueuedConnection
  // onto the UI thread (see dispatch()): once we start joining the HTTP
  // thread pool below, the UI thread must remain free to service any call
  // that is already in flight, not accept new ones.
  mStopping.store(true);
  // Wake any client blocked in toolWaitForStop() on mStopCv immediately:
  // otherwise it holds an httplib worker thread for up to timeout_ms
  // (max 300s) while the join-with-processEvents loop below waits for that
  // very worker to finish, stalling shutdown. See toolWaitForStop().
  mStopCv.notify_all();
  mRunning.store(false);
  mServer->stop();

  if (mHttpThread.joinable()) {
    if (QThread::currentThread() == thread()) {
      // stop() is running on the UI thread. httplib's listen_internal()
      // calls ThreadPool::shutdown(), which joins every worker thread. If a
      // worker is currently blocked inside runOnUiThread() waiting on a
      // BlockingQueuedConnection into this (UI) thread, a plain
      // mHttpThread.join() here would deadlock: the UI thread would be
      // stuck in join() and could never pump its event loop to deliver the
      // queued call, so the worker (and thus the join) would never
      // complete. Work around this by joining on a helper thread while
      // pumping the UI event loop here, so any in-flight
      // BlockingQueuedConnection call can still be delivered and finish,
      // unblocking the worker thread and therefore the join.
      std::atomic<bool> joined{false};
      std::thread joiner([this, &joined] {
        mHttpThread.join();
        joined.store(true);
      });
      while (!joined.load()) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents,
                                        50);
      }
      joiner.join();
    } else {
      mHttpThread.join();
    }
  }
  mServer.reset();
  mStopping.store(false);
  // Clear any stale MCP stop state (e.g. left over from a breakpoint pause)
  // so a later re-enable via the menu does not report a stale "stopped"
  // before any new stop event has occurred.
  clearStopState();
  // Leave the emulation runnable if we had it halted.
  if (MSH2) MSH2->debugHaltRequest = 0;
  if (SSH2) SSH2->debugHaltRequest = 0;
}

QByteArray DebugMcpServer::handleRequest(const QByteArray& body,
                                         int* httpStatus) {
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    *httpStatus = 400;
    QJsonObject err{{"jsonrpc", "2.0"},
                    {"id", QJsonValue::Null},
                    {"error", QJsonObject{{"code", -32700},
                                          {"message", "parse error"}}}};
    return QJsonDocument(err).toJson(QJsonDocument::Compact);
  }
  QJsonObject req = doc.object();
  if (!req.contains("id")) {  // notification
    *httpStatus = 202;
    return QByteArray();
  }
  *httpStatus = 200;
  return QJsonDocument(dispatch(req)).toJson(QJsonDocument::Compact);
}

QJsonObject DebugMcpServer::dispatch(const QJsonObject& req) {
  const QString method = req.value("method").toString();
  const QJsonValue id = req.value("id");
  QJsonObject resp{{"jsonrpc", "2.0"}, {"id", id}};

  if (method.isEmpty()) {
    resp["error"] = QJsonObject{{"code", -32600},
                                {"message", "invalid request: missing method"}};
  } else if (method == "initialize") {
    QString ver = req.value("params").toObject()
                      .value("protocolVersion").toString("2025-03-26");
    resp["result"] = QJsonObject{
        {"protocolVersion", ver},
        {"capabilities", QJsonObject{{"tools", QJsonObject{}}}},
        {"serverInfo", QJsonObject{{"name", "yabasanshiro-debug"},
                                   {"version", "1.0.0"}}}};
  } else if (method == "ping") {
    resp["result"] = QJsonObject{};
  } else if (method == "tools/list") {
    resp["result"] = QJsonObject{{"tools", toolDefinitions()}};
  } else if (method == "tools/call") {
    QJsonObject params = req.value("params").toObject();
    QString name = params.value("name").toString();
    QJsonObject args = params.value("arguments").toObject();
    if (name == "wait_for_stop") {
      resp["result"] = toolWaitForStop(args);  // blocks on HTTP thread
    } else if (mStopping.load()) {
      // Do not queue a BlockingQueuedConnection call onto the UI thread
      // while stop() is joining the HTTP thread pool: the UI thread may
      // already be pumping events specifically to drain in-flight calls,
      // not to accept new ones. See stop().
      resp["error"] = QJsonObject{{"code", -32000},
                                  {"message", "server shutting down"}};
    } else {
      resp["result"] = runOnUiThread(
          [this, name, args] { return callTool(name, args); });
    }
  } else {
    resp["error"] = QJsonObject{{"code", -32601},
                                {"message", "method not found: " + method}};
  }
  return resp;
}

QJsonObject DebugMcpServer::runOnUiThread(
    const std::function<QJsonObject()>& fn) {
  QJsonObject result;
  if (QThread::currentThread() == thread()) {
    result = fn();
  } else {
    QMetaObject::invokeMethod(
        this, [&result, &fn] { result = fn(); },
        Qt::BlockingQueuedConnection);
  }
  return result;
}

QJsonObject DebugMcpServer::textResult(const QString& text) {
  return QJsonObject{
      {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}}},
      {"isError", false}};
}

QJsonObject DebugMcpServer::errorResult(const QString& text) {
  return QJsonObject{
      {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}}},
      {"isError", true}};
}

SH2_struct* DebugMcpServer::resolveCpu(const QJsonObject& args,
                                       QString* error) const {
  const QString cpu = args.value("cpu").toString("master");
  if (cpu == "master") return MSH2;
  if (cpu == "slave") return SSH2;
  *error = "invalid cpu (use \"master\" or \"slave\")";
  return nullptr;
}

bool DebugMcpServer::isStopped() {
  std::lock_guard<std::mutex> lk(mStopMutex);
  return mStopped;
}

void DebugMcpServer::setStopState(const QString& reason, const QString& cpu,
                                  quint32 addr) {
  {
    std::lock_guard<std::mutex> lk(mStopMutex);
    mStopped = true;
    mStopReason = reason;
    mStopCpu = cpu;
    mStopAddr = addr;
    mStopSerial++;
  }
  mStopCv.notify_all();
}

void DebugMcpServer::clearStopState() {
  std::lock_guard<std::mutex> lk(mStopMutex);
  mStopped = false;
  mStopReason.clear();
  mStopCpu.clear();
  mStopAddr = 0;
}

QString DebugMcpServer::finishStepStop(quint64 entrySerial, SH2_struct* ctx) {
  bool bpWon;
  {
    std::lock_guard<std::mutex> lk(mStopMutex);
    bpWon = (mStopReason == "breakpoint" && mStopSerial != entrySerial);
  }
  if (bpWon)
    return "\nnote: a breakpoint was hit during this operation; stop "
           "reason is breakpoint, not step (see debug_status)\n";
  setStopState("step", ctx == MSH2 ? "master" : "slave", ctx->regs.PC);
  return QString();
}

QString DebugMcpServer::disasmRange(quint32 addr, int count, quint32 markPc) {
  QString out;
  char buf[128];
  for (int i = 0; i < count; i++) {
    quint32 a = addr + i * 2;
    SH2Disasm(a, MappedMemoryReadWordNocache(a, NULL), 0, NULL, buf);
    out += QString("%1 %2\n").arg(a == markPc ? ">" : " ").arg(buf);
  }
  return out;
}

QString DebugMcpServer::formatRegsAndDisasm(SH2_struct* ctx) {
  const sh2regs_struct& r = ctx->regs;
  QString t;
  for (int i = 0; i < 16; i++) {
    t += QString("R%1=0x%2%3")
             .arg(i)
             .arg(QString::number(r.R[i], 16).toUpper())
             .arg((i % 4 == 3) ? "\n" : "  ");
  }
  t += QString("SR=0x%1 GBR=0x%2 VBR=0x%3 MACH=0x%4 MACL=0x%5 PR=0x%6 PC=0x%7\n")
           .arg(QString::number(r.SR.all, 16).toUpper(),
                QString::number(r.GBR, 16).toUpper(),
                QString::number(r.VBR, 16).toUpper(),
                QString::number(r.MACH, 16).toUpper(),
                QString::number(r.MACL, 16).toUpper(),
                QString::number(r.PR, 16).toUpper(),
                QString::number(r.PC, 16).toUpper());
  char buf[128];
  SH2Disasm(r.PC, MappedMemoryReadWordNocache(r.PC, NULL), 0, NULL, buf);
  t += QString("0x%1: %2\n").arg(QString::number(r.PC, 16).toUpper(),
                                 QString::fromLatin1(buf));
  return t;
}

QJsonObject DebugMcpServer::callTool(const QString& name,
                                     const QJsonObject& args) {
  // NOTE: emulationRunning() is false while paused (it checks !mPause), so
  // it cannot be used here: every tool call after "pause" (including
  // debug_status/resume/step) would otherwise be rejected. init() == 0
  // means YabauseInit() has succeeded, i.e. a session (game or BIOS) is
  // loaded, independent of running/paused state.
  if (mYabauseThread->init() != 0)
    return errorResult("emulation is not running (no game loaded)");
  ensureCallbackRegistered();

  if (name == "debug_status") {
    QString t;
    bool stopped;
    QString reason, cpu;
    quint32 addr;
    {
      std::lock_guard<std::mutex> lk(mStopMutex);
      stopped = mStopped; reason = mStopReason; cpu = mStopCpu;
      addr = mStopAddr;
    }
    t += QString("state: %1\n").arg(stopped ? "stopped" : "running");
    if (stopped)
      t += QString("stop reason: %1 cpu=%2 addr=0x%3\n")
               .arg(reason, cpu, QString::number(addr, 16).toUpper());
    t += QString("MSH2 PC=0x%1  SSH2 PC=0x%2\n")
             .arg(QString::number(SH2Core->GetPC(MSH2), 16).toUpper(),
                  QString::number(SH2Core->GetPC(SSH2), 16).toUpper());
    t += QString("frame: %1\n").arg(yabsys.frame_count);
    if (cdip)
      t += QString("game: %1 %2\n")
               .arg(QString::fromLatin1(cdip->itemnum, sizeof(cdip->itemnum))
                        .trimmed(),
                    QString::fromLatin1(cdip->gamename, sizeof(cdip->gamename))
                        .trimmed());
    t += QString("sh2 core: %1 (id=%2)\n").arg(SH2Core->Name).arg(SH2Core->id);
    t += QString("history recording: %1\n")
             .arg(MSH2->history_enabled ? "on" : "off");
    if (SH2Core->id != SH2CORE_DEBUGINTERPRETER)
      t += "note: breakpoints/step/history need the debug interpreter core; "
           "use set_debug_option {\"cpu_core\":\"interpreter\"}\n";
    return textResult(t);
  }

  if (name == "pause") {
    if (isStopped()) return errorResult("already stopped");
    if (!mYabauseThread->pauseEmulation(true, false))
      return errorResult("failed to pause emulation (renderer not ready?)");
    setStopState("pause", "master", SH2Core->GetPC(MSH2));
    return textResult("stopped (frame boundary)");
  }

  if (name == "resume") {
    if (!isStopped()) return errorResult("not stopped");
    // Attempt the actual resume before touching halt requests / stop
    // state, so a failure (e.g. renderer not ready) leaves the debug
    // session exactly as it was instead of reporting "resumed" while
    // emulation is still paused.
    if (!mYabauseThread->pauseEmulation(false, false))
      return errorResult("failed to resume emulation (renderer not ready?)");
    MSH2->debugHaltRequest = 0;
    SSH2->debugHaltRequest = 0;
    clearStopState();
    return textResult("resumed");
  }

  if (name == "load_state") {
    int slot = qBound(0, args.value("slot").toInt(0), 10);
    const bool wasRunning = !isStopped();
    if (wasRunning) {
      if (!mYabauseThread->pauseEmulation(true, false))
        return errorResult("failed to pause emulation for state load");
    }
    QString dir = QtYabause::volatileSettings()
                      ->value("General/SaveStates", getDataDirPath())
                      .toString();
    int r = YabLoadStateSlot(dir.toLatin1().constData(), slot);
    if (wasRunning)
      mYabauseThread->pauseEmulation(false, false);
    if (r != 0)
      return errorResult(QString("YabLoadStateSlot failed (%1)").arg(r));
    return textResult(QString("state slot %1 loaded").arg(slot));
  }

  if (name == "step") {
    if (!isStopped())
      return errorResult("not stopped; use pause or wait_for_stop first");
    if (SH2Core->id != SH2CORE_DEBUGINTERPRETER)
      return errorResult("step needs the debug interpreter core; "
                         "use set_debug_option {\"cpu_core\":\"interpreter\"}");
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);
    int count = qBound(1, args.value("count").toInt(1), 10000);
    quint64 entrySerial;
    {
      std::lock_guard<std::mutex> lk(mStopMutex);
      entrySerial = mStopSerial;
    }
    ctx->debugHaltRequest = 0;
    for (int i = 0; i < count; i++) SH2Step(ctx);
    // If count > 1 steps onto a breakpoint address, the breakpoint
    // callback (onBreakpointHit) may have already recorded a "breakpoint"
    // stop; finishStepStop() preserves that instead of overwriting it.
    return textResult(formatRegsAndDisasm(ctx) +
                      finishStepStop(entrySerial, ctx));
  }

  if (name == "step_over" || name == "step_out") {
    if (!isStopped())
      return errorResult("not stopped; use pause or wait_for_stop first");
    if (SH2Core->id != SH2CORE_DEBUGINTERPRETER)
      return errorResult("step needs the debug interpreter core; "
                         "use set_debug_option {\"cpu_core\":\"interpreter\"}");
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);

    quint64 entrySerial;
    {
      std::lock_guard<std::mutex> lk(mStopMutex);
      entrySerial = mStopSerial;
    }

    // NOTE: sStepDone is a function-local static used as the callback's
    // completion flag. This is safe only because step_over/step_out are
    // always invoked serially on the UI thread (never reentered while a
    // previous call is still pending), so a captureless-lambda-style
    // static works as a poor man's closure without needing real capture
    // storage. Do not call this tool from more than one thread.
    static volatile int sStepDone;
    sStepDone = 0;
    auto cb = [](void*, u32, void*) { sStepDone = 1; };
    ctx->debugHaltRequest = 0;
    if (name == "step_over") {
      int r = SH2StepOver(ctx, cb);
      if (r == 0) {
        // Not a call instruction: SH2StepOver already single-stepped.
        sStepDone = 1;
      }
    } else {
      SH2StepOut(ctx, cb);
    }
    // Pump this CPU only, bounded. Crossing an interrupt-wait loop may
    // exhaust the budget: advise breakpoints in that case. Note that if a
    // breakpoint fires while pumping (ctx->debugHaltRequest gets set by
    // onBreakpointHit), further SH2Exec() calls here are no-ops until the
    // budget is exhausted; the breakpoint's stop state is preserved below
    // by finishStepStop() rather than overwritten.
    const int kMaxSlices = 100000;  // x100 cycles = 10M cycles budget
    for (int i = 0; i < kMaxSlices && !sStepDone; i++) SH2Exec(ctx, 100);
    if (!sStepDone) {
      ctx->stepOverOut.enabled = 0;
      QString bpNote = finishStepStop(entrySerial, ctx);
      if (!bpNote.isEmpty())
        return textResult(formatRegsAndDisasm(ctx) + bpNote);
      return errorResult(
          "step did not complete within 10M cycles (likely waiting on an "
          "interrupt); set a breakpoint after the call and resume instead");
    }
    return textResult(formatRegsAndDisasm(ctx) +
                      finishStepStop(entrySerial, ctx));
  }

  if (name == "breakpoint_add") {
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);
    quint32 addr;
    if (!parseAddr(args.value("address"), &addr))
      return errorResult("invalid address");
    const QString type = args.value("type").toString("code");
    int r;
    if (type == "code") {
      if (args.contains("reg_index")) {
        int regIndex = args.value("reg_index").toInt();
        if (regIndex < 0 || regIndex > 22)
          return errorResult("reg_index must be 0-15 (R0-R15) or 16-22 (SR,GBR,VBR,MACH,MACL,PR,PC)");
        quint32 regValue;
        if (!parseAddr(args.value("reg_value"), &regValue))
          return errorResult("invalid reg_value");
        r = SH2AddCodeBreakpointEx(ctx, addr, regIndex, regValue);
      } else {
        r = SH2AddCodeBreakpoint(ctx, addr);
      }
    } else if (type == "memory") {
      u32 flags = 0;
      if (args.value("read").toBool(true))
        flags |= BREAK_BYTEREAD | BREAK_WORDREAD | BREAK_LONGREAD;
      if (args.value("write").toBool(true))
        flags |= BREAK_BYTEWRITE | BREAK_WORDWRITE | BREAK_LONGWRITE;
      if (flags == 0)
        return errorResult("at least one of read or write flags must be enabled for memory breakpoints");
      quint32 value = 0;
      int checkValue = 0;
      if (args.contains("value")) {
        if (!parseAddr(args.value("value"), &value))
          return errorResult("invalid value");
        checkValue = 1;
      }
      r = SH2AddMemoryBreakpoint(ctx, addr, flags, value, checkValue);
    } else {
      return errorResult("type must be \"code\" or \"memory\"");
    }
    if (r != 0) return errorResult("failed to add breakpoint (table full or duplicate address)");
    return textResult(QString("breakpoint added at 0x%1")
                          .arg(QString::number(addr, 16).toUpper()));
  }

  if (name == "breakpoint_remove") {
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);
    quint32 addr;
    if (!parseAddr(args.value("address"), &addr))
      return errorResult("invalid address");
    const QString type = args.value("type").toString("code");
    int r;
    if (type == "code") {
      r = SH2DelCodeBreakpoint(ctx, addr);
    } else if (type == "memory") {
      r = SH2DelMemoryBreakpoint(ctx, addr);
    } else {
      return errorResult("type must be \"code\" or \"memory\"");
    }
    if (r != 0) return errorResult("breakpoint not found");
    return textResult(QString("breakpoint removed at 0x%1")
                          .arg(QString::number(addr, 16).toUpper()));
  }

  if (name == "breakpoint_list") {
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);

    const codebreakpoint_struct* cbp = SH2GetBreakpointList(ctx);
    const memorybreakpoint_struct* mbp = SH2GetMemoryBreakpointList(ctx);

    QString text;
    text += "Code breakpoints:\n";
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
      if (cbp[i].addr != 0xFFFFFFFF) {
        text += QString("  %1")
                    .arg(static_cast<uint32_t>(cbp[i].addr), 8, 16, QChar('0')).toUpper();
        if (cbp[i].hasCondition && cbp[i].regIndex >= 0) {
          QString regName;
          if (cbp[i].regIndex < 16) {
            regName = QString("R%1").arg(cbp[i].regIndex);
          } else if (cbp[i].regIndex <= 22) {
            static const char* regNames[] = {"SR", "GBR", "VBR", "MACH", "MACL", "PR", "PC"};
            regName = regNames[cbp[i].regIndex - 16];
          } else {
            regName = QString("R%1").arg(cbp[i].regIndex);
          }
          text += QString(" %1=%2")
                      .arg(regName)
                      .arg(static_cast<uint32_t>(cbp[i].regValue), 8, 16, QChar('0')).toUpper();
        }
        text += "\n";
      }
    }
    text += "Memory breakpoints:\n";
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
      if (mbp[i].addr != 0xFFFFFFFF) {
        text += QString("  %1 flags=%2")
                    .arg(static_cast<uint32_t>(mbp[i].addr), 8, 16, QChar('0')).toUpper()
                    .arg(static_cast<uint32_t>(mbp[i].flags), 2, 16, QChar('0')).toUpper();
        if (mbp[i].checkValue) {
          text += QString(" value=%1")
                      .arg(static_cast<uint32_t>(mbp[i].value), 8, 16, QChar('0')).toUpper();
        }
        text += "\n";
      }
    }
    return textResult(text);
  }

  if (name == "registers_get") {
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);
    return textResult(formatRegsAndDisasm(ctx));
  }

  if (name == "registers_set") {
    if (!isStopped())
      return errorResult("not stopped; use pause first");
    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);

    const QString regName = args.value("reg").toString();
    quint32 value = 0;
    if (!parseAddr(args.value("value"), &value))
      return errorResult("invalid value");

    sh2regs_struct regs = ctx->regs;
    bool found = false;

    if (regName.startsWith("R")) {
      bool ok = false;
      int idx = regName.mid(1).toInt(&ok, 10);
      if (ok && idx >= 0 && idx < 16) {
        regs.R[idx] = value;
        found = true;
      }
    } else if (regName == "SR") {
      regs.SR.all = value;
      found = true;
    } else if (regName == "GBR") {
      regs.GBR = value;
      found = true;
    } else if (regName == "VBR") {
      regs.VBR = value;
      found = true;
    } else if (regName == "MACH") {
      regs.MACH = value;
      found = true;
    } else if (regName == "MACL") {
      regs.MACL = value;
      found = true;
    } else if (regName == "PR") {
      regs.PR = value;
      found = true;
    } else if (regName == "PC") {
      regs.PC = value;
      found = true;
    }

    if (!found)
      return errorResult("invalid register name (use R0-R15, SR, GBR, VBR, MACH, MACL, PR, PC)");

    SH2SetRegisters(ctx, &regs);
    return textResult(QString("register %1 set to 0x%2").arg(regName).arg(QString::number(value, 16).toUpper()));
  }

  if (name == "memory_read") {
    quint32 addr = 0;
    if (!parseAddr(args.value("address"), &addr))
      return errorResult("invalid address");

    int len = qBound(1, args.value("length").toInt(16), 65536);
    const QString format = args.value("format").toString("hexdump");

    if (format == "hexdump") {
      QString out;
      quint64 end = (quint64)addr + len;
      for (quint64 base = addr; base < end; base += 16) {
        QString hex, ascii;
        for (int i = 0; i < 16 && base + i < end; i++) {
          u8 b = MappedMemoryReadByteNocache((quint32)(base + i), NULL);
          hex += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
          ascii += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
        }
        out += QString("%1: %2 %3\n").arg((quint32)base, 8, 16, QChar('0')).arg(hex, -48).arg(ascii);
      }
      return textResult(out);
    } else if (format == "u8") {
      QString out;
      quint64 end = (quint64)addr + len;
      for (quint64 base = addr; base < end; base++) {
        u8 b = MappedMemoryReadByteNocache((quint32)base, NULL);
        out += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
        if (((base - addr + 1) % 16 == 0) && (base + 1 < end)) out += "\n";
      }
      if (len % 16 != 0) out += "\n";
      return textResult(out);
    } else if (format == "u16") {
      if (addr % 2 != 0)
        return errorResult("u16 format requires address to be 2-byte aligned");
      QString out;
      int count = len / 2;
      quint64 end = (quint64)addr + len;
      for (quint64 base = addr; base < end; base += 2) {
        u16 w = MappedMemoryReadWordNocache((quint32)base, NULL);
        out += QString("%1 ").arg(w, 4, 16, QChar('0')).toUpper();
        if (((base - addr) / 2 + 1) % 8 == 0) out += "\n";
      }
      if (count % 8 != 0) out += "\n";
      return textResult(out);
    } else if (format == "u32") {
      if (addr % 4 != 0)
        return errorResult("u32 format requires address to be 4-byte aligned");
      QString out;
      int count = len / 4;
      quint64 end = (quint64)addr + len;
      for (quint64 base = addr; base < end; base += 4) {
        u32 l = MappedMemoryReadLongNocache((quint32)base, NULL);
        out += QString("%1 ").arg(l, 8, 16, QChar('0')).toUpper();
        if (((base - addr) / 4 + 1) % 4 == 0) out += "\n";
      }
      if (count % 4 != 0) out += "\n";
      return textResult(out);
    } else {
      return errorResult("format must be hexdump, u8, u16, or u32");
    }
  }

  if (name == "memory_write") {
    quint32 addr = 0;
    if (!parseAddr(args.value("address"), &addr))
      return errorResult("invalid address");

    if (args.contains("bytes")) {
      QString hexStr = args.value("bytes").toString();
      if (hexStr.length() % 2 != 0)
        return errorResult("bytes hex string must have even length");
      if (hexStr.length() > 131072)
        return errorResult("bytes hex string too long (max 65536 bytes = 131072 hex chars)");
      for (int i = 0; i < hexStr.length(); i += 2) {
        QString byteStr = hexStr.mid(i, 2);
        bool ok = false;
        u8 b = byteStr.toUInt(&ok, 16);
        if (!ok)
          return errorResult("invalid hex byte in bytes string");
        MappedMemoryWriteByteNocache(addr + i / 2, b, NULL);
      }
      return textResult(QString("wrote %1 bytes at 0x%2").arg(hexStr.length() / 2).arg(QString::number(addr, 16).toUpper()));
    } else {
      const QString size = args.value("size").toString();
      quint32 value = 0;
      if (!parseAddr(args.value("value"), &value))
        return errorResult("invalid value");

      if (size == "byte") {
        MappedMemoryWriteByteNocache(addr, (u8)value, NULL);
        return textResult(QString("wrote byte 0x%1 at 0x%2").arg(QString::number(value, 16).toUpper()).arg(QString::number(addr, 16).toUpper()));
      } else if (size == "word") {
        if (addr % 2 != 0)
          return errorResult("word write requires address to be 2-byte aligned");
        MappedMemoryWriteWordNocache(addr, (u16)value, NULL);
        return textResult(QString("wrote word 0x%1 at 0x%2").arg(QString::number(value, 16).toUpper()).arg(QString::number(addr, 16).toUpper()));
      } else if (size == "long") {
        if (addr % 4 != 0)
          return errorResult("long write requires address to be 4-byte aligned");
        MappedMemoryWriteLongNocache(addr, value, NULL);
        return textResult(QString("wrote long 0x%1 at 0x%2").arg(QString::number(value, 16).toUpper()).arg(QString::number(addr, 16).toUpper()));
      } else {
        return errorResult("size must be byte, word, or long");
      }
    }
  }

  if (name == "disassemble") {
    quint32 addr = 0;
    if (!parseAddr(args.value("address"), &addr))
      return errorResult("invalid address");

    int count = qBound(1, args.value("count").toInt(16), 256);

    QString err;
    SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);

    quint32 markPc = ctx->regs.PC;
    QString out = disasmRange(addr, count, markPc);
    return textResult(out);
  }

  if (name == "set_debug_option") {
    if (!args.contains("history") && !args.contains("cpu_core"))
      return errorResult("nothing to set");

    // Validate every argument before mutating any state, so an invalid
    // argument never leaves earlier options (e.g. history) applied while
    // reporting failure.
    bool wantHistory = args.contains("history");
    u32 historyOn = 0;
    if (wantHistory) {
      const QString h = args.value("history").toString();
      if (h != "on" && h != "off")
        return errorResult("history must be \"on\" or \"off\"");
      historyOn = (h == "on") ? 1 : 0;
    }

    bool wantCore = args.contains("cpu_core");
    int coreid = 0;
    if (wantCore) {
      const QString want = args.value("cpu_core").toString();
      if (want == "interpreter") coreid = SH2CORE_DEBUGINTERPRETER;
      else if (want == "dynarec")
        coreid = mYabauseThread->yabauseConf()->sh2coretype;
      else return errorResult("cpu_core must be interpreter or dynarec");
    }

    QString out;
    if (wantHistory) {
      MSH2->history_enabled = historyOn;
      SSH2->history_enabled = historyOn;
      out += QString("history recording: %1\n").arg(historyOn ? "on" : "off");
    }

    if (wantCore) {
      bool wasRunning = !isStopped();
      if (wasRunning && !mYabauseThread->pauseEmulation(true, false)) {
        QString err = "failed to pause for core switch (renderer not ready?)";
        if (wantHistory) err += "\nnote: history setting was already applied";
        return errorResult(err);
      }
      int r = SH2ChangeCore(coreid);
      bool resumeOk = true;
      if (wasRunning) resumeOk = mYabauseThread->pauseEmulation(false, false);
      if (r != 0) {
        QString err = "core switch failed";
        if (wantHistory) err += "\nnote: history setting was already applied";
        return errorResult(err);
      }
      out += QString("sh2 core: %1\n").arg(SH2Core->Name);
      if (!resumeOk)
        out += "warning: failed to resume emulation after core switch "
               "(renderer not ready?)\n";
    }

    return textResult(out);
  }

  if (name == "exec_history") {
    QString err; SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);
    if (!ctx->history_enabled)
      return errorResult("history recording is off; enable with "
                         "set_debug_option {\"history\":\"on\"}");
    if (!isStopped())
      return errorResult("not stopped; pause first for a consistent snapshot");
    int count = qBound(1, args.value("count").toInt(64), MAX_DMPHISTORY);
    bool withRegs = args.value("with_registers").toBool(false);
    QString out;
    char buf[128];
    // Oldest first; entry at pchistory_index is the most recent.
    for (int i = count - 1; i >= 0; i--) {
      u32 idx = (ctx->pchistory_index - i) & (MAX_DMPHISTORY - 1);
      u32 pc = ctx->pchistory[idx];
      if (pc == 0) continue;  // ring not fully wrapped yet: skip garbage entry
      SH2Disasm(pc, MappedMemoryReadWordNocache(pc, NULL), 0, NULL, buf);
      out += QString("%1\n").arg(buf);
      if (withRegs) {
        const sh2regs_struct& r = ctx->regshistory[idx];
        out += QString("    R0=%1 R4=%2 R5=%3 R14=%4 R15=%5 SR=%6 PR=%7\n")
                   .arg(r.R[0], 8, 16, QChar('0'))
                   .arg(r.R[4], 8, 16, QChar('0'))
                   .arg(r.R[5], 8, 16, QChar('0'))
                   .arg(r.R[14], 8, 16, QChar('0'))
                   .arg(r.R[15], 8, 16, QChar('0'))
                   .arg(r.SR.all, 8, 16, QChar('0'))
                   .arg(r.PR, 8, 16, QChar('0'));
      }
    }
    return textResult(out);
  }

  if (name == "backtrace") {
    QString err; SH2_struct* ctx = resolveCpu(args, &err);
    if (!ctx) return errorResult(err);
    int size = 0;
    u32* list = SH2GetBacktraceList(ctx, &size);
    if (size == 0) return textResult("(backtrace empty)");
    QString out;
    for (int i = size - 1; i >= 0; i--)
      out += QString("0x%1\n").arg(list[i], 8, 16, QChar('0'));
    return textResult(out);
  }

  return errorResult("unknown tool: " + name);
}

QJsonObject DebugMcpServer::toolWaitForStop(const QJsonObject& args) {
  // Same "no game loaded" guard as callTool(). Read directly from the HTTP
  // thread rather than marshalling via runOnUiThread: YabauseThread::init()
  // is a plain int set once by initEmulation()/deInitEmulation() on the UI
  // thread, so this is a benign, well-known data race (worst case: a
  // one-frame-stale answer to a coarse pre-check). The actual wait below
  // is properly synchronized via mStopMutex/mStopCv regardless.
  if (mYabauseThread->init() != 0)
    return errorResult("emulation is not running (no game loaded)");

  int timeoutMs = qBound(1, args.value("timeout_ms").toInt(10000), 300000);
  std::unique_lock<std::mutex> lk(mStopMutex);
  // stop() may already be shutting the server down (see stop()): do not
  // block a worker thread here in that case.
  if (mStopping.load())
    return textResult("stopped: false (server shutting down)");
  if (mStopped)
    return textResult(QString("stopped: true\nreason: %1 cpu=%2 addr=0x%3")
                          .arg(mStopReason, mStopCpu,
                               QString::number(mStopAddr, 16).toUpper()));
  quint64 startSerial = mStopSerial;
  // mStopping is included in the predicate so stop()'s mStopCv.notify_all()
  // wakes this wait immediately instead of leaving it blocked for up to
  // timeout_ms while stop() waits for this worker thread to finish.
  bool ok = mStopCv.wait_for(
      lk, std::chrono::milliseconds(timeoutMs), [&] {
        return mStopping.load() ||
               (mStopped && mStopSerial != startSerial);
      });
  if (mStopping.load())
    return textResult("stopped: false (server shutting down)");
  if (!mStopped || !ok)
    return textResult("stopped: false (timeout; still running)");
  return textResult(QString("stopped: true\nreason: %1 cpu=%2 addr=0x%3")
                        .arg(mStopReason, mStopCpu,
                             QString::number(mStopAddr, 16).toUpper()));
}

QJsonArray DebugMcpServer::toolDefinitions() const {
  const char* json = R"JSON([
    {"name":"debug_status","description":"Get emulator debug status: run/stop state, stop reason, PCs of both SH2 CPUs, frame count, game id, CPU core, history recording state.","inputSchema":{"type":"object","properties":{}}},
    {"name":"pause","description":"Pause emulation at the next frame boundary and mark it stopped for debugging.","inputSchema":{"type":"object","properties":{}}},
    {"name":"resume","description":"Clear halt requests on both SH2 CPUs and resume emulation. Requires being stopped.","inputSchema":{"type":"object","properties":{}}},
    {"name":"load_state","description":"Load a numbered savestate slot (<itemnum>_<slot>.yss from the configured SaveStates directory). Pauses emulation around the load and resumes if it was running.","inputSchema":{"type":"object","properties":{"slot":{"type":"integer"}}}},
    {"name":"step","description":"Single-step the given SH2 CPU by count instructions while stopped. Requires the debug interpreter core. Typical flow: pause -> step/step_over/step_out (repeat) -> resume, or set_breakpoint -> resume -> wait_for_stop.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]},"count":{"type":"integer"}}}},
    {"name":"step_over","description":"Step the given SH2 CPU over the next instruction, not descending into calls. Requires the debug interpreter core and being stopped.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]}}}},
    {"name":"step_out","description":"Run the given SH2 CPU until it returns from the current subroutine. Requires the debug interpreter core and being stopped.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]}}}},
    {"name":"wait_for_stop","description":"Block (on the HTTP thread) until the emulator stops (breakpoint hit, pause, or step) or timeout_ms elapses. If already stopped, returns immediately. Use after resume + set_breakpoint to wait for a breakpoint hit.","inputSchema":{"type":"object","properties":{"timeout_ms":{"type":"integer"}}}},
    {"name":"breakpoint_add","description":"Add a code or memory breakpoint on the given SH2 CPU. Code breakpoints can have an optional register condition (reg_index and reg_value). Memory breakpoints support read/write/byte/word/long flags and optional value comparison.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]},"address":{"type":"string"},"type":{"type":"string","enum":["code","memory"]},"reg_index":{"type":"integer"},"reg_value":{"type":"string"},"read":{"type":"boolean"},"write":{"type":"boolean"},"value":{"type":"string"}}}},
    {"name":"breakpoint_remove","description":"Remove a code or memory breakpoint at the given address from the given SH2 CPU.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]},"address":{"type":"string"},"type":{"type":"string","enum":["code","memory"]}}}},
    {"name":"breakpoint_list","description":"List all active code and memory breakpoints for the given SH2 CPU.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]}}}},
    {"name":"registers_get","description":"Get register values for the given SH2 CPU: R0-R15, SR, GBR, VBR, MACH, MACL, PR, PC plus a disassembly of the current PC.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]}}}},
    {"name":"registers_set","description":"Set a register value on the given SH2 CPU. Requires being stopped. Register names: R0-R15, SR, GBR, VBR, MACH, MACL, PR, PC.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]},"reg":{"type":"string"},"value":{"type":"string"}}}},
    {"name":"memory_read","description":"Read memory and format as hexdump (default), u8, u16, or u32 values. u16/u32 require aligned addresses.","inputSchema":{"type":"object","properties":{"address":{"type":"string"},"length":{"type":"integer"},"format":{"type":"string","enum":["hexdump","u8","u16","u32"]}}}},
    {"name":"memory_write","description":"Write to memory either as a single value (size + value) or bulk hex bytes. Aligned writes (word/long) require aligned addresses.","inputSchema":{"type":"object","properties":{"address":{"type":"string"},"size":{"type":"string","enum":["byte","word","long"]},"value":{"type":"string"},"bytes":{"type":"string"}}}},
    {"name":"disassemble","description":"Disassemble instructions starting at the given address. The current CPU's PC is marked with >.","inputSchema":{"type":"object","properties":{"address":{"type":"string"},"count":{"type":"integer"},"cpu":{"type":"string","enum":["master","slave"]}}}},
    {"name":"exec_history","description":"List the last count executed instructions for the given SH2 CPU, oldest first. Requires history recording to be on (set_debug_option {\"history\":\"on\"}) and the emulator to be stopped.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]},"count":{"type":"integer"},"with_registers":{"type":"boolean"}}}},
    {"name":"backtrace","description":"List the call stack (return addresses) for the given SH2 CPU, most recent call first.","inputSchema":{"type":"object","properties":{"cpu":{"type":"string","enum":["master","slave"]}}}},
    {"name":"set_debug_option","description":"Set debug options: history (\"on\"/\"off\", instruction history recording) and/or cpu_core (\"interpreter\" switches to the debug interpreter core; \"dynarec\" switches back to the configured core). Pauses/resumes around the core switch if emulation is running.","inputSchema":{"type":"object","properties":{"history":{"type":"string","enum":["on","off"]},"cpu_core":{"type":"string","enum":["interpreter","dynarec"]}}}}
  ])JSON";
  return QJsonDocument::fromJson(json).array();
}

void DebugMcpServer::breakpointHandler(void* context, u32 addr,
                                       void* userdata) {
  DebugMcpServer* self = static_cast<DebugMcpServer*>(userdata);
  self->onBreakpointHit(static_cast<SH2_struct*>(context), addr);
}

void DebugMcpServer::onBreakpointHit(SH2_struct* context, u32 addr) {
  // This callback fires synchronously from inside SH2 execution, which on the
  // Qt Vulkan port runs re-entrantly within QYabVulkanWidget::paintEvent() ->
  // execEmulation() -> PERCore->HandleEvents() -- i.e. in the MIDDLE of the
  // current frame's rendering. Calling pauseEmulation() here would emit the
  // YabauseThread::pause signal, whose UIYabause::pause slot calls
  // mYabVulkanWidget->updateView() -> YabauseThread::resize() ->
  // VIDCore->Resize(), recreating the Vulkan swapchain mid-frame. When
  // HandleEvents() then resumes to present the in-flight frame, it blocks
  // forever on a stale fence/semaphore (observed UI-thread hang in
  // paintAndFlush). Setting debugHaltRequest freezes the CPU immediately so
  // no further game instructions execute this frame, and the frame finishes
  // and presents normally. The actual pause is deferred to the next frame
  // boundary via a queued call, matching the safe timing of a menu pause.
  context->debugHaltRequest = 1;
  setStopState("breakpoint", (context == MSH2) ? "master" : "slave",
              (addr != 0) ? addr : context->regs.PC);
  quint64 pauseSerial;
  {
    std::lock_guard<std::mutex> lk(mStopMutex);
    pauseSerial = mStopSerial;
  }
  YabauseThread* yt = mYabauseThread;
  // Capturing `this` is safe here because UIYabause constructs YabauseThread
  // before DebugMcpServer (both parented to UIYabause), and Qt purges any
  // queued invokeMethod call targeting `yt` once `yt` is destroyed; if that
  // construction order ever changes, this assumption must be revisited.
  QMetaObject::invokeMethod(
      yt,
      [this, yt, pauseSerial]() {
        // Defense-in-depth: a resume() could land between queueing this
        // deferred pause and it running. Only pause if we are still stopped
        // for the same stop event; otherwise the late pause would silently
        // re-pause a running emulator (mStopped=false yet emulation halted,
        // an unrecoverable state). mStopSerial advances on every stop event.
        {
          std::lock_guard<std::mutex> lk(mStopMutex);
          if (!mStopped || mStopSerial != pauseSerial) return;
        }
        yt->pauseEmulation(true, false);
      },
      Qt::QueuedConnection);
}

void DebugMcpServer::ensureCallbackRegistered() {
  if (MSH2) SH2SetBreakpointCallBack(MSH2, breakpointHandler, this);
  if (SSH2) SH2SetBreakpointCallBack(SSH2, breakpointHandler, this);
}
