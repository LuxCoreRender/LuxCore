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
	short no[3];
	char flag, bweight;
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
// ----------------------------------------------------------------------

#define DNA_DEPRECATED

// -------------------------------------------------------------------------------------
// From blender/source/blender/makesdna/DNA_ID.h

struct Library;
struct IDProperty;
struct IDOverrideLibrary;

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

struct BLI_mempool;
struct CustomDataExternal;

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

struct MLoopTri_Store {
  /* WARNING! swapping between array (ready-to-be-used data) and array_wip
   * (where data is actually computed)
   * shall always be protected by same lock as one used for looptris computing. */
  struct MLoopTri *array, *array_wip;
  int len;
  int len_alloc;
};

struct EditMeshData;
struct SubdivCCG;
struct LinkNode;
struct ShrinkwrapBoundaryData;

typedef struct Mesh_Runtime {
  /* Evaluated mesh for objects which do not have effective modifiers.
   * This mesh is used as a result of modifier stack evaluation.
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

  /**
   * Legacy face storage (quads & tries only),
   * faces are now stored in #Mesh.mpoly & #Mesh.mloop arrays.
   *
   * \note This would be marked deprecated however the particles still use this at run-time
   * for placing particles on the mesh (something which should be eventually upgraded).
   */
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

  int face_sets_color_seed;
  /* Stores the initial Face Set to be rendered white. This way the overlay can be enabled by
   * default and Face Sets can be used without affecting the color of the mesh. */
  int face_sets_color_default;

  /** Deprecated multiresolution modeling data, only keep for loading old files. */
  struct Multires *mr DNA_DEPRECATED;

  Mesh_Runtime runtime;
} Mesh;

} // namespace blender
} // namespace luxcore

#endif
