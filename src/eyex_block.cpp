#include "eyex_block.h"

#include "logging.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace DyingLightHeadTracking::eyex_block {

namespace {

constexpr const char* kEyeXDll = "Tobii.EyeX.Client.dll";
constexpr const char* kInitializeName = "txInitializeEyeX";

// Every EyeX client function returns a TX_RESULT, and the engine tests each one
// against 2 before trusting it or any out-parameter (or pre-zeroes the
// out-parameter and checks that instead). 2 is success, so anything else reads
// as "no eye tracker" at every call site.
constexpr int kNotOk = 1;

struct Patched {
    ULONG_PTR* slot;
    ULONG_PTR original;
};

std::vector<Patched> g_patched;

int RefuseCall() { return kNotOk; }

int RefuseInitialize() {
    Log::Line("Tobii eye tracking: the game asked to start EyeX and was refused, so its eye "
              "tracking features stay off");
    return kNotOk;
}

bool WriteSlot(ULONG_PTR* slot, ULONG_PTR value) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtect)) return false;
    *slot = value;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), oldProtect, &ignored);
    return true;
}

}  // namespace

bool Install(HMODULE engine) {
    auto* base = reinterpret_cast<uint8_t*>(engine);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    const auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; desc->Name != 0; ++desc) {
        if (_stricmp(reinterpret_cast<const char*>(base + desc->Name), kEyeXDll) == 0) break;
    }
    if (desc->Name == 0) {
        Log::Line("Tobii eye tracking: the engine does not import %s on this build; nothing to "
                  "switch off", kEyeXDll);
        return true;
    }

    const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
    auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
    for (; names->u1.AddressOfData != 0; ++names, ++slots) {
        bool isInitialize = false;
        if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) {
            const auto* byName =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            isInitialize = std::strcmp(reinterpret_cast<const char*>(byName->Name), kInitializeName) == 0;
        }
        auto* slot = reinterpret_cast<ULONG_PTR*>(&slots->u1.Function);
        const ULONG_PTR original = *slot;
        const auto stub = reinterpret_cast<ULONG_PTR>(isInitialize ? &RefuseInitialize : &RefuseCall);
        if (!WriteSlot(slot, stub)) {
            Log::Line("ERROR: Tobii eye tracking: could not write the engine's import table "
                      "(error %lu); the game's eye tracking may move the view",
                      GetLastError());
            Remove();
            return false;
        }
        g_patched.push_back({slot, original});
    }
    Log::Line("Tobii eye tracking: %zu EyeX imports of the engine now refuse every call",
              g_patched.size());
    return true;
}

void Remove() {
    for (const Patched& p : g_patched) {
        if (!WriteSlot(p.slot, p.original)) {
            Log::Line("ERROR: Tobii eye tracking: could not restore an engine import (error %lu)",
                      GetLastError());
        }
    }
    g_patched.clear();
}

}  // namespace DyingLightHeadTracking::eyex_block
