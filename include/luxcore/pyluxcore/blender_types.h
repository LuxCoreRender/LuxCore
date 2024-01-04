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

#ifndef _BLENDER_TYPES_H
#define	_BLENDER_TYPES_H

#include <Python.h>

namespace luxcore {
namespace blender {

//------------------------------------------------------------------------------
// Blender definitions and structures
//------------------------------------------------------------------------------

static const int ME_SMOOTH = 1;

enum eImbFileType {
	IMB_FTYPE_PNG = 1,
	IMB_FTYPE_TGA = 2,
	IMB_FTYPE_JPG = 3,
	IMB_FTYPE_BMP = 4,
	IMB_FTYPE_OPENEXR = 5,
	IMB_FTYPE_IMAGIC = 6,
	IMB_FTYPE_PSD = 7,
#ifdef WITH_OPENJPEG
	IMB_FTYPE_JP2 = 8,
#endif
	IMB_FTYPE_RADHDR = 9,
	IMB_FTYPE_TIF = 10,
#ifdef WITH_CINEON
	IMB_FTYPE_CINEON = 11,
	IMB_FTYPE_DPX = 12,
#endif

	IMB_FTYPE_DDS = 13,
#ifdef WITH_WEBP
	IMB_FTYPE_WEBP = 14,
#endif
};

typedef struct MLoopTri {
	unsigned int tri[3];
} MLoopTri;

typedef struct MLoopUV {
	float uv[2];
	int flag;
} MLoopUV;

typedef struct MLoopCol {
	unsigned char r, g, b, a;
} MLoopCol;

typedef struct MLoop {
	/** Vertex index. */
	unsigned int v;
	/** Edge index. */
	unsigned int e;
} MLoop;

typedef struct MPoly {
	/* offset into loop array and number of loops in the face */
	int loopstart;
	int totloop;
	short mat_nr;
	char flag, _pad;
} MPoly;

struct MVert {
	float co[3];
	char flag, bweight;
	char _pad[2];
};

typedef enum ImBufOwnership {
	/* The ImBuf simply shares pointer with data owned by someone else, and will not perform any
	 * memory management when the ImBuf frees the buffer. */
	IB_DO_NOT_TAKE_OWNERSHIP = 0,

	/* The ImBuf takes ownership of the buffer data, and will use MEM_freeN() to free this memory
	 * when the ImBuf needs to free the data. */
	 IB_TAKE_OWNERSHIP = 1,
} ImBufOwnership;

typedef struct ImBufByteBuffer {
	uint8_t* data;
	ImBufOwnership ownership;

	struct ColorSpace* colorspace;
} ImBufByteBuffer;

typedef struct ImBufFloatBuffer {
	float* data;
	ImBufOwnership ownership;

	struct ColorSpace* colorspace;
} ImBufFloatBuffer;

typedef struct ImBufGPU {
	/* Texture which corresponds to the state of the ImBug on the GPU.
	 *
	 * Allocation is supposed to happen outside of the ImBug module from a proper GPU context.
	 * De-referencing the ImBuf or its GPU texture can happen from any state. */
	 /* TODO(sergey): This should become a list of textures, to support having high-res ImBuf on GPU
	  * without hitting hardware limitations. */
	struct GPUTexture* texture;
} ImBufGPU;

typedef struct ImbFormatOptions {
	short flag;
	/** Quality serves dual purpose as quality number for JPEG or compression amount for PNG. */
	char quality;
} ImbFormatOptions;

#define IMB_MIPMAP_LEVELS 20
#define IMB_FILEPATH_SIZE 1024

typedef struct DDSData {
	/** DDS fourcc info */
	unsigned int fourcc;
	/** The number of mipmaps in the dds file */
	unsigned int nummipmaps;
	/** The compressed image data */
	unsigned char* data;
	/** The size of the compressed data */
	unsigned int size;
} DDSData;

typedef struct rcti {
	int xmin, xmax;
	int ymin, ymax;
} rcti;

typedef struct ImBuf {
	/* dimensions */
	/** Width and Height of our image buffer.
	 * Should be 'unsigned int' since most formats use this.
	 * but this is problematic with texture math in `imagetexture.c`
	 * avoid problems and use int. - campbell */
	int x, y;

	/** Active amount of bits/bit-planes. */
	unsigned char planes;
	/** Number of channels in `rect_float` (0 = 4 channel default) */
	int channels;

	/* flags */
	/** Controls which components should exist. */
	int flags;

	/* pixels */

	/**
	 * Image pixel buffer (8bit representation):
	 * - color space defaults to `sRGB`.
	 * - alpha defaults to 'straight'.
	 */
	ImBufByteBuffer byte_buffer;

	/**
	 * Image pixel buffer (float representation):
	 * - color space defaults to 'linear' (`rec709`).
	 * - alpha defaults to 'premul'.
	 * \note May need gamma correction to `sRGB` when generating 8bit representations.
	 * \note Formats that support higher more than 8 but channels load as floats.
	 */
	ImBufFloatBuffer float_buffer;

	/* Image buffer on the GPU. */
	ImBufGPU gpu;

	/** Resolution in pixels per meter. Multiply by `0.0254` for DPI. */
	double ppm[2];

	/* parameters used by conversion between byte and float */
	/** random dither value, for conversion from float -> byte rect */
	float dither;

	/* mipmapping */
	/** MipMap levels, a series of halved images */
	struct ImBuf* mipmap[IMB_MIPMAP_LEVELS];
	int miptot, miplevel;

	/* externally used data */
	/** reference index for ImBuf lists */
	int index;
	/** used to set imbuf to dirty and other stuff */
	int userflags;
	/** image metadata */
	struct IDProperty* metadata;
	/** temporary storage */
	void* userdata;

	/* file information */
	/** file type we are going to save as */
	enum eImbFileType ftype;
	/** file format specific flags */
	ImbFormatOptions foptions;
	/** The absolute file path associated with this image. */
	char filepath[IMB_FILEPATH_SIZE];

	/* memory cache limiter */
	/** reference counter for multiple users */
	int refcounter;

	/* some parameters to pass along for packing images */
	/** Compressed image only used with PNG and EXR currently. */
	ImBufByteBuffer encoded_buffer;
	/** Size of data written to `encoded_buffer`. */
	unsigned int encoded_size;
	/** Size of `encoded_buffer` */
	unsigned int encoded_buffer_size;

	/* color management */
	/** array of per-display display buffers dirty flags */
	unsigned int* display_buffer_flags;
	/** cache used by color management */
	struct ColormanageCache* colormanage_cache;
	int colormanage_flag;
	rcti invalid_rect;

	/* information for compressed textures */
	struct DDSData dds_data;
} ImBuf;

struct RenderPass {
	struct RenderPass* next, * prev;
	int channels;
	char name[64];   /* amount defined in IMB_openexr.h */
	char chan_id[8]; /* amount defined in IMB_openexr.h */

	struct ImBuf* ibuf;

	int rectx, recty;

	char fullname[64]; /* EXR_PASS_MAXNAME */
	char view[64];     /* EXR_VIEW_MAXNAME */
	int view_id;       /* quick lookup */

	char _pad0[4];
};


// from blender/source/blender/python/mathutils/mathutils_Matrix.h
typedef struct {
	// The following is expanded form of the macro BASE_MATH_MEMBERS(matrix)
	PyObject_VAR_HEAD float *matrix;  // The only thing we are interested in
	PyObject *cb_user;
	unsigned char cb_type;
	unsigned char cb_subtype;
	unsigned char flag;

	unsigned short num_col;
	unsigned short num_row;
} MatrixObject;


// ----------------------------------------------------------------------
// Mesh and dependencies (required for fast custom split normals support)

// List of relevant files (as one line so it can be copy/pasted to open all files quickly in an editor)
// blender/source/blender/makesdna/DNA_ID.h blender/source/blender/makesdna/DNA_customdata_types.h blender/source/blender/makesdna/DNA_mesh_types.h
// ----------------------------------------------------------------------

#define DNA_DEPRECATED

// Forward declarations of irrelevant structs
struct Library;
struct IDProperty;
struct IDOverrideLibrary;
struct BLI_mempool;
struct CustomDataExternal;
struct EditMeshData;
struct SubdivCCG;
struct LinkNode;
struct ShrinkwrapBoundaryData;
struct AnimData;
struct Ipo;
struct Key;
struct Material;
struct MSelect;
struct MFace;
struct MTFace;
struct TFace;
struct MEdge;
struct MDeformVert;
struct MCol;
struct BMEditMesh;
struct Multires;

namespace blender_3_6 {		
	template<typename T> class Span;
	template<typename T> class MutableSpan;
	template<typename T> class OffsetIndices;

	using float3 = float[3];
	
	template<typename T> struct Bounds {
		T min;
		T max;
	};

	namespace bke {
		struct MeshRuntime;
		class AttributeAccessor;
		class MutableAttributeAccessor;
		struct LooseEdgeCache;
	}  // namespace bke

	struct MeshRuntimeHandle;

	// -------------------------------------------------------------------------------------
	// From blender/source/blender/makesdna/DNA_listBase.h
	typedef struct ListBase {
		void* first, * last;
	} ListBase;
	// -------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------
	// From blender/source/blender/...
	typedef struct ID {
		void* next, * prev;
		struct ID* newid;
		struct Library* lib;
		/** MAX_ID_NAME. */
		char name[66];
		/**
		 * LIB_... flags report on status of the data-block this ID belongs to
		 * (persistent, saved to and read from .blend).
		 */
		short flag;
		/**
		 * LIB_TAG_... tags (runtime only, cleared at read time).
		 */
		int tag;
		int us;
		int icon_id;
		int recalc;
		char _pad[4];
		IDProperty* properties;

		/** Reference linked ID which this one overrides. */
		IDOverrideLibrary* override_library;

		/**
		 * Only set for data-blocks which are coming from copy-on-write, points to
		 * the original version of it.
		 */
		struct ID* orig_id;

		void* py_instance;
	} ID;
	// -------------------------------------------------------------------------------------
	typedef struct AnonymousAttributeIDHandle AnonymousAttributeIDHandle;

	// -------------------------------------------------------------------------------------
	// From blender/source/blender/makesdna/DNA_customdata_types.h

	/** Descriptor and storage for a custom data layer. */
	typedef struct CustomDataLayer {
		/** Type of data in layer. */
		int type;
		/** In editmode, offset of layer in block. */
		int offset;
		/** General purpose flag. */
		int flag;
		/** Number of the active layer of this type. */
		int active;
		/** Number of the layer to render. */
		int active_rnd;
		/** Number of the layer to render. */
		int active_clone;
		/** Number of the layer to render. */
		int active_mask;
		/** Shape key-block unique id reference. */
		int uid;
		/** Layer name, MAX_CUSTOMDATA_LAYER_NAME. */
		char name[68];
		char _pad1[4];
		/** Layer data. */
		void* data;
		/**
		 * Run-time identifier for this layer. Can be used to retrieve information about where this
		 * attribute was created.
		 */
		const AnonymousAttributeIDHandle* anonymous_id;
	} CustomDataLayer;


	typedef struct CustomData {
		/** CustomDataLayers, ordered by type. */
		CustomDataLayer* layers;
		/**
		 * runtime only! - maps types to indices of first layer of that type,
		 * MUST be >= CD_NUMTYPES, but we can't use a define here.
		 * Correct size is ensured in CustomData_update_typemap assert().
		 */
		int typemap[52];
		char _pad[4];
		/** Number of layers, size of layers array. */
		int totlayer, maxlayer;
		/** In editmode, total size of all data layers. */
		int totsize;
		/** (BMesh Only): Memory pool for allocation of blocks. */
		struct BLI_mempool* pool;
		/** External file storing custom-data layers. */
		CustomDataExternal* external;
	} CustomData;


	/** #CustomData.type */
	typedef enum CustomDataType {
		/* Used by GLSL attributes in the cases when we need a delayed CD type
		 * assignment (in the cases when we don't know in advance which layer
		 * we are addressing).
		 */
		CD_AUTO_FROM_NAME = -1,

#ifdef DNA_DEPRECATED_ALLOW
		CD_MVERT = 0,   /* DEPRECATED */
		CD_MSTICKY = 1, /* DEPRECATED */
#endif
		CD_MDEFORMVERT = 2, /* Array of `MDeformVert`. */
		CD_MEDGE = 3,
		CD_MFACE = 4,
		CD_MTFACE = 5,
		CD_MCOL = 6,
		CD_ORIGINDEX = 7,
		/**
		 * Used for derived face corner normals on mesh `ldata`, since currently they are not computed
		 * lazily. Derived vertex and polygon normals are stored in #Mesh_Runtime.
		 */
		 CD_NORMAL = 8,
		 CD_FACEMAP = 9, /* exclusive face group, each face can only be part of one */
		 CD_PROP_FLOAT = 10,
		 CD_PROP_INT32 = 11,
		 CD_PROP_STRING = 12,
		 CD_ORIGSPACE = 13, /* for modifier stack face location mapping */
		 CD_ORCO = 14,      /* undeformed vertex coordinates, normalized to 0..1 range */
#ifdef DNA_DEPRECATED_ALLOW
		 CD_MTEXPOLY = 15, /* deprecated */
#endif
		 CD_MLOOPUV = 16,
		 CD_PROP_BYTE_COLOR = 17,
		 CD_TANGENT = 18,
		 CD_MDISPS = 19,
		 CD_PREVIEW_MCOL = 20,           /* For displaying weight-paint colors. */
										 /*  CD_ID_MCOL          = 21, */
		 /* CD_TEXTURE_MLOOPCOL = 22, */ /* UNUSED */
		 CD_CLOTH_ORCO = 23,
		 /* CD_RECAST = 24, */ /* UNUSED */

		 CD_MPOLY = 25,
		 CD_MLOOP = 26,
		 CD_SHAPE_KEYINDEX = 27,
		 CD_SHAPEKEY = 28,
		 CD_BWEIGHT = 29,
		 /** Subdivision sharpness data per edge or per vertex. */
		 CD_CREASE = 30,
		 CD_ORIGSPACE_MLOOP = 31,
		 CD_PREVIEW_MLOOPCOL = 32,
		 CD_BM_ELEM_PYPTR = 33,

		 CD_PAINT_MASK = 34,
		 CD_GRID_PAINT_MASK = 35,
		 CD_MVERT_SKIN = 36,
		 CD_FREESTYLE_EDGE = 37,
		 CD_FREESTYLE_FACE = 38,
		 CD_MLOOPTANGENT = 39,
		 CD_TESSLOOPNORMAL = 40,
		 CD_CUSTOMLOOPNORMAL = 41,
		 CD_SCULPT_FACE_SETS = 42,

		 /* CD_LOCATION = 43, */ /* UNUSED */
		 /* CD_RADIUS = 44, */   /* UNUSED */
		 CD_PROP_INT8 = 45,
		 /* CD_HAIRMAPPING = 46, */ /* UNUSED, can be reused. */

		 CD_PROP_COLOR = 47,
		 CD_PROP_FLOAT3 = 48,
		 CD_PROP_FLOAT2 = 49,
		 CD_PROP_BOOL = 50,

		 CD_HAIRLENGTH = 51,

		 CD_NUMTYPES = 52,
	} CustomDataType;

	// -------------------------------------------------------------------------------------
	// From blender/source/blender/makesdna/DNA_mesh_types.h

	typedef struct CustomData_MeshMasks {
		uint64_t vmask;
		uint64_t emask;
		uint64_t fmask;
		uint64_t pmask;
		uint64_t lmask;
	} CustomData_MeshMasks;

	typedef struct Mesh {
		ID id;
		/** Animation data (must be immediately after id for utilities to use it). */
		struct AnimData* adt;

		/** Old animation system, deprecated for 2.5. */
		struct Ipo* ipo DNA_DEPRECATED;
		struct Key* key;

		/**
		 * An array of materials, with length #totcol. These can be overridden by material slots
		 * on #Object. Indices in the "material_index" attribute control which material is used for every
		 * face.
		 */
		struct Material** mat;

		/** The number of vertices (#MVert) in the mesh, and the size of #vdata. */
		int totvert;
		/** The number of edges (#MEdge) in the mesh, and the size of #edata. */
		int totedge;
		/** The number of polygons/faces (#MPoly) in the mesh, and the size of #pdata. */
		int totpoly;
		/** The number of face corners (#MLoop) in the mesh, and the size of #ldata. */
		int totloop;
		
		/**
		   * Array owned by mesh. May be null of there are no polygons. Index of the first corner of each
		   * polygon, with the total number of corners at the end. See #Mesh::polys() and #OffsetIndices.
		   */
		int* poly_offset_indices;
		
		CustomData vdata, edata, pdata, ldata;

		/**
		 * List of vertex group (#bDeformGroup) names and flags only. Actual weights are stored in dvert.
		 * \note This pointer is for convenient access to the #CD_MDEFORMVERT layer in #vdata.
		 */
		ListBase vertex_group_names;
		/** The active index in the #vertex_group_names list. */
		int vertex_group_active_index;

		/**
		 * The index of the active attribute in the UI. The attribute list is a combination of the
		 * generic type attributes from vertex, edge, face, and corner custom data.
		 */
		int attributes_active_index;

		/**
		 * Runtime storage of the edit mode mesh. If it exists, it generally has the most up-to-date
		 * information about the mesh.
		 * \note When the object is available, the preferred access method is #BKE_editmesh_from_object.
		 */
		struct BMEditMesh* edit_mesh;

		/**
		 * This array represents the selection order when the user manually picks elements in edit-mode,
		 * some tools take advantage of this information. All elements in this array are expected to be
		 * selected, see #BKE_mesh_mselect_validate which ensures this. For procedurally created meshes,
		 * this is generally empty (selections are stored as boolean attributes in the corresponding
		 * custom data).
		 */
		struct MSelect* mselect;

		/** The length of the #mselect array. */
		int totselect;

		/**
		 * In most cases the last selected element (see #mselect) represents the active element.
		 * For faces we make an exception and store the active face separately so it can be active
		 * even when no faces are selected. This is done to prevent flickering in the material properties
		 * and UV Editor which base the content they display on the current material which is controlled
		 * by the active face.
		 *
		 * \note This is mainly stored for use in edit-mode.
		 */
		int act_face;

		/**
		 * An optional mesh owned elsewhere (by #Main) that can be used to override
		 * the texture space #loc and #size.
		 * \note Vertex indices should be aligned for this to work usefully.
		 */
		struct Mesh* texcomesh;

		/** Texture space location and size, used for procedural coordinates when rendering. */
		float texspace_location[3];
		float texspace_size[3];
		char texspace_flag;

		/** Various flags used when editing the mesh. */
		char editflag;
		/** Mostly more flags used when editing or displaying the mesh. */
		uint16_t flag;

		/**
		 * The angle for auto smooth in radians. `M_PI` (180 degrees) causes all edges to be smooth.
		 */
		float smoothresh;

		/** Per-mesh settings for voxel remesh. */
		float remesh_voxel_size;
		float remesh_voxel_adaptivity;

		int face_sets_color_seed;
		/* Stores the initial Face Set to be rendered white. This way the overlay can be enabled by
		 * default and Face Sets can be used without affecting the color of the mesh. */
		int face_sets_color_default;

		/** The color attribute currently selected in the list and edited by a user. */
		char* active_color_attribute;
		/** The color attribute used by default (i.e. for rendering) if no name is given explicitly. */
		char* default_color_attribute;

		/**
		 * User-defined symmetry flag (#eMeshSymmetryType) that causes editing operations to maintain
		 * symmetrical geometry. Supported by operations such as transform and weight-painting.
		 */
		char symmetry;

		/** Choice between different remesh methods in the UI. */
		char remesh_mode;

		/** The length of the #mat array. */
		short totcol;

		/**
		 * Deprecated flag for choosing whether to store specific custom data that was built into #Mesh
		 * structs in edit mode. Replaced by separating that data to separate layers. Kept for forward
		 * and backwards compatibility.
		 */
		char cd_flag DNA_DEPRECATED;
		char subdiv DNA_DEPRECATED;
		char subdivr DNA_DEPRECATED;
		char subsurftype DNA_DEPRECATED;

		/** Deprecated pointer to mesh polygons, kept for forward compatibility. */
		struct MPoly* mpoly DNA_DEPRECATED;
		/** Deprecated pointer to face corners, kept for forward compatibility. */
		struct MLoop* mloop DNA_DEPRECATED;

		/** Deprecated array of mesh vertices, kept for reading old files, now stored in #CustomData. */
		struct MVert* mvert DNA_DEPRECATED;
		/** Deprecated array of mesh edges, kept for reading old files, now stored in #CustomData. */
		struct MEdge* medge DNA_DEPRECATED;
		/** Deprecated "Vertex group" data. Kept for reading old files, now stored in #CustomData. */
		struct MDeformVert* dvert DNA_DEPRECATED;
		/** Deprecated runtime data for tessellation face UVs and texture, kept for reading old files. */
		struct MTFace* mtface DNA_DEPRECATED;
		/** Deprecated, use mtface. */
		struct TFace* tface DNA_DEPRECATED;
		/** Deprecated array of colors for the tessellated faces, kept for reading old files. */
		struct MCol* mcol DNA_DEPRECATED;
		/** Deprecated face storage (quads & triangles only). Kept for reading old files. */
		struct MFace* mface DNA_DEPRECATED;

		/**
		 * Deprecated storage of old faces (only triangles or quads).
		 *
		 * \note This would be marked deprecated, however the particles still use this at run-time
		 * for placing particles on the mesh (something which should be eventually upgraded).
		 */
		CustomData fdata;
		/* Deprecated size of #fdata. */
		int totface;

		char _pad1[4];

		/**
		 * Data that isn't saved in files, including caches of derived data, temporary data to improve
		 * the editing experience, etc. The struct is created when reading files and can be accessed
		 * without null checks, with the exception of some temporary meshes which should allocate and
		 * free the data if they are passed to functions that expect run-time data.
		 */
		MeshRuntimeHandle* runtime;

		Span<float3> vert_positions() const;
		/** Write access to vertex data. */
		MutableSpan<float3> vert_positions_for_write();
		/**
		 * Array of edges, containing vertex indices. For simple triangle or quad meshes, edges could be
		 * calculated from the polygon and "corner edge" arrays, however, edges need to be stored
		 * explicitly to edge domain attributes and to support loose edges that aren't connected to
		 * faces.
		 */
		Span<MEdge> edges() const;
		/** Write access to edge data. */
		MutableSpan<MEdge> edges_for_write();
		/**
		 * Face topology storage of the offset of each face's section of the face corners. The size of
		 * each polygon is encoded using the next offset value. Can be used to slice the #corner_verts or
		 * #corner_edges arrays to find the vertices or edges that make up each face.
		 */
		OffsetIndices<int> polys() const;
		/** The first corner index of every polygon. */
		Span<int> poly_offsets() const;
		/** Write access to #poly_offsets data. */
		MutableSpan<int> poly_offsets_for_write();

		/**
		 * Array of vertices for every face corner,  stored in the ".corner_vert" integer attribute.
		 * For example, the vertices in a face can be retrieved with the #slice method:
		 * \code{.cc}
		 * const Span<int> poly_verts = corner_verts.slice(poly.loopstart, poly.totloop);
		 * \endcode
		 * Such a span can often be passed as an argument in lieu of a polygon and the entire corner
		 * verts array.
		 */
		Span<int> corner_verts() const;
		/** Write access to the #corner_verts data. */
		MutableSpan<int> corner_verts_for_write();

		/**
		 * Array of edges following every face corner traveling around each face, stored in the
		 * ".corner_edge" attribute. The array sliced the same way as the #corner_verts data. The edge
		 * previous to a corner must be accessed with the index of the previous face corner.
		 */
		Span<int> corner_edges() const;
		/** Write access to the #corner_edges data. */
		MutableSpan<int> corner_edges_for_write();

		bke::AttributeAccessor attributes() const;
		bke::MutableAttributeAccessor attributes_for_write();

		/**
		 * Vertex group data, encoded as an array of indices and weights for every vertex.
		 * \warning: May be empty.
		 */
		Span<MDeformVert> deform_verts() const;
		/** Write access to vertex group data. */
		MutableSpan<MDeformVert> deform_verts_for_write();

		/**
		 * Cached triangulation of the mesh.
		 */
		Span<MLoopTri> looptris() const;

		/** Set cached mesh bounds to a known-correct value to avoid their lazy calculation later on. */
		void bounds_set_eager(const Bounds<float3>& bounds);

		/**
		 * Cached information about loose edges, calculated lazily when necessary.
		 */
		const bke::LooseEdgeCache& loose_edges() const;
		/**
		 * Explicitly set the cached number of loose edges to zero. This can improve performance
		 * later on, because finding loose edges lazily can be skipped entirely.
		 *
		 * \note To allow setting this status on meshes without changing them, this does not tag the
		 * cache dirty. If the mesh was changed first, the relevant dirty tags should be called first.
		 */
		void loose_edges_tag_none() const;

		/**
		 * Normal direction of polygons, defined by positions and the winding direction of face corners.
		 */
		Span<float3> poly_normals() const;
		/**
		 * Normal direction of vertices, defined as the weighted average of face normals
		 * surrounding each vertex and the normalized position for loose vertices.
		 */
		Span<float3> vert_normals() const;
	} Mesh;

}// namespace blender 3.6

} // namespace blender
} // namespace luxcore

#endif
