/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

/*!
 * \file
 *
 * \brief LuxCore is new the LuxRender C++/Python API.
 * \author Bucciarelli David et al.
 * \version 1.7
 * \date October 2013
 *
 */

/*!
 * \mainpage
 * \section intro Introduction
 * LuxCore is the new LuxRender C++/Python API. It can be used to create and
 * render scenes. It includes the support for advanced new features like editing
 * materials, lights, geometry, interactive rendering and more.
 */

#ifndef _LUXCORE_H
#define	_LUXCORE_H

#include <cstddef>
#include <stdexcept>
#include <string>

#include <luxcore/cfg.h>
#include <luxrays/utils/properties.h>
#include <luxrays/utils/proputils.h>
#include <luxrays/utils/exportdefs.h>
#include <luxrays/utils/cyhair/cyHairFile.h>
#include <slg/renderconfig.h>
#include <slg/rendersession.h>
#include <slg/renderstate.h>
#include <slg/scene/scene.h>
#include <slg/film/film.h>

/*! \mainpage LuxCore
 *
 * \section intro_sec Introduction
 *
 * LuxCore is the new LuxRender C++ API.
 */

/*!
 * \namespace luxcore
 *
 * \brief The LuxCore classes are defined within this namespace.
 */
namespace luxcore {

class FilmImpl;	

CPP_EXPORT CPP_API void (*LuxCore_LogHandler)(const char *msg); // LuxCore Log Handler

#define LC_LOG(a) { if (luxcore::LuxCore_LogHandler) { std::stringstream _LUXCORE_LOG_LOCAL_SS; _LUXCORE_LOG_LOCAL_SS << a; luxcore::LuxCore_LogHandler(_LUXCORE_LOG_LOCAL_SS.str().c_str()); } }

/*!
 * \brief Initializes LuxCore API. This function has to be called before
 * anything else. This function is thread safe. It can be called multiple times
 * if you want to update the log handler.
 *
 * \param LogHandler is a pointer to a function receiving all LuxCore log messages.
 */
CPP_EXPORT CPP_API void Init(void (*LogHandler)(const char *) = NULL);

/*!
 * \brief Parses a scene described using LuxRender SDL (Scene Description Language).
 *
 * \param fileName is the name of the file to parse.
 * \param renderConfig is where the rendering configuration properties are returned.
 * \param scene is where the scene properties are returned.
 */
CPP_EXPORT CPP_API void ParseLXS(const std::string &fileName, luxrays::Properties &renderConfig, luxrays::Properties &scene);

/*!
 * \brief File the OpenCL kernel cache with entries
 *
 * \param config defines how to fill the cache. The supported properties are:
 * kernelcachefill.renderengine.types, kernelcachefill.sampler.types,
 * kernelcachefill.camera.types, kernelcachefill.geometry.types, kernelcachefill.light.types,
 * kernelcachefill.material.types, kernelcachefill.texture.types. They can be used to
 * define the list of types to use, for instance with a
 * Property("kernelcachefill.renderengine.types")("PATHOCL", "TILEPATHOCL", "RTPATHOCL").
 */
CPP_EXPORT CPP_API void KernelCacheFill(const luxrays::Properties &config, void (*ProgressHandler)(const size_t, const size_t) = NULL);

class RenderSession;

/*!
 * \brief Film stores all the outputs of a rendering. It can be obtained
 * from a RenderSession or as stand alone object loaded from a file.
 */
CPP_EXPORT class CPP_API Film {
public:
	/*!
	* \brief Types of Film channel available.
	*/
	typedef enum {
		// This list must be aligned with slg::FilmOutputs::FilmOutputType
		OUTPUT_RGB,
		OUTPUT_RGBA,
		OUTPUT_RGB_IMAGEPIPELINE,
		OUTPUT_RGBA_IMAGEPIPELINE,
		OUTPUT_ALPHA,
		OUTPUT_DEPTH,
		OUTPUT_POSITION,
		OUTPUT_GEOMETRY_NORMAL,
		OUTPUT_SHADING_NORMAL,
		OUTPUT_MATERIAL_ID,
		OUTPUT_DIRECT_DIFFUSE,
		OUTPUT_DIRECT_GLOSSY,
		OUTPUT_EMISSION,
		OUTPUT_INDIRECT_DIFFUSE,
		OUTPUT_INDIRECT_GLOSSY,
		OUTPUT_INDIRECT_SPECULAR,
		OUTPUT_MATERIAL_ID_MASK,
		OUTPUT_DIRECT_SHADOW_MASK,
		OUTPUT_INDIRECT_SHADOW_MASK,
		OUTPUT_RADIANCE_GROUP,
		OUTPUT_UV,
		OUTPUT_RAYCOUNT,
		OUTPUT_BY_MATERIAL_ID,
		OUTPUT_IRRADIANCE,
		OUTPUT_OBJECT_ID,
		OUTPUT_OBJECT_ID_MASK ,
		OUTPUT_BY_OBJECT_ID,
		OUTPUT_FRAMEBUFFER_MASK
	} FilmOutputType;

	/*!
	 * \brief Types of Film channel available.
	 */
	typedef enum {
		// This list must be aligned with slg::Film::FilmChannelType
		CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED = 1 << 0,
		CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED = 1 << 1,
		CHANNEL_ALPHA = 1 << 2,
		CHANNEL_IMAGEPIPELINE = 1 << 3,
		CHANNEL_DEPTH = 1 << 4,
		CHANNEL_POSITION = 1 << 5,
		CHANNEL_GEOMETRY_NORMAL = 1 << 6,
		CHANNEL_SHADING_NORMAL = 1 << 7,
		CHANNEL_MATERIAL_ID = 1 << 8,
		CHANNEL_DIRECT_DIFFUSE = 1 << 9,
		CHANNEL_DIRECT_GLOSSY = 1 << 10,
		CHANNEL_EMISSION = 1 << 11,
		CHANNEL_INDIRECT_DIFFUSE = 1 << 12,
		CHANNEL_INDIRECT_GLOSSY = 1 << 13,
		CHANNEL_INDIRECT_SPECULAR = 1 << 14,
		CHANNEL_MATERIAL_ID_MASK = 1 << 15,
		CHANNEL_DIRECT_SHADOW_MASK = 1 << 16,
		CHANNEL_INDIRECT_SHADOW_MASK = 1 << 17,
		CHANNEL_UV = 1 << 18,
		CHANNEL_RAYCOUNT = 1 << 19,
		CHANNEL_BY_MATERIAL_ID = 1 << 20,
		CHANNEL_IRRADIANCE = 1 << 21,
		CHANNEL_OBJECT_ID = 1 << 22,
		CHANNEL_OBJECT_ID_MASK = 1 << 23,
		CHANNEL_BY_OBJECT_ID = 1 << 24,
		CHANNEL_FRAMEBUFFER_MASK = 1 << 25
	} FilmChannelType;

	virtual ~Film();

	/*!
	 * \brief Loads a stand alone Film (i.e. not connected to a rendering session)
	 * from a file.
	 * 
	 * \param fileName is the name of the file with the serialized film to read.
	 */
	static Film *Create(const std::string &fileName);

	/*!
	 * \brief Returns the Film width.
	 * 
	 * \return the Film width.
	 */
	virtual unsigned int GetWidth() const = 0;
	/*!
	 * \brief Returns the Film height.
	 * 
	 * \return the Film width.
	 */
	virtual unsigned int GetHeight() const = 0;
	/*!
	 * \brief Saves all Film output channels defined in the current
	 * RenderSession. This method can not be used with a standalone film.
	 */
	virtual void SaveOutputs() const = 0;

	/*!
	 * \brief Saves the specified Film output channels.
	 * 
	 * \param fileName is the name of the file where to save the output channel.
	 * \param type is the Film output channel to use. It must be one
	 * of the enabled channels.
	 * \param props can include some additional information defined by the
	 * following property:
	 * "id" for the ID of MATERIAL_ID_MASK,
	 * "id" for the index of RADIANCE_GROUP,
	 * "id" for the ID of BY_MATERIAL_ID.
	 * "id" for the ID of OBJECT_ID_MASK,
	 * "id" for the ID of BY_OBJECT_ID.
	 */
	virtual void SaveOutput(const std::string &fileName, const FilmOutputType type, const luxrays::Properties &props) const = 0;

	/*!
	 * \brief Serializes a Film in a file.
	 * 
	 * \param fileName is the name of the file where to serialize the film.
	 */
	virtual void SaveFilm(const std::string &fileName) const = 0;

	/*!
	 * \brief Returns the total sample count.
	 *
	 * \return the total sample count.
	 */
	virtual double GetTotalSampleCount() const = 0;
	/*!
	 * \brief Returns the size (in float or unsigned int) of a Film output channel.
	 *
	 * \param type is the Film output channel to use.
	 *
	 * \return the size (in float or unsigned int) of a Film output channel.
	 */
	virtual size_t GetOutputSize(const FilmOutputType type) const = 0;
	/*!
	 * \brief Returns if a film channel output is available.
	 *
	 * \param type is the Film output channel to use.
	 *
	 * \return true if the output is available, false otherwise.
	 */
	virtual bool HasOutput(const FilmOutputType type) const = 0;
	/*!
	 * \brief Returns the number of radiance groups.
	 *
	 * \return the number of radiance groups.
	 */
	virtual unsigned int GetRadianceGroupCount() const = 0;
	/*!
	 * \brief Fills the buffer with a Film output channel.
	 *
	 * \param type is the Film output channel to use. It must be one
	 * of the enabled channels in RenderConfig. The supported template types are
	 * float and unsigned int.
	 * \param buffer is the place where the data will be copied.
	 * \param index of the buffer to use. Usually 0, however, for instance,
	 * if more than one light group is used, select the group to return.
	 */
	template<class T> void GetOutput(const FilmOutputType type, T *buffer, const unsigned int index = 0) {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	/*!
	 * \brief Returns the number of channels of the passed type.
	 *
	 * \param type is the Film output channel to use.
	 *
	 * \return the number of channels. Returns 0 if the channel is not available.
	 */
	virtual unsigned int GetChannelCount(const FilmChannelType type) const = 0;
	/*!
	 * \brief Returns a pointer to the type of channel requested. The channel is
	 * not normalized (if it has a weight channel).
	 *
	 * \param type is the Film output channel to return. It must be one
	 * of the enabled channels in RenderConfig. The supported template types are
	 * float and unsigned int.
	 * \param index of the buffer to use. Usually 0, however, for instance,
	 * if more than one light group is used, select the group to return.
	 * 
	 * \return a pointer to the requested raw buffer.
	 */
	template<class T> const T *GetChannel(const FilmChannelType type, const unsigned int index = 0) {
		throw std::runtime_error("Called Film::GetChannel() with wrong type");
	}

	/*!
	 * \brief Sets configuration Properties with new values. This method can be
	 * used only when the Film is not in use by a RenderSession. Image pipeline
	 * and radiance scale values can be redefined with this method.
	 * 
	 * \param props are the Properties to set. 
	 */
	virtual void Parse(const luxrays::Properties &props) = 0;

	friend class RenderSession;

protected:
	virtual void GetOutputFloat(const FilmOutputType type, float *buffer, const unsigned int index) = 0;
	virtual void GetOutputUInt(const FilmOutputType type, unsigned int *buffer, const unsigned int index) = 0;
	
	virtual const float *GetChannelFloat(const FilmChannelType type, const unsigned int index) = 0;
	virtual const unsigned int *GetChannelUInt(const FilmChannelType type, const unsigned int index) = 0;

private:
	static Film *Create(const RenderSession &session);
};

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const unsigned int index);
template<> void Film::GetOutput<unsigned int>(const FilmOutputType type, unsigned int *buffer, const unsigned int index);
template<> const float *Film::GetChannel<float>(const FilmChannelType type, const unsigned int index);
template<> const unsigned int *Film::GetChannel<unsigned int>(const FilmChannelType type, const unsigned int index);

class Scene;

/*!
 * \brief Camera stores camera definitions.
 */
CPP_EXPORT class CPP_API Camera {
public:
	/*!
	* \brief Types of cameras.
	*/
	typedef enum {
		// This list must be aligned with slg::Camera::CameraType
		PERSPECTIVE,
		ORTHOGRAPHIC,
		STEREO,
		ENVIRONMENT
	} CameraType;

	~Camera();

	/*!
	 * \brief Returns the camera type.
	 *
	 * \return a camera type.
	 */
	const CameraType GetType() const;
	/*!
	 * \brief Translates by vector t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 *
	 * \param t is the translation vector.
	 */
	void Translate(const luxrays::Vector &t) const;
	/*!
	 * \brief Translates left by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param t is the translation distance.
	 */
	void TranslateLeft(const float t) const;
	/*!
	 * \brief Translates right by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 *
	 * \param t is the translation distance.
	 */
	void TranslateRight(const float t) const;
	/*!
	 * \brief Translates forward by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 *
	 * \param t is the translation distance.
	 */
	void TranslateForward(const float t) const;
	/*!
	 * \brief Translates backward by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 *
	 * \param t is the translation distance.
	 */
	void TranslateBackward(const float t) const;

	/*!
	 * \brief Rotates by angle around the axis. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is the rotation angle.
	 * \param axis is the rotation axis.
	 */
	void Rotate(const float angle, const luxrays::Vector &axis) const;
	/*!
	* \brief Rotates left by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is the rotation angle.
	 */
	void RotateLeft(const float angle) const;
	/*!
	 * \brief Rotates right by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is the rotation angle.
	 */
	void RotateRight(const float angle) const;
	/*!
	 * \brief Rotates up by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is the rotation angle.
	 */
	void RotateUp(const float angle) const;
	/*!
	 * \brief Rotates down by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is the rotation angle.
	 */
	void RotateDown(const float angle) const;

	friend class Scene;

private:
	Camera(const Scene &scene);
	
	const Scene &scene;
};

/*!
 * \brief Scene stores textures, materials and objects definitions.
 */
CPP_EXPORT class CPP_API Scene {
public:	
	/*!
	* \brief Types of image map channel selection.
	*/
	typedef enum {
		// This list must be aligned with slg::ImageMapStorage::ChannelSelectionType
		DEFAULT,
		RED,
		GREEN,
		BLUE,
		ALPHA,
		MEAN,
		WEIGHTED_MEAN,
		RGB
	} ChannelSelectionType;
	/*!
	* \brief Types of strands tessellation.
	*/
	typedef enum {
		// This list must be aligned with slg::StrendsShape::TessellationType
		TESSEL_RIBBON,
		TESSEL_RIBBON_ADAPTIVE,
		TESSEL_SOLID,
		TESSEL_SOLID_ADAPTIVE
	} StrandsTessellationType;

	/*!
	 * \brief Constructs a new empty Scene.
	 *
	 * \param imageScale defines the scale used for storing any kind of image in memory.
	 */
	Scene(const float imageScale = 1.f);
	/*!
	 * \brief Constructs a new Scene as defined in fileName file.
	 *
	 * \param fileName is the name of the file with the scene description to read.
	 * \param imageScale defines the scale used for storing any kind of image in memory.
	 */
	Scene(const std::string &fileName, const float imageScale = 1.f);
	~Scene();
	
	/*!
	 * \brief Returns the DataSet of the Scene. It is available only
	 * during the rendering (i.e. after a RenderSession::Start()).
	 *
	 * \return a reference to the DataSet of this Scene.
	 */
	const luxrays::DataSet &GetDataSet() const;
	/*!
	 * \brief Returns the Camera of the scene.
	 *
	 * \return a reference to the Camera of this Scene. It is available only
	 * during the rendering (i.e. after a RenderSession::Start()).
	 */
	const Camera &GetCamera() const;
	/*!
	 * \brief Defines an image map (to be later used in textures, infinite lights, etc.).
	 * The memory allocated for cols array is NOT freed by the Scene class nor
	 * is used after the execution of this method.
	 *
	 * \param imgMapName is the name of the defined image map.
	 * \param pixels is a pointer to an array of image map pixels.
	 * \param gamma is the gamma correction value of the image.
	 * \param channels is the number of data used for each pixel (1 or 3).
	 * \param width is the width of the image map.
	 * \param height is the height of the image map.
	 */
	template<class T> void DefineImageMap(const std::string &imgMapName,
			T *pixels, const float gamma, const unsigned int channels,
			const unsigned int width, const unsigned int height,
			ChannelSelectionType selectionType) {
		scene->DefineImageMap<T>(imgMapName, pixels, gamma, channels,
				width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType);
	}
	/*!
	 * \brief Check if an image map with the given name has been defined.
	 *
	 * \param imgMapName is the name to check.
	 *
	 * \return true if the image map has been defined, false otherwise.
	 */
	bool IsImageMapDefined(const std::string &imgMapName) const;
	/*!
	 * \brief Sets if the Scene class destructor will delete the arrays
	 * pointed to by the defined meshes.
	 *
	 * \param v defines if the Scene class destructor will delete the mesh data.
	 */
	void SetDeleteMeshData(const bool v);
	/*!
	 * \brief Defines a mesh (to be later used in one or more scene objects). The
	 * memory allocated for the ExtTriangleMesh is always freed by the Scene class,
	 * however freeing of memory for the vertices, triangle indices, etc. depends
	 * on the setting of SetDeleteMeshData().
	 *
	 * \param meshName is the name of the defined mesh.
	 * \param mesh is a pointer to the mesh to be used.
	 */
	void DefineMesh(const std::string &meshName, luxrays::ExtTriangleMesh *mesh);
	/*!
	 * \brief Defines a mesh (to be later used in one or more scene objects). The
	 * memory allocated for the ExtTriangleMesh is always freed by the Scene class,
	 * however freeing of memory for the vertices, triangle indices, etc. depends
	 * on the setting of SetDeleteMeshData().
	 * NOTE: vertices and triangles buffers MUST be allocated with
	 * Scene::AllocVerticesBuffer() and Scene::AllocTrianglesBuffer().
	 *
	 * \param meshName is the name of the defined mesh.
	 * \param plyNbVerts is the number of mesh vertices.
	 * \param plyNbTris is the number of mesh triangles.
	 * \param p is a pointer to an array of vertices. Embree accelerator has
	 * a very special requirement. The 4 bytes after the z-coordinate of the
	 * last vertex have to be readable memory, thus padding is required.
	 * \param vi is a pointer to an array of triangles.
	 * \param n is a pointer to an array of normals. It can be NULL.
	 * \param uv is a pointer to an array of UV coordinates. It can be NULL.
	 * \param cols is a pointer to an array of vertices colors. It can be NULL.
	 * \param alphas is a pointer to an array of vertices alphas. It can be NULL.
	 */
	void DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
		luxrays::Spectrum *cols, float *alphas);
	/*!
	 * \brief Save a previously defined mesh to file system in PLY format.
	 *
	 * \param meshName is the name of the defined mesh to be saved.
	 * \param fileName is the name of the file where to save the mesh.
	 */
	void SaveMesh(const std::string &meshName, const std::string &fileName);
	/*!
	 * \brief Defines a mesh (to be later used in one or more scene objects) starting
	 * from the strands/hairs definition included in strandsFile.
	 *
	 * \param shapeName is the name of the defined shape.
	 * \param strandsFile includes all information about the strands .
	 * \param tesselType is the tessellation used to transform the strands in a triangle mesh.
	 * \param adaptiveMaxDepth is maximum number of subdivisions for adaptive tessellation.
	 * \param adaptiveError is the error threshold for adaptive tessellation.
	 * \param solidSideCount is the number of sides for solid tessellation.
	 * \param solidCapBottom is a flag to set if strands has to have a bottom cap.
	 * \param solidCapTop is a flag to set if strands has to have a top cap.
	 * \param useCameraPosition is a flag to set if ribbon tessellation has to
	 * be faced toward the camera.
	 */
	void DefineStrands(const std::string &shapeName, const luxrays::cyHairFile &strandsFile,
		const StrandsTessellationType tesselType,
		const unsigned int adaptiveMaxDepth, const float adaptiveError,
		const unsigned int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition);
	/*!
	 * \brief Check if a mesh with the given name has been defined.
	 *
	 * \param meshName is the name to check.
	 *
	 * \return true if the mesh has been defined, false otherwise.
	 */
	bool IsMeshDefined(const std::string &meshName) const;
	/*!
	 * \brief Check if a texture with the given name has been defined.
	 *
	 * \param texName is the name to check.
	 *
	 * \return true if the texture has been defined, false otherwise.
	 */
	bool IsTextureDefined(const std::string &texName) const;
	/*!
	 * \brief Check if a material with the given name has been defined.
	 *
	 * \param matName is the name to check.
	 *
	 * \return true if the material has been defined, false otherwise.
	 */
	bool IsMaterialDefined(const std::string &matName) const;
	/*!
	 * \brief Returns the number of light sources in the Scene.
	 *
	 * \return the number of light sources in the Scene.
	 */	
	const unsigned int GetLightCount() const;
	/*!
	 * \brief Returns the number of objects in the Scene.
	 *
	 * \return the number of objects in the Scene.
	 */	
	const unsigned int GetObjectCount() const;

	/*!
	 * \brief Edits or creates camera, textures, materials and/or objects
	 * based on the Properties defined.
	 *
	 * \param props are the Properties with the definition of camera, textures,
	 * materials and/or objects.
	 */
	void Parse(const luxrays::Properties &props);

	/*!
	 * \brief Apply a transformation to an object
	 *
	 * \param objName is the name of the object to transform.
	 * \param trans is the transformation to apply.
	 */
	void UpdateObjectTransformation(const std::string &objName, const luxrays::Transform &trans);
	/*!
	 * \brief Apply a new material to an object
	 *
	 * \param objName is the name of the object to apply the material to.
	 * \param matName is the new material name.
	 */	
	void UpdateObjectMaterial(const std::string &objName, const std::string &matName);
	
	/*!
	 * \brief Deletes an object from the scene.
	 *
	 * \param objName is the name of the object to delete.
	 */
	void DeleteObject(const std::string &objName);

	/*!
	 * \brief Deletes a light from the scene.
	 *
	 * \param lightName is the name of the object to delete. Note: to delete
	 * area lights, use DeleteObject().
	 */
	void DeleteLight(const std::string &lightName);

	/*!
	 * \brief Removes all unused image maps.
	 */
	void RemoveUnusedImageMaps();
	/*!
	 * \brief Removes all unused textures.
	 */
	void RemoveUnusedTextures();
	/*!
	 * \brief Removes all unused materials.
	 */
	void RemoveUnusedMaterials();
	/*!
	 * \brief Removes all unused meshes.
	 */
	void RemoveUnusedMeshes();

	/*!
	 * \brief Returns all the Properties required to define this Scene.
	 *
	 * \return a reference to the Properties of this Scene.
	 */
	const luxrays::Properties &ToProperties() const;

	/*!
	 * \brief This must be used to allocate Mesh vertices buffer.
	 */
	static luxrays::Point *AllocVerticesBuffer(const unsigned int meshVertCount);
	/*!
	 * \brief This must be used to allocate Mesh triangles buffer.
	 */
	static luxrays::Triangle *AllocTrianglesBuffer(const unsigned int meshTriCount);

	friend class RenderConfig;
	friend class Camera;

private:
	Scene(slg::Scene *scn);

	mutable luxrays::Properties scenePropertiesCache;

	slg::Scene *scene;
	Camera camera;
	bool allocatedScene;
};

/*!
 * \brief RenderConfig stores all the configuration settings used to render a
 * scene.
 */
CPP_EXPORT class CPP_API RenderConfig {
public:
	/*!
	 * \brief Constructs a new RenderConfig using the provided Properties and
	 * (optional) Scene.
	 *
	 * \param props are the Properties used to build the new RenderConfig.
	 * \param scene is the Scene used to build the new RenderConfig. If specified,
	 * the Scene will not be deleted by the destructor. If NULL, the Scene will be
	 * read from the file specified in the "scene.file" Property and deleted by
	 * the destructor.
	 */
	RenderConfig(const luxrays::Properties &props, Scene *scene = NULL);
	~RenderConfig();

	/*!
	 * \brief Returns a reference to the Properties used to create the RenderConfig.
	 *
	 * \return the RenderConfig properties.
	 */
	const luxrays::Properties &GetProperties() const;
	/*!
	 * \brief Returns the Property with the given name or the default value if it
	 * has not been defined.
	 *
	 * \return the Property with the given name.
	 */
	const luxrays::Property GetProperty(const std::string &name) const;

	/*!
	 * \brief Returns a reference to all Properties (including default values)
	 * defining the RenderConfig.
	 *
	 * \return the RenderConfig properties.
	 */
	const luxrays::Properties &ToProperties() const;

	/*!
	 * \brief Returns a reference to the Scene used in the RenderConfig.
	 *
	 * \return the reference to the RenderConfig Scene.
	 */
	Scene &GetScene() const;

	/*!
	 * \brief Sets configuration Properties with new values. This method can be
	 * used only when the RenderConfig is not in use by a RenderSession.
	 * 
	 * \param props are the Properties to set. 
	 */
	void Parse(const luxrays::Properties &props);
	/*!
	 * \brief Deletes any configuration Property starting with the given prefix.
	 * This method can be used only when the RenderConfig is not in use by a
	 * RenderSession.
	 * 
	 * \param prefix is the prefix of the Properties to delete.
	 */
	void Delete(const std::string &prefix);

	/*!
	 * \brief Return the configured Film width, height, sub-region width, height,
	 * and if sub-region is enabled.
	 * 
	 * \param filmFullWidth is where the configured Film width is returned if the
	 * pointer is not NULL. 
	 * \param filmFullHeight is where the configured Film height is returned if the
	 * pointer is not NULL. 
	 * \param filmSubRegion is an array of 4 values with the horizontal
	 * (followed by the vertical) begin and end of the Film region to
	 * render (in pixels).
	 *
	 * \return true if there is a sub-region to render, false otherwise.
	 */
	bool GetFilmSize(unsigned int *filmFullWidth, unsigned int *filmFullHeight,
		unsigned int *filmSubRegion) const;

	/*!
	 * \brief Delete the scene passed to the constructor when the class
	 * destructor is invoked.
	 */
	void DeleteSceneOnExit();
	
	/*!
	 * \brief Returns a Properties container with all default values.
	 * 
	 * \return the default Properties.
	 */
	static const luxrays::Properties &GetDefaultProperties();

	friend class RenderSession;

private:
	slg::RenderConfig *renderConfig;

	Scene *scene;
	bool allocatedScene;
};

/*!
 * \brief RenderState is used to resume a rendering from a previous saved point.
 */
CPP_EXPORT class CPP_API RenderState {
public:
	/*!
	 * \brief Constructs a new RenderState from a file.
	 *
	 * \param fileName id the file name of the render state file to load.
	 */
	RenderState(const std::string &fileName);
	~RenderState();
	
	/*!
	 * \brief Serializes a RenderState in a file.
	 * 
	 * \param fileName is the name of the file where to serialize the render state.
	 */
	void Save(const std::string &fileName) const;

	friend class RenderSession;

protected:
	RenderState(slg::RenderState *state);

private:
	slg::RenderState *renderState;
};

/*!
 * \brief RenderSession executes a rendering based on the RenderConfig provided.
 */
CPP_EXPORT class CPP_API RenderSession {
public:
	/*!
	 * \brief Constructs a new RenderSession using the provided RenderConfig.
	 *
	 * \param config is the RenderConfig used to create the rendering session. The
	 * RenderConfig is not deleted by the destructor.
	 * \param startState is the optional RenderState to use to resume rendering. The
	 * memory for RenderState is freed by RenderSession.
	 * \param startFilm is the optional Film to use to resume rendering. The
	 * memory for Film is freed by RenderSession.
	 */
	RenderSession(const RenderConfig *config, RenderState *startState = NULL, Film *startFilm = NULL);

	/*!
	 * \brief Constructs a new RenderSession using the provided RenderConfig.
	 *
	 * \param config is the RenderConfig used to create the rendering session. The
	 * RenderConfig is not deleted by the destructor.
	 * \param startStateFileName is the file name of a RenderState to use to resume rendering.
	 * \param startFilmFileName is the file name of a Film to use to resume rendering.
	 */
	RenderSession(const RenderConfig *config, const std::string &startStateFileName, const std::string &startFilmFileName);

	~RenderSession();

	/*!
	 * \brief Returns a reference to the RenderingConfig used to create this
	 * RenderSession.
	 *
	 * \return a reference to the RenderingConfig.
	 */
	const RenderConfig &GetRenderConfig() const;

	/*!
	 * \brief Returns a pointer to the current RenderState. The session must be
	 * paused.
	 *
	 * \return a pointer to the RenderState.
	 */
	RenderState *GetRenderState();

	/*!
	 * \brief Starts the rendering.
	 */
	void Start();
	/*!
	 * \brief Stops the rendering.
	 */
	void Stop();

	/*!
	 * \brief It can be used to check if the session has been started.
	 */
	bool IsStarted() const;

	/*!
	 * \brief Stops the rendering and allows to edit the Scene.
	 */
	void BeginSceneEdit();
	/*!
	 * \brief Ends the Scene editing and start the rendering again.
	 */
	void EndSceneEdit();

	/*!
	 * \brief It can be used to check if the session is in scene editing mode.
	 */
	bool IsInSceneEdit() const;

	/*!
	 * \brief Pause the rendering.
	 */
	void Pause();

	/*!
	 * \brief Resume the rendering.
	 */
	void Resume();

	/*!
	 * \brief It can be used to check if the session is in scene editing mode.
	 */
	bool IsInPause() const;

	/*!
	 * \brief It can be used to check if the rendering is over.
	 */
	bool HasDone() const;

	/*!
	 * \brief Used to wait for the end of the rendering.
	 */
	void WaitForDone() const;

	/*!
	 * \brief Used to wait for the next frame with real-time render engines like
	 * RTPATHOCL. It does nothing with other render engines.
	 */
	void WaitNewFrame();

	/*!
	 * \brief Checks if it is time to save the film according to the RenderConfig.
	 *
	 * \return true if it is time to save the Film, false otherwise.
	 */
	bool NeedPeriodicFilmSave();
	/*!
	 * \brief Returns a reference to a Film with the output of the rendering.
	 *
	 * \return the reference to the Film.
	 */
	Film &GetFilm();

	/*!
	 * \brief Updates the statistics.
	 */
	void UpdateStats();
	/*!
	 * \brief Returns a list of statistics related to the ongoing rendering. The
	 * returned Properties is granted to have content only after the first call
	 * to the UpdateStats method.
	 *
	 * \return a Properties container with the statistics.
	 */
	const luxrays::Properties &GetStats() const;

	/*!
	 * \brief Dynamic edit the definition of RenderConfig properties
	 *
	 * \param props are the Properties with the definition of: film.imagepipeline(s).*,
	 * film.radiancescales.*, film.outputs.*, film.width or film.height.
	 */
	void Parse(const luxrays::Properties &props);

	friend class Film;
	friend class FilmImpl;

private:
	const RenderConfig *renderConfig;
	FilmImpl *film;

	slg::RenderSession *renderSession;
	luxrays::Properties stats;
};

}

#endif	/* _LUXCORE_H */
