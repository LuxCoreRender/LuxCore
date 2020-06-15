/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and proprietary
 * rights in and to this software, related documentation and any modifications thereto.
 * Any use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation is strictly
 * prohibited.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
 * AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
 * SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
 * LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
 * BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES
 */

/// @file
/// @author NVIDIA Corporation
/// @brief  OptiX public API header

#ifndef __optix_optix_function_table_h__
#define __optix_optix_function_table_h__

/// The OptiX ABI version.
#define OPTIX_ABI_VERSION 22

#ifndef OPTIX_DEFINE_ABI_VERSION_ONLY

#include "optix_types.h"

#if !defined( OPTIX_DONT_INCLUDE_CUDA )
// If OPTIX_DONT_INCLUDE_CUDA is defined, cuda driver types must be defined through other
// means before including optix headers.
#include <cuda.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// \defgroup optix_function_table Function Table
/// \brief OptiX Function Table

/** \addtogroup optix_function_table
@{
*/

/// The function table containing all API functions.
///
/// See #optixInit() and #optixInitWithHandle().
typedef struct OptixFunctionTable
{
    /// \name Error handling
    //@ {

    /// See ::optixGetErrorName().
    const char* ( *optixGetErrorName )( OptixResult result );

    /// See ::optixGetErrorString().
    const char* ( *optixGetErrorString )( OptixResult result );

    //@ }
    /// \name Device context
    //@ {

    /// See ::optixDeviceContextCreate().
    OptixResult ( *optixDeviceContextCreate )( CUcontext fromContext, const OptixDeviceContextOptions* options, OptixDeviceContext* context );

    /// See ::optixDeviceContextDestroy().
    OptixResult ( *optixDeviceContextDestroy )( OptixDeviceContext context );

    /// See ::optixDeviceContextGetProperty().
    OptixResult ( *optixDeviceContextGetProperty )( OptixDeviceContext context, OptixDeviceProperty property, void* value, size_t sizeInBytes );

    /// See ::optixDeviceContextSetLogCallback().
    OptixResult ( *optixDeviceContextSetLogCallback )( OptixDeviceContext context,
                                                       OptixLogCallback   callbackFunction,
                                                       void*              callbackData,
                                                       unsigned int       callbackLevel );

    /// See ::optixDeviceContextSetCacheEnabled().
    OptixResult ( *optixDeviceContextSetCacheEnabled )( OptixDeviceContext context, int enabled );

    /// See ::optixDeviceContextSetCacheLocation().
    OptixResult ( *optixDeviceContextSetCacheLocation )( OptixDeviceContext context, const char* location );

    /// See ::optixDeviceContextSetCacheDatabaseSizes().
    OptixResult ( *optixDeviceContextSetCacheDatabaseSizes )( OptixDeviceContext context, size_t lowWaterMark, size_t highWaterMark );

    /// See ::optixDeviceContextGetCacheEnabled().
    OptixResult ( *optixDeviceContextGetCacheEnabled )( OptixDeviceContext context, int* enabled );

    /// See ::optixDeviceContextGetCacheLocation().
    OptixResult ( *optixDeviceContextGetCacheLocation )( OptixDeviceContext context, char* location, size_t locationSize );

    /// See ::optixDeviceContextGetCacheDatabaseSizes().
    OptixResult ( *optixDeviceContextGetCacheDatabaseSizes )( OptixDeviceContext context, size_t* lowWaterMark, size_t* highWaterMark );

    //@ }
    /// \name Modules
    //@ {

    /// See ::optixModuleCreateFromPTX().
    OptixResult ( *optixModuleCreateFromPTX )( OptixDeviceContext                 context,
                                               const OptixModuleCompileOptions*   moduleCompileOptions,
                                               const OptixPipelineCompileOptions* pipelineCompileOptions,
                                               const char*                        PTX,
                                               size_t                             PTXsize,
                                               char*                              logString,
                                               size_t*                            logStringSize,
                                               OptixModule*                       module );

    /// See ::optixModuleDestroy().
    OptixResult ( *optixModuleDestroy )( OptixModule module );

    //@ }
    /// \name Program groups
    //@ {

    /// See ::optixProgramGroupCreate().
    OptixResult ( *optixProgramGroupCreate )( OptixDeviceContext              context,
                                              const OptixProgramGroupDesc*    programDescriptions,
                                              unsigned int                    numProgramGroups,
                                              const OptixProgramGroupOptions* options,
                                              char*                           logString,
                                              size_t*                         logStringSize,
                                              OptixProgramGroup*              programGroups );

    /// See ::optixProgramGroupDestroy().
    OptixResult ( *optixProgramGroupDestroy )( OptixProgramGroup programGroup );

    /// See ::optixProgramGroupGetStackSize().
    OptixResult ( *optixProgramGroupGetStackSize )( OptixProgramGroup programGroup, OptixStackSizes* stackSizes );

    //@ }
    /// \name Pipeline
    //@ {

    /// See ::optixPipelineCreate().
    OptixResult ( *optixPipelineCreate )( OptixDeviceContext                 context,
                                          const OptixPipelineCompileOptions* pipelineCompileOptions,
                                          const OptixPipelineLinkOptions*    pipelineLinkOptions,
                                          const OptixProgramGroup*           programGroups,
                                          unsigned int                       numProgramGroups,
                                          char*                              logString,
                                          size_t*                            logStringSize,
                                          OptixPipeline*                     pipeline );

    /// See ::optixPipelineDestroy().
    OptixResult ( *optixPipelineDestroy )( OptixPipeline pipeline );

    /// See ::optixPipelineSetStackSize().
    OptixResult ( *optixPipelineSetStackSize )( OptixPipeline pipeline,
                                                unsigned int  directCallableStackSizeFromTraversal,
                                                unsigned int  directCallableStackSizeFromState,
                                                unsigned int  continuationStackSize,
                                                unsigned int  maxTraversableGraphDepth );

    //@ }
    /// \name Acceleration structures
    //@ {

    /// See ::optixAccelComputeMemoryUsage().
    OptixResult ( *optixAccelComputeMemoryUsage )( OptixDeviceContext            context,
                                                   const OptixAccelBuildOptions* accelOptions,
                                                   const OptixBuildInput*        buildInputs,
                                                   unsigned int                  numBuildInputs,
                                                   OptixAccelBufferSizes*        bufferSizes );

    /// See ::optixAccelBuild().
    OptixResult ( *optixAccelBuild )( OptixDeviceContext            context,
                                      CUstream                      stream,
                                      const OptixAccelBuildOptions* accelOptions,
                                      const OptixBuildInput*        buildInputs,
                                      unsigned int                  numBuildInputs,
                                      CUdeviceptr                   tempBuffer,
                                      size_t                        tempBufferSizeInBytes,
                                      CUdeviceptr                   outputBuffer,
                                      size_t                        outputBufferSizeInBytes,
                                      OptixTraversableHandle*       outputHandle,
                                      const OptixAccelEmitDesc*     emittedProperties,
                                      unsigned int                  numEmittedProperties );

    /// See ::optixAccelGetRelocationInfo().
    OptixResult ( *optixAccelGetRelocationInfo )( OptixDeviceContext context, OptixTraversableHandle handle, OptixAccelRelocationInfo* info );


    /// See ::optixAccelCheckRelocationCompatibility().
    OptixResult ( *optixAccelCheckRelocationCompatibility )( OptixDeviceContext              context,
                                                             const OptixAccelRelocationInfo* info,
                                                             int*                            compatible );

    /// See ::optixAccelRelocate().
    OptixResult ( *optixAccelRelocate )( OptixDeviceContext              context,
                                         CUstream                        stream,
                                         const OptixAccelRelocationInfo* info,
                                         CUdeviceptr                     instanceTraversableHandles,
                                         size_t                          numInstanceTraversableHandles,
                                         CUdeviceptr                     targetAccel,
                                         size_t                          targetAccelSizeInBytes,
                                         OptixTraversableHandle*         targetHandle );


    /// See ::optixAccelCompact().
    OptixResult ( *optixAccelCompact )( OptixDeviceContext      context,
                                        CUstream                stream,
                                        OptixTraversableHandle  inputHandle,
                                        CUdeviceptr             outputBuffer,
                                        size_t                  outputBufferSizeInBytes,
                                        OptixTraversableHandle* outputHandle );

    /// See ::optixConvertPointerToTraversableHandle().
    OptixResult ( *optixConvertPointerToTraversableHandle )( OptixDeviceContext      onDevice,
                                                             CUdeviceptr             pointer,
                                                             OptixTraversableType    traversableType,
                                                             OptixTraversableHandle* traversableHandle );

    //@ }
    /// \name Launch
    //@ {

    /// See ::optixConvertPointerToTraversableHandle().
    OptixResult ( *optixSbtRecordPackHeader )( OptixProgramGroup programGroup, void* sbtRecordHeaderHostPointer );

    /// See ::optixConvertPointerToTraversableHandle().
    OptixResult ( *optixLaunch )( OptixPipeline                  pipeline,
                                  CUstream                       stream,
                                  CUdeviceptr                    pipelineParams,
                                  size_t                         pipelineParamsSize,
                                  const OptixShaderBindingTable* sbt,
                                  unsigned int                   width,
                                  unsigned int                   height,
                                  unsigned int                   depth );

    //@ }
    /// \name Denoiser
    //@ {

    /// See ::optixDenoiserCreate().
    OptixResult ( *optixDenoiserCreate )( OptixDeviceContext context, const OptixDenoiserOptions* options, OptixDenoiser* returnHandle );

    /// See ::optixDenoiserDestroy().
    OptixResult ( *optixDenoiserDestroy )( OptixDenoiser handle );

    /// See ::optixDenoiserComputeMemoryResources().
    OptixResult ( *optixDenoiserComputeMemoryResources )( const OptixDenoiser handle,
                                                          unsigned int        maximumOutputWidth,
                                                          unsigned int        maximumOutputHeight,
                                                          OptixDenoiserSizes* returnSizes );

    /// See ::optixDenoiserSetup().
    OptixResult ( *optixDenoiserSetup )( OptixDenoiser denoiser,
                                         CUstream      stream,
                                         unsigned int  outputWidth,
                                         unsigned int  outputHeight,
                                         CUdeviceptr   state,
                                         size_t        stateSizeInBytes,
                                         CUdeviceptr   scratch,
                                         size_t        scratchSizeInBytes );


    /// See ::optixDenoiserInvoke().
    OptixResult ( *optixDenoiserInvoke )( OptixDenoiser              denoiser,
                                          CUstream                   stream,
                                          const OptixDenoiserParams* params,
                                          CUdeviceptr                denoiserState,
                                          size_t                     denoiserStateSizeInBytes,
                                          const OptixImage2D*        inputLayers,
                                          unsigned int               numInputLayers,
                                          unsigned int               inputOffsetX,
                                          unsigned int               inputOffsetY,
                                          const OptixImage2D*        outputLayer,
                                          CUdeviceptr                scratch,
                                          size_t                     scratchSizeInBytes );

    /// See ::optixDenoiserSetModel().
    OptixResult ( *optixDenoiserSetModel )( OptixDenoiser handle, OptixDenoiserModelKind kind, void* data, size_t sizeInBytes );

    /// See ::optixDenoiserComputeIntensity().
    OptixResult ( *optixDenoiserComputeIntensity )( OptixDenoiser       handle,
                                                    CUstream            stream,
                                                    const OptixImage2D* inputImage,
                                                    CUdeviceptr         outputIntensity,
                                                    CUdeviceptr         scratch,
                                                    size_t              scratchSizeInBytes );

    //@ }

} OptixFunctionTable;

/*@}*/  // end group optix_function_table

#ifdef __cplusplus
}
#endif

#endif /* OPTIX_DEFINE_ABI_VERSION_ONLY */

#endif /* __optix_optix_function_table_h__ */
