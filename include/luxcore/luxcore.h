/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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
 * \version 1.0
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
#include <luxrays/utils/exportdefs.h>
#include "luxrays/utils/cyhair/cyHairFile.h"
#include <slg/renderconfig.h>
#include <slg/rendersession.h>
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

class RenderSession;

/*!
 * \brief Film stores all the outputs of a rendering. It can be obtained only
 * from a RenderSession.
 */
CPP_EXPORT class CPP_API Film {
public:
	/*!
	* \brief Types of Film channel available.
	*/
	typedef enum {
		OUTPUT_RGB = slg::FilmOutputs::RGB,
		OUTPUT_RGBA = slg::FilmOutputs::RGBA,
		OUTPUT_RGB_TONEMAPPED = slg::FilmOutputs::RGB_TONEMAPPED,
		OUTPUT_RGBA_TONEMAPPED = slg::FilmOutputs::RGBA_TONEMAPPED,
		OUTPUT_ALPHA = slg::FilmOutputs::ALPHA,
		OUTPUT_DEPTH = slg::FilmOutputs::DEPTH,
		OUTPUT_POSITION = slg::FilmOutputs::POSITION,
		OUTPUT_GEOMETRY_NORMAL = slg::FilmOutputs::GEOMETRY_NORMAL,
		OUTPUT_SHADING_NORMAL = slg::FilmOutputs::SHADING_NORMAL,
		OUTPUT_MATERIAL_ID = slg::FilmOutputs::MATERIAL_ID,
		OUTPUT_DIRECT_DIFFUSE = slg::FilmOutputs::DIRECT_DIFFUSE,
		OUTPUT_DIRECT_GLOSSY = slg::FilmOutputs::DIRECT_GLOSSY,
		OUTPUT_EMISSION = slg::FilmOutputs::EMISSION,
		OUTPUT_INDIRECT_DIFFUSE = slg::FilmOutputs::INDIRECT_DIFFUSE,
		OUTPUT_INDIRECT_GLOSSY = slg::FilmOutputs::INDIRECT_GLOSSY,
		OUTPUT_INDIRECT_SPECULAR = slg::FilmOutputs::INDIRECT_SPECULAR,
		OUTPUT_MATERIAL_ID_MASK = slg::FilmOutputs::MATERIAL_ID_MASK,
		OUTPUT_DIRECT_SHADOW_MASK = slg::FilmOutputs::DIRECT_SHADOW_MASK,
		OUTPUT_INDIRECT_SHADOW_MASK = slg::FilmOutputs::INDIRECT_SHADOW_MASK,
		OUTPUT_RADIANCE_GROUP = slg::FilmOutputs::RADIANCE_GROUP,
		OUTPUT_UV = slg::FilmOutputs::UV,
		OUTPUT_RAYCOUNT = slg::FilmOutputs::RAYCOUNT,
		BY_MATERIAL_ID = slg::FilmOutputs::BY_MATERIAL_ID,
		IRRADIANCE = slg::FilmOutputs::IRRADIANCE
	} FilmOutputType;

	/*!
	 * \brief Types of Film channel available.
	 */
	typedef enum {
		CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED = slg::Film::RADIANCE_PER_PIXEL_NORMALIZED,
		CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED = slg::Film::RADIANCE_PER_SCREEN_NORMALIZED,
		CHANNEL_ALPHA = slg::Film::ALPHA,
		CHANNEL_RGB_TONEMAPPED = slg::Film::RGB_TONEMAPPED,
		CHANNEL_DEPTH = slg::Film::DEPTH,
		CHANNEL_POSITION = slg::Film::POSITION,
		CHANNEL_GEOMETRY_NORMAL = slg::Film::GEOMETRY_NORMAL,
		CHANNEL_SHADING_NORMAL = slg::Film::SHADING_NORMAL,
		CHANNEL_MATERIAL_ID = slg::Film::MATERIAL_ID,
		CHANNEL_DIRECT_DIFFUSE = slg::Film::DIRECT_DIFFUSE,
		CHANNEL_DIRECT_GLOSSY = slg::Film::DIRECT_GLOSSY,
		CHANNEL_EMISSION = slg::Film::EMISSION,
		CHANNEL_INDIRECT_DIFFUSE = slg::Film::INDIRECT_DIFFUSE,
		CHANNEL_INDIRECT_GLOSSY = slg::Film::INDIRECT_GLOSSY,
		CHANNEL_INDIRECT_SPECULAR = slg::Film::INDIRECT_SPECULAR,
		CHANNEL_MATERIAL_ID_MASK = slg::Film::MATERIAL_ID_MASK,
		CHANNEL_DIRECT_SHADOW_MASK = slg::Film::DIRECT_SHADOW_MASK,
		CHANNEL_INDIRECT_SHADOW_MASK = slg::Film::INDIRECT_SHADOW_MASK,
		CHANNEL_UV = slg::Film::UV,
		CHANNEL_RAYCOUNT = slg::Film::RAYCOUNT,
		CHANNEL_BY_MATERIAL_ID = slg::Film::BY_MATERIAL_ID,
		CHANNEL_IRRADIANCE = slg::Film::IRRADIANCE
	} FilmChannelType;

	~Film();

	/*!
	 * \brief Returns the Film width.
	 * 
	 * \return the Film width.
	 */
	u_int GetWidth() const;
	/*!
	 * \brief Returns the Film height.
	 * 
	 * \return the Film width.
	 */
	u_int GetHeight() const;
	/*!
	 * \brief Saves all Film output channels.
	 */
	void Save() const;

	/*!
	 * \brief Returns the total sample count.
	 *
	 * \return the total sample count.
	 */
	double GetTotalSampleCount() const;
	/*!
	 * \brief Returns the size (in float or u_int) of a Film output channel.
	 *
	 * \param type is the Film output channel to use.
	 *
	 * \return the size (in float or u_int) of a Film output channel.
	 */
	size_t GetOutputSize(const FilmOutputType type) const;
	/*!
	 * \brief Returns if a film channel output is available.
	 *
	 * \param type is the Film output channel to use.
	 *
	 * \return true if the output is available, false otherwise.
	 */
	bool HasOutput(const FilmOutputType type) const;
	/*!
	 * \brief Returns the number of radiance groups.
	 *
	 * \return the number of radiance groups.
	 */
	u_int GetRadianceGroupCount() const;
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
	template<class T> void GetOutput(const FilmOutputType type, T *buffer, const u_int index = 0) {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	/*!
	 * \brief Returns the number of channels of the passed type.
	 *
	 * \param type is the Film output channel to use.
	 *
	 * \return the number of channels. Returns 0 if the channel is not available.
	 */
	u_int GetChannelCount(const FilmChannelType type) const;
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
	template<class T> const T *GetChannel(const FilmChannelType type, const u_int index = 0) {
		throw std::runtime_error("Called Film::GetChannel() with wrong type");
	}

	friend class RenderSession;

private:
	Film(const RenderSession &session);

	const RenderSession &renderSession;
};

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const u_int index);
template<> void Film::GetOutput<u_int>(const FilmOutputType type, u_int *buffer, const u_int index);
template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index);
template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index);

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
		ORTHOGRAPHIC = slg::Camera::ORTHOGRAPHIC,
		PERSPECTIVE = slg::Camera::PERSPECTIVE,
		STEREO = slg::Camera::STEREO
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
		DEFAULT = slg::ImageMapStorage::DEFAULT,
		RED = slg::ImageMapStorage::RED,
		GREEN = slg::ImageMapStorage::GREEN,
		BLUE = slg::ImageMapStorage::BLUE,
		ALPHA = slg::ImageMapStorage::ALPHA,
		MEAN = slg::ImageMapStorage::MEAN,
		WEIGHTED_MEAN = slg::ImageMapStorage::WEIGHTED_MEAN,
		RGB = slg::ImageMapStorage::RGB
	} ChannelSelectionType;
	/*!
	* \brief Types of strands tessellation.
	*/
	typedef enum {
		TESSEL_RIBBON = slg::StrendsShape::TESSEL_RIBBON,
		TESSEL_RIBBON_ADAPTIVE = slg::StrendsShape::TESSEL_RIBBON_ADAPTIVE,
		TESSEL_SOLID = slg::StrendsShape::TESSEL_SOLID,
		TESSEL_SOLID_ADAPTIVE = slg::StrendsShape::TESSEL_SOLID_ADAPTIVE
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
	 * \brief Returns all the Properties required to define this Scene.
	 *
	 * \return a reference to the Properties of this Scene.
	 */
	const luxrays::Properties &GetProperties() const;
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
	 * The memory allocated for cols array is always freed by the Scene class.
	 *
	 * NOTE: the use of this method is deprecated. NOTICE THE DIFFERENCE IN
	 *  MEMORY HANDLING BETWEEN THE OLD DefineImageMap() AND THE NEW ONE.
	 *
	 * \param imgMapName is the name of the defined image map.
	 * \param cols is a pointer to an array of float.
	 * \param gamma is the gamma correction value of the image.
	 * \param channels is the number of float used for each pixel (1 or 3).
	 * \param width is the width of the image map.
	 * \param height is the height of the image map.
	 */
	//void DefineImageMap(const std::string &imgMapName, float *cols, const float gamma,
	//	const u_int channels, const u_int width, const u_int height);
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
			T *pixels, const float gamma, const u_int channels,
			const u_int width, const u_int height,
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
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
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
	const u_int GetLightCount() const;
	/*!
	 * \brief Returns the number of objects in the Scene.
	 *
	 * \return the number of objects in the Scene.
	 */	
	const u_int GetObjectCount() const;

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
	 * \brief This must be used to allocate Mesh vertices buffer.
	 */
	static luxrays::Point *AllocVerticesBuffer(const u_int meshVertCount);
	/*!
	 * \brief This must be used to allocate Mesh triangles buffer.
	 */
	static luxrays::Triangle *AllocTrianglesBuffer(const u_int meshTriCount);

	friend class RenderConfig;
	friend class Camera;

private:
	Scene(slg::Scene *scn);

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
	 * \brief Returns a reference to the Scene used in the RenderConfig.
	 *
	 * \return the reference to the RenderConfig Scene.
	 */
	Scene &GetScene();

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
	bool GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const;

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
 * \brief RenderSession execute a rendering based on the RenderConfig provided.
 */
CPP_EXPORT class CPP_API RenderSession {
public:
	/*!
	 * \brief Constructs a new RenderSession using the provided RenderConfig.
	 *
	 * \param config is the RenderConfig used to create the rendering session. The
	 * RenderConfig is not deleted by the destructor.
	 */
	RenderSession(const RenderConfig *config);
	~RenderSession();

	/*!
	 * \brief Returns a reference to the RenderingConfig used to create this
	 * RenderSession.
	 *
	 * \return a reference to the RenderingConfig.
	 */
	const RenderConfig &GetRenderConfig() const;

	/*!
	 * \brief Starts the rendering.
	 */
	void Start();
	/*!
	 * \brief Stops the rendering.
	 */
	void Stop();

	/*!
	 * \brief Suspends the rendering and allows to edit the Scene.
	 */
	void BeginSceneEdit();
	/*!
	 * \brief Ends the Scene editing and un-suspends the rendering.
	 */
	void EndSceneEdit();

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
	 * RTPATHOCL or RTBIASPATHOCL. It does nothing with other render engines.
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

	friend class Film;

private:
	const RenderConfig *renderConfig;
	Film film;

	slg::RenderSession *renderSession;
	luxrays::Properties stats;
};

}

#endif
