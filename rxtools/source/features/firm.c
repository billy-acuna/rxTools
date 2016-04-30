/*
 * Copyright (C) 2015 The PASTA Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <elf.h>
#include <reboot.h>
#include "firm.h"
#include "mpcore.h"
#include "hid.h"
#include "lang.h"
#include "console.h"
#include "fs.h"
#include "ncch.h"
#include "draw.h"
#include "menu.h"
#include "fileexplorer.h"
#include "CTRDecryptor.h"
#include "TitleKeyDecrypt.h"
#include "configuration.h"
#include "lang.h"
#include "crypto.h"
#include "aes.h"
#include "progress.h"

const wchar_t *firmPathFmt= L"" FIRM_PATH_FMT;
const wchar_t *firmPatchPathFmt = L"" FIRM_PATCH_PATH_FMT;

unsigned int emuNandMounted = 0;
_Noreturn void (* const _softreset)() = (void *)0x080F0000;

_Noreturn void execReboot(uint32_t, void *, uintptr_t, const Elf32_Shdr *);

static FRESULT loadExecReboot()
{
	FIL fd;
	FRESULT r;
	UINT br;

	r = f_open(&fd, L"" SYS_PATH "/reboot.bin", FA_READ);
	if (r != FR_OK)
		return r;

	r = f_read(&fd, (void*)0x080F0000, 0x8000, &br);
	if (r != FR_OK)
		return r;

	f_close(&fd);
	_softreset();
}

static int loadFirm(wchar_t *path, UINT *fsz)
{
	FIL fd;
	FRESULT r;

	r = f_open(&fd, path, FA_READ);
	if (r != FR_OK)
		return r;

	r = f_read(&fd, (void *)FIRM_ADDR, f_size(&fd), fsz);
	if (r != FR_OK)
		return r;

	f_close(&fd);

	return ((FirmHdr *)FIRM_ADDR)->magic == 'MRIF' ? 0 : -1;
}

static int decryptFirmKtrArm9(void *p)
{
	uint8_t key[AES_BLOCK_SIZE];
	PartitionInfo info;
	Arm9Hdr *hdr;
	FirmSeg *seg, *btm;

	seg = ((FirmHdr *)p)->segs;
	for (btm = seg + FIRM_SEG_NUM; seg->isArm11; seg++)
		 if (seg == btm)
			 return -1;

	hdr = (void *)(p + seg->offset);

	info.ctr = &hdr->ctr;
	info.buffer = (uint8_t *)hdr + 0x800;
	info.keyY = hdr->keyY;
	info.size = atoi(hdr->size);

	use_aeskey(0x11);
	if (hdr->ext.pad[0] == 0xFFFFFFFF) {
		info.keyslot = 0x15;
		aes_decrypt(hdr->keyX, key, 1, AES_ECB_DECRYPT_MODE);
		setup_aeskeyX(info.keyslot, key);
	} else {
		info.keyslot = 0x16;
		aes_decrypt(hdr->ext.s.keyX_0x16, key, 1, AES_ECB_DECRYPT_MODE);
	}

	return DecryptPartition(&info);
}

uint8_t* decryptFirmTitleNcch(uint8_t* title, size_t *size)
{
	ctr_ncchheader NCCH = *((ctr_ncchheader*)title);
	aes_ctr ctr;
	if (NCCH.magic != 'HCCN') return NULL;
	ncch_get_counter(&NCCH, &ctr, NCCHTYPE_EXEFS);
	PartitionInfo INFO = {title + NCCH.exefsoffset * NCCH_MEDIA_UNIT_SIZE, NCCH.signature, (aes_ctr_old*)&ctr.data, NCCH.exefssize * NCCH_MEDIA_UNIT_SIZE, 0x2C};
//	DecryptPartition(&INFO);

	aes_set_key(&(aes_key){(aes_key_data*)NCCH.signature, AES_CNT_INPUT_BE_NORMAL, 0x2C, KEYY});
	aes(title + NCCH.exefsoffset * NCCH_MEDIA_UNIT_SIZE, title + NCCH.exefsoffset * NCCH_MEDIA_UNIT_SIZE, NCCH.exefssize * NCCH_MEDIA_UNIT_SIZE, &ctr, AES_CTR_DECRYPT_MODE | AES_CNT_INPUT_BE_NORMAL | AES_CNT_OUTPUT_BE_NORMAL);

	if (size != NULL)
		*size = INFO.size - sizeof(ctr_ncchheader);

	uint8_t* firm = (uint8_t*)(INFO.buffer + sizeof(ctr_ncchheader));

	if (getMpInfo() == MPINFO_KTR)
	    if (decryptFirmKtrArm9(firm))
			return NULL;
	return firm;
}

uint8_t *decryptFirmTitle(uint8_t *title, size_t size, size_t *firmSize, uint8_t key[16])
{
/*
	aes_ctr_old ctr __attribute__((aligned(32))) = {0};
	setup_aeskeyN(0x2C, key);
	use_aeskey(0x2C);
	set_ctr(AES_BIG_INPUT | AES_NORMAL_INPUT, &ctr);
	ctr = *(aes_ctr_old*)(title + size - AES_BLOCK_SIZE);
	aes_decrypt(title, title, (size + AES_BLOCK_SIZE - 1)/AES_BLOCK_SIZE, AES_CBC_DECRYPT_MODE);
*/
	aes_ctr ctr = {{{0}}, AES_CNT_INPUT_BE_NORMAL};
	aes_set_key(&(aes_key){(aes_key_data*)key, AES_CNT_INPUT_BE_NORMAL, 0x2C, NORMALKEY});
	aes(title, title, size, &ctr, AES_CBC_DECRYPT_MODE | AES_CNT_INPUT_BE_NORMAL | AES_CNT_OUTPUT_BE_NORMAL);

	return decryptFirmTitleNcch(title, firmSize);
}

static void setAgbBios()
{
	File agb_firm;
	wchar_t path[_MAX_LFN];
	unsigned char svc = (cfgs[CFG_AGB_BIOS].val.i ? 0x26 : 0x01);

	getFirmPath(path, TID_CTR_AGB_FIRM);
	if (FileOpen(&agb_firm, path, 0))
	{
		FileWrite(&agb_firm, &svc, 1, 0xD7A12);
		FileClose(&agb_firm);
	}
}

int rxMode(int_fast8_t drive)
{
	wchar_t path[64];
	const char *shstrtab;
	const wchar_t *msg;
	uint8_t keyx[16];
	uint32_t tid;
	int r, sector;
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr, *btm;
	void *keyxArg;
	FIL fd;
	UINT br, fsz;

	if (drive > 1) {
		sector = checkEmuNAND();
		if (sector == 0) {
			ConsoleInit();
			ConsoleSetTitle(L"EMUNAND NOT FOUND!");
			print(L"The emunand was not found on\n");
			print(L"your SDCard. \n");
			print(L"\n");
			print(L"Press A to boot SYSNAND\n");
			ConsoleShow();

			WaitForButton(keys[KEY_A].mask);

//			swprintf(path, _MAX_LFN, L"/rxTools/Theme/%u/boot.bin",
//				cfgs[CFG_THEME].val.i);
//			DrawSplash(&bottomScreen, path);
		}
	} else
		sector = 0;

	r = getMpInfo();
	switch (r) {
		case MPINFO_KTR:
			tid = TID_KTR_NATIVE_FIRM;
			break;

		case MPINFO_CTR:
			tid = TID_CTR_NATIVE_FIRM;
			break;

		default:
			msg = L"Unknown Platform: %d";
			goto fail;
	}

	setAgbBios();

	if (sysver < 7 && f_open(&fd, L"slot0x25KeyX.bin", FA_READ) == FR_OK) {
		f_read(&fd, keyx, sizeof(keyx), &br);
		f_close(&fd);
		keyxArg = keyx;
	} else
		keyxArg = NULL;

	getFirmPath(path, tid);
	r = loadFirm(path, &fsz);
	if (r) {
		msg = L"Failed to load NATIVE_FIRM: %d\n"
			L"Reboot rxTools and try again.\n";
		goto fail;
	}

	((FirmHdr *)FIRM_ADDR)->arm9Entry = 0x0801B01C;

	getFirmPatchPath(path, tid);
	r = f_open(&fd, path, FA_READ);
	if (r != FR_OK)
		goto patchFail;

	r = f_read(&fd, (void *)PATCH_ADDR, PATCH_SIZE, &br);
	if (r != FR_OK)
		goto patchFail;

	f_close(&fd);

	ehdr = (void *)PATCH_ADDR;
	shdr = (void *)(PATCH_ADDR + ehdr->e_shoff);
	shstrtab = (char *)PATCH_ADDR + shdr[ehdr->e_shstrndx].sh_offset;
	for (btm = shdr + ehdr->e_shnum; shdr != btm; shdr++) {
		if (!strcmp(shstrtab + shdr->sh_name, ".patch.p9.reboot.body")) {
			execReboot(sector, keyxArg, ehdr->e_entry, shdr);
			__builtin_unreachable();
		}
	}

	msg = L".patch.p9.reboot.body not found\n"
		L"Please check your installation.\n";
fail:
	ConsoleInit();
	ConsoleSetTitle(L"rxMode");
	print(msg, r);
	print(L"\n");
	print(strings[STR_PRESS_BUTTON_ACTION],
		strings[STR_BUTTON_A], strings[STR_CONTINUE]);
	ConsoleShow();
	WaitForButton(keys[KEY_A].mask);

	return r;

patchFail:
	msg = L"Failed to load the patch: %d\n"
		L"Check your installation.\n";
	goto fail;
}

void rxModeWithSplash(int_fast8_t drive)
{
//	wchar_t s[_MAX_LFN];

//	swprintf(s, _MAX_LFN, L"/rxTools/Theme/%u/boot.bin",
//		cfgs[CFG_THEME].val.i);
//	DrawSplash(&bottomScreen, s);
	rxMode(drive);
}

//Just patches signatures check, loads in sysnand
int PastaMode(){
	/*PastaMode is ready for n3ds BUT there's an unresolved bug which affects nand reading functions, like nand_readsectors(0, 0xF0000 / 0x200, firm, FIRM0);*/

	uint8_t* firm = (void*)FIRM_ADDR;
	nand_readsectors(0, 0xF0000 / 0x200, firm, FIRM0);
	if (strncmp((char*)firm, "FIRM", 4))
		nand_readsectors(0, 0xF0000 / 0x200, firm, FIRM1);

	if(getMpInfo() == MPINFO_CTR)
	{
		//o3ds patches
		unsigned char sign1[] = { 0xC1, 0x17, 0x49, 0x1C, 0x31, 0xD0, 0x68, 0x46, 0x01, 0x78, 0x40, 0x1C, 0x00, 0x29, 0x10, 0xD1 };
		unsigned char sign2[] = { 0xC0, 0x1C, 0x76, 0xE7, 0x20, 0x00, 0x74, 0xE7, 0x22, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
		unsigned char patch1[] = { 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD };
		unsigned char patch2[] = { 0x00, 0x20 };

		for (int i = 0; i < 0xF0000; i++){
			if (!memcmp(firm + i, sign1, 16)){
				memcpy(firm + i, patch1, 6);
			}
			if (!memcmp(firm + i, sign2, 16)){
				memcpy(firm + i, patch2, 2);
			}
		}
	}
	else
	{
		//new 3ds patches
		decryptFirmKtrArm9((void *)FIRM_ADDR);
		uint8_t patch0[] = { 0x00, 0x20, 0x3B, 0xE0 };
        uint8_t patch1[] = { 0x00, 0x20, 0x08, 0xE0 };
        memcpy((uint32_t*)(FIRM_ADDR + 0xB39D8), patch0, 4);
		memcpy((uint32_t*)(FIRM_ADDR + 0xB9204), patch1, 4);
	}

	return loadExecReboot();
}

void FirmLoader(wchar_t *firm_path){

	UINT fsz;
	if (loadFirm(firm_path, &fsz))
	{
		ConsoleInit();
		ConsoleSetTitle(strings[STR_LOAD], strings[STR_FIRMWARE_FILE]);
		print(strings[STR_WRONG], L"", strings[STR_FIRMWARE_FILE]);
		print(strings[STR_PRESS_BUTTON_ACTION], strings[STR_BUTTON_A], strings[STR_CONTINUE]);
		ConsoleShow();
		WaitForButton(keys[KEY_A].mask);
		return;
	}
		if (loadExecReboot())
	{
		ConsoleInit();
		ConsoleSetTitle(strings[STR_LOAD], strings[STR_FIRMWARE_FILE]);
		print(strings[STR_ERROR_LAUNCHING], strings[STR_FIRMWARE_FILE]);
		print(strings[STR_PRESS_BUTTON_ACTION], strings[STR_BUTTON_A], strings[STR_CONTINUE]);
		ConsoleShow();
		WaitForButton(keys[KEY_A].mask);
		return;
	}
}
