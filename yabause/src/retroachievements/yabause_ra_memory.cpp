/*
YabaSanshiro RetroAchievements Memory Interface
Copyright 2025 YabaSanshiro Team

This file implements Saturn memory access for RetroAchievements.
*/

#include "yabause_ra_integration.h"

// Include YabaSanshiro memory system headers
extern "C" {
#include "memory.h"
#include "yabause.h"
#include "cs2.h"
#include "vdp1.h"
#include "vdp2.h"
#include "scsp.h"
}

#include "../android_debug_log.h"

// Use the unified logging macros
#define LOGD(...) RA_MEMORY_LOGD(__VA_ARGS__)
#define LOGE(...) RA_MEMORY_LOGE(__VA_ARGS__)

namespace YabauseRA {

/**
 * Map RetroAchievements memory addresses to Saturn memory regions
 * 
 * Saturn Memory Map for RetroAchievements:
 * 0x00000000-0x000FFFFF: BIOS ROM (1MB)
 * 0x00200000-0x002FFFFF: Work RAM Low (1MB)  
 * 0x06000000-0x07FFFFFF: Work RAM High (32MB)
 * 0x05A00000-0x05AFFFFF: VDP1 VRAM (512KB)
 * 0x05C00000-0x05FFFFFF: VDP2 VRAM (4MB)
 * 0x05800000-0x058FFFFF: Color RAM (4KB, but mapped to larger region)
 * 0x25A00000-0x25A7FFFF: Sound RAM (512KB)
 */
uint32_t yabause_ra_read_memory_impl(uint32_t address, uint8_t* buffer, uint32_t num_bytes) {
    if (!buffer || num_bytes == 0) {
        return 0;
    }

    uint32_t bytes_read = 0;

    //LOGD( "ra_read_memory %08X", address );

    // Ensure we don't read beyond requested bytes
    for (uint32_t i = 0; i < num_bytes; i++) {
        uint32_t current_addr = address + i;
        uint8_t value = 0;
        bool success = false;

        success = true;
        if( (current_addr & 0x00100000)  != 0 ) {
            value = HighWram[current_addr&0xFFFFF];
        }else{
            value = LowWram[current_addr&0xFFFFF];
        }
#if 0
        // BIOS ROM region (0x00000000-0x000FFFFF)
        if (current_addr <= 0x000FFFFF) {
            if (BiosRom && current_addr < 0x100000) {
                value = BiosRom[current_addr];
                success = true;
            }
        }
        // Work RAM Low (0x00200000-0x002FFFFF) 
        else if (current_addr >= 0x00200000 && current_addr <= 0x002FFFFF) {
            if (LowWram) {
                value = LowWram[current_addr - 0x00200000];
                success = true;
            }
        }
        // Work RAM High (0x06000000-0x07FFFFFF)
        else if (current_addr >= 0x06000000 && current_addr <= 0x07FFFFFF) {
            if (HighWram) {
                uint32_t offset = current_addr - 0x06000000;
                if (offset < 0x2000000) { // 32MB
                    value = HighWram[offset];
                    success = true;
                }
            }
        }
        // VDP1 VRAM (0x05A00000-0x05AFFFFF)
        else if (current_addr >= 0x05A00000 && current_addr <= 0x05AFFFFF) {
            if (Vdp1Ram) {
                uint32_t offset = current_addr - 0x05A00000;
                if (offset < 0x100000) { // 512KB
                    value = Vdp1Ram[offset];
                    success = true;
                }
            }
        }
        // VDP2 VRAM (0x05C00000-0x05FFFFFF)
        else if (current_addr >= 0x05C00000 && current_addr <= 0x05FFFFFF) {
            if (Vdp2Ram) {
                uint32_t offset = current_addr - 0x05C00000;
                if (offset < 0x400000) { // 4MB
                    value = Vdp2Ram[offset];
                    success = true;
                }
            }
        }
        // Color RAM (0x05800000-0x058FFFFF)
        else if (current_addr >= 0x05800000 && current_addr <= 0x058FFFFF) {
            if (Vdp2ColorRam) {
                uint32_t offset = (current_addr - 0x05800000) % 0x1000; // 4KB, repeated
                value = ((uint8_t*)Vdp2ColorRam)[offset];
                success = true;
            }
        }
        // Sound RAM (0x25A00000-0x25A7FFFF)
        else if (current_addr >= 0x25A00000 && current_addr <= 0x25A7FFFF) {
            if (SoundRam) {
                uint32_t offset = current_addr - 0x25A00000;
                if (offset < 0x80000) { // 512KB
                    value = SoundRam[offset];
                    success = true;
                }
            }
        }
        // Try using the standard memory read function for other regions
        else {
            // Use YabaSanshiro's memory system for unmapped regions
            u32 cycle = 0;
            value = MappedMemoryReadByteNocache(current_addr, &cycle);
            success = true; // Assume success, as MappedMemoryReadByte handles invalid addresses
        }
#endif

        if (success) {
            buffer[i] = value;
            bytes_read++;
        } else {
            // If read fails, stop reading and return what we've read so far
            break;
        }

    }

    return bytes_read;
}

// Forward declaration for the actual implementation
uint32_t yabause_ra_read_memory_impl(uint32_t address, uint8_t* buffer, uint32_t num_bytes);

/**
 * C-style wrapper function for the memory read implementation
 */
extern "C" uint32_t yabause_ra_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes) {
    return yabause_ra_read_memory_impl(address, buffer, num_bytes);
}

/**
 * Read a single byte from Saturn memory (convenience function)
 */
uint8_t readByte(uint32_t address) {
    uint8_t value = 0;
    yabause_ra_read_memory_impl(address, &value, 1);
    return value;
}

/**
 * Read a 16-bit word from Saturn memory (convenience function)
 */
uint16_t readWord(uint32_t address) {
    uint8_t bytes[2];
    uint32_t read = yabause_ra_read_memory_impl(address, bytes, 2);
    if (read == 2) {
        // Saturn is big-endian
        return (bytes[0] << 8) | bytes[1];
    }
    return 0;
}

/**
 * Read a 32-bit long from Saturn memory (convenience function)
 */
uint32_t readLong(uint32_t address) {
    uint8_t bytes[4];
    uint32_t read = yabause_ra_read_memory_impl(address, bytes, 4);
    if (read == 4) {
        // Saturn is big-endian
        return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    }
    return 0;
}

} // namespace YabauseRA