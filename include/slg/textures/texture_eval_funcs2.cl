// code continue from texture_eval_funcs1.cl. I have to split the string constant
// in multiple parts because VisulStudio C++ limit of 64 kbytes

		//----------------------------------------------------------------------
		// CHECKERBOARD2D
		//----------------------------------------------------------------------
		case CHECKERBOARD2D: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = CheckerBoard2DTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, &tex->checkerBoard2D.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = CheckerBoard2DTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, &tex->checkerBoard2D.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// CHECKERBOARD3D
		//----------------------------------------------------------------------
		case CHECKERBOARD3D: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = CheckerBoard3DTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, &tex->checkerBoard3D.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = CheckerBoard3DTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, &tex->checkerBoard3D.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// CLOUD_TEX
		//----------------------------------------------------------------------
		case CLOUD_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = CloudTexture_ConstEvaluateFloat(hitPoint,
							tex->cloud.radius, tex->cloud.numspheres,
							tex->cloud.spheresize, tex->cloud.sharpness,
							tex->cloud.basefadedistance, tex->cloud.baseflatness,
							tex->cloud.variability, tex->cloud.omega,
							tex->cloud.noisescale, tex->cloud.noiseoffset,
							tex->cloud.turbulence, tex->cloud.octaves,
							&tex->cloud.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = CloudTexture_ConstEvaluateSpectrum(hitPoint,
							tex->cloud.radius, tex->cloud.numspheres,
							tex->cloud.spheresize, tex->cloud.sharpness,
							tex->cloud.basefadedistance, tex->cloud.baseflatness,
							tex->cloud.variability, tex->cloud.omega,
							tex->cloud.noisescale, tex->cloud.noiseoffset,
							tex->cloud.turbulence, tex->cloud.octaves,
							&tex->cloud.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// FBM_TEX
		//----------------------------------------------------------------------
		case FBM_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = FBMTexture_ConstEvaluateFloat(hitPoint,
							tex->fbm.omega, tex->fbm.octaves,
							&tex->fbm.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = FBMTexture_ConstEvaluateSpectrum(hitPoint,
							tex->fbm.omega, tex->fbm.octaves,
							&tex->fbm.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// MARBLE
		//----------------------------------------------------------------------
		case MARBLE: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = MarbleTexture_ConstEvaluateFloat(hitPoint,
							tex->marble.scale, tex->marble.omega,
							tex->marble.octaves, tex->marble.variation,
							&tex->marble.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = MarbleTexture_ConstEvaluateSpectrum(hitPoint,
							tex->marble.scale, tex->marble.omega,
							tex->marble.octaves, tex->marble.variation,
							&tex->marble.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// DOTS
		//----------------------------------------------------------------------
		case DOTS: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = DotsTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, &tex->checkerBoard2D.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = DotsTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, &tex->checkerBoard2D.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BRICK
		//----------------------------------------------------------------------
		case BRICK: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2, tex3;
					EvalStack_PopFloat(tex3);
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = BrickTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, tex3,
							tex->brick.bond,
							tex->brick.brickwidth, tex->brick.brickheight,
							tex->brick.brickdepth, tex->brick.mortarsize,
							(float3)(tex->brick.offsetx, tex->brick.offsety, tex->brick.offsetz),
							tex->brick.run , tex->brick.mortarwidth,
							tex->brick.mortarheight, tex->brick.mortardepth,
							tex->brick.proportion, tex->brick.invproportion,
							&tex->brick.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2, tex3;
					EvalStack_PopFloat3(tex3);
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = BrickTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, tex3,
							tex->brick.bond,
							tex->brick.brickwidth, tex->brick.brickheight,
							tex->brick.brickdepth, tex->brick.mortarsize,
							(float3)(tex->brick.offsetx, tex->brick.offsety, tex->brick.offsetz),
							tex->brick.run , tex->brick.mortarwidth,
							tex->brick.mortarheight, tex->brick.mortardepth,
							tex->brick.proportion, tex->brick.invproportion,
							&tex->brick.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// WINDY
		//----------------------------------------------------------------------
		case WINDY: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = WindyTexture_ConstEvaluateFloat(hitPoint,
							&tex->windy.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = WindyTexture_ConstEvaluateSpectrum(hitPoint,
							&tex->windy.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// WRINKLED
		//----------------------------------------------------------------------
		case WRINKLED: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = WrinkledTexture_ConstEvaluateFloat(hitPoint,
							tex->wrinkled.omega, tex->wrinkled.octaves,
							&tex->wrinkled.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = WrinkledTexture_ConstEvaluateSpectrum(hitPoint,
							tex->wrinkled.omega, tex->wrinkled.octaves,
							&tex->wrinkled.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// UV_TEX
		//----------------------------------------------------------------------
		case UV_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = UVTexture_ConstEvaluateFloat(hitPoint,
							&tex->uvTex.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = UVTexture_ConstEvaluateSpectrum(hitPoint,
							&tex->uvTex.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BAND_TEX
		//----------------------------------------------------------------------
		case BAND_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = BandTexture_ConstEvaluateFloat(hitPoint,
							tex->band.interpType, tex->band.size,
							tex->band.offsets, tex->band.values,
							tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float3 eval = BandTexture_ConstEvaluateSpectrum(hitPoint,
							tex->band.interpType, tex->band.size,
							tex->band.offsets, tex->band.values,
							tex1);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// For the very special case of Triplanar texture evaluation
		//
		// EVAL_TRIPLANAR_STEP_1, EVAL_TRIPLANAR_STEP_2, EVAL_TRIPLANAR_STEP_3
		// and EVAL_TRIPLANAR
		//----------------------------------------------------------------------
		case TRIPLANAR_TEX: {
			switch (evalType) {
				case EVAL_TRIPLANAR_STEP_1: {
					// Save original UV
					const uint uvIndex = tex->triplanarTex.uvIndex;
					EvalStack_PushFloat(hitPoint->uv[uvIndex].u);
					EvalStack_PushFloat(hitPoint->uv[uvIndex].v);

					// Compute localPoint
					float3 localShadeN;
					const float3 localPoint = TextureMapping3D_Map(&tex->triplanarTex.mapping,
							hitPoint, &localShadeN);

					// Compute the 3 weights
					float weightsX = Sqr(Sqr(localShadeN.x));
					float weightsY = Sqr(Sqr(localShadeN.y));
					float weightsZ = Sqr(Sqr(localShadeN.z));

					const float sum = weightsX + weightsY + weightsZ;
					weightsX = weightsX / sum;
					weightsY = weightsY / sum;
					weightsZ = weightsZ / sum;

					// Save 3 weights	
					EvalStack_PushFloat(weightsX);
					EvalStack_PushFloat(weightsY);
					EvalStack_PushFloat(weightsZ);
					// Save localPoint
					EvalStack_PushFloat3(localPoint);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					hitPointTmp->uv[uvIndex].u = localPoint.y;
					hitPointTmp->uv[uvIndex].v = localPoint.z;
					break;
				}
				case EVAL_TRIPLANAR_STEP_2: {
					// Read localPoint
					//
					// Note: -3 is there to skip the result of EVAL_TRIPLANAR_STEP_1
					const float3 localPoint = EvalStack_ReadFloat3(-3 - 3);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					const uint uvIndex = tex->triplanarTex.uvIndex;
					hitPointTmp->uv[uvIndex].u = localPoint.x;
					hitPointTmp->uv[uvIndex].v = localPoint.z;
					break;
				}
				case EVAL_TRIPLANAR_STEP_3: {
					// Read localPoint
					//
					// Note: -6 is there to skip the result of EVAL_TRIPLANAR_STEP_1 and EVAL_TRIPLANAR_STEP_2
					const float3 localPoint = EvalStack_ReadFloat3(-3 - 6);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					const uint uvIndex = tex->triplanarTex.uvIndex;
					hitPointTmp->uv[uvIndex].u = localPoint.x;
					hitPointTmp->uv[uvIndex].v = localPoint.y;
					break;
				}
				case EVAL_FLOAT:
				case EVAL_SPECTRUM: {
					// Read textures evaluation
					float3 tex1, tex2, tex3;
					EvalStack_PopFloat3(tex3);
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);
					
					// Read localPoint
					float3 localPoint;
					EvalStack_PopFloat3(localPoint);

					// Read 3 weights
					float weightX, weightY, weightZ;
					EvalStack_PopFloat(weightZ);
					EvalStack_PopFloat(weightY);
					EvalStack_PopFloat(weightX);

					// Restore original UV
					const uint uvIndex = tex->triplanarTex.uvIndex;
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					EvalStack_PopFloat(hitPointTmp->uv[uvIndex].v);
					EvalStack_PopFloat(hitPointTmp->uv[uvIndex].u);

					const float3 result = tex1 * weightX + tex2 * weightY + tex3 * weightZ;
					if (evalType == EVAL_FLOAT) {
						EvalStack_PushFloat(Spectrum_Y(result));
					} else {
						EvalStack_PushFloat3(result);
					}
					break;
				}
				case EVAL_BUMP_TRIPLANAR_STEP_1: {
					// Save original hit point
					const float3 p = VLOAD3F(&hitPoint->p.x);
					EvalStack_PushFloat3(p);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					hitPointTmp->p.x = p.x + sampleDistance;
					hitPointTmp->p.y = p.y;
					hitPointTmp->p.z = p.z;
					break;
				}
				case EVAL_BUMP_TRIPLANAR_STEP_2: {
					// Read original hit point
					//
					// Note: -1 is there to skip the result of EVAL_BUMP_TRIPLANAR_STEP_1
					const float3 p = EvalStack_ReadFloat3(-1 - 3);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					hitPointTmp->p.x = p.x;
					hitPointTmp->p.y = p.y + sampleDistance;
					hitPointTmp->p.z = p.z;
					break;
				}
				case EVAL_BUMP_TRIPLANAR_STEP_3: {
					// Read original hit point
					//
					// Note: -2 is there to skip the result of EVAL_BUMP_TRIPLANAR_STEP_2
					const float3 p = EvalStack_ReadFloat3(-2 - 3);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					hitPointTmp->p.x = p.x;
					hitPointTmp->p.y = p.y;
					hitPointTmp->p.z = p.z + sampleDistance;
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					if (tex->triplanarTex.enableUVlessBumpMap) {
						float evalFloatTexBase, evalFloatTexOffsetX,
								evalFloatTexOffsetY, evalFloatTexOffsetZ;

						// Read X/Y/Z textures evaluation
						EvalStack_PopFloat(evalFloatTexOffsetZ);
						EvalStack_PopFloat(evalFloatTexOffsetY);
						EvalStack_PopFloat(evalFloatTexOffsetX);

						// Read and restore original hit point
						float3 p;
						EvalStack_PopFloat3(p);
						__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
						VSTORE3F(p, &hitPointTmp->p.x);

						// Read base textures evaluation
						EvalStack_PopFloat(evalFloatTexBase);

						const float3 shadeN = TriplanarTexture_BumpUVLess(hitPoint,
								sampleDistance, evalFloatTexBase, evalFloatTexOffsetX,
								evalFloatTexOffsetY, evalFloatTexOffsetZ);
						EvalStack_PushFloat3(shadeN);
					} else
						Texture_EvalOpGenericBump(evalStack, evalStackOffset,
								hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// FRESNELCOLOR_TEX
		//----------------------------------------------------------------------
		case FRESNELCOLOR_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = FresnelColorTexture_ConstEvaluateFloat();
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = FresnelColorTexture_ConstEvaluateSpectrum();
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// FRESNELCONST_TEX
		//----------------------------------------------------------------------
		case FRESNELCONST_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = FresnelColorTexture_ConstEvaluateFloat();
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = FresnelColorTexture_ConstEvaluateSpectrum();
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		default:
			// Something wrong here
			break;
	}
}

//------------------------------------------------------------------------------
// Texture_GetFloatValue()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float Texture_GetFloatValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("===============================================================\n");
	printf("Texture_GetFloatValue()\n");
	printf("===============================================================\n");
#endif

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalFloatOpStartIndex = startTex->evalFloatOpStartIndex;
	const uint evalFloatOpLength = startTex->evalFloatOpLength;

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("texIndex=%d evalFloatOpStartIndex=%d evalFloatOpLength=%d\n", texIndex, evalFloatOpStartIndex, evalFloatOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalFloatOpLength; ++i) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
		printf("EvalOp: #%d\n", i);
#endif

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalFloatOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint, 0.f TEXTURES_PARAM);
	}
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	float result;
	EvalStack_PopFloat(result);

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("Result=%f\n", result);
#endif
	
	return result;
}

//------------------------------------------------------------------------------
// Texture_GetSpectrumValue()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Texture_GetSpectrumValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("===============================================================\n");
	printf("Texture_GetSpectrumValue()\n");
	printf("===============================================================\n");
#endif

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalSpectrumOpStartIndex = startTex->evalSpectrumOpStartIndex;
	const uint evalSpectrumOpLength = startTex->evalSpectrumOpLength;

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("texIndex=%d evalSpectrumOpStartIndex=%d evalSpectrumOpLength=%d\n", texIndex, evalSpectrumOpStartIndex, evalSpectrumOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalSpectrumOpLength; ++i) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
		printf("EvalOp: #%d\n", i);
#endif

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalSpectrumOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint, 0.f TEXTURES_PARAM);
	}
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif
	
	float3 result;
	EvalStack_PopFloat3(result);


#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);
#endif

	return result;
}

//------------------------------------------------------------------------------
// Texture_Bump()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Texture_Bump(const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("===============================================================\n");
	printf("Texture_Bump()\n");
	printf("===============================================================\n");
#endif

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalBumpOpStartIndex = startTex->evalBumpOpStartIndex;
	const uint evalBumpOpLength = startTex->evalBumpOpLength;

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("texIndex=%d evalSpectrumOpStartIndex=%d evalSpectrumOpLength=%d\n", texIndex, evalBumpOpStartIndex, evalBumpOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalBumpOpLength; ++i) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
		printf("EvalOp: #%d\n", i);
#endif

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalBumpOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint, sampleDistance TEXTURES_PARAM);
	}
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	float3 result;
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("Result=(%f, %f, %f)\n", result.x, result.y, result.z);
#endif

	return result;
}
