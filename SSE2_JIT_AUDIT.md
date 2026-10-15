# PCSX2 2.8.2 experimental SSE2 — JIT audit (in progress)

Starting point: experimental-sse2-v2.8.2 @ 6e5f52126e6eb0cc740cedc8f266f9933924f815.
Build 7 and 8 both stopped at JIT opcode 66 45 0F 3A 21 FB 10 (SSE4.1 INSERTPS).
This is a *preparation branch*; it does not trigger the production SSE2 build workflow.

## Prepared SSE2 substitutions (not compiled/tested yet)
- microVU_Alloc.inl: writeQreg INSERTPS with same-register-safe SSE2 path.
- microVU_Misc.inl: EXTRACTPS stores with PSHUFD/MOVSS; BLENDPS with per-lane SSE2 copies.
- microVU_Upper.inl: CLIP PBLENDW mask composition via SSE2.
- Vif_UnpackSSE.cpp: PMOVSX/PMOVZX widening and clearing the W lane via SSE2.
- iFPU.cpp: MFC1 INSERTPS replacement.
- iR5900Misc.cpp: MFSA BLENDPD replacement.

## Still requiring SSE2 rewrites before a full build/test
- microVU_Clamp.inl: PMIN.SD + PMIN.UD (signed/unsigned bitwise clamping).
- iFPU.cpp: fpuFloat3 PMIN.SD + PMIN.UD.
- iMMI.cpp: PEXTR.D, BLEND.PS, PMAX.SD, PABS.D/W, PMIN.SD,
  PBLEND.W, PMOVSX.DQ, SSSE3 MOVSHDUP/MOVSLDUP, PMUL.DQ and other cases.
- iCore.cpp: PINSR.Q.
- ix86-32/iR5900Move.cpp and ix86-32/iR5900MultDiv.cpp: PINSR.Q/PEXTR.Q.
- Audit all x86 emitters and compiler intrinsics, including dynamic CPU feature dispatch.
- Review aliasing, register pressure, sign semantics and generated opcode coverage.

Do **not** merge the preparation branch into the auto-build branch before completing this audit and checking generated instructions.


## Second batch: VU1/EE functionality (SSE2)
- EE instruction families MMI0/MMI1/MMI2/MMI3 and MOVE/MULTDIV use PCSX2's interpreter fallback in this experimental build only. Standard EE JIT remains enabled for other categories; this is a functionality-first trade-off.
- EE iCore PINSRQ lane-0 writes are emitted as PUSH/MOVLPS/POP with upper XMM half preserved.
- VU1 and EE/FPU signed-then-unsigned integer clamp uses SSE2 PCMPGTD/PANDN/PAND/POR with a preserved scratch XMM, rather than approximate floating point MINPS/MAXPS.
- Not yet validated on a real Phenom II. Do not claim the game boots based on compilation success.

## Build 11: GTA startup DPPS/0x71 diagnosis (2026-10-14)
- Crash report: SSE2_ILLEGAL_INSTRUCTION(3).txt, 0xC000001D, JIT MEM_PRIVATE, bytes `66 0F 3A 40 DB 71`.
- Objdump decodes as `dpps xmm3,xmm3,0x71`, followed by `movss xmm15,xmm3`.
- Root source: `pcsx2/x86/microVU_Lower.inl`, `mVU_sumXYZ`, called by VU1 ELENG/ERLENG/ERSADD and another EFU path.
- SSE2 gated replacement: MULPS, PSHUFD, ADDSS, PSLLDQ, PSRLDQ, scratch register acquired/released from microVU allocator. Retains register output lane masks and preserves PQ other lanes.
- Broad review of `pcsx2/x86/microVU_*.inl` found no other active DPPS emission; legacy DPPS is retained only under non-experimental preprocessor branch.
- Build-only validation still required; game/runtime test on Phenom II pending. Do not mistake the BIOS success for complete VU1 support.

## Build 12 preparation — GS software JIT SSE4.1 texture instructions
- October 15, 2026 software renderer crash: EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D), PC 00000271F4F30334, MEM_PRIVATE JIT.
- Decoded: PEXTRD EAX,XMM0,1 (66 0F 3A 16 C0 01), immediately followed by PINSRD XMM5,[R13+RAX*4],1 (66 41 0F 3A 22 6C 85 00 01).
- Source: `GS/Renderers/SW/GSDrawScanlineCodeGenerator.all.cpp`, texture fetch `ReadTexelImpl`.
- SSE2-gated replacements of all PEXTRD/PINSRD call sites in that source, plus GSSetupPrim PEXTRD.
- These replacements use unaligned XMM stack stores/loads and preserve live GPR/XMM state; potentially slower than native SSE4.1.
- No validation on Phenom II yet. Additional SW renderer SSE4.1 operations are possible; preserve last working hardware-rendering build 11.
