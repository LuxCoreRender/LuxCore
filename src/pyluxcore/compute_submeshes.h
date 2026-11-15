#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <string>
#include <vector>

namespace py = pybind11;

// C++ port of BlendLuxCore's mesh_converter.compute_submeshes(): weld points,
// find varying-data islands, group triangles by (material, island) and gather
// the per-submesh varying data. Takes the numpy arrays extracted from Blender
// and returns (payloads, info): a list of per-submesh dicts with numpy arrays
// (zero-copy wrapped C++ buffers) ready for Scene.DefineMeshExt, plus stats.
// The GIL is released for the whole computation.
py::tuple compute_submeshes_c(
	py::array_t<float, py::array::c_style | py::array::forcecast> vertex_points,
	py::array_t<uint32_t, py::array::c_style | py::array::forcecast> loop_vertices,
	py::array_t<float, py::array::c_style | py::array::forcecast> loop_normals,
	py::array_t<uint32_t, py::array::c_style | py::array::forcecast> triangle_loops,
	py::array_t<uint32_t, py::array::c_style | py::array::forcecast> loop_triangle_materials,
	std::vector<py::array_t<float, py::array::c_style | py::array::forcecast>> loop_uvs,
	std::vector<py::array_t<float, py::array::c_style | py::array::forcecast>> color_attrs,
	std::vector<std::string> color_domains,
	py::object edge_verts,   // None or int32 [E,2]
	py::object edge_crease,  // None or float32 [E]
	py::object transform,    // None or float32 [4,4]
	bool is_viewport_render,
	bool use_instancing,
	bool share_points,
	std::string mesh_key);
