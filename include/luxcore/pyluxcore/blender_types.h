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

typedef struct MLoopTri {
	unsigned int tri[3];
	unsigned int poly;
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

struct RenderPass {
	struct RenderPass *next, *prev;
	int channels;
	char name[64];
	char chan_id[8];
	float *rect;  // The only thing we are interested in
	int rectx, recty;

	char fullname[64];
	char view[64];
	int view_id;

	int pad;
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


namespace blender_2_82 {

typedef struct ID {
  void *next, *prev;
  struct ID *newid;
  struct Library *lib;
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
  IDProperty *properties;

  /** Reference linked ID which this one overrides. */
  IDOverrideLibrary *override_library;

  /**
   * Only set for data-blocks which are coming from copy-on-write, points to
   * the original version of it.
   */
  struct ID *orig_id;

  void *py_instance;
} ID;

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
  /** Shape keyblock unique id reference. */
  int uid;
  /** Layer name, MAX_CUSTOMDATA_LAYER_NAME. */
  char name[64];
  /** Layer data. */
  void *data;
} CustomDataLayer;

typedef struct CustomData {
  /** CustomDataLayers, ordered by type. */
  CustomDataLayer *layers;
  /**
   * runtime only! - maps types to indices of first layer of that type,
   * MUST be >= CD_NUMTYPES, but we cant use a define here.
   * Correct size is ensured in CustomData_update_typemap assert().
   */
  int typemap[42];
  char _pad0[4];
  /** Number of layers, size of layers array. */
  int totlayer, maxlayer;
  /** In editmode, total size of all data layers. */
  int totsize;
  /** (BMesh Only): Memory pool for allocation of blocks. */
  struct BLI_mempool *pool;
  /** External file storing customdata layers. */
  CustomDataExternal *external;
} CustomData;

typedef enum CustomDataType {
  /* Used by GLSL attributes in the cases when we need a delayed CD type
   * assignment (in the cases when we don't know in advance which layer
   * we are addressing).
   */
  CD_AUTO_FROM_NAME = -1,

  CD_MVERT = 0,
#ifdef DNA_DEPRECATED_ALLOW
  CD_MSTICKY = 1, /* DEPRECATED */
#endif
  CD_MDEFORMVERT = 2,
  CD_MEDGE = 3,
  CD_MFACE = 4,
  CD_MTFACE = 5,
  CD_MCOL = 6,
  CD_ORIGINDEX = 7,
  CD_NORMAL = 8,
  CD_FACEMAP = 9, /* exclusive face group, each face can only be part of one */
  CD_PROP_FLT = 10,
  CD_PROP_INT = 11,
  CD_PROP_STR = 12,
  CD_ORIGSPACE = 13, /* for modifier stack face location mapping */
  CD_ORCO = 14,      /* undeformed vertex coordinates, normalized to 0..1 range */
#ifdef DNA_DEPRECATED_ALLOW
  CD_MTEXPOLY = 15, /* deprecated */
#endif
  CD_MLOOPUV = 16,
  CD_MLOOPCOL = 17,
  CD_TANGENT = 18,
  CD_MDISPS = 19,
  CD_PREVIEW_MCOL = 20, /* for displaying weightpaint colors */
                        /*  CD_ID_MCOL          = 21, */
  CD_TEXTURE_MLOOPCOL = 22,
  CD_CLOTH_ORCO = 23,
  CD_RECAST = 24,

  /* BMESH ONLY START */
  CD_MPOLY = 25,
  CD_MLOOP = 26,
  CD_SHAPE_KEYINDEX = 27,
  CD_SHAPEKEY = 28,
  CD_BWEIGHT = 29,
  CD_CREASE = 30,
  CD_ORIGSPACE_MLOOP = 31,
  CD_PREVIEW_MLOOPCOL = 32,
  CD_BM_ELEM_PYPTR = 33,
  /* BMESH ONLY END */

  CD_PAINT_MASK = 34,
  CD_GRID_PAINT_MASK = 35,
  CD_MVERT_SKIN = 36,
  CD_FREESTYLE_EDGE = 37,
  CD_FREESTYLE_FACE = 38,
  CD_MLOOPTANGENT = 39,
  CD_TESSLOOPNORMAL = 40,
  CD_CUSTOMLOOPNORMAL = 41,

  CD_NUMTYPES = 42,
} CustomDataType;

struct MLoopTri_Store {
  /* WARNING! swapping between array (ready-to-be-used data) and array_wip
   * (where data is actually computed)
   * shall always be protected by same lock as one used for looptris computing. */
  struct MLoopTri *array, *array_wip;
  int len;
  int len_alloc;
};

typedef struct Mesh_Runtime {
  /* Evaluated mesh for objects which do not have effective modifiers. This mesh is sued as a
   * result of modifier stack evaluation.
   * Since modifier stack evaluation is threaded on object level we need some synchronization. */
  struct Mesh *mesh_eval;
  void *eval_mutex;

  struct EditMeshData *edit_data;
  void *batch_cache;

  struct SubdivCCG *subdiv_ccg;
  void *_pad1;
  int subdiv_ccg_tot_level;
  char _pad2[4];

  int64_t cd_dirty_vert;
  int64_t cd_dirty_edge;
  int64_t cd_dirty_loop;
  int64_t cd_dirty_poly;

  struct MLoopTri_Store looptris;

  /** 'BVHCache', for 'BKE_bvhutil.c' */
  struct LinkNode *bvh_cache;

  /** Non-manifold boundary data for Shrinkwrap Target Project. */
  struct ShrinkwrapBoundaryData *shrinkwrap_data;

  /** Set by modifier stack if only deformed from original. */
  char deformed_only;
  /**
   * Copied from edit-mesh (hint, draw with editmesh data).
   * In the future we may leave the mesh-data empty
   * since its not needed if we can use edit-mesh data. */
  char is_original;
  char _pad[6];
} Mesh_Runtime;

typedef struct Mesh {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Key *key;
  struct Material **mat;
  struct MSelect *mselect;

  /* BMESH ONLY */
  /*new face structures*/
  struct MPoly *mpoly;
  struct MLoop *mloop;
  struct MLoopUV *mloopuv;
  struct MLoopCol *mloopcol;
  /* END BMESH ONLY */

  /* mface stores the tessellation (triangulation) of the mesh,
   * real faces are now stored in nface.*/
  /** Array of mesh object mode faces for tessellation. */
  struct MFace *mface;
  /** Store tessellation face UV's and texture here. */
  struct MTFace *mtface;
  /** Deprecated, use mtface. */
  struct TFace *tface DNA_DEPRECATED;
  /** Array of verts. */
  struct MVert *mvert;
  /** Array of edges. */
  struct MEdge *medge;
  /** Deformgroup vertices. */
  struct MDeformVert *dvert;

  /* array of colors for the tessellated faces, must be number of tessellated
   * faces * 4 in length */
  struct MCol *mcol;
  struct Mesh *texcomesh;

  /* When the object is available, the preferred access method is: BKE_editmesh_from_object(ob) */
  /** Not saved in file!. */
  struct BMEditMesh *edit_mesh;

  struct CustomData vdata, edata, fdata;

  /* BMESH ONLY */
  struct CustomData pdata, ldata;
  /* END BMESH ONLY */

  int totvert, totedge, totface, totselect;

  /* BMESH ONLY */
  int totpoly, totloop;
  /* END BMESH ONLY */

  /* the last selected vertex/edge/face are used for the active face however
   * this means the active face must always be selected, this is to keep track
   * of the last selected face and is similar to the old active face flag where
   * the face does not need to be selected, -1 is inactive */
  int act_face;

  /* texture space, copied as one block in editobject.c */
  float loc[3];
  float size[3];

  short texflag, flag;
  float smoothresh;

  /* customdata flag, for bevel-weight and crease, which are now optional */
  char cd_flag, _pad;

  char subdiv DNA_DEPRECATED, subdivr DNA_DEPRECATED;
  /** Only kept for backwards compat, not used anymore. */
  char subsurftype DNA_DEPRECATED;
  char editflag;

  short totcol;

  float remesh_voxel_size;
  float remesh_voxel_adaptivity;
  char remesh_mode;
  char _pad1[3];
  /** Deprecated multiresolution modeling data, only keep for loading old files. */
  struct Multires *mr DNA_DEPRECATED;

  Mesh_Runtime runtime;
} Mesh;

} // namespace blender_2_82

namespace blender_2_83 {

// -------------------------------------------------------------------------------------
// From blender/source/blender/makesdna/DNA_ID.h

typedef struct ID {
  void *next, *prev;
  struct ID *newid;
  struct Library *lib;
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
  /**
   * Used by undo code. recalc_after_undo_push contains the changes between the
   * last undo push and the current state. This is accumulated as IDs are tagged
   * for update in the depsgraph, and only cleared on undo push.
   *
   * recalc_up_to_undo_push is saved to undo memory, and is the value of
   * recalc_after_undo_push at the time of the undo push. This means it can be
   * used to find the changes between undo states.
   */
  int recalc_up_to_undo_push;
  int recalc_after_undo_push;

  /**
   * A session-wide unique identifier for a given ID, that remain the same across potential
   * re-allocations (e.g. due to undo/redo steps).
   */
  unsigned int session_uuid;

  IDProperty *properties;

  /** Reference linked ID which this one overrides. */
  IDOverrideLibrary *override_library;

  /**
   * Only set for data-blocks which are coming from copy-on-write, points to
   * the original version of it.
   */
  struct ID *orig_id;

  void *py_instance;
} ID;

// -------------------------------------------------------------------------------------
// From blender/source/blender/makesdna/DNA_listBase.h
typedef struct ListBase {
	void *first, *last;
} ListBase;


// -------------------------------------------------------------------------------------
// From blender/source/blender/makesdna/DNA_customdata_types.h

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
  /** Shape keyblock unique id reference. */
  int uid;
  /** Layer name, MAX_CUSTOMDATA_LAYER_NAME. */
  char name[64];
  /** Layer data. */
  void *data;
} CustomDataLayer;

typedef struct CustomData {
  /** CustomDataLayers, ordered by type. */
  CustomDataLayer *layers;
  /**
   * runtime only! - maps types to indices of first layer of that type,
   * MUST be >= CD_NUMTYPES, but we cant use a define here.
   * Correct size is ensured in CustomData_update_typemap assert().
   */
  int typemap[47];
  /** Number of layers, size of layers array. */
  int totlayer, maxlayer;
  /** In editmode, total size of all data layers. */
  int totsize;
  /** (BMesh Only): Memory pool for allocation of blocks. */
  struct BLI_mempool *pool;
  /** External file storing customdata layers. */
  CustomDataExternal *external;
} CustomData;

typedef enum CustomDataType {
  /* Used by GLSL attributes in the cases when we need a delayed CD type
   * assignment (in the cases when we don't know in advance which layer
   * we are addressing).
   */
  CD_AUTO_FROM_NAME = -1,

  CD_MVERT = 0,
#ifdef DNA_DEPRECATED_ALLOW
  CD_MSTICKY = 1, /* DEPRECATED */
#endif
  CD_MDEFORMVERT = 2,
  CD_MEDGE = 3,
  CD_MFACE = 4,
  CD_MTFACE = 5,
  CD_MCOL = 6,
  CD_ORIGINDEX = 7,
  CD_NORMAL = 8,
  CD_FACEMAP = 9, /* exclusive face group, each face can only be part of one */
  CD_PROP_FLT = 10,
  CD_PROP_INT = 11,
  CD_PROP_STR = 12,
  CD_ORIGSPACE = 13, /* for modifier stack face location mapping */
  CD_ORCO = 14,      /* undeformed vertex coordinates, normalized to 0..1 range */
#ifdef DNA_DEPRECATED_ALLOW
  CD_MTEXPOLY = 15, /* deprecated */
#endif
  CD_MLOOPUV = 16,
  CD_MLOOPCOL = 17,
  CD_TANGENT = 18,
  CD_MDISPS = 19,
  CD_PREVIEW_MCOL = 20,           /* for displaying weightpaint colors */
                                  /*  CD_ID_MCOL          = 21, */
  /* CD_TEXTURE_MLOOPCOL = 22, */ /* UNUSED */
  CD_CLOTH_ORCO = 23,
  CD_RECAST = 24,

  /* BMESH ONLY START */
  CD_MPOLY = 25,
  CD_MLOOP = 26,
  CD_SHAPE_KEYINDEX = 27,
  CD_SHAPEKEY = 28,
  CD_BWEIGHT = 29,
  CD_CREASE = 30,
  CD_ORIGSPACE_MLOOP = 31,
  CD_PREVIEW_MLOOPCOL = 32,
  CD_BM_ELEM_PYPTR = 33,
  /* BMESH ONLY END */

  CD_PAINT_MASK = 34,
  CD_GRID_PAINT_MASK = 35,
  CD_MVERT_SKIN = 36,
  CD_FREESTYLE_EDGE = 37,
  CD_FREESTYLE_FACE = 38,
  CD_MLOOPTANGENT = 39,
  CD_TESSLOOPNORMAL = 40,
  CD_CUSTOMLOOPNORMAL = 41,
  CD_SCULPT_FACE_SETS = 42,

  /* Hair and PointCloud */
  CD_LOCATION = 43,
  CD_RADIUS = 44,
  CD_HAIRCURVE = 45,
  CD_HAIRMAPPING = 46,

  CD_NUMTYPES = 47,
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

struct MLoopTri_Store {
  /* WARNING! swapping between array (ready-to-be-used data) and array_wip
   * (where data is actually computed)
   * shall always be protected by same lock as one used for looptris computing. */
  struct MLoopTri *array, *array_wip;
  int len;
  int len_alloc;
};

typedef struct Mesh_Runtime {
	/* Evaluated mesh for objects which do not have effective modifiers.
	 * This mesh is used as a result of modifier stack evaluation.
	 * Since modifier stack evaluation is threaded on object level we need some synchronization. */
	struct Mesh *mesh_eval;
	void *eval_mutex;

	/* A separate mutex is needed for normal calculation, because sometimes
	 * the normals are needed while #eval_mutex is already locked. */
	void *normals_mutex;

	/** Needed to ensure some thread-safety during render data pre-processing. */
	void *render_mutex;

	/** Lazily initialized SoA data from the #edit_mesh field in #Mesh. */
	struct EditMeshData *edit_data;

	/**
	 * Data used to efficiently draw the mesh in the viewport, especially useful when
	 * the same mesh is used in many objects or instances. See `draw_cache_impl_mesh.c`.
	 */
	void *batch_cache;

	/** Cache for derived triangulation of the mesh. */
	struct MLoopTri_Store looptris;

	/** Cache for BVH trees generated for the mesh. Defined in 'BKE_bvhutil.c' */
	struct BVHCache *bvh_cache;

	/** Cache of non-manifold boundary data for Shrinkwrap Target Project. */
	struct ShrinkwrapBoundaryData *shrinkwrap_data;

	/** Needed in case we need to lazily initialize the mesh. */
	CustomData_MeshMasks cd_mask_extra;

	struct SubdivCCG *subdiv_ccg;
	int subdiv_ccg_tot_level;

	/** Set by modifier stack if only deformed from original. */
	char deformed_only;
	/**
	 * Copied from edit-mesh (hint, draw with edit-mesh data when true).
	 *
	 * Modifiers that edit the mesh data in-place must set this to false
	 * (most #eModifierTypeType_NonGeometrical modifiers). Otherwise the edit-mesh
	 * data will be used for drawing, missing changes from modifiers. See T79517.
	 */
	char is_original;

	/** #eMeshWrapperType and others. */
	char wrapper_type;
	/**
	 * A type mask from wrapper_type,
	 * in case there are differences in finalizing logic between types.
	 */
	char wrapper_type_finalize;

	int subsurf_resolution;
	/**
	 * Settings for lazily evaluating the subdivision on the CPU if needed. These are
	 * set in the modifier when GPU subdivision can be performed.
	 */
	char subsurf_apply_render;
	char subsurf_use_optimal_display;

	/**
	 * Caches for lazily computed vertex and polygon normals. These are stored here rather than in
	 * #CustomData because they can be calculated on a const mesh, and adding custom data layers on a
	 * const mesh is not thread-safe.
	 */
	char vert_normals_dirty;
	char poly_normals_dirty;
	float(*vert_normals)[3];
	float(*poly_normals)[3];

	void *_pad2;
} Mesh_Runtime;


typedef struct Mesh {
	ID id;
	/** Animation data (must be immediately after id for utilities to use it). */
	struct AnimData *adt;

	/** Old animation system, deprecated for 2.5. */
	struct Ipo *ipo DNA_DEPRECATED;
	struct Key *key;

	/**
	 * An array of materials, with length #totcol. These can be overridden by material slots
	 * on #Object. Indices in #MPoly.mat_nr control which material is used for every face.
	 */
	struct Material **mat;

	/**
	 * Array of vertices. Edges and faces are defined by indices into this array.
	 * \note This pointer is for convenient access to the #CD_MVERT layer in #vdata.
	 */
	struct MVert *mvert;
	/**
	 * Array of edges, containing vertex indices. For simple triangle or quad meshes, edges could be
	 * calculated from the #MPoly and #MLoop arrays, however, edges need to be stored explicitly to
	 * edge domain attributes and to support loose edges that aren't connected to faces.
	 * \note This pointer is for convenient access to the #CD_MEDGE layer in #edata.
	 */
	struct MEdge *medge;
	/**
	 * Face topology storage of the size and offset of each face's section of the #mloop face corner
	 * array. Also stores various flags and the `material_index` attribute.
	 * \note This pointer is for convenient access to the #CD_MPOLY layer in #pdata.
	 */
	struct MPoly *mpoly;
	/**
	 * The vertex and edge index at each face corner.
	 * \note This pointer is for convenient access to the #CD_MLOOP layer in #ldata.
	 */
	struct MLoop *mloop;

	/** The number of vertices (#MVert) in the mesh, and the size of #vdata. */
	int totvert;
	/** The number of edges (#MEdge) in the mesh, and the size of #edata. */
	int totedge;
	/** The number of polygons/faces (#MPoly) in the mesh, and the size of #pdata. */
	int totpoly;
	/** The number of face corners (#MLoop) in the mesh, and the size of #ldata. */
	int totloop;

	CustomData vdata, edata, pdata, ldata;

	/** "Vertex group" vertices. */
	struct MDeformVert *dvert;
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
	 * 2D vector data used for UVs. "UV" data can also be stored as generic attributes in #ldata.
	 * \note This pointer is for convenient access to the #CD_MLOOPUV layer in #ldata.
	 */
	struct MLoopUV *mloopuv;
	/**
	 * The active vertex corner color layer, if it exists. Also called "Vertex Color" in Blender's
	 * UI, even though it is stored per face corner.
	 * \note This pointer is for convenient access to the #CD_MLOOPCOL layer in #ldata.
	 */
	struct MLoopCol *mloopcol;

	/**
	 * Runtime storage of the edit mode mesh. If it exists, it generally has the most up-to-date
	 * information about the mesh.
	 * \note When the object is available, the preferred access method is #BKE_editmesh_from_object.
	 */
	struct BMEditMesh *edit_mesh;

	/**
	 * This array represents the selection order when the user manually picks elements in edit-mode,
	 * some tools take advantage of this information. All elements in this array are expected to be
	 * selected, see #BKE_mesh_mselect_validate which ensures this. For procedurally created meshes,
	 * this is generally empty (selections are stored as boolean attributes in the corresponding
	 * custom data).
	 */
	struct MSelect *mselect;

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
	struct Mesh *texcomesh;

	/** Texture space location and size, used for procedural coordinates when rendering. */
	float loc[3];
	float size[3];
	char texflag;

	/** Various flags used when editing the mesh. */
	char editflag;
	/** Mostly more flags used when editing or displaying the mesh. */
	short flag;

	/**
	 * The angle for auto smooth in radians. `M_PI` (180 degrees) causes all edges to be smooth.
	 */
	float smoothresh;

	/**
	 * Flag for choosing whether or not so store bevel weight and crease as custom data layers in the
	 * edit mesh (they are always stored in #MVert and #MEdge currently). In the future, this data
	 * may be stored as generic named attributes (see T89054 and T93602).
	 */
	char cd_flag;

	/**
	 * User-defined symmetry flag (#eMeshSymmetryType) that causes editing operations to maintain
	 * symmetrical geometry. Supported by operations such as transform and weight-painting.
	 */
	char symmetry;

	/** The length of the #mat array. */
	short totcol;

	/** Choice between different remesh methods in the UI. */
	char remesh_mode;

	char subdiv DNA_DEPRECATED;
	char subdivr DNA_DEPRECATED;
	char subsurftype DNA_DEPRECATED;

	/**
	 * Deprecated. Store of runtime data for tessellation face UVs and texture.
	 *
	 * \note This would be marked deprecated, however the particles still use this at run-time
	 * for placing particles on the mesh (something which should be eventually upgraded).
	 */
	struct MTFace *mtface;
	/** Deprecated, use mtface. */
	struct TFace *tface DNA_DEPRECATED;

	/* Deprecated. Array of colors for the tessellated faces, must be number of tessellated
	 * faces * 4 in length. This is stored in #fdata, and deprecated. */
	struct MCol *mcol;

	/**
	 * Deprecated face storage (quads & triangles only);
	 * faces are now pointed to by #Mesh.mpoly and #Mesh.mloop.
	 *
	 * \note This would be marked deprecated, however the particles still use this at run-time
	 * for placing particles on the mesh (something which should be eventually upgraded).
	 */
	struct MFace *mface;
	/* Deprecated storage of old faces (only triangles or quads). */
	CustomData fdata;
	/* Deprecated size of #fdata. */
	int totface;

	/** Per-mesh settings for voxel remesh. */
	float remesh_voxel_size;
	float remesh_voxel_adaptivity;

	int face_sets_color_seed;
	/* Stores the initial Face Set to be rendered white. This way the overlay can be enabled by
	 * default and Face Sets can be used without affecting the color of the mesh. */
	int face_sets_color_default;

	char _pad1[4];

	void *_pad2;

	Mesh_Runtime runtime;
} Mesh;


} // namespace blender_2_83

namespace blender_3_4 {		
	template<typename T> class Span;
	template<typename T> class MutableSpan;

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

	// -------------------------------------------------------------------------------------
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
		/** Shape keyblock unique id reference. */
		int uid;
		/** Layer name, MAX_CUSTOMDATA_LAYER_NAME. */
		char name[64];
		/** Layer data. */
		void* data;
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
		/** External file storing customdata layers. */
		CustomDataExternal* external;
	} CustomData;

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
		float loc[3];
		float size[3];
		char texflag;

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
	} Mesh;

	/** #CustomData.type */
	typedef enum CustomDataType {
		/* Used by GLSL attributes in the cases when we need a delayed CD type
		 * assignment (in the cases when we don't know in advance which layer
		 * we are addressing).
		 */
		CD_AUTO_FROM_NAME = -1,

		CD_MVERT = 0,
#ifdef DNA_DEPRECATED_ALLOW
		CD_MSTICKY = 1, /* DEPRECATED */
#endif
		CD_MDEFORMVERT = 2,
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

}// namespace blender 3.4

} // namespace blender
} // namespace luxcore

#endif
