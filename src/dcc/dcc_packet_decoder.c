/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file dcc_packet_decoder.c
 * @brief DCC packet parsing, XOR validation, address matching, and callback
 * dispatch for decoders.
 *
 * @author Jim Kueneman
 * @date 11 Apr 2026
 */

#include "dcc_packet_decoder.h"

#ifdef DCC_COMPILE_DECODER


// =============================================================================
// Static state
// =============================================================================

static const interface_dcc_packet_decoder_t *_interface;
static dcc_address_t _my_address;
static dcc_address_type_enum _my_address_type;
    /** @brief Cached CV29 bit 1: true = 28/128-step, false = 14-step. */
static bool _use_28_step;

    /** @brief Cached CV29 bit 0: true = direction reversed. */
static bool _direction_reversed;

    /** @brief Cached CV29 bit 5: true = use CV17/CV18 long address. */
static bool _use_extended_address;

    /** @brief Cached CV541 bit 6: true = output-address mode for accessory. */
static bool _use_output_address;

    /** @brief Consecutive reset packet counter for service mode detection. */
static uint8_t _reset_count;

    /** @brief True when enough consecutive resets have been seen to accept
     *  service mode packets (per NMRA S-9.2.3). */
static bool _service_mode_active;

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Read CV17/CV18 and cache the long address.
     */
static void _update_extended_address(void) {

    uint8_t high_byte;
    uint8_t low_byte;

    if (_interface->cv_read(DCC_CV_EXTENDED_ADDRESS_HIGH, &high_byte) &&
        _interface->cv_read(DCC_CV_EXTENDED_ADDRESS_LOW, &low_byte)) {

        _my_address = ((uint16_t)(high_byte & 0x3F) << 8) | low_byte;
        _my_address_type = DCC_ADDRESS_LONG;

    }

}

    /**
     * @brief Read CV1 and cache the short address.
     */
static void _update_primary_address(void) {

    uint8_t address_byte;

    if (_interface->cv_read(DCC_CV_PRIMARY_ADDRESS, &address_byte)) {

        _my_address = address_byte;
        _my_address_type = DCC_ADDRESS_SHORT;

    }

}

    /**
     * @brief Read CV513/CV521/CV541 and cache the accessory decoder address.
     *
     * @details Two addressing modes selected by CV541 bit 6:
     *
     * Decoder-address (bit 6 = 0):
     * -# CV513 bits 0-5 = 6 LSBs of 9-bit board address
     * -# CV521 bits 0-2 = 3 MSBs of 9-bit board address
     * -# 512 board addresses, each with 4 output pairs
     *
     * Output-address (bit 6 = 1):
     * -# Accessory-Output = (CV513 + CV521 * 256) - 1
     * -# 11-bit flat output address (0-2047)
     * -# Per NMRA S-9.2.2, factory default CV513=1, CV521=0 => output 0
     */
static void _update_accessory_address(void) {

    uint8_t cv541_value;
    uint8_t address_low_byte;
    uint8_t address_high_byte;

    if (_interface->cv_read(DCC_CV_ACC_CONFIG, &cv541_value)) {

        _my_address_type = (cv541_value & DCC_CV541_BASIC_EXTENDED_BIT)
            ? DCC_ADDRESS_ACCESSORY_EXTENDED
            : DCC_ADDRESS_ACCESSORY;
        _use_output_address = (cv541_value & DCC_CV541_ADDRESS_METHOD_BIT)
            ? true
            : false;

    }

    if (_interface->cv_read(DCC_CV_ACC_ADDRESS_LSB, &address_low_byte) &&
        _interface->cv_read(DCC_CV_ACC_ADDRESS_MSB, &address_high_byte)) {

        if (_use_output_address) {

            uint16_t raw = ((uint16_t)address_high_byte << 8) | address_low_byte;
            _my_address = (raw > 0) ? (raw - 1) : 0;

        } else {

            _my_address = ((uint16_t)(address_high_byte & 0x07) << 6) | (address_low_byte & 0x3F);

        }

    }

}

    /**
     * @brief Read CV541 to determine multi-function vs accessory, then cache
     *        the appropriate address and configuration CVs.
     *
     * @details Algorithm:
     * -# If cv_read is unavailable, set safe multi-function defaults
     * -# Read CV541; if bit 7 is set this is an accessory decoder
     * -# Accessory path: read CV513/CV521 for address, CV541 bit 5 for type
     * -# Multi-function path: read CV29 for speed mode, direction, address mode
     * -# Multi-function extended address: read CV17/CV18
     * -# Multi-function short address: read CV1
     */
static void _update_address_cv_cache(void) {

    uint8_t cv541_value;
    uint8_t cv29_value;

    if (!_interface->cv_read) {

        _use_28_step = true;
        _direction_reversed = false;
        _use_extended_address = false;
        return;

    }

    /* Check CV541 to determine decoder class */
    if (_interface->cv_read(DCC_CV_ACC_CONFIG, &cv541_value) &&
        (cv541_value & DCC_CV541_ACCESSORY_DECODER_BIT)) {

        _update_accessory_address();
        return;

    }

    /* Multi-function decoder: read CV29 */
    if (!_interface->cv_read(DCC_CV_CONFIG, &cv29_value)) {

        _use_28_step = true;
        _direction_reversed = false;
        _use_extended_address = false;
        return;

    }

    _use_28_step = (cv29_value & DCC_CV29_SPEED_STEPS_BIT) ? true : false;
    _direction_reversed = (cv29_value & DCC_CV29_DIRECTION_BIT) ? true : false;
    _use_extended_address = (cv29_value & DCC_CV29_EXTENDED_ADDRESS_BIT) ? true : false;

    if (_use_extended_address) {

        _update_extended_address();

    } else {

        _update_primary_address();

    }

}

    /**
     * @brief Validate XOR error detection byte.
     * @param data Raw packet bytes.
     * @param byte_count Number of bytes.
     * @return true if XOR check passes (last byte == XOR of all preceding).
     */
static bool _validate_xor(const uint8_t *data, uint8_t byte_count) {

    uint8_t xor_byte = 0;
    uint8_t byte_index;

    if (byte_count < 2) {

        return false;

    }

    for (byte_index = 0; byte_index < byte_count - 1; byte_index++) {

        xor_byte ^= data[byte_index];

    }

    return (xor_byte == data[byte_count - 1]);

}

    /**
     * @brief Dispatch a 128-step speed command.
     * @param address DCC address.
     * @param speed_byte Second instruction byte (direction + speed).
     */
static void _dispatch_speed_128(uint16_t address, uint8_t speed_byte) {

    bool direction = (speed_byte & 0x80) ? true : false;
    direction = (direction != _direction_reversed);
    uint8_t speed = speed_byte & 0x7F;

    if (speed == DCC_SPEED_128_ESTOP) {

        if (_interface->on_emergency_stop) {

            _interface->on_emergency_stop(address);

        }

        return;

    }

    if (_interface->on_speed_command) {

        _interface->on_speed_command(address, speed, direction, DCC_SPEED_MODE_128);

    }

}

    /**
     * @brief 28-step decode lookup table per NMRA S-9.2 Figure 2.
     *
     * Index = encoded value ((SSSS << 1) | C), range 0-31.
     * Value = API speed (0 = stop, 0xFF = e-stop, 2-29 = steps 1-28).
     */
#define DCC_SPEED_28_STOP   0
#define DCC_SPEED_28_ESTOP  0xFF

static const uint8_t _speed_28_decode[32] = {

    DCC_SPEED_28_STOP,  DCC_SPEED_28_STOP,                          /* encoded  0-1:  Stop     */
    DCC_SPEED_28_ESTOP, DCC_SPEED_28_ESTOP,                         /* encoded  2-3:  E-Stop   */
     2,  3,  4,  5,  6,  7,  8,  9,                        /* encoded  4-11: Step 1-8 */
    10, 11, 12, 13, 14, 15, 16, 17,                        /* encoded 12-19: Step 9-16 */
    18, 19, 20, 21, 22, 23, 24, 25,                        /* encoded 20-27: Step 17-24 */
    26, 27, 28, 29                                          /* encoded 28-31: Step 25-28 */

};

    /**
     * @brief Dispatch a 28-step speed command.
     * @param address DCC address.
     * @param instruction The speed/direction instruction byte (01Dxxxxx).
     */
static void _dispatch_speed_28(uint16_t address, uint8_t instruction) {

    bool direction = (instruction & 0x20) ? true : false;
    direction = (direction != _direction_reversed);
    uint8_t speed_c = (instruction >> 4) & 0x01;
    uint8_t speed_ssss = instruction & 0x0F;
    uint8_t encoded = (speed_ssss << 1) | speed_c;
    uint8_t speed = _speed_28_decode[encoded];

    if (speed == DCC_SPEED_28_ESTOP) {

        if (_interface->on_emergency_stop) {

            _interface->on_emergency_stop(address);

        }

    } else {

        if (_interface->on_speed_command) {

            _interface->on_speed_command(address, speed, direction, DCC_SPEED_MODE_28);

        }

    }

}

    /**
     * @brief Dispatch a 14-step speed command.
     * @param address DCC address.
     * @param instruction The speed/direction instruction byte (01Dxxxxx).
     */
static void _dispatch_speed_14(uint16_t address, uint8_t instruction) {

    bool direction = (instruction & 0x20) ? true : false;
    direction = (direction != _direction_reversed);
    uint8_t speed = instruction & 0x0F;

    if (speed == 1) {

        /* E-Stop */
        if (_interface->on_emergency_stop) {

            _interface->on_emergency_stop(address);

        }

    } else {

        /* Stop (0) and steps 1-14 (2-15) pass through directly */
        if (_interface->on_speed_command) {

            _interface->on_speed_command(address, speed, direction, DCC_SPEED_MODE_14);

        }

    }

}

    /**
     * @brief Dispatch a 14 or 28-step speed command.
     * @param address DCC address.
     * @param instruction The speed/direction instruction byte (01Dxxxxx).
     */
static void _dispatch_speed_14_28(uint16_t address, uint8_t instruction) {

    if (_use_28_step) {

        _dispatch_speed_28(address, instruction);

    } else {

        _dispatch_speed_14(address, instruction);

    }

}

    /**
     * @brief Dispatch function group callbacks.
     * @param address DCC address.
     * @param start_function First function number in this group.
     * @param count Number of functions in this group.
     * @param bits Bitmask of function states (bit 0 = first function).
     */
static void _dispatch_functions(uint16_t address, uint8_t start_function, uint8_t count, uint8_t bits) {

    uint8_t function_index;

    if (!_interface->on_function_command) {

        return;

    }

    for (function_index = 0; function_index < count; function_index++) {

        _interface->on_function_command(address, start_function + function_index, (bits & (1 << function_index)) ? true : false);

    }

}

    /**
     * @brief Dispatch a function group 1 instruction (FL, F1-F4).
     * @param address DCC address.
     * @param instruction The function group 1 byte (100DDDDD).
     */
static void _dispatch_func_group1(uint16_t address, uint8_t instruction) {

    uint8_t bits;

    if (!_interface->on_function_command) {

        return;

    }

    /* FL is in bit 4, F1-F4 are in bits 0-3 */
    _interface->on_function_command(address, 0, (instruction & 0x10) ? true : false);

    bits = instruction & 0x0F;
    _dispatch_functions(address, 1, 4, bits);

}

    /**
     * @brief Fire an ACK pulse if the interface supports it.
     */
static void _fire_ack(void) {

    if (_interface->fire_ack_pulse) {

        _interface->fire_ack_pulse();

    }

}

    /**
     * @brief Write a CV value and fire notifications.
     * @param cv_number 1-based CV number.
     * @param data_byte Value to write.
     * @param is_service_mode true to fire ACK pulse on success.
     * @return true if the write succeeded.
     */
static bool _cv_write_and_notify(uint16_t cv_number, uint8_t data_byte, bool is_service_mode) {

    if (!_interface->cv_write) {

        return false;

    }

    if (!_interface->cv_write(cv_number, data_byte)) {

        return false;

    }

    if (is_service_mode) {

        _fire_ack();

    }

    if (cv_number == DCC_CV_CONFIG ||
        cv_number == DCC_CV_PRIMARY_ADDRESS ||
        cv_number == DCC_CV_EXTENDED_ADDRESS_HIGH ||
        cv_number == DCC_CV_EXTENDED_ADDRESS_LOW ||
        cv_number == DCC_CV_ACC_CONFIG ||
        cv_number == DCC_CV_ACC_ADDRESS_LSB ||
        cv_number == DCC_CV_ACC_ADDRESS_MSB) {

        _update_address_cv_cache();

    }

    if (_interface->on_cv_write) {

        _interface->on_cv_write(cv_number, data_byte, is_service_mode);

    }

    return true;

}

    /**
     * @brief Read-modify-write a single CV bit.
     * @param cv_number 1-based CV number.
     * @param bit_position Bit position (0-7).
     * @param bit_value Desired bit value.
     * @param is_service_mode true to fire ACK pulse on success.
     */
static void _cv_bit_manipulate(uint16_t cv_number, uint8_t bit_position, bool bit_value, bool is_service_mode) {

    uint8_t current_value;

    if (!_interface->cv_write || !_interface->cv_read) {

        return;

    }

    if (!_interface->cv_read(cv_number, &current_value)) {

        return;

    }

    if (bit_value) {

        current_value |= (1 << bit_position);

    } else {

        current_value &= ~(1 << bit_position);

    }

    _cv_write_and_notify(cv_number, current_value, is_service_mode);

}

    /**
     * @brief Dispatch CV access long form instruction.
     * @param address DCC address (for callback).
     * @param instruction_bytesInstruction bytes pointer.
     * @param instruction_byte_count Number of instruction bytes.
     */
static void _dispatch_cv_access(uint16_t address, const uint8_t *instruction_bytes, uint8_t instruction_byte_count) {

    uint8_t cv_command;
    uint16_t cv_number;
    uint8_t data_byte;

    if (instruction_byte_count < 3) {

        return;

    }

    cv_command = instruction_bytes[0] & 0x0C;
    cv_number = (uint16_t)((instruction_bytes[0] & 0x03) << 8) | instruction_bytes[1];
    cv_number += 1;  /* Convert 0-based wire encoding to 1-based CV number */
    data_byte = instruction_bytes[2];

    if (cv_command == 0x0C) {

        /* Write: 111011AA */
        _cv_write_and_notify(cv_number, data_byte, false);

    } else if (cv_command == 0x08) {

        /* Bit manipulation: 111010AA, data = 111KDBBB */
        uint8_t bit_position = data_byte & 0x07;
        bool bit_value = (data_byte & 0x08) ? true : false;
        bool is_write = (data_byte & 0x10) ? true : false;

        if (is_write) {

            _cv_bit_manipulate(cv_number, bit_position, bit_value, false);

        }

        if (_interface->on_cv_bit) {

            _interface->on_cv_bit(cv_number, bit_position, bit_value, false);

        }

    } else if (cv_command == 0x04) {

        /* Verify: 111001AA — notify application, ACK is hardware-specific */
        if (_interface->on_cv_verify) {

            _interface->on_cv_verify(cv_number, data_byte, false);

        }

    }

}

    /**
     * @brief Write a CV value and fire accessory-specific notification.
     * @param cv_number 1-based CV number.
     * @param data_byte Value to write.
     * @return true if the write succeeded.
     */
static bool _cv_write_and_notify_acc(uint16_t cv_number, uint8_t data_byte) {

    if (!_interface->cv_write) {

        return false;

    }

    if (!_interface->cv_write(cv_number, data_byte)) {

        return false;

    }

    if (cv_number == DCC_CV_CONFIG ||
        cv_number == DCC_CV_PRIMARY_ADDRESS ||
        cv_number == DCC_CV_EXTENDED_ADDRESS_HIGH ||
        cv_number == DCC_CV_EXTENDED_ADDRESS_LOW ||
        cv_number == DCC_CV_ACC_CONFIG ||
        cv_number == DCC_CV_ACC_ADDRESS_LSB ||
        cv_number == DCC_CV_ACC_ADDRESS_MSB) {

        _update_address_cv_cache();

    }

    if (_interface->on_acc_cv_write) {

        _interface->on_acc_cv_write(cv_number, data_byte);

    }

    return true;

}

    /**
     * @brief Read-modify-write a single CV bit (accessory path).
     * @param cv_number 1-based CV number.
     * @param bit_position Bit position (0-7).
     * @param bit_value Desired bit value.
     */
static void _cv_bit_manipulate_acc(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    uint8_t current_value;

    if (!_interface->cv_write || !_interface->cv_read) {

        return;

    }

    if (!_interface->cv_read(cv_number, &current_value)) {

        return;

    }

    if (bit_value) {

        current_value |= (1 << bit_position);

    } else {

        current_value &= ~(1 << bit_position);

    }

    _cv_write_and_notify_acc(cv_number, current_value);

}

    /**
     * @brief Dispatch accessory CV access long form instruction.
     * @param instruction_bytesInstruction bytes pointer (starting at CV instruction).
     * @param instruction_byte_count Number of instruction bytes.
     */
static void _dispatch_acc_cv_access(const uint8_t *instruction_bytes, uint8_t instruction_byte_count) {

    uint8_t cv_command;
    uint16_t cv_number;
    uint8_t data_byte;

    if (instruction_byte_count < 3) {

        return;

    }

    cv_command = instruction_bytes[0] & 0x0C;
    cv_number = (uint16_t)((instruction_bytes[0] & 0x03) << 8) | instruction_bytes[1];
    cv_number += 1;  /* Convert 0-based wire encoding to 1-based CV number */
    data_byte = instruction_bytes[2];

    if (cv_command == 0x0C) {

        /* Write: 111011AA */
        _cv_write_and_notify_acc(cv_number, data_byte);

    } else if (cv_command == 0x08) {

        /* Bit manipulation: 111010AA, data = 111KDBBB */
        uint8_t bit_position = data_byte & 0x07;
        bool bit_value = (data_byte & 0x08) ? true : false;
        bool is_write = (data_byte & 0x10) ? true : false;

        if (is_write) {

            _cv_bit_manipulate_acc(cv_number, bit_position, bit_value);

        }

        if (_interface->on_acc_cv_bit) {

            _interface->on_acc_cv_bit(cv_number, bit_position, bit_value);

        }

    } else if (cv_command == 0x04) {

        /* Verify: 111001AA — notify application */
        if (_interface->on_acc_cv_verify) {

            _interface->on_acc_cv_verify(cv_number, data_byte);

        }

    }

}

    /**
     * @brief Dispatch a basic accessory instruction.
     * @param data Raw packet bytes.
     * @param byte_count Number of bytes.
     */
static void _dispatch_accessory_basic(const uint8_t *data, uint8_t byte_count) {

    uint16_t board_address;
    uint8_t output_pair;
    bool activate;
    uint8_t addr_low;
    uint8_t addr_high_inv;

    if (byte_count < 3) {

        return;

    }

    /* CV access long form (ops-mode): 6-byte packet, byte 2 starts with 1110 */
    if (byte_count == 6 && (data[2] & 0xF0) == 0xE0) {

        uint8_t cv_addr_low = data[0] & 0x3F;
        uint8_t cv_addr_high_inv = (data[1] >> 4) & 0x07;
        uint16_t cv_board_address = (uint16_t)cv_addr_low |
                    ((uint16_t)(~cv_addr_high_inv & 0x07) << 6);

        if (_use_output_address) {

            /* A1=bit2, A0=bit1 per S-9.2.1 2025 notation */
            uint16_t cv_output_address = (cv_board_address << 2) |
                        ((data[1] >> 1) & 0x03);

            if (cv_output_address != _my_address) {

                return;

            }

        } else {

            if (cv_board_address != _my_address) {

                return;

            }

        }

        _dispatch_acc_cv_access(&data[2], 3);
        return;

    }

    /* Byte 0: 10AAAAAA — low 6 address bits (A2-A7) */
    addr_low = data[0] & 0x3F;

    /* Byte 1: 1AAACDDD — high 3 bits INVERTED (A8-A10), C=activate */
    addr_high_inv = (data[1] >> 4) & 0x07;
    board_address = (uint16_t)addr_low | ((uint16_t)(~addr_high_inv & 0x07) << 6);

    activate = (data[1] & 0x08) ? true : false;

    if (_use_output_address) {

        /* Output-address mode: DDD bits 0-1 are A0-A1, fold into 11-bit
         * flat address.  DDD bit 2 is the R (direction) bit. */
        uint16_t output_address = (board_address << 2) | (data[1] & 0x03);
        output_pair = (data[1] >> 2) & 0x01;

        if (_interface->on_accessory_basic_command) {

            _interface->on_accessory_basic_command(output_address, output_pair, activate);

        }

    } else {

        /* Decoder-address mode: DDD is the 3-bit output pair field */
        output_pair = data[1] & 0x07;

        if (_interface->on_accessory_basic_command) {

            _interface->on_accessory_basic_command(board_address, output_pair, activate);

        }

    }

}

    /**
     * @brief Dispatch an extended accessory instruction.
     * @param data Raw packet bytes.
     * @param byte_count Number of bytes.
     */
static void _dispatch_accessory_extended(const uint8_t *data, uint8_t byte_count) {

    uint16_t address;
    uint8_t aspect;
    uint8_t addr_low;
    uint8_t addr_high_inv;

    if (byte_count < 4) {

        return;

    }

    /* CV access long form (ops-mode): 6-byte packet, byte 2 starts with 1110 */
    if (byte_count == 6 && (data[2] & 0xF0) == 0xE0) {

        uint8_t cv_addr_low = data[0] & 0x3F;
        uint8_t cv_addr_high_inv = ((data[1] >> 4) & 0x07);
        uint16_t cv_address = (uint16_t)cv_addr_low |
                    ((uint16_t)(~cv_addr_high_inv & 0x07) << 6);
        cv_address |= (uint16_t)((data[1] >> 1) & 0x03) << 9;

        if (cv_address != _my_address) {

            return;

        }

        _dispatch_acc_cv_access(&data[2], 3);
        return;

    }

    /* Byte 0: 10AAAAAA — low 6 address bits */
    addr_low = data[0] & 0x3F;

    /* Byte 1: 0AAA0AA1 — high bits INVERTED */
    addr_high_inv = ((data[1] >> 4) & 0x07);
    address = (uint16_t)addr_low | ((uint16_t)(~addr_high_inv & 0x07) << 6);

    /* Extended also uses bits 1-2 of byte 1 for more address bits */
    address |= (uint16_t)((data[1] >> 1) & 0x03) << 9;

    aspect = data[2];

    if (_interface->on_accessory_extended_command) {

        _interface->on_accessory_extended_command(address, aspect);

    }

}

    /**
     * @brief Dispatch advanced operations (001xxxxx).
     * @param address DCC address.
     * @param instruction_bytesInstruction bytes pointer.
     * @param instruction_byte_count Number of instruction bytes.
     */
static void _dispatch_advanced_ops(uint16_t address, const uint8_t *instruction_bytes, uint8_t instruction_byte_count) {

    uint8_t first = instruction_bytes[0];

    if (first == DCC_ADV_OPS_128_SPEED && instruction_byte_count >= 2) {

        _dispatch_speed_128(address, instruction_bytes[1]);

    } else if (first == DCC_ADV_OPS_ANALOG_FUNCTION && instruction_byte_count >= 3) {

        if (_interface->on_analog_function) {

            _interface->on_analog_function(address, instruction_bytes[1], instruction_bytes[2]);

        }

    } else if (first == DCC_ADV_OPS_SPEED_RESTRICTION && instruction_byte_count >= 2) {

        bool enabled = (instruction_bytes[1] & 0x80) ? true : false;
        uint8_t limit = instruction_bytes[1] & 0x7F;

        if (_interface->on_speed_restriction) {

            _interface->on_speed_restriction(address, enabled, limit);

        }

    }

}

    /**
     * @brief Dispatch feature expansion instructions (110xxxxx).
     * @param address DCC address.
     * @param instruction_bytesInstruction bytes pointer.
     * @param instruction_byte_count Number of instruction bytes.
     */
static void _dispatch_feature_expansion(uint16_t address, const uint8_t *instruction_bytes, uint8_t instruction_byte_count) {

    uint8_t first = instruction_bytes[0];

    if (first == DCC_FEAT_F13_F20) {

        _dispatch_functions(address, 13, 8, instruction_bytes[1]);

    } else if (first == DCC_FEAT_F21_F28) {

        _dispatch_functions(address, 21, 8, instruction_bytes[1]);

    } else if (first == DCC_FEAT_F29_F36) {

        _dispatch_functions(address, 29, 8, instruction_bytes[1]);

    } else if (first == DCC_FEAT_F37_F44) {

        _dispatch_functions(address, 37, 8, instruction_bytes[1]);

    } else if (first == DCC_FEAT_F45_F52) {

        _dispatch_functions(address, 45, 8, instruction_bytes[1]);

    } else if (first == DCC_FEAT_F53_F60) {

        _dispatch_functions(address, 53, 8, instruction_bytes[1]);

    } else if (first == DCC_FEAT_BINARY_STATE_SHORT) {

        /* Binary state short: 0xDD + DLLLLLLL (state 1-127) */
        uint8_t state_num = instruction_bytes[1] & 0x7F;
        bool active = (instruction_bytes[1] & 0x80) ? true : false;

        if (_interface->on_binary_state_short) {

            _interface->on_binary_state_short(address, state_num, active);

        }

    } else if (first == DCC_FEAT_BINARY_STATE_LONG && instruction_byte_count >= 3) {

        /* Binary state long: 0xDC + DLLLLLLL + HHHHHHHH (3 bytes) */
        uint16_t state_num = (uint16_t)(instruction_bytes[2] << 7) | (instruction_bytes[1] & 0x7F);
        bool active = (instruction_bytes[1] & 0x80) ? true : false;

        if (_interface->on_binary_state_long) {

            _interface->on_binary_state_long(address, state_num, active);

        }

    } else if (first == DCC_FEAT_F61_F68) {

        /* F61-F68: 0xDC + data (2 bytes only — same opcode as BSL) */
        _dispatch_functions(address, 61, 8, instruction_bytes[1]);

    }

}

    /**
     * @brief Dispatch instruction bytes for a multi-function decoder.
     * @param address DCC address.
     * @param instruction_bytesPointer to first instruction byte.
     * @param instruction_byte_count Number of instruction bytes.
     */
static void _dispatch_instruction(uint16_t address, const uint8_t *instruction_bytes, uint8_t instruction_byte_count) {

    uint8_t first;

    if (instruction_byte_count < 1) {

        return;

    }

    first = instruction_bytes[0];

    if ((first & 0xF0) == 0x10 && instruction_byte_count >= 2) {

        /* Consist control: 0001xxxx */
        bool direction_normal = (first == DCC_CONSIST_SET_NORMAL);
        uint8_t consist_address = instruction_bytes[1] & 0x7F;

        if (_interface->on_consist_command) {

            _interface->on_consist_command(address, consist_address, direction_normal);

        }

    } else if ((first & 0xC0) == 0x40) {

        /* Speed/direction 14/28-step: 01Dxxxxx */
        _dispatch_speed_14_28(address, first);

    } else if ((first & 0xE0) == 0x20) {

        /* Advanced operations: 001xxxxx */
        _dispatch_advanced_ops(address, instruction_bytes, instruction_byte_count);

    } else if ((first & 0xE0) == 0x80) {

        /* Function group 1: 100xxxxx */
        _dispatch_func_group1(address, first);

    } else if ((first & 0xF0) == 0xB0) {

        /* Function group 2a (F5-F8): 1011xxxx */
        _dispatch_functions(address, 5, 4, first & 0x0F);

    } else if ((first & 0xF0) == 0xA0) {

        /* Function group 2b (F9-F12): 1010xxxx */
        _dispatch_functions(address, 9, 4, first & 0x0F);

    } else if ((first & 0xE0) == 0xC0 && instruction_byte_count >= 2) {

        /* Feature expansion: 110xxxxx */
        _dispatch_feature_expansion(address, instruction_bytes, instruction_byte_count);

    } else if ((first & 0xE0) == 0xE0) {

        /* CV access long form: 111xxxxx */
        _dispatch_cv_access(address, instruction_bytes, instruction_byte_count);

    }

}

    /**
     * @brief Verify a CV byte value and fire ACK if it matches.
     * @param cv_number 1-based CV number.
     * @param expected Expected value.
     */
static void _cv_verify_byte(uint16_t cv_number, uint8_t expected) {

    if (_interface->cv_read) {

        uint8_t stored;

        if (_interface->cv_read(cv_number, &stored) && stored == expected) {

            _fire_ack();

        }

    }

    if (_interface->on_cv_verify) {

        _interface->on_cv_verify(cv_number, expected, true);

    }

}

    /**
     * @brief Verify a single CV bit and fire ACK if it matches.
     * @param cv_number 1-based CV number.
     * @param bit_position Bit position (0-7).
     * @param bit_value Expected bit value.
     */
static void _cv_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    uint8_t current_value;

    if (!_interface->cv_read) {

        return;

    }

    if (!_interface->cv_read(cv_number, &current_value)) {

        return;

    }

    if (((current_value >> bit_position) & 0x01) == bit_value) {

        _fire_ack();

    }

}

    /**
     * @brief Dispatch a service mode bit manipulate operation.
     * @param cv_number 1-based CV number.
     * @param data_byte The bit-manipulate data byte (111KDBBB).
     */
static void _dispatch_service_mode_bit_manipulate(uint16_t cv_number, uint8_t data_byte) {

    uint8_t bit_position = data_byte & 0x07;
    bool bit_value = (data_byte & 0x08) ? true : false;
    bool is_write = (data_byte & 0x10) ? true : false;

    if (is_write) {

        _cv_bit_manipulate(cv_number, bit_position, bit_value, true);

    } else {

        _cv_verify_bit(cv_number, bit_position, bit_value);

    }

    if (_interface->on_cv_bit) {

        _interface->on_cv_bit(cv_number, bit_position, bit_value, true);

    }

}

    /**
     * @brief Dispatch a direct-mode service mode packet (4 raw bytes).
     * @param data Raw packet bytes.
     */
static void _dispatch_service_mode_direct(const uint8_t *data) {

    uint8_t cc = (data[0] >> 2) & 0x03;
    uint16_t cv_number = (uint16_t)((data[0] & 0x03) << 8) | data[1];
    cv_number += 1;  /* 0-based wire → 1-based CV */
    uint8_t data_byte = data[2];

    if (cc == 0x03) {

        /* Write byte (CC=11) */
        _cv_write_and_notify(cv_number, data_byte, true);

    } else if (cc == 0x01) {

        /* Verify byte (CC=01) */
        _cv_verify_byte(cv_number, data_byte);

    } else if (cc == 0x02) {

        /* Bit manipulate (CC=10) */
        _dispatch_service_mode_bit_manipulate(cv_number, data_byte);

    }

}

    /**
     * @brief Dispatch a register/paged-mode service mode packet (3 raw bytes).
     * @param data Raw packet bytes.
     */
static void _dispatch_service_mode_register(const uint8_t *data) {

    bool is_write = (data[0] & 0x08) ? true : false;
    uint8_t register_number = (data[0] & 0x07) + 1;  /* 0-based → 1-based */
    uint8_t data_byte = data[1];

    /* Registers 1-8 map to CVs 1-8 */
    uint16_t cv_number = (uint16_t)register_number;

    if (is_write) {

        _cv_write_and_notify(cv_number, data_byte, true);

    } else {

        _cv_verify_byte(cv_number, data_byte);

    }

}

static void _dispatch_service_mode(const uint8_t *data, uint8_t byte_count) {

    if (byte_count == 4) {

        _dispatch_service_mode_direct(data);

    } else if (byte_count == 3) {

        _dispatch_service_mode_register(data);

    }

}

// =============================================================================
// Public API
// =============================================================================

void DccPacketDecoder_initialize(const interface_dcc_packet_decoder_t *interface) {

    _interface = interface;
    _my_address = 0;
    _my_address_type = DCC_ADDRESS_SHORT;
    _direction_reversed = false;
    _use_extended_address = false;
    _use_output_address = false;
    _reset_count = 0;
    _service_mode_active = false;
    _update_address_cv_cache();

}

    /**
     * @brief Route an accessory decoder packet to basic or extended handler.
     * @param data Raw packet bytes.
     * @param byte_count Number of bytes.
     */
static void _dispatch_accessory(const uint8_t *data, uint8_t byte_count) {

    if (_my_address_type != DCC_ADDRESS_ACCESSORY &&
        _my_address_type != DCC_ADDRESS_ACCESSORY_EXTENDED) {

        return;

    }

    /* Check if extended (byte 1 bit 3 = 0 and bit 0 = 1) */
    if ((data[1] & 0x89) == 0x01) {

        _dispatch_accessory_extended(data, byte_count);

    } else {

        _dispatch_accessory_basic(data, byte_count);

    }

}

void DccPacketDecoder_process_packet(const uint8_t *data, uint8_t byte_count) {

    uint16_t packet_address;
    uint8_t inst_start;

    if (!_validate_xor(data, byte_count)) {

        return;

    }

    /* Idle packet — ignore.  Does not affect service mode reset counter
     * because idle packets can appear between reset packets. */
    if (data[0] == 0xFF) {

        return;

    }

    /* Broadcast reset: address 0, data 0.
     * Per NMRA S-9.2.3, consecutive reset packets signal the start of
     * a service mode sequence. Track them to gate service mode acceptance. */
    if (data[0] == 0x00 && byte_count >= 3 && data[1] == 0x00) {

        if (_reset_count < 255) {

            _reset_count++;

        }

        if (_reset_count >= DCC_SERVICE_MODE_RESET_PRE_COUNT) {

            _service_mode_active = true;

        }

        return;

    }

    /* Service mode packet (S-9.2.3): byte 0 in 0x70-0x7F.
     * Only accepted after enough consecutive reset packets have been
     * received — this prevents normal short-address packets (addr 112-127)
     * from being misinterpreted as service mode. */
    if ((data[0] & 0xF0) == 0x70 && _service_mode_active) {

        _dispatch_service_mode(data, byte_count);
        return;

    }

    /* Any other packet type — not service mode.  Clear the reset counter
     * and service mode state. */
    _reset_count = 0;
    _service_mode_active = false;

    /* Accessory decoder packet */
    if ((data[0] & 0xC0) == 0x80) {

        _dispatch_accessory(data, byte_count);
        return;

    }

    /* Multi-function decoder packet */
    if (data[0] <= 0x7F) {

        /* Short address */
        packet_address = data[0];
        inst_start = 1;

        /* Address match: must match our address, or broadcast */
        if (packet_address != _my_address &&
            _my_address_type != DCC_ADDRESS_BROADCAST &&
            packet_address != DCC_ADDRESS_BROADCAST_VALUE) {

            return;

        }

    } else if (data[0] <= 0xE7 && byte_count >= 4) {

        /* Long address */
        packet_address = ((uint16_t)(data[0] & 0x3F) << 8) | data[1];
        inst_start = 2;

        if (_my_address_type != DCC_ADDRESS_LONG || packet_address != _my_address) {

            return;

        }

    } else {

        /* Reserved or unrecognized address range */
        return;

    }

    /* Dispatch instruction bytes (excluding address and XOR) */
    _dispatch_instruction(packet_address, &data[inst_start], byte_count - inst_start - 1);

}

#endif /* DCC_COMPILE_DECODER */
