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

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>
#include <stdint.h>

typedef union {
	uint8_t data[16];
	uint32_t data32[4];
} aes_ctr_old;  

#define AES_BIG_INPUT      1
#define AES_LITTLE_INPUT   0
#define AES_NORMAL_INPUT   4
#define AES_REVERSED_INPUT 0
#define AES_MAX_KEYSLOT	0x3F
#define AES_BLOCK_SIZE	0x10
/*
#define AES_CCM_DECRYPT_MODE (0 << 27)
#define AES_CCM_ENCRYPT_MODE (1 << 27)
#define AES_CTR_MODE         (2 << 27)
#define AES_CTR_MODE         (2 << 27)
#define AES_CBC_DECRYPT_MODE (4 << 27)
#define AES_CBC_ENCRYPT_MODE (5 << 27)
#define AES_ECB_DECRYPT_MODE (6 << 27)
#define AES_ECB_ENCRYPT_MODE (7 << 27)
*/
#define REG_AESCNT     ((volatile uint32_t*)0x10009000)
#define REG_AESBLKCNT  ((volatile uint32_t*)0x10009004)
#define REG_AESWRFIFO  ((volatile uint32_t*)0x10009008)
#define REG_AESRDFIFO  ((volatile uint32_t*)0x1000900C)
#define REG_AESKEYSEL  ((volatile uint8_t *)0x10009010)
#define REG_AESKEYCNT  ((volatile uint8_t *)0x10009011)
#define REG_AESCTR     ((volatile uint32_t*)0x10009020)
#define REG_AESKEYNFIFO ((volatile uint32_t*)0x10009100)
#define REG_AESKEYXFIFO ((volatile uint32_t*)0x10009104)
#define REG_AESKEYYFIFO ((volatile uint32_t*)0x10009108)
#define REG_AESKEYFIFO ((volatile uint32_t*)0x10009108)

#define AES_CNT_START         0x80000000
#define AES_CNT_UPDATE_KEYSLOT	0x04000000
#define AES_CNT_INPUT_ORDER   0x02000000
#define AES_CNT_OUTPUT_ORDER  0x01000000
#define AES_CNT_INPUT_ENDIAN  0x00800000
#define AES_CNT_OUTPUT_ENDIAN 0x00400000
#define AES_CNT_FLUSH_READ    0x00000800
#define AES_CNT_FLUSH_WRITE   0x00000400

void add_ctr(aes_ctr_old *ctr, uint32_t carry);
void setup_aeskeyN(uint_fast8_t keyslot, void* keyx);
void setup_aeskeyX(uint_fast8_t keyslot, void* keyx);
void decrypt(void* key, void* inbuf, void* outbuf, size_t size);
void setup_aeskey(uint_fast8_t keyno, int value, void* key);
void use_aeskey(uint_fast8_t keyno);
void set_ctr(int mode, aes_ctr_old *iv);
void aes_decrypt(void* inbuf, void* outbuf, size_t size, uint32_t mode);
void _decrypt(uint32_t value, void* inbuf, void* outbuf, size_t blocks);
void aes_fifos(void* inbuf, void* outbuf, size_t blocks);
void set_aeswrfifo(uint32_t value);
uint32_t read_aesrdfifo(void);
uint32_t aes_getwritecount();
uint32_t aes_getreadcount();
uint32_t aescnt_checkwrite();
uint32_t aescnt_checkread();

#define AES_CCM_DECRYPT_MODE	0
#define AES_CCM_ENCRYPT_MODE	0x08000000
#define AES_CTR_DECRYPT_MODE	0x10000000
#define AES_CTR_MODE		AES_CTR_DECRYPT_MODE
#define AES_CTR_ENCRYPT_MODE	0x18000000
#define AES_CBC_DECRYPT_MODE	0x20000000
#define AES_CBC_ENCRYPT_MODE	0x28000000
#define AES_ECB_DECRYPT_MODE	0x30000000
#define AES_ECB_ENCRYPT_MODE	0x38000000

#define AES_MODE	(AES_CCM_DECRYPT_MODE | AES_CCM_ENCRYPT_MODE | AES_CTR_DECRYPT_MODE | AES_CTR_ENCRYPT_MODE | AES_CBC_DECRYPT_MODE | AES_CBC_ENCRYPT_MODE | AES_ECB_DECRYPT_MODE | AES_ECB_ENCRYPT_MODE)

#define AES_CNT_INPUT_MODE	(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)
#define AES_CNT_OUTPUT_MODE	(AES_CNT_OUTPUT_ENDIAN | AES_CNT_OUTPUT_ORDER)

#define AES_CNT_OUTPUT_BE_NORMAL	(AES_CNT_OUTPUT_ENDIAN | AES_CNT_OUTPUT_ORDER)
#define AES_CNT_OUTPUT_BE_REVERSE	AES_CNT_OUTPUT_ENDIAN
#define AES_CNT_OUTPUT_LE_NORMAL	AES_CNT_OUTPUT_ORDER
#define AES_CNT_OUTPUT_LE_REVERSE	0
#define AES_CNT_INPUT_BE_NORMAL		(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)
#define AES_CNT_INPUT_BE_REVERSE	AES_CNT_INPUT_ENDIAN
#define AES_CNT_INPUT_LE_NORMAL		AES_CNT_INPUT_ORDER
#define AES_CNT_INPUT_LE_REVERSE	0

#define AES_KEYCNT_KEYGEN_3DS		0
#define AES_KEYCNT_KEYGEN_DSI		0x40
#define AES_KEYCNT_FIFO_FLUSH_ENABLE	0x80

#endif
