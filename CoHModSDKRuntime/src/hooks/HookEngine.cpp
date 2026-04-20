#include "HookEngine.hpp"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>

namespace {
#ifdef _M_IX86
    constexpr std::size_t kMinimumPatchLength = 5;
    constexpr std::size_t kMaxInstructionLength = 15;
    constexpr std::size_t kMaxTrampolineSize = 64;

    enum class RelativeKind {
        None,
        CallRel32,
        JmpRel32,
        JmpRel8,
        JccRel8,
        JccRel32,
    };

    struct DecodedInstruction {
        std::size_t length = 0;
        std::size_t prefixLength = 0;
        std::size_t opcodeOffset = 0;
        std::size_t relativeOffset = 0;
        RelativeKind relativeKind = RelativeKind::None;
        bool hasModRm = false;
        std::uint8_t modrm = 0;
        bool terminal = false;
    };

    bool IsPrefixByte(std::uint8_t byte, bool& operandSize16, bool& addressSize16) {
        switch (byte) {
        case 0xF0:
        case 0xF2:
        case 0xF3:
        case 0x2E:
        case 0x36:
        case 0x3E:
        case 0x26:
        case 0x64:
        case 0x65:
            return true;
        case 0x66:
            operandSize16 = true;
            return true;
        case 0x67:
            addressSize16 = true;
            return true;
        default:
            return false;
        }
    }

    bool HasModRmByte(std::uint8_t opcode) {
        switch (opcode) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x62:
        case 0x63:
        case 0x69:
        case 0x6B:
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        case 0xC0:
        case 0xC1:
        case 0xC6:
        case 0xC7:
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF:
        case 0xF6:
        case 0xF7:
        case 0xFE:
        case 0xFF:
            return true;
        default:
            return false;
        }
    }

    bool HasExtendedModRmByte(std::uint8_t opcode) {
        switch (opcode) {
        case 0x1F:
        case 0xAF:
        case 0xB6:
        case 0xB7:
        case 0xBE:
        case 0xBF:
            return true;
        default:
            return false;
        }
    }

    bool DecodeModRm(const std::uint8_t* code, std::size_t& offset, bool addressSize16, DecodedInstruction& instruction) {
        if (addressSize16 || (offset >= kMaxInstructionLength)) {
            return false;
        }

        instruction.hasModRm = true;
        instruction.modrm = code[offset++];

        const std::uint8_t mod = instruction.modrm >> 6;
        const std::uint8_t rm = instruction.modrm & 0x07;

        if ((mod != 0x03) && (rm == 0x04)) {
            if (offset >= kMaxInstructionLength) {
                return false;
            }

            const std::uint8_t sib = code[offset++];
            if ((mod == 0x00) && ((sib & 0x07) == 0x05)) {
                offset += 4;
            }
        }

        if ((mod == 0x00) && (rm == 0x05)) {
            offset += 4;
        }
        else if (mod == 0x01) {
            offset += 1;
        }
        else if (mod == 0x02) {
            offset += 4;
        }

        return (offset <= kMaxInstructionLength);
    }

    std::size_t GetImmediateSize(std::uint8_t opcode, bool operandSize16, std::uint8_t modrm) {
        switch (opcode) {
        case 0x04:
        case 0x0C:
        case 0x14:
        case 0x1C:
        case 0x24:
        case 0x2C:
        case 0x34:
        case 0x3C:
        case 0x6A:
        case 0xA8:
        case 0xC0:
        case 0xC1:
        case 0xC6:
            return 1;
        case 0x05:
        case 0x0D:
        case 0x15:
        case 0x1D:
        case 0x25:
        case 0x2D:
        case 0x35:
        case 0x3D:
        case 0x68:
        case 0x69:
        case 0x81:
        case 0xA9:
        case 0xC7:
            return operandSize16 ? 2 : 4;
        case 0x6B:
        case 0x80:
        case 0x82:
        case 0x83:
            return 1;
        case 0xC2:
        case 0xCA:
            return 2;
        case 0xC8:
            return 3;
        case 0xF6:
            return (((modrm >> 3) & 0x07) <= 1) ? 1 : 0;
        case 0xF7:
            return (((modrm >> 3) & 0x07) <= 1) ? (operandSize16 ? 2 : 4) : 0;
        default:
            return 0;
        }
    }

    bool DecodeInstruction(const std::uint8_t* code, DecodedInstruction& instruction) {
        bool operandSize16 = false;
        bool addressSize16 = false;
        std::size_t offset = 0;

        while ((offset < kMaxInstructionLength) && IsPrefixByte(code[offset], operandSize16, addressSize16)) {
            ++offset;
        }

        if (offset >= kMaxInstructionLength) {
            return false;
        }

        instruction.prefixLength = offset;
        instruction.opcodeOffset = offset;

        const std::uint8_t opcode = code[offset++];

        if (opcode == 0x0F) {
            if ((offset >= kMaxInstructionLength) || operandSize16) {
                return false;
            }

            const std::uint8_t extendedOpcode = code[offset++];
            if ((extendedOpcode >= 0x80) && (extendedOpcode <= 0x8F)) {
                instruction.length = offset + 4;
                instruction.relativeOffset = offset;
                instruction.relativeKind = RelativeKind::JccRel32;
                return (instruction.length <= kMaxInstructionLength);
            }

            if (!HasExtendedModRmByte(extendedOpcode) || !DecodeModRm(code, offset, addressSize16, instruction)) {
                return false;
            }

            instruction.length = offset;
            return true;
        }

        if (operandSize16 && ((opcode == 0xE8) || (opcode == 0xE9))) {
            return false;
        }

        if ((opcode >= 0x70) && (opcode <= 0x7F)) {
            instruction.length = offset + 1;
            instruction.relativeOffset = offset;
            instruction.relativeKind = RelativeKind::JccRel8;
            return true;
        }

        if ((opcode >= 0xB0) && (opcode <= 0xB7)) {
            instruction.length = offset + 1;
            return true;
        }

        if ((opcode >= 0xB8) && (opcode <= 0xBF)) {
            instruction.length = offset + (operandSize16 ? 2 : 4);
            return (instruction.length <= kMaxInstructionLength);
        }

        if ((opcode >= 0x40) && (opcode <= 0x5F)) {
            instruction.length = offset;
            return true;
        }

        switch (opcode) {
        case 0x60:
        case 0x61:
        case 0x90:
        case 0x9B:
        case 0x98:
        case 0x99:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xC3:
        case 0xC9:
        case 0xCC:
            instruction.length = offset;
            instruction.terminal = (opcode == 0xC3);
            return true;
        case 0xC2:
        case 0xCA:
            instruction.length = offset + 2;
            instruction.terminal = true;
            return true;
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
            if (addressSize16) {
                return false;
            }
            instruction.length = offset + 4;
            return true;
        case 0x04:
        case 0x0C:
        case 0x14:
        case 0x1C:
        case 0x24:
        case 0x2C:
        case 0x34:
        case 0x3C:
        case 0x68:
        case 0x6A:
        case 0xA8:
        case 0xA9:
            instruction.length = offset + GetImmediateSize(opcode, operandSize16, 0);
            return (instruction.length <= kMaxInstructionLength);
        case 0xE8:
            instruction.length = offset + 4;
            instruction.relativeOffset = offset;
            instruction.relativeKind = RelativeKind::CallRel32;
            return true;
        case 0xE9:
            instruction.length = offset + 4;
            instruction.relativeOffset = offset;
            instruction.relativeKind = RelativeKind::JmpRel32;
            instruction.terminal = true;
            return true;
        case 0xEB:
            instruction.length = offset + 1;
            instruction.relativeOffset = offset;
            instruction.relativeKind = RelativeKind::JmpRel8;
            instruction.terminal = true;
            return true;
        case 0xEA:
        case 0x9A:
            return false;
        default:
            break;
        }

        if (!HasModRmByte(opcode) || !DecodeModRm(code, offset, addressSize16, instruction)) {
            return false;
        }

        if (opcode == 0xFF) {
            const std::uint8_t operation = (instruction.modrm >> 3) & 0x07;
            if ((operation == 0x03) || (operation == 0x05)) {
                return false;
            }
            if (operation == 0x04) {
                instruction.terminal = true;
            }
        }

        instruction.length = offset + GetImmediateSize(opcode, operandSize16, instruction.modrm);
        return (instruction.length <= kMaxInstructionLength);
    }

    bool TryGetRelative32(const std::uint8_t* instruction, std::size_t length, const std::uint8_t* destination, std::int32_t& relativeOffset) {
        const std::intptr_t delta = reinterpret_cast<const std::uint8_t*>(destination) - (instruction + length);
        if ((delta < (std::numeric_limits<std::int32_t>::min)()) || (delta > (std::numeric_limits<std::int32_t>::max)())) {
            return false;
        }

        relativeOffset = static_cast<std::int32_t>(delta);
        return true;
    }

    bool RelocateInstruction(const std::uint8_t* source, const DecodedInstruction& instruction, std::uint8_t* destination, std::size_t& bytesWritten) {
        if ((instruction.relativeKind != RelativeKind::None) && (instruction.prefixLength != 0)) {
            return false;
        }

        switch (instruction.relativeKind) {
        case RelativeKind::None:
            std::memcpy(destination, source, instruction.length);
            bytesWritten = instruction.length;
            return true;
        case RelativeKind::CallRel32:
        case RelativeKind::JmpRel32:
        case RelativeKind::JccRel32: {
            std::memcpy(destination, source, instruction.length);

            const std::int32_t originalOffset = *reinterpret_cast<const std::int32_t*>(source + instruction.relativeOffset);
            const std::uint8_t* absoluteTarget = source + instruction.length + originalOffset;
            std::int32_t relocatedOffset = 0;
            if (!TryGetRelative32(destination, instruction.length, absoluteTarget, relocatedOffset)) {
                return false;
            }

            std::memcpy(destination + instruction.relativeOffset, &relocatedOffset, sizeof(relocatedOffset));
            bytesWritten = instruction.length;
            return true;
        }
        case RelativeKind::JmpRel8: {
            const std::int8_t originalOffset = *reinterpret_cast<const std::int8_t*>(source + instruction.relativeOffset);
            const std::uint8_t* absoluteTarget = source + instruction.length + originalOffset;
            destination[0] = 0xE9;
            std::int32_t relocatedOffset = 0;
            if (!TryGetRelative32(destination, kMinimumPatchLength, absoluteTarget, relocatedOffset)) {
                return false;
            }

            std::memcpy(destination + 1, &relocatedOffset, sizeof(relocatedOffset));
            bytesWritten = kMinimumPatchLength;
            return true;
        }
        case RelativeKind::JccRel8: {
            const std::int8_t originalOffset = *reinterpret_cast<const std::int8_t*>(source + instruction.relativeOffset);
            const std::uint8_t* absoluteTarget = source + instruction.length + originalOffset;
            destination[0] = 0x0F;
            destination[1] = static_cast<std::uint8_t>(0x80 | (source[instruction.opcodeOffset] & 0x0F));
            std::int32_t relocatedOffset = 0;
            if (!TryGetRelative32(destination, 6, absoluteTarget, relocatedOffset)) {
                return false;
            }

            std::memcpy(destination + 2, &relocatedOffset, sizeof(relocatedOffset));
            bytesWritten = 6;
            return true;
        }
        }

        return false;
    }

    bool WriteMemory(void* destination, const void* source, std::size_t size) {
        DWORD oldProtect = 0;
        if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }

        std::memcpy(destination, source, size);
        FlushInstructionCache(GetCurrentProcess(), destination, size);

        DWORD restoredProtect = 0;
        VirtualProtect(destination, size, oldProtect, &restoredProtect);
        return true;
    }
#endif
}

bool HookEngine::CreateHook(void* targetFunction, void* detourFunction, void** originalFunction) {
#ifndef _M_IX86
    if (originalFunction != nullptr) {
        *originalFunction = nullptr;
    }
    return false;
#else
    if ((targetFunction == nullptr) || (detourFunction == nullptr)) {
        return false;
    }

    std::scoped_lock lock(mutex);
    if (FindHookEntry(targetFunction) != nullptr) {
        return false;
    }

    HookEntry hook = {};
    hook.target = static_cast<std::uint8_t*>(targetFunction);
    hook.detour = static_cast<std::uint8_t*>(detourFunction);

    if (!BuildTrampoline(hook)) {
        if (hook.trampoline != nullptr) {
            VirtualFree(hook.trampoline, 0, MEM_RELEASE);
        }
        return false;
    }

    if (originalFunction != nullptr) {
        *originalFunction = hook.trampoline;
    }

    hooks.push_back(hook);
    return true;
#endif
}

bool HookEngine::EnableHook(void* targetFunction) {
#ifndef _M_IX86
    return false;
#else
    std::scoped_lock lock(mutex);
    HookEntry* hook = FindHookEntry(targetFunction);
    return (hook != nullptr) && EnableHookInternal(*hook);
#endif
}

bool HookEngine::EnableAllHooks() {
#ifndef _M_IX86
    return false;
#else
    std::scoped_lock lock(mutex);
    bool success = true;

    for (HookEntry& hook : hooks) {
        success = EnableHookInternal(hook) && success;
    }

    return success;
#endif
}

bool HookEngine::DisableHook(void* targetFunction) {
#ifndef _M_IX86
    return false;
#else
    std::scoped_lock lock(mutex);
    HookEntry* hook = FindHookEntry(targetFunction);
    return (hook != nullptr) && DisableHookInternal(*hook);
#endif
}

bool HookEngine::DisableAllHooks() {
#ifndef _M_IX86
    return false;
#else
    std::scoped_lock lock(mutex);
    bool success = true;

    for (HookEntry& hook : hooks) {
        success = DisableHookInternal(hook) && success;
    }

    return success;
#endif
}

HookEngine::HookEntry* HookEngine::FindHookEntry(void* targetFunction) {
    for (HookEntry& hook : hooks) {
        if (hook.target == targetFunction) {
            return &hook;
        }
    }

    return nullptr;
}

bool HookEngine::EnableHookInternal(HookEntry& hook) {
#ifndef _M_IX86
    return false;
#else
    if (hook.enabled) {
        return true;
    }

    std::array<std::uint8_t, kMaxOriginalByteCount> patch = {};
    patch[0] = 0xE9;

    std::int32_t jumpOffset = 0;
    if (!TryGetRelative32(hook.target, kMinimumPatchLength, hook.detour, jumpOffset)) {
        return false;
    }

    std::memcpy(patch.data() + 1, &jumpOffset, sizeof(jumpOffset));
    std::fill(patch.begin() + kMinimumPatchLength, patch.begin() + hook.patchLength, 0x90);

    if (!WriteMemory(hook.target, patch.data(), hook.patchLength)) {
        return false;
    }

    hook.enabled = true;
    return true;
#endif
}

bool HookEngine::DisableHookInternal(HookEntry& hook) {
#ifndef _M_IX86
    return false;
#else
    if (!hook.enabled) {
        return true;
    }

    if (!WriteMemory(hook.target, hook.originalBytes.data(), hook.patchLength)) {
        return false;
    }

    hook.enabled = false;
    return true;
#endif
}

bool HookEngine::BuildTrampoline(HookEntry& hook) {
#ifndef _M_IX86
    return false;
#else
    hook.trampoline = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, kMaxTrampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (hook.trampoline == nullptr) {
        return false;
    }

    std::size_t sourceOffset = 0;
    std::size_t trampolineOffset = 0;
    bool terminalInstructionCopied = false;

    while (sourceOffset < kMinimumPatchLength) {
        DecodedInstruction instruction = {};
        if (!DecodeInstruction(hook.target + sourceOffset, instruction)) {
            return false;
        }

        if ((sourceOffset + instruction.length) > hook.originalBytes.size()) {
            return false;
        }

        std::size_t bytesWritten = 0;
        if (!RelocateInstruction(hook.target + sourceOffset, instruction, hook.trampoline + trampolineOffset, bytesWritten)) {
            return false;
        }

        sourceOffset += instruction.length;
        trampolineOffset += bytesWritten;
        terminalInstructionCopied = instruction.terminal;

        if (terminalInstructionCopied && (sourceOffset < kMinimumPatchLength)) {
            return false;
        }
    }

    if (!terminalInstructionCopied) {
        hook.trampoline[trampolineOffset++] = 0xE9;
        std::int32_t jumpBackOffset = 0;
        if (!TryGetRelative32(hook.trampoline + trampolineOffset - 1, kMinimumPatchLength, hook.target + sourceOffset, jumpBackOffset)) {
            return false;
        }

        std::memcpy(hook.trampoline + trampolineOffset, &jumpBackOffset, sizeof(jumpBackOffset));
        trampolineOffset += sizeof(jumpBackOffset);
    }

    std::memcpy(hook.originalBytes.data(), hook.target, sourceOffset);
    hook.patchLength = sourceOffset;
    return (trampolineOffset <= kMaxTrampolineSize);
#endif
}
