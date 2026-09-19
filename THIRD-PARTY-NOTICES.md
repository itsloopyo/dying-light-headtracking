# Third-Party Notices

This mod links against and ships alongside the components below. Their licences
are reproduced here, or travel beside the component in the release ZIP, because
the binary distribution has to carry them.

## cameraunlock-core

- **Version:** `8d18bb3859015b001574ebcb50302b093bc85dac`
- **License:** MIT
- **Upstream:** https://github.com/itsloopyo/cameraunlock-core
- **Usage:** Statically linked into `DyingLightHeadTracking.asi` for the OpenTrack UDP receiver, pose interpolation and smoothing, the position processor, the lean clamp policy, the FOV zoom compensation, the hotkey poller, the RTTI vtable lookup, the byte-pattern scanner, the PE fingerprint reader, the INI reader, the file log and the crash handler.
- **Bundled:** yes

Copyright (c) 2026 itsloopyo. Same MIT terms as this mod's own `LICENSE`, which
ships in the release ZIP.

---

## MinHook

- **Version:** v1.3.4, vendored inside cameraunlock-core with one local change:
  `MH_Initialize` uses the process heap instead of creating a private one. The
  change is recorded in `cameraunlock-core/vendor/minhook/LOCAL-CHANGES.md`.
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** Statically linked into `DyingLightHeadTracking.asi` to detour the engine's camera entry point and its world raycast.
- **Bundled:** yes

MinHook includes the Hacker Disassembler Engine 32/64, under the same
BSD-2-Clause terms. Its licence, reproduced in full as the binary distribution
requires:

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## Ultimate ASI Loader

- **Version:** v9.7.4 (`6b440669144c4a0bef5718ab155df160d231cd42`)
- **License:** MIT
- **Upstream:** https://github.com/ThirteenAG/Ultimate-ASI-Loader
- **Usage:** Loads `DyingLightHeadTracking.asi` into the game. The upstream `dinput8.dll` is installed unmodified into the game folder as `winmm.dll`.
- **Bundled:** yes. Bundled in release ZIP from `vendor/ultimate-asi-loader/`, which is the install-time source; the installer does not fetch it from the network.

Copyright (c) 2023 ThirteenAG. The upstream `LICENSE` travels with it in every
release ZIP.

---

## OpenTrack

- **Version:** n/a (protocol only)
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** The mod receives head pose over OpenTrack's UDP protocol. No OpenTrack code is included.
- **Bundled:** no

---

## Dying Light

Dying Light is Copyright (c) Techland. This mod is unofficial and is not
affiliated with or endorsed by Techland.

No game code, no extracted game assets and no game data files are copied,
decompiled or redistributed here. The mod calls the game's own exported C++
functions at run time and reads nothing out of the game's files on disk.

### Where the engine details in this repository came from

`engine_x64_rwdi.dll` exports 6817 decorated C++ symbols and
`gamedll_x64_rwdi.dll` imports 2387 of them. Every function and interface this
mod uses is one of those exports, and the decorated names carry their own
signatures - the class, the parameter types and the return type - so the
declarations in `src/engine_api.h` are transcriptions of the exported names and
of the Microsoft C++ x64 calling convention, not of any Techland source.

Three facts in `src/engine_api.cpp` are about the shape of the shipped binary
rather than about its names: that `ILevel::IsLoading` reads the game object
through a RIP-relative load in its first instruction, that `IBaseCamera`'s
engine object hangs off `+8`, and that `IBaseCamera::PointToScreen` returns
pixels measured from the top-left. Those come from the shipped binary's
instructions. They are statements about a layout, not copied code.
