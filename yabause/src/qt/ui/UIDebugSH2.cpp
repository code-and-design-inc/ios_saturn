/*	Copyright 2012-2013 Theo Berkau <cwx@cyberwarriorx.com>

        This file is part of Yabause.

        Yabause is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        Yabause is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with Yabause; if not, write to the Free Software
        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
   USA
*/

#include "UIDebugSH2.h"
#include "../CommonDialogs.h"
#include "../Settings.h"
#include "UIYabause.h"
#include <QClipboard>
#include <QApplication>

int SH2Dis(u32 addr, char *string) {
  SH2Disasm(addr, MappedMemoryReadWordNocache(addr, NULL), 0, NULL, string);
  return 2;
}

void SH2BreakpointHandler(SH2_struct *context, u32 addr, void *userdata) {
  UIYabause *ui = QtYabause::mainWindow(false);

  if (context == MSH2)
    emit ui->breakpointHandlerMSH2(userdata == NULL ? true : false);
  else
    emit ui->breakpointHandlerSSH2(userdata == NULL ? true : false);
}

UIDebugSH2::UIDebugSH2(bool master, YabauseThread *mYabauseThread, QWidget *p)
    : UIDebugCPU(mYabauseThread, p) {
  if (master) {
    this->setWindowTitle(QtYabause::translate("Debug Master SH2"));
    debugSH2 = MSH2;
  } else {
    this->setWindowTitle(QtYabause::translate("Debug Slave SH2"));
    debugSH2 = SSH2;
  }
  gbRegisters->setTitle(QtYabause::translate("SH2 Registers"));

  if (debugSH2) {
    const codebreakpoint_struct *cbp;
    const memorybreakpoint_struct *mbp;
    int i;

    cbp = SH2GetBreakpointList(debugSH2);
    mbp = SH2GetMemoryBreakpointList(debugSH2);

    bool hasBreakpoints = false;
    for (i = 0; i < MAX_BREAKPOINTS; i++) {
      if (cbp[i].addr != 0xFFFFFFFF)
        hasBreakpoints = true;
      if (mbp[i].addr != 0xFFFFFFFF)
        hasBreakpoints = true;
    }

    if (!hasBreakpoints) {
      // Load breakpoints
      u64 gameid = Cs2GetGameId();
      if (gameid != 0) {
        Settings *settings = QtYabause::settings();
        QString keyBase = QString("Breakpoints/%1/%2")
                              .arg(gameid)
                              .arg(debugSH2 == MSH2 ? "MSH2" : "SSH2");

        // Code Breakpoints
        int size = settings->beginReadArray(keyBase + "/Code");
        for (int i = 0; i < size; ++i) {
          settings->setArrayIndex(i);
          u32 addr = settings->value("Address").toUInt();
          int regIndex = settings->value("RegIndex", -1).toInt();
          u32 regValue = settings->value("RegValue", 0).toUInt();
          int hasCondition = settings->value("HasCondition", 0).toInt();
          if (hasCondition && regIndex >= 0) {
            SH2AddCodeBreakpointEx(debugSH2, addr, regIndex, regValue);
          } else {
            SH2AddCodeBreakpoint(debugSH2, addr);
          }
        }
        settings->endArray();

        // Memory Breakpoints
        size = settings->beginReadArray(keyBase + "/Memory");
        for (int i = 0; i < size; ++i) {
          settings->setArrayIndex(i);
          u32 addr = settings->value("Address").toUInt();
          u32 flags = settings->value("Flags").toUInt();
          u32 value = settings->value("Value").toUInt();
          bool checkValue = settings->value("CheckValue", false).toBool();
          SH2AddMemoryBreakpoint(debugSH2, addr, flags, value,
                                 checkValue ? 1 : 0);
        }
        settings->endArray();
      }
    }

    // Refresh pointers in case they changed (unlikely but safe)
    cbp = SH2GetBreakpointList(debugSH2);
    mbp = SH2GetMemoryBreakpointList(debugSH2);

    for (i = 0; i < MAX_BREAKPOINTS; i++) {
      QString text;
      if (cbp[i].addr != 0xFFFFFFFF) {
        text = QString("%1")
                   .arg(static_cast<uint32_t>(cbp[i].addr), 8, 16, QChar('0'))
                   .toUpper();
        if (cbp[i].hasCondition && cbp[i].regIndex >= 0) {
          QString regName;
          if (cbp[i].regIndex < 16) {
            regName = QString("R%1").arg(cbp[i].regIndex);
          } else {
            static const char* regNames[] = {"SR", "GBR", "VBR", "MACH", "MACL", "PR", "PC"};
            regName = regNames[cbp[i].regIndex - 16];
          }
          text += QString(" %1=%2")
                      .arg(regName)
                      .arg(static_cast<uint32_t>(cbp[i].regValue), 8, 16, QChar('0')).toUpper();
        }
        lwCodeBreakpoints->addItem(text);
      }

      if (mbp[i].addr != 0xFFFFFFFF) {
        text = QString("%1")
                   .arg(static_cast<uint32_t>(mbp[i].addr), 8, 16, QChar('0'))
                   .toUpper();
        lwMemoryBreakpoints->addItem(text);
      }
    }

    lwDisassembledCode->setDisassembleFunction(SH2Dis);
    lwDisassembledCode->setEndAddress(0x06100000);
    lwDisassembledCode->setMinimumInstructionSize(2);
    gbBackTrace->setVisible(true);

    SH2SetBreakpointCallBack(
        debugSH2, (void (*)(void *, u32, void *))SH2BreakpointHandler, NULL);
  }

  updateAll();

  if (debugSH2 && debugSH2->trackInfLoop.enabled)
    pbReserved1->setText(QtYabause::translate("Loop Track Stop"));
  else
    pbReserved1->setText(QtYabause::translate("Loop Track Start"));
  pbReserved2->setText(QtYabause::translate("Loop Track Clear"));
  pbReserved3->setText(QtYabause::translate("Inline Assembly"));
  pbReserved4->setText(QtYabause::translate("Copy Disasm"));

  pbStepOver->setVisible(true);
  pbStepOut->setVisible(true);
  pbReserved1->setVisible(true);
  pbReserved2->setVisible(true);
  pbReserved3->setVisible(true);
  pbReserved4->setVisible(true);
}

void UIDebugSH2::updateRegList() {
  int i;
  sh2regs_struct sh2regs;
  QString str;

  if (debugSH2 == NULL)
    return;

  SH2GetRegisters(debugSH2, &sh2regs);
  lwRegisters->clear();

  // R0〜R15のレジスタを表示
  for (i = 0; i < 16; i++) {
    str = QString("R%1 =  %2")
              .arg(i, 2, 10, QChar('0'))
              .arg((uint32_t)sh2regs.R[i], 8, 16, QChar('0'))
              .toUpper();
    lwRegisters->addItem(str);
  }

  // SR
  str = QString("SR =   %1")
            .arg((uint32_t)sh2regs.SR.all, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);

  // GBR
  str = QString("GBR =  %1")
            .arg((uint32_t)sh2regs.GBR, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);

  // VBR
  str = QString("VBR =  %1")
            .arg((uint32_t)sh2regs.VBR, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);

  // MACH
  str = QString("MACH = %1")
            .arg((uint32_t)sh2regs.MACH, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);

  // MACL
  str = QString("MACL = %1")
            .arg((uint32_t)sh2regs.MACL, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);

  // PR
  str = QString("PR =   %1")
            .arg((uint32_t)sh2regs.PR, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);

  // PC
  str = QString("PC =   %1")
            .arg((uint32_t)sh2regs.PC, 8, 16, QChar('0'))
            .toUpper();
  lwRegisters->addItem(str);
}

void UIDebugSH2::updateCodeList(u32 addr) {
  addr &= 0x0FFFFFFF;
  lwDisassembledCode->goToAddress(addr);
  lwDisassembledCode->setPC(addr);
}

void UIDebugSH2::updateBackTrace() {
  int size;
  u32 *addr = SH2GetBacktraceList(debugSH2, &size);

  lwBackTrace->clear();
  for (int i = 0; i < size; i++)
    lwBackTrace->addItem(
        QString("%1").arg(addr[i], 8, 16, QChar('0')).toUpper());
  lwBackTrace->addItem(
      QString("%1").arg(debugSH2->regs.PC, 8, 16, QChar('0')).toUpper());
}

void UIDebugSH2::updateTrackInfLoop() {
  if (debugSH2) {
    tilInfo_struct *match = debugSH2->trackInfLoop.match;

    twTrackInfLoop->clearContents();
    twTrackInfLoop->setRowCount(0);
    twTrackInfLoop->setSortingEnabled(false);
    for (int i = 0; i < debugSH2->trackInfLoop.num; i++) {
      twTrackInfLoop->insertRow(i);
      QTableWidgetItem *newItem = new QTableWidgetItem(
          QString("%1").arg(match[i].addr, 8, 16, QChar('0')).toUpper());
      twTrackInfLoop->setItem(i, 0, newItem);

      newItem = new QTableWidgetItem();
      newItem->setData(Qt::DisplayRole, (qulonglong)match[i].count);
      twTrackInfLoop->setItem(i, 1, newItem);
    }
    twTrackInfLoop->setSortingEnabled(true);
  }
}

void UIDebugSH2::updateAll() {
  updateRegList();
  if (debugSH2) {
    sh2regs_struct sh2regs;

    SH2GetRegisters(debugSH2, &sh2regs);
    updateCodeList(sh2regs.PC);
    updateBackTrace();
    updateTrackInfLoop();
  }
}

u32 UIDebugSH2::getRegister(int index, int *size) {
  sh2regs_struct sh2regs;
  u32 value;

  SH2GetRegisters(debugSH2, &sh2regs);

  if (index < 16)
    value = sh2regs.R[index];
  else {
    switch (index) {
    case 16:
      value = sh2regs.SR.all;
      break;
    case 17:
      value = sh2regs.GBR;
      break;
    case 18:
      value = sh2regs.VBR;
      break;
    case 19:
      value = sh2regs.MACH;
      break;
    case 20:
      value = sh2regs.MACL;
      break;
    case 21:
      value = sh2regs.PR;
      break;
    case 22:
      value = sh2regs.PC;
      break;
    default:
      value = 0;
      break;
    }
  }

  *size = 4;
  return value;
}

void UIDebugSH2::setRegister(int index, u32 value) {
  sh2regs_struct sh2regs;

  SH2GetRegisters(debugSH2, &sh2regs);

  if (index < 16)
    sh2regs.R[index] = value;
  else {
    switch (index) {
    case 16:
      sh2regs.SR.all = value;
      break;
    case 17:
      sh2regs.GBR = value;
      break;
    case 18:
      sh2regs.VBR = value;
      break;
    case 19:
      sh2regs.MACH = value;
      break;
    case 20:
      sh2regs.MACL = value;
      break;
    case 21:
      sh2regs.PR = value;
      break;
    case 22:
      sh2regs.PC = value;
      updateCodeList(sh2regs.PC);
      break;
    }
  }

  SH2SetRegisters(debugSH2, &sh2regs);
}

bool UIDebugSH2::addCodeBreakpoint(u32 addr) {
  if (!debugSH2)
    return false;
  if (SH2AddCodeBreakpoint(debugSH2, addr) == 0) {
    saveBreakpoints();
    return true;
  }
  return false;
}

bool UIDebugSH2::addCodeBreakpointEx(u32 addr, int regIndex, u32 regValue) {
  if (!debugSH2)
    return false;
  if (SH2AddCodeBreakpointEx(debugSH2, addr, regIndex, regValue) == 0) {
    saveBreakpoints();
    return true;
  }
  return false;
}

bool UIDebugSH2::delCodeBreakpoint(u32 addr) {
  if (!debugSH2)
    return false;
  if (SH2DelCodeBreakpoint(debugSH2, addr) == 0) {
    saveBreakpoints();
    return true;
  }
  return false;
}

bool UIDebugSH2::addMemoryBreakpoint(u32 addr, u32 flags, u32 value,
                                     int checkValue) {
  if (!debugSH2)
    return false;
  if (SH2AddMemoryBreakpoint(debugSH2, addr, flags, value, checkValue) == 0) {
    saveBreakpoints();
    return true;
  }
  return false;
}

bool UIDebugSH2::delMemoryBreakpoint(u32 addr) {
  if (!debugSH2)
    return false;
  if (SH2DelMemoryBreakpoint(debugSH2, addr) == 0) {
    saveBreakpoints();
    return true;
  }
  return false;
}

void UIDebugSH2::stepInto() {
  if (debugSH2) {
    SH2Step(debugSH2);
    updateAll();
  }
}

void UIDebugSH2::stepOver() {
  if (debugSH2) {
    if (SH2StepOver(debugSH2,
                    (void (*)(void *, u32, void *))SH2BreakpointHandler) == 0)
      updateAll();
    else
      // Close dialog and wait
      this->accept();
  }
}

void UIDebugSH2::stepOut() {
  if (debugSH2) {
    SH2StepOut(debugSH2, (void (*)(void *, u32, void *))SH2BreakpointHandler);

    // Close dialog and wait
    this->accept();
  }
}

void UIDebugSH2::reserved1() {
  if (debugSH2) {
    if (!debugSH2->trackInfLoop.enabled) {
      SH2TrackInfLoopStart(debugSH2);
      pbReserved1->setText(QtYabause::translate("Loop Track Stop"));
    } else {
      SH2TrackInfLoopStop(debugSH2);
      pbReserved1->setText(QtYabause::translate("Loop Track Start"));
    }
  }
}

void UIDebugSH2::reserved2() {
  if (debugSH2)
    SH2TrackInfLoopClear(debugSH2);
  updateAll();
}

void UIDebugSH2::reserved3() {
  if (debugSH2) {
    bool ok;

    for (;;) {
      QString text = QInputDialog::getText(
          this, QtYabause::translate("Assembly code"),
          QtYabause::translate("Enter new assembly code") + ":",
          QLineEdit::Normal, QString(), &ok);

      if (ok && !text.isEmpty()) {
        char errorMsg[512];
        int op = sh2iasm(text.toLatin1().data(), errorMsg);
        if (op != 0) {
          MappedMemoryWriteWord(debugSH2->regs.PC, op, NULL);
          break;
        } else
          QMessageBox::critical(QApplication::activeWindow(),
                                QtYabause::translate("Error"),
                                QString(errorMsg));
      } else if (!ok)
        break;
    }
  }
  updateAll();
}

void UIDebugSH2::reserved4() {
  if (debugSH2) {
    QString disasmText;
    sh2regs_struct sh2regs;
    SH2GetRegisters(debugSH2, &sh2regs);
    u32 currentPC = sh2regs.PC;

    // Calculate start address (100 steps before PC)
    // Each instruction is 2 bytes for SH2
    u32 startAddr = (currentPC >= 200) ? (currentPC - 200) : 0;

    // Disassemble 200 instructions (100 before + 100 after PC)
    u32 addr = startAddr;
    for (int i = 0; i < 200 && addr < 0x06100000; i++) {
      char text[256];
      int offset = SH2Dis(addr, text);

      // Mark current PC with arrow
      if (addr == currentPC) {
        disasmText += QString(">>> %1\n").arg(text);
      } else {
        disasmText += QString("    %1\n").arg(text);
      }

      addr += offset;
    }

    // Copy to clipboard
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(disasmText);

    QMessageBox::information(this, QtYabause::translate("Copy Disassembly"),
                             QtYabause::translate("Disassembled code copied to clipboard (200 instructions)"));
  }
}

void UIDebugSH2::saveBreakpoints() {
  if (!debugSH2)
    return;

  u64 gameid = Cs2GetGameId();
  if (gameid == 0)
    return;

  Settings *settings = QtYabause::settings();
  QString keyBase = QString("Breakpoints/%1/%2")
                        .arg(gameid)
                        .arg(debugSH2 == MSH2 ? "MSH2" : "SSH2");

  const codebreakpoint_struct *cbp = SH2GetBreakpointList(debugSH2);
  const memorybreakpoint_struct *mbp = SH2GetMemoryBreakpointList(debugSH2);

  // Save Code Breakpoints
  settings->beginWriteArray(keyBase + "/Code");
  int count = 0;
  for (int i = 0; i < MAX_BREAKPOINTS; i++) {
    if (cbp[i].addr != 0xFFFFFFFF) {
      settings->setArrayIndex(count++);
      settings->setValue("Address", cbp[i].addr);
      settings->setValue("RegIndex", cbp[i].regIndex);
      settings->setValue("RegValue", cbp[i].regValue);
      settings->setValue("HasCondition", cbp[i].hasCondition);
    }
  }
  settings->endArray();

  // Save Memory Breakpoints
  settings->beginWriteArray(keyBase + "/Memory");
  count = 0;
  for (int i = 0; i < MAX_BREAKPOINTS; i++) {
    if (mbp[i].addr != 0xFFFFFFFF) {
      settings->setArrayIndex(count++);
      settings->setValue("Address", mbp[i].addr);
      settings->setValue("Flags", mbp[i].flags);
      settings->setValue("Value",
                         (u32)mbp[i].value); // mbp[i].value is u32* in struct?
                                             // No, let's check definition.
      // In sh2core.h:
      // typedef struct {
      //    u32 addr;
      //    u32 flags;
      //    u32 value;
      //    int checkValue;
      // } memorybreakpoint_struct;
      // So it is u32.
      settings->setValue("Value", mbp[i].value);
      settings->setValue("CheckValue", mbp[i].checkValue != 0);
    }
  }
  settings->endArray();
}
