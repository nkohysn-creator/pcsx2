// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once

// Exact SSE2 replacement for signed PMINSD followed by unsigned PMINUD.
// This is intended only for the existing matching finite-value bounds:
// each unsigned negative bound has the same bits as (signed positive bound XOR 0x80000000).
#if defined(PCSX2_EXPERIMENTAL_SSE2)
namespace SSE2JitCompat
{
inline void ClampToFinite(const x86Emitter::xRegisterSSE& reg, const x86Emitter::xIndirectVoid& max_bound)
{
	using namespace x86Emitter;
	alignas(16) static const u32 sign_bias[4] = {0x80000000, 0x80000000, 0x80000000, 0x80000000};

	// The scratch XMM is preserved on the generated-code stack and never kept
	// as an untracked live value in the EE/VU1 register allocators.
	const xRegisterSSE& scratch = (reg.Id == xmm15.Id) ? xmm14 : xmm15;
	xSUB(rsp, 16);
	xMOVUPS(ptr128[rsp], scratch);

	// Signed minimum: cmpgt mask selects the upper finite bound.
	xMOVAPS(scratch, reg);
	xPCMP.GTD(scratch, max_bound);
	xPANDN(scratch, reg);
	xPCMP.GTD(reg, max_bound);
	xPAND(reg, max_bound);
	xPOR(reg, scratch);

	// Unsigned minimum: XOR with the sign bit converts unsigned ordering
	// into signed ordering. The biased lower bound equals max_bound here.
	xPXOR(reg, ptr128[sign_bias]);
	xMOVAPS(scratch, reg);
	xPCMP.GTD(scratch, max_bound);
	xPANDN(scratch, reg);
	xPCMP.GTD(reg, max_bound);
	xPAND(reg, max_bound);
	xPOR(reg, scratch);
	xPXOR(reg, ptr128[sign_bias]);

	xMOVUPS(scratch, ptr128[rsp]);
	xADD(rsp, 16);
}
} // namespace SSE2JitCompat
#endif
