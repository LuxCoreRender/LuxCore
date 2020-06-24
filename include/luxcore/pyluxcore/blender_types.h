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
// Note: all types of not needed pointers have been replaced with void *
// ----------------------------------------------------------------------

typedef struct ID {
    void *next, *prev;
    void *newid;
    void *lib;
    char name[66];
    short flag;
    int tag;
    int us;
    int icon_id;
    int recalc;
    int recalc_up_to_undo_push;
    int recalc_after_undo_push;
    unsigned int session_uuid;
    void *properties;
    void *override_library;
    void *orig_id;
    void *py_instance;
} ID;

// From blender/source/blender/makesdna/DNA_mesh_types.h
struct MLoopTri_Store {
    void *array, *array_wip;
    int len;
    int len_alloc;
};

typedef struct Mesh_Runtime {
    void *mesh_eval;
    void *eval_mutex;

    void *edit_data;
    void *batch_cache;

    void *subdiv_ccg;
    void *_pad1;
    int subdiv_ccg_tot_level;
    char _pad2[4];

    int64_t cd_dirty_vert;
    int64_t cd_dirty_edge;
    int64_t cd_dirty_loop;
    int64_t cd_dirty_poly;

    struct MLoopTri_Store looptris;

    void *bvh_cache;

    void *shrinkwrap_data;

    char deformed_only;
    char is_original;
    char _pad[6];
} Mesh_Runtime;

// From blender/source/blender/makesdna/DNA_customdata_types.h
// CustomDataType
static const int CD_NORMAL = 8;

typedef struct CustomDataLayer {
    int type;
    int offset;
    int flag;
    int active;
    int active_rnd;
    int active_clone;
    int active_mask;
    int uid;
    char name[64];
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
    int totlayer, maxlayer;
    int totsize;
    void *pool;
    void *external;
} CustomData;

// from blender/source/blender/makesdna/DNA_mesh_types.h
typedef struct Mesh {
    ID id;
    void *adt;

    void *ipo;
    void *key;
    void *mat;
    void *mselect;

    void *mpoly;
    void *mloop;
    void *mloopuv;
    void *mloopcol;

    void *mface;
    void *mtface;
    void *tface;
    void *mvert;
    void *medge;
    void *dvert;

    void *mcol;
    void *texcomesh;

    void *edit_mesh;

    struct CustomData vdata, edata, fdata;
    struct CustomData pdata, ldata;

    int totvert, totedge, totface, totselect;

    int totpoly, totloop;
    
    int act_face;

    float loc[3];
    float size[3];

    short texflag, flag;
    float smoothresh;

    char cd_flag, _pad;

    char subdiv, subdivr;
    char subsurftype;
    char editflag;

    short totcol;

    float remesh_voxel_size;
    float remesh_voxel_adaptivity;
    char remesh_mode;

    char _pad1[3];

    int face_sets_color_seed;
    int face_sets_color_default;

    void *mr;

    Mesh_Runtime runtime;
} Mesh;

} // namespace blender
} // namespace luxcore

#endif
