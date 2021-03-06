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

#include "i2c.h"

//-----------------------------------------------------------------------------

static const struct { uint_fast8_t bus_id, reg_addr; } dev_data[] = {
    {0, 0x4A}, {0, 0x7A}, {0, 0x78},
    {1, 0x4A}, {1, 0x78}, {1, 0x2C},
    {1, 0x2E}, {1, 0x40}, {1, 0x44},
    {2, 0xD6}, {2, 0xD0}, {2, 0xD2},
    {2, 0xA4}, {2, 0x9A}, {2, 0xA0},
};

const inline uint_fast8_t i2cGetDeviceBusId(uint_fast8_t device_id) {
    return dev_data[device_id].bus_id;
}

const inline uint_fast8_t i2cGetDeviceRegAddr(uint_fast8_t device_id) {
    return dev_data[device_id].reg_addr;
}

//-----------------------------------------------------------------------------

static volatile uint8_t* const reg_data_addrs[] = {
    (volatile uint8_t*)(I2C1_REG_OFF + I2C_REG_DATA),
    (volatile uint8_t*)(I2C2_REG_OFF + I2C_REG_DATA),
    (volatile uint8_t*)(I2C3_REG_OFF + I2C_REG_DATA),
};

inline volatile uint8_t* const i2cGetDataReg(uint_fast8_t bus_id) {
    return reg_data_addrs[bus_id];
}

//-----------------------------------------------------------------------------

static volatile uint8_t* const reg_cnt_addrs[] = {
    (volatile uint8_t*)(I2C1_REG_OFF + I2C_REG_CNT),
    (volatile uint8_t*)(I2C2_REG_OFF + I2C_REG_CNT),
    (volatile uint8_t*)(I2C3_REG_OFF + I2C_REG_CNT),
};

inline volatile uint8_t* const i2cGetCntReg(uint_fast8_t bus_id) {
    return reg_cnt_addrs[bus_id];
}

//-----------------------------------------------------------------------------

inline void i2cWaitBusy(uint_fast8_t bus_id) {
    while (*i2cGetCntReg(bus_id) & 0x80);
}

inline uint_fast8_t i2cGetResult(uint_fast8_t bus_id) {
    i2cWaitBusy(bus_id);
    return (*i2cGetCntReg(bus_id) >> 4) & 1;
}

void i2cStop(uint_fast8_t bus_id, uint_fast8_t arg0) {
    *i2cGetCntReg(bus_id) = (arg0 << 5) | 0xC0;
    i2cWaitBusy(bus_id);
    *i2cGetCntReg(bus_id) = 0xC5;
}

//-----------------------------------------------------------------------------

uint_fast8_t i2cSelectDevice(uint_fast8_t bus_id, uint_fast8_t dev_reg) {
    i2cWaitBusy(bus_id);
    *i2cGetDataReg(bus_id) = dev_reg;
    *i2cGetCntReg(bus_id) = 0xC2;
    return i2cGetResult(bus_id);
}

uint_fast8_t i2cSelectRegister(uint_fast8_t bus_id, uint_fast8_t reg) {
    i2cWaitBusy(bus_id);
    *i2cGetDataReg(bus_id) = reg;
    *i2cGetCntReg(bus_id) = 0xC0;
    return i2cGetResult(bus_id);
}

//-----------------------------------------------------------------------------

uint_fast8_t i2cReadRegister(uint_fast8_t dev_id, uint_fast8_t reg) {
    uint_fast8_t bus_id = i2cGetDeviceBusId(dev_id);
    uint_fast8_t dev_addr = i2cGetDeviceRegAddr(dev_id);

    for (size_t i = 8; i--;) {
        if (i2cSelectDevice(bus_id, dev_addr) && i2cSelectRegister(bus_id, reg)) {
            if (i2cSelectDevice(bus_id, dev_addr | 1)) {
                i2cWaitBusy(bus_id);
                i2cStop(bus_id, 1);
                i2cWaitBusy(bus_id);
                return *i2cGetDataReg(bus_id);
            }
        }
        *i2cGetCntReg(bus_id) = 0xC5;
        i2cWaitBusy(bus_id);
    }
    return 0xff;
}

uint_fast8_t i2cReadRegisterBuffer(uint_fast8_t dev_id, uint_fast8_t reg, uint8_t* buffer, size_t buf_size) {
    uint_fast8_t bus_id = i2cGetDeviceBusId(dev_id);
    uint_fast8_t dev_addr = i2cGetDeviceRegAddr(dev_id);

    size_t j = 0;
    while (!i2cSelectDevice(bus_id, dev_addr)
        || !i2cSelectRegister(bus_id, reg)
        || !i2cSelectDevice(bus_id, dev_addr | 1))
    {
        i2cWaitBusy(bus_id);
        *i2cGetCntReg(bus_id) = 0xC5;
        i2cWaitBusy(bus_id);
        if (++j >= 8)
            return 0;
    }

    if (buf_size != 1) {
        for (size_t i = 0; i < buf_size - 1; i++) {
            i2cWaitBusy(bus_id);
            *i2cGetCntReg(bus_id) = 0xF0;
            i2cWaitBusy(bus_id);
            buffer[i] = *i2cGetDataReg(bus_id);
          }
    }

    i2cWaitBusy(bus_id);
    *i2cGetCntReg(bus_id) = 0xE1;
    i2cWaitBusy(bus_id);
    *buffer = *i2cGetDataReg(bus_id);
    return 1;
}

uint_fast8_t i2cWriteRegister(uint_fast8_t dev_id, uint_fast8_t reg, uint_fast8_t data) {
    uint_fast8_t bus_id = i2cGetDeviceBusId(dev_id);
    uint_fast8_t dev_addr = i2cGetDeviceRegAddr(dev_id);

    for (size_t i = 8; i--;) {
        if (i2cSelectDevice(bus_id, dev_addr) && i2cSelectRegister(bus_id, reg)) {
            i2cWaitBusy(bus_id);
            *i2cGetDataReg(bus_id) = data;
            *i2cGetCntReg(bus_id) = 0xC1;
            i2cStop(bus_id, 0);
            if (i2cGetResult(bus_id))
                return 1;
        }
        *i2cGetCntReg(bus_id) = 0xC5;
        i2cWaitBusy(bus_id);
    }

    return 0;
}