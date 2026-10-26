#line 2 "evalstack_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

// Macros used mostly for Texture and Material evluations

#if defined(LUXRAYS_CUDA_DEVICE) && defined(PARAM_MATERIAL_EVAL_STACK_SIZE) && defined(PARAM_TEXTURE_EVAL_STACK_SIZE)
// With the stack sizes of the scene known at compile time, each evaluation
// uses a stack in CUDA local memory: interleaved per thread by the hardware
// (coalesced) and cached in L1. A global stack is written and read back
// within one evaluation, and the reads go to L2 because stores evict L1.
#define EVAL_STACK_STRIDE 1
#define MATERIAL_EVAL_STACK_DECL float evalStackLocal[PARAM_MATERIAL_EVAL_STACK_SIZE]; __global float *evalStack = evalStackLocal
#define TEXTURE_EVAL_STACK_DECL float evalStackLocal[PARAM_TEXTURE_EVAL_STACK_SIZE]; __global float *evalStack = evalStackLocal
#else
// The stacks of all threads are interleaved: element i of the stack of
// thread gid is at evalStack[i * EVAL_STACK_STRIDE] with
// evalStack = &stacks[gid], so the threads of a warp access consecutive
// addresses (coalesced) instead of one cache line each
#define EVAL_STACK_STRIDE get_global_size(0)
#define MATERIAL_EVAL_STACK_DECL __global float *evalStack = &matEvalStacks[get_global_id(0)]
#define TEXTURE_EVAL_STACK_DECL __global float *evalStack = &texEvalStacks[get_global_id(0)]
#endif
#define EVAL_STACK_ELEM(i) evalStack[(i) * EVAL_STACK_STRIDE]

#define EvalStack_PushInt(a) { ((__global int *)evalStack)[(*evalStackOffset) * EVAL_STACK_STRIDE] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PopInt(a) { *evalStackOffset = *evalStackOffset - 1; a = ((__global int *)evalStack)[(*evalStackOffset) * EVAL_STACK_STRIDE]; }

#define EvalStack_PushUInt(a) { ((__global uint *)evalStack)[(*evalStackOffset) * EVAL_STACK_STRIDE] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PopUInt(a) { *evalStackOffset = *evalStackOffset - 1; a = ((__global uint *)evalStack)[(*evalStackOffset) * EVAL_STACK_STRIDE]; }

#define EvalStack_PushFloat(a) { EVAL_STACK_ELEM(*evalStackOffset) = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PushFloat2(a) { EvalStack_PushFloat(a.x); EvalStack_PushFloat(a.y); }
#define EvalStack_PushFloat3(a) { EvalStack_PushFloat(a.x); EvalStack_PushFloat(a.y); EvalStack_PushFloat(a.z); }
#define EvalStack_PopFloat(a) { *evalStackOffset = *evalStackOffset - 1; a = EVAL_STACK_ELEM(*evalStackOffset); }
#define EvalStack_PopFloat2(a) { EvalStack_PopFloat(a.y); EvalStack_PopFloat(a.x); }
#define EvalStack_PopFloat3(a) { EvalStack_PopFloat(a.z); EvalStack_PopFloat(a.y); EvalStack_PopFloat(a.x); }
#define EvalStack_ReadFloat(x) (EVAL_STACK_ELEM((*evalStackOffset) + x))
#define EvalStack_ReadFloat2(x) (MAKE_FLOAT2(EVAL_STACK_ELEM((*evalStackOffset) + x), EVAL_STACK_ELEM((*evalStackOffset) + x + 1)))
#define EvalStack_ReadFloat3(x) (MAKE_FLOAT3(EVAL_STACK_ELEM((*evalStackOffset) + x), EVAL_STACK_ELEM((*evalStackOffset) + x + 1), EVAL_STACK_ELEM((*evalStackOffset) + x + 2)))

#define EvalStack_PushBSDFEvent(a) { ((__global BSDFEvent *)evalStack)[(*evalStackOffset) * EVAL_STACK_STRIDE] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PopBSDFEvent(a) { *evalStackOffset = *evalStackOffset - 1; a = ((__global BSDFEvent *)evalStack)[(*evalStackOffset) * EVAL_STACK_STRIDE]; }
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
