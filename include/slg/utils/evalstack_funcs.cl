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

// MAcros used mostly for Texture and Material evluations

#define EvalStack_PushUInt(a) { ((__global uint *)evalStack)[*evalStackOffset] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PopUInt(a) { *evalStackOffset = *evalStackOffset - 1; a = ((__global uint *)evalStack)[*evalStackOffset]; }

#define EvalStack_PushFloat(a) { evalStack[*evalStackOffset] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PushFloat2(a) { EvalStack_PushFloat(a.s0); EvalStack_PushFloat(a.s1); }
#define EvalStack_PushFloat3(a) { EvalStack_PushFloat(a.s0); EvalStack_PushFloat(a.s1); EvalStack_PushFloat(a.s2); }
#define EvalStack_PopFloat(a) { *evalStackOffset = *evalStackOffset - 1; a = evalStack[*evalStackOffset]; }
#define EvalStack_PopFloat2(a) { EvalStack_PopFloat(a.s1); EvalStack_PopFloat(a.s0); }
#define EvalStack_PopFloat3(a) { EvalStack_PopFloat(a.s2); EvalStack_PopFloat(a.s1); EvalStack_PopFloat(a.s0); }
#define EvalStack_ReadFloat(x) (evalStack[(*evalStackOffset) + x])
#define EvalStack_ReadFloat2(x) ((float2)(evalStack[(*evalStackOffset) + x], evalStack[(*evalStackOffset) + x + 1]))
#define EvalStack_ReadFloat3(x) ((float3)(evalStack[(*evalStackOffset) + x], evalStack[(*evalStackOffset) + x + 1], evalStack[(*evalStackOffset) + x + 2]))
