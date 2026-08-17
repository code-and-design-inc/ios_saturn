/*  Copyright 2007 Guillaume Duhamel

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file m68kmusashi.c
\brief Musashi 68000 interface.
*/

#include "m68kmusashi.h"
#include "musashi/m68k.h"
#include "m68kcore.h"
#include "musashi/m68kcpu.h"
#include "memory.h"

struct ReadWriteFuncs
{
   M68K_READ  *r_8;
   M68K_READ  *r_16;
   M68K_WRITE *w_8;
   M68K_WRITE *w_16;
}rw_funcs;

static int M68KMusashiInit(void) {

   m68k_init();
   m68k_set_reset_instr_callback(m68k_pulse_reset);
   m68k_set_cpu_type(M68K_CPU_TYPE_68000);

   return 0;
}

static void M68KMusashiDeInit(void) {
}

static void M68KMusashiReset(void) {
   m68k_pulse_reset();
}

static s32 FASTCALL M68KMusashiExec(s32 cycle) {
   return m68k_execute(cycle);
}

static void M68KMusashiSync(void) {
}

static u32 M68KMusashiGetDReg(u32 num) {
   return m68k_get_reg(NULL, M68K_REG_D0 + num);
}

static u32 M68KMusashiGetAReg(u32 num) {
   return m68k_get_reg(NULL, M68K_REG_A0 + num);
}

static u32 M68KMusashiGetPC(void) {
   return m68k_get_reg(NULL, M68K_REG_PC);
}

static u32 M68KMusashiGetSR(void) {
   return m68k_get_reg(NULL, M68K_REG_SR);
}

static u32 M68KMusashiGetUSP(void) {
   return m68k_get_reg(NULL, M68K_REG_USP);
}

static u32 M68KMusashiGetMSP(void) {
   return m68k_get_reg(NULL, M68K_REG_MSP);
}

static void M68KMusashiSetDReg(u32 num, u32 val) {
   m68k_set_reg(M68K_REG_D0 + num, val);
}

static void M68KMusashiSetAReg(u32 num, u32 val) {
   m68k_set_reg(M68K_REG_A0 + num, val);
}

static void M68KMusashiSetPC(u32 val) {
   m68k_set_reg(M68K_REG_PC, val);
}

static void M68KMusashiSetSR(u32 val) {
   m68k_set_reg(M68K_REG_SR, val);
}

static void M68KMusashiSetUSP(u32 val) {
   m68k_set_reg(M68K_REG_USP, val);
}

static void M68KMusashiSetMSP(u32 val) {
   m68k_set_reg(M68K_REG_MSP, val);
}

static void M68KMusashiSetFetch(u32 low_adr, u32 high_adr, pointer fetch_adr) {
}

static void FASTCALL M68KMusashiSetIRQ(s32 level) {
   // Level 0 must reach the core as well: it deasserts the interrupt request.
   // The SCSP drives IPL as a level, so when its interrupt sources are cleared
   // (SCIRE/SCIEB writes, SCSP reset) a request that was never serviced - the
   // sound driver may run with SR mask 7 for a long time - has to disappear.
   // Dropping the 0 here left such a request pending across a sound CPU reset,
   // where it fired into the freshly restarted driver before it had installed
   // its own vectors (X-Men vs. Street Fighter after a clock change).
   m68k_set_irq(level);
}

static void FASTCALL M68KMusashiWriteNotify(u32 address, u32 size) {
}

unsigned int  m68k_read_memory_8(unsigned int address)
{
   return rw_funcs.r_8(address);
}

unsigned int  m68k_read_memory_16(unsigned int address)
{
   return rw_funcs.r_16(address);
}

unsigned int  m68k_read_memory_32(unsigned int address)
{
   u16 val1 = rw_funcs.r_16(address);

   return (val1 << 16 | rw_funcs.r_16(address + 2));
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
   rw_funcs.w_8(address, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
   rw_funcs.w_16(address, value);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
   rw_funcs.w_16(address, value >> 16 );
   rw_funcs.w_16(address + 2, value & 0xffff);
}

static void M68KMusashiSetReadB(M68K_READ *Func) {
   rw_funcs.r_8 = Func;
}

static void M68KMusashiSetReadW(M68K_READ *Func) {
   rw_funcs.r_16 = Func;
}

static void M68KMusashiSetWriteB(M68K_WRITE *Func) {
   rw_funcs.w_8 = Func;
}

static void M68KMusashiSetWriteW(M68K_WRITE *Func) {
   rw_funcs.w_16 = Func;
}

static void M68KMusashiSaveState(FILE *fp) {
   IOCheck_struct check = { 0, 0 };
   int i;
   u32 temp;
   u32 sr;

   // Save D registers (same as C68K: 8 x 4 = 32 bytes)
   for (i = 0; i < 8; i++) {
      temp = m68k_get_reg(NULL, M68K_REG_D0 + i);
      ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   }

   // Save A registers (same as C68K: 8 x 4 = 32 bytes)
   for (i = 0; i < 8; i++) {
      temp = m68k_get_reg(NULL, M68K_REG_A0 + i);
      ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   }

   // Save flags (compatible with C68K format: 7 x 4 = 28 bytes)
   sr = m68k_get_reg(NULL, M68K_REG_SR);
   temp = (sr & 0x01) ? 1 : 0;  // C flag
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = (sr & 0x02) ? 1 : 0;  // V flag
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = (sr & 0x04) ? 0 : 1;  // notZ flag (inverted)
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = (sr & 0x08) ? 1 : 0;  // N flag
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = (sr & 0x10) ? 1 : 0;  // X flag
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = (sr >> 8) & 0x07;     // I flag (interrupt mask)
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = (sr & 0x2000) ? 1 : 0;  // S flag (supervisor)
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);

   // Save USP (4 bytes)
   temp = m68k_get_reg(NULL, M68K_REG_USP);
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);

   // Save PC (4 bytes)
   temp = m68k_get_reg(NULL, M68K_REG_PC);
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);

   // Save Status and IRQLine (8 bytes) - use padding for compatibility
   temp = sr;  // Status = full SR
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
   temp = 0;   // IRQLine placeholder
   ywrite(&check, (void *)&temp, sizeof(s32), 1, fp);

   // Save cycle counters (16 bytes) - padding for C68K compatibility
   temp = 0;  // CycleToDo
   ywrite(&check, (void *)&temp, sizeof(s32), 1, fp);
   temp = 0;  // CycleIO
   ywrite(&check, (void *)&temp, sizeof(s32), 1, fp);
   temp = 0;  // CycleSup
   ywrite(&check, (void *)&temp, sizeof(s32), 1, fp);
   temp = 0;  // dirty1
   ywrite(&check, (void *)&temp, sizeof(u32), 1, fp);
}

static void M68KMusashiLoadState(FILE *fp) {
   IOCheck_struct check = { 0, 0 };
   int i;
   u32 temp;
   u32 sr = 0;
   u32 flag_C, flag_V, flag_notZ, flag_N, flag_X, flag_I, flag_S;

   // Load D registers (32 bytes)
   for (i = 0; i < 8; i++) {
      yread(&check, (void *)&temp, sizeof(u32), 1, fp);
      m68k_set_reg(M68K_REG_D0 + i, temp);
   }

   // Load A registers (32 bytes)
   for (i = 0; i < 8; i++) {
      yread(&check, (void *)&temp, sizeof(u32), 1, fp);
      m68k_set_reg(M68K_REG_A0 + i, temp);
   }

   // Load flags (28 bytes)
   yread(&check, (void *)&flag_C, sizeof(u32), 1, fp);
   yread(&check, (void *)&flag_V, sizeof(u32), 1, fp);
   yread(&check, (void *)&flag_notZ, sizeof(u32), 1, fp);
   yread(&check, (void *)&flag_N, sizeof(u32), 1, fp);
   yread(&check, (void *)&flag_X, sizeof(u32), 1, fp);
   yread(&check, (void *)&flag_I, sizeof(u32), 1, fp);
   yread(&check, (void *)&flag_S, sizeof(u32), 1, fp);

   // Reconstruct SR from flags
   sr = 0;
   if (flag_C) sr |= 0x01;
   if (flag_V) sr |= 0x02;
   if (!flag_notZ) sr |= 0x04;  // notZ is inverted
   if (flag_N) sr |= 0x08;
   if (flag_X) sr |= 0x10;
   sr |= (flag_I & 0x07) << 8;
   if (flag_S) sr |= 0x2000;

   // Load USP (4 bytes)
   yread(&check, (void *)&temp, sizeof(u32), 1, fp);
   m68k_set_reg(M68K_REG_USP, temp);

   // Load PC (4 bytes)
   yread(&check, (void *)&temp, sizeof(u32), 1, fp);
   m68k_set_reg(M68K_REG_PC, temp);

   // Load Status (use this instead of reconstructed SR if available)
   yread(&check, (void *)&temp, sizeof(u32), 1, fp);
   if (temp != 0) sr = temp;  // Use saved Status if valid
   m68k_set_reg(M68K_REG_SR, sr);

   // Skip IRQLine (4 bytes)
   yread(&check, (void *)&temp, sizeof(s32), 1, fp);

   // Skip cycle counters (16 bytes)
   yread(&check, (void *)&temp, sizeof(s32), 1, fp);  // CycleToDo
   yread(&check, (void *)&temp, sizeof(s32), 1, fp);  // CycleIO
   yread(&check, (void *)&temp, sizeof(s32), 1, fp);  // CycleSup
   yread(&check, (void *)&temp, sizeof(u32), 1, fp);  // dirty1
}

M68K_struct M68KMusashi = {
   3,
   "Musashi Interface",
   M68KMusashiInit,
   M68KMusashiDeInit,
   M68KMusashiReset,
   M68KMusashiExec,
   M68KMusashiSync,
   M68KMusashiGetDReg,
   M68KMusashiGetAReg,
   M68KMusashiGetPC,
   M68KMusashiGetSR,
   M68KMusashiGetUSP,
   M68KMusashiGetMSP,
   M68KMusashiSetDReg,
   M68KMusashiSetAReg,
   M68KMusashiSetPC,
   M68KMusashiSetSR,
   M68KMusashiSetUSP,
   M68KMusashiSetMSP,
   M68KMusashiSetFetch,
   M68KMusashiSetIRQ,
   M68KMusashiWriteNotify,
   M68KMusashiSetReadB,
   M68KMusashiSetReadW,
   M68KMusashiSetWriteB,
   M68KMusashiSetWriteW,
   M68KMusashiSaveState,
   M68KMusashiLoadState
};