/*  Copyright 2006 Theo Berkau

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

/*! \file bios.h
    \brief Header for emulated bios functions required for running games and saving backup ram.
*/

#ifndef BIOS_H
#define BIOS_H

#include "sh2core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char filename[12];
   char comment[11];
   u8 language;
   u8 year;
   u8 month;
   u8 day;
   u8 hour;
   u8 minute;
   u8 week;
   u32 date;
   u32 datasize;
   u16 blocksize;
} saveinfo_struct;

typedef struct
{
   u8 id;
   char name[32];
} deviceinfo_struct;

typedef struct
{
 u32 totalsize;
 u32 totalblock;
 u32 blocksize;
 u32 freesize;
 u32 freeblock;
 u32 datanum;
} devicestatus_struct;

void BiosInit(void);
int FASTCALL BiosHandleFunc(SH2_struct * sh);

// Virtual BUP library entry addresses used by the extend_backup hijack when
// a REAL BIOS is loaded. They must not collide with executable BIOS code:
// the previously used 0x380..0x3AC range is real code (the BIOS itself does
// "bsr 0x380" from its SYS_CHGSCUIM service at 0x200), so trapping it hung
// the master SH2 as soon as a game combined BUP_Init with SYS_CHGSCUIM
// (Area 51). This range sits inside the real BUP library body right after
// the BUP_Init entry point (0x7D600, already trapped) which is unreachable
// once BUP_Init itself is hijacked. The emulated BIOS keeps the classic
// 0x380-based entries (its ROM is empty, no collision).
#define BUP_TABLE_REAL_BASE 0x0007D604u
#define BUP_TABLE_REAL_LAST (BUP_TABLE_REAL_BASE + 0x2Cu)

deviceinfo_struct *BupGetDeviceList(int *numdevices);
int BupGetStats(u32 device, u32 *freespace, u32 *maxspace);
saveinfo_struct *BupGetSaveList(u32 device, int *numsaves);
int BupDeleteSave(u32 device, const char *savename);
void BupFormat(u32 device);
int BupCopySave(u32 srcdevice, u32 dstdevice, const char *savename);
int BupImportSave(u32 device, const char *filename);
int BupExportSave(u32 device, const char *savename, const char *filename);

void FASTCALL BiosBUPInit(SH2_struct * sh);

int BiosBUPImport( u32 device, saveinfo_struct * saveinfo, const char * buf, int bufsize );
int BiosBUPExport(u32 device, const char *savename, char ** buf, int * bufsize );
int BiosBUPStatusMem( int device, devicestatus_struct * status );

typedef void(*ON_BACKUP_WRITE_CALLBACK)(const char * fname, int deviceId, char * before, char * after, int size);
void BiosSetOnBackupWrite(ON_BACKUP_WRITE_CALLBACK cbk);


#ifdef __cplusplus
}
#endif


#endif //  BIOS_H

