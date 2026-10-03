# LuxCore Simplify Shape Algorithm Documentation

## Overview

The **legacy Simplify shape** algorithm in `src/slg/shapes/simplify.cpp` is based on **Sven Forstmann's Fast Quadric Mesh Simplification** (MIT license). This document provides a detailed explanation of the algorithm.

---

## Algorithm Overview

The algorithm reduces the number of triangles in a mesh while preserving visual quality using **Quadric Error Metrics (QEM)**. It works by iteratively collapsing the least-cost edge until a target triangle count is reached.

---

## Data Structures

### 1. SymetricMatrix (Quadric Matrix)

- 10-element symmetric 4x4 matrix stored compactly
- Represents error quadratics for vertex positions
- Used to compute the cost of collapsing edges
- Supports:
  - Matrix addition (`operator+` and `operator+=`)
  - Determinant calculation
  - Plane creation from normal + distance

```cpp
class SymetricMatrix {
public:
    float m[10];  // [0]=m11, [1]=m12, [2]=m13, [3]=m14, [4]=m22, [5]=m23, [6]=m24, [7]=m33, [8]=m34, [9]=m44
    
    // Constructors for identity, explicit values, and plane
    SymetricMatrix(const float c = 0.f);
    SymetricMatrix(float m11, m12, m13, m14, m22, m23, m24, m33, m34, m44);
    SymetricMatrix(const float a, b, c, d); // Plane: ax+by+cz+d=0
    
    // Operators
    float operator[](int c) const;
    const SymetricMatrix operator+(const SymetricMatrix &n) const;
    SymetricMatrix& operator+=(const SymetricMatrix& n);
    float det(...) const; // Determinant for 3x3 submatrix
};
```

### 2. SimplifyVertex

```cpp
struct SimplifyVertex {
    Point p;            // 3D position
    Normal norm;        // Vertex normal
    UV uv;              // Texture coordinates
    Spectrum col;       // Vertex color
    float alpha;        // Vertex alpha

    u_int tstart, tcount; // Index range into refs array for triangles referencing this vertex
    SymetricMatrix q;   // Quadric error matrix
    bool border;        // Border vertex flag
};
```

### 3. SimplifyTriangle

```cpp
struct SimplifyTriangle {
    u_int v[3];         // Vertex indices
    Normal geometryN;   // Face normal
    float err[3];       // Collapse error for each edge (0,1,2 correspond to edges v0-v1, v1-v2, v2-v0)
    bool deleted;       // Triangle has been removed
    bool dirty;         // Triangle needs error recalculation
};
```

### 4. SimplifyRef

- References a triangle and a vertex within it
- Used for prioritizing edge collapses

```cpp
struct SimplifyRef {
    u_int tid;      // Triangle index
    u_int tvertex;  // Vertex index within the triangle (0, 1, or 2)
};
```

---

## Main Algorithm Steps

### Phase 1: Initialization

**Constructor:** `Simplify(const ExtTriangleMesh &srcMesh)`

1. **Copy mesh data**: Extract vertices, normals, UVs, colors, alphas, and triangles from source mesh
2. **Store vertex attributes** in `SimplifyVertex` structures
3. **Store triangle indices** in `SimplifyTriangle` structures

### Phase 2: Decimation

**Method:** `void Decimate(const float targetTriangleCount, CameraConstRef camera, const float screenSize, const bool preserveBorder)`

#### Step 2.1: Setup

- Calculate target triangle count from percentage: `Max(1u, Floor2UInt(srcMesh.GetTotalTriangleCount() * target))`
- Set camera reference for screen-space error calculation
- Set edge screen size parameter
- Set border preservation flag
- Initialize queue size to 10% of total triangles: `Max(64u, Floor2UInt(triangles.size() * .1f))`

#### Step 2.2: Initialize Mesh State

```cpp
for (u_int i = 0; i < triangles.size(); ++i)
    triangles[i].deleted = false;
```

#### Step 2.3: Main Iteration Loop

```cpp
for (u_int iteration = 0; iteration < 64; ++iteration)
```

**Termination condition:**
```cpp
if (startTriangleCount - deletedTriangles <= targetTriangleCount)
    break;
```

##### Step 2.3.1: Update Mesh

**Method:** `UpdateMesh(const u_int iteration)`

- **Compact triangles** (for iterations > 0):
  - Remove deleted triangles from array
  - Resize triangles vector

- **Initialize Quadrics** (first iteration only, `iteration == 0`):
  ```cpp
  for (u_int i = 0; i < vertices.size(); ++i)
      vertices[i].q = SymetricMatrix(0.0);
  
  for (u_int i = 0; i < triangles.size(); ++i) {
      SimplifyTriangle &t = triangles[i];
      SimplifyVertex &v0 = vertices[t.v[0]];
      SimplifyVertex &v1 = vertices[t.v[1]];
      SimplifyVertex &v2 = vertices[t.v[2]];
      
      const Normal geometryN(Normalize(Cross(v1.p - v0.p, v2.p - v0.p)));
      t.geometryN = geometryN;
      
      // Create plane quadric: ax+by+cz+d=0 where d=-normal•v0
      const SymetricMatrix sm(geometryN.x, geometryN.y, geometryN.z,
                              -Dot(Vector(geometryN), Vector(v0.p)));
      v0.q += sm;
      v1.q += sm;
      v2.q += sm;
  }
  ```

- **Calculate Edge Errors** (first iteration only):
  ```cpp
  for (u_int i = 0; i < triangles.size(); ++i) {
      SimplifyTriangle &t = triangles[i];
      UpdateTriangleError(t);
  }
  ```

- **Build Reference ID List**:
  ```cpp
  // Count triangle references per vertex
  for (u_int i = 0; i < triangles.size(); ++i) {
      SimplifyTriangle &t = triangles[i];
      vertices[t.v[0]].tcount++;
      vertices[t.v[1]].tcount++;
      vertices[t.v[2]].tcount++;
  }
  
  // Allocate starting indices
  u_int tstart = 0;
  for (u_int i = 0; i < vertices.size(); ++i) {
      SimplifyVertex &v = vertices[i];
      v.tstart = tstart;
      tstart += v.tcount;
      v.tcount = 0;
  }
  
  // Fill references
  refs.resize(triangles.size() * 3);
  for (u_int i = 0; i < triangles.size(); ++i) {
      SimplifyTriangle &t = triangles[i];
      for (u_int j = 0; j < 3; ++j) {
          SimplifyVertex &v = vertices[t.v[j]];
          refs[v.tstart + v.tcount].tid = i;
          refs[v.tstart + v.tcount].tvertex = j;
          v.tcount++;
      }
  }
  ```

- **Identify Boundary Vertices** (first iteration only):
  ```cpp
  for (u_int i = 0; i < vertices.size(); ++i)
      vertices[i].border = false;
  
  vector<u_int> vcount, vids;
  for (u_int i = 0; i < vertices.size(); ++i) {
      SimplifyVertex &v = vertices[i];
      vcount.clear();
      vids.clear();
      
      // Count connections to each vertex
      for (u_int j = 0; j < v.tcount; ++j) {
          int k = refs[v.tstart + j].tid;
          SimplifyTriangle &t = triangles[k];
          
          for (u_int k = 0; k < 3; ++k) {
              u_int ofs = 0;
              u_int id = t.v[k];
              
              // Find if id already in vids
              while (ofs < vcount.size()) {
                  if (vids[ofs] == id) break;
                  ofs++;
              }
              
              if (ofs == vcount.size()) {
                  vcount.push_back(1);
                  vids.push_back(id);
              } else {
                  vcount[ofs]++;
              }
          }
      }
      
      // Mark vertices with only one connection as border
      for (u_int j = 0; j < vcount.size(); ++j) {
          if (vcount[j] == 1)
              vertices[vids[j]].border = true;
      }
  }
  ```

##### Step 2.3.2: Identify Candidates


```cpp
priority_queue<SimplifyRef, vector<SimplifyRef>, SimplifyRefErrCompare>
    candidateQueue{ SimplifyRefErrCompare(*this) };

for (u_int i = 0; i < triangles.size(); ++i) {
    const SimplifyTriangle &t = triangles[i];
    
    // Find the (valid) triangle vertex with the minimum error
    u_int minErrorIndex = NULL_INDEX;
    float minError = numeric_limits<float>::infinity();
    
    for (u_int j = 0; j < 3; ++j) {
        const u_int i0 = t.v[j];
        SimplifyVertex &v0 = vertices[i0];
        const u_int i1 = t.v[(j + 1) % 3];
        SimplifyVertex &v1 = vertices[i1];
        
        // Border check
        if (preserveBorder) {
            if (v0.border && v1.border)
                continue;
        } else {
            if (v0.border != v1.border)
                continue;
        }
        
        // Compute vertex to collapse to
        Point p;
        CalculateCollapseError(i0, i1, &p);
        
        // Don't remove if flipped
        if (Flipped(p, i0, i1)) continue;
        if (Flipped(p, i1, i0)) continue;
        
        if (t.err[j] < minError) {
            minErrorIndex = j;
            minError = t.err[j];
        }
    }
    
    if (minErrorIndex == NULL_INDEX) continue;
    
    // Add to candidate queue (limited size)
    if (candidateQueue.size() < maxCandidateQueueSize) {
        candidateQueue.push(SimplifyRef{i, minErrorIndex});
        continue;
    }
    
    const SimplifyRef &top = candidateQueue.top();
    if (t.err[minErrorIndex] < triangles[top.tid].err[top.tvertex]) {
        candidateQueue.pop();
        candidateQueue.push(SimplifyRef{i, minErrorIndex});
    }
}
```

##### Step 2.3.3: Collapse Edges

**Method:** `bool CollapseEdge(const u_int triangleIndex, const u_int startVertexIndex, vector<bool> &deleted0, vector<bool> &deleted1)`

For each candidate edge:

1. **Check validity**:
   ```cpp
   SimplifyTriangle &t = triangles[triangleIndex];
   if (t.deleted) return false;
   if (t.dirty) return false;
   ```

2. **Get vertices**:
   ```cpp
   const u_int i0 = t.v[startVertexIndex];
   SimplifyVertex &v0 = vertices[i0];
   const u_int i1 = t.v[(startVertexIndex + 1) % 3];
   SimplifyVertex &v1 = vertices[i1];
   ```

3. **Border check**:
   ```cpp
   if (v0.border != v1.border) return false;
   ```

4. **Compute collapse position**:
   ```cpp
   Point p;
   CalculateCollapseError(i0, i1, &p);
   ```

5. **Check for flipping**:
   ```cpp
   deleted0.resize(v0.tcount);
   deleted1.resize(v1.tcount);
   
   if (Flipped(p, i0, i1, &deleted0)) return false;
   if (Flipped(p, i1, i0, &deleted1)) return false;
   ```

6. **Save original vertex attributes**:
   ```cpp
   // Save positions
   const Point triPoint0 = vertices[t.v[0]].p;
   const Point triPoint1 = vertices[t.v[1]].p;
   const Point triPoint2 = vertices[t.v[2]].p;
   
   // Save normals
   const Normal triNorm0 = vertices[t.v[0]].norm;
   const Normal triNorm1 = vertices[t.v[1]].norm;
   const Normal triNorm2 = vertices[t.v[2]].norm;
   
   // Save UVs, colors, alphas...
   ```

7. **Collapse the edge**:
   ```cpp
   // Move v0 to collapse position
   v0.p = p;
   v0.q = v1.q + v0.q;  // Combine quadrics
   
   // Interpolate other vertex attributes using barycentric coordinates
   float b1, b2;
   if (Triangle::GetBaryCoords(triPoint0, triPoint1, triPoint2, p, &b1, &b2)) {
       const float b0 = 1.f - b1 - b2;
       
       if (hasNormals)
           v0.norm = Normalize(b0 * triNorm0 + b1 * triNorm1 + b2 * triNorm2);
       if (hasUVs)
           v0.uv = b0 * triUV0 + b1 * triUV1 + b2 * triUV2;
       if (hasColors)
           v0.col = b0 * triCol0 + b1 * triCol1 + b2 * triCol2;
       if (hasAlphas)
           v0.alpha = b0 * triAlpha0 + b1 * triAlpha1 + b2 * triAlpha2;
   } else {
       // Fallback for malformed triangles
       if (hasNormals) v0.norm = triNorm0;
       if (hasUVs) v0.uv = triUV0;
       if (hasColors) v0.col = triCol0;
       if (hasAlphas) v0.alpha = triAlpha0;
   }
   ```

8. **Update triangle connections**:
   ```cpp
   const u_int tstart = refs.size();
   UpdateTriangles(i0, v0, deleted0);
   UpdateTriangles(i0, v1, deleted1);
   const u_int tcount = refs.size() - tstart;
   
   // Compact or append references
   if (tcount <= v0.tcount) {
       if (tcount)
           copy(&refs[tstart], &refs[tstart] + tcount, &refs[v0.tstart]);
   } else {
       v0.tstart = tstart;
   }
   v0.tcount = tcount;
   ```

**Method:** `UpdateTriangles(const u_int i0, const SimplifyVertex &v, const vector<bool> &deleted)`
```cpp
for (u_int k = 0; k < v.tcount; ++k) {
    const SimplifyRef &r = refs[v.tstart + k];
    SimplifyTriangle &t = triangles[r.tid];
    
    if (t.deleted) continue;
    
    if (deleted[k]) {
        t.deleted = true;
        deletedTriangles++;
        continue;
    }
    
    // Remap vertex reference
    t.v[r.tvertex] = i0;
    t.dirty = true;
    UpdateTriangleError(t);
    
    refs.push_back(r);
}
```

**Method:** `UpdateTriangleError(SimplifyTriangle &t)`
```cpp
t.err[0] = CalculateCollapseError(t.v[0], t.v[1]) *
            CalculateCollapseScreenErrorScale(vertices[t.v[0]].p, vertices[t.v[1]].p);
t.err[1] = CalculateCollapseError(t.v[1], t.v[2]) *
            CalculateCollapseScreenErrorScale(vertices[t.v[1]].p, vertices[t.v[2]].p);
t.err[2] = CalculateCollapseError(t.v[2], t.v[0]) *
            CalculateCollapseScreenErrorScale(vertices[t.v[2]].p, vertices[t.v[0]].p);
```

##### Step 2.3.4: Clear Dirty Flags

```cpp
for (u_int i = 0; i < triangles.size(); ++i)
    triangles[i].dirty = false;
```

#### Step 2.4: Check Progress

```cpp
const u_int iterationDeletedTriangles = deletedTriangles - initialdeletedTriangles;
SDL_LOG("Simplify iteration " << iteration << " (" << candidateList.size() << " edge candidates, deleted " 
       << iterationDeletedTriangles << "/" << deletedTriangles << " of " << startTriangleCount << " triangles)");

if (iterationDeletedTriangles == 0) break;
```

### Phase 3: Compact Mesh

**Method:** `CompactMesh()`

After all iterations, clean up the mesh:

1. **Compact triangles**:
   ```cpp
   u_int dst = 0;
   for (u_int i = 0; i < triangles.size(); ++i) {
       if (!triangles[i].deleted) {
           triangles[dst++] = triangles[i];
           vertices[t.v[0]].tcount = 1;
           vertices[t.v[1]].tcount = 1;
           vertices[t.v[2]].tcount = 1;
       }
   }
   triangles.resize(dst);
   ```

2. **Compact vertices**:
   ```cpp
   dst = 0;
   for (u_int i = 0; i < vertices.size(); ++i) {
       if (vertices[i].tcount) {
           vertices[i].tstart = dst;
           vertices[dst].p = vertices[i].p;
           vertices[dst].norm = vertices[i].norm;
           vertices[dst].uv = vertices[i].uv;
           vertices[dst].col = vertices[i].col;
           vertices[dst].alpha = vertices[i].alpha;
           dst++;
       }
   }
   vertices.resize(dst);
   ```

3. **Renumber vertex indices**:
   ```cpp
   for (u_int i = 0; i < triangles.size(); ++i) {
       SimplifyTriangle &t = triangles[i];
       t.v[0] = vertices[t.v[0]].tstart;
       t.v[1] = vertices[t.v[1]].tstart;
       t.v[2] = vertices[t.v[2]].tstart;
   }
   ```

### Phase 4: Output

**Method:** `ExtTriangleMeshUPtr GetExtMesh() const`

Converts the simplified internal representation back to an `ExtTriangleMesh`:

1. Create new vertex buffer with compacted positions
2. Create new normal buffer (if source had normals)
3. Create new UV buffer (if source had UVs)
4. Create new color buffer (if source had colors)
5. Create new alpha buffer (if source had alphas)
6. Create new triangle buffer with updated indices
7. Return as `ExtTriangleMeshUPtr`

---

## Error Metrics

### Geometric Error

**Method:** `float CalculateCollapseError(const u_int v1Index, const u_int v2Index, Point *pResult = nullptr)`

Uses **Quadric Error Metrics** to compute the cost of collapsing an edge:

1. Combine quadrics: `q = vertices[v1Index].q + vertices[v2Index].q`
2. Test collapse to v1, v2, and midpoint
3. For each position, compute: `VertexError(q, x, y, z)`
4. Return minimum error + 1 (to ensure positive values)

**VertexError formula:**
```
q[0] * x² + 2*q[1] * x*y + 2*q[2] * x*z + 2*q[3] * x +
q[4] * y² + 2*q[5] * y*z + 2*q[6] * y +
q[7] * z² + 2*q[8] * z + q[9]
```

### Screen-Space Error Scaling

**Method:** `float CalculateCollapseScreenErrorScale(const Point &v0, const Point &v1)`

Scales geometric error by screen-space edge length:

```cpp
if (edgeScreenSize > 0.f) {
    const float notVisibleScale = .5f;
    
    // Project vertices to screen space
    float v0x, v0y;
    if (!camera->GetSamplePosition(v0, &v0x, &v0y) || !IsValid(v0x) || !IsValid(v0y))
        return notVisibleScale;
    
    // Normalize to [0,1] range
    v0x /= camera->filmWidth;
    v0y /= camera->filmHeight;
    
    float v1x, v1y;
    if (!camera->GetSamplePosition(v1, &v1x, &v1y) || !IsValid(v1x) || !IsValid(v1y))
        return notVisibleScale;
    
    v1x /= camera->filmWidth;
    v1y /= camera->filmHeight;
    
    // Compute edge length in normalized screen coordinates
    const float edge = sqrtf(Sqr(v0x - v1x) + Sqr(v0y - v1y));
    if (edge == 0.f) return notVisibleScale;
    
    return Max(edge / edgeScreenSize, notVisibleScale);
} else {
    return 1.f;
}
```

This ensures edges that are visually small contribute less to the error metric.

---

## Flipping Prevention

**Method:** `bool Flipped(const Point &p, const u_int i0, const u_int i1, vector<bool> *deleted = nullptr)`

Checks if a triangle would flip (become inverted) when an edge is collapsed:

```cpp
const SimplifyVertex &v0 = vertices[i0];

for (u_int k = 0; k < v0.tcount; ++k) {
    const SimplifyTriangle &t = triangles[refs[v0.tstart + k].tid];
    if (t.deleted) continue;
    
    const u_int s = refs[v0.tstart + k].tvertex;
    const u_int id1 = t.v[(s + 1) % 3];
    const u_int id2 = t.v[(s + 2) % 3];
    
    // Skip if this triangle contains the other vertex
    if (id1 == i1 || id2 == i1) {
        if (deleted) (*deleted)[k] = true;
        continue;
    }
    
    // Check if the triangle is too narrow
    const Vector d1 = Normalize(vertices[id1].p - p);
    const Vector d2 = Normalize(vertices[id2].p - p);
    if (AbsDot(d1, d2) > .999f)
        return true;  // Too narrow - would be degenerate
    
    // Check if the normal is changing side
    const Normal geometryN(Normalize(Cross(d1, d2)));
    if (Dot(geometryN, t.geometryN) < .2f)
        return true;  // Normal flipped
    
    if (deleted) (*deleted)[k] = false;
}

return false;
```

---

## Algorithm Flow Diagram

```
Input Mesh
     │
     ▼
Initialize Simplify structure (copy mesh data)
     │
     ▼
For each iteration (max 64):
     │
     ├─── UpdateMesh(iteration)
     │     │
     │     ├─── Compact triangles (remove deleted)
     │     │
     │     ├─── Initialize quadrics (1st iteration only)
     │     │     │
     │     │     └─── For each triangle: add plane quadric to all 3 vertices
     │     │
     │     ├─── Calculate edge errors (1st iteration only)
     │     │
     │     ├─── Build reference ID list
     │     │
     │     └─── Identify boundary vertices (1st iteration only)
     │
     ├─── Build candidate queue (priority queue sorted by error)
     │     │
     │     └─── For each triangle: find edge with minimum error
     │
     └─── Collapse edges (up to maxCandidateQueueSize)
           │
           ├─── Check validity (not deleted, not dirty)
           │
           ├─── Border check (if preserveBorder enabled)
           │
           ├─── Compute collapse position (minimum error position)
           │
           ├─── Check for flipping
           │
           ├─── Save original vertex attributes
           │
           ├─── Collapse edge: move v0 to p, combine quadrics
           │
           ├─── Interpolate attributes using barycentric coordinates
           │
           └─── Update triangle connections
                 │
                 ├─── Remap vertex references
                 ├─── Mark deleted triangles
                 └─── Update dirty flags and recalculate errors
     │
     ▼
Check termination (target reached or no progress)
     │
     ▼
Compact final mesh (remove deleted triangles and vertices)
     │
     ▼
Output simplified ExtTriangleMesh
```

---

## Algorithm Parameters

| Parameter | Type | Description | Default | Range |
|-----------|------|-------------|---------|-------|
| `target` | float | Target triangle count as fraction of original | 0.25 | 0.0 - 1.0 |
| `edgeScreenSize` | float | Screen-space edge size for error scaling | 0.0 | 0.0 - 1.0 |
| `preserveBorder` | bool | Preserve mesh borders during simplification | false | true/false |
| `maxCandidateQueueSize` | u_int | Max candidates per iteration | 10% of triangles, min 64 | ≥ 64 |

---

## Complexity Analysis

| Aspect | Complexity | Notes |
|--------|------------|-------|
| **Time (per iteration)** | O(n + k log k) | n = triangles, k = queue size |
| **Space** | O(n) | Vertices, triangles, references |
| **Total iterations** | O(log n) | Typically, capped at 64 |
| **Overall** | O(n log n) | In practice |
| **Memory per vertex** | O(1) | Fixed size structures |
| **Memory per triangle** | O(1) | Fixed size structures |

---

## Strengths

1. **Quadric Error Metrics**: Provides good visual quality preservation by minimizing geometric error
2. **Screen-space weighting**: Adapts simplification based on visible detail - small edges on screen are simplified more aggressively
3. **Border preservation**: Option to maintain mesh boundaries for better silhouette preservation
4. **Attribute interpolation**: Preserves normals, UVs, colors, alphas during collapse using barycentric coordinates
5. **Flipping prevention**: Avoids creating inverted or degenerate triangles through robust geometric checks
6. **Incremental updates**: Only recomputes errors for affected triangles, not the entire mesh
7. **Progressive**: Works in iterations, can be interrupted and resumed
8. **Stable**: Uses priority queue to always pick the best candidate

---

## Limitations

1. **No texture seams preservation**: May cause texture distortion across UV seams
2. **No material boundaries**: Doesn't respect material differences - may merge vertices from different materials
3. **No UV seams preservation**: May cause UV distortion across UV chart boundaries
4. **Fixed iteration limit**: Max 64 iterations regardless of mesh complexity
5. **Greedy approach**: Makes locally optimal choices, not globally optimal
6. **No curvature-aware simplification**: Doesn't specifically preserve high-curvature areas
7. **No feature detection**: Doesn't identify and preserve sharp edges or corners

---

## Comparison with Other Methods

| Method | Quality | Speed | Memory | Complexity |
|--------|--------|-------|--------|------------|
| Quadric Error Metrics (this) | High | Medium | Medium | Medium |
| Edge Collapse | Medium | Fast | Low | Low |
| Vertex Clustering | Low | Fast | Low | Low |
| Progressive Meshes | High | Slow | High | High |
| View-Dependent | Very High | Slow | High | High |

---

## References

- **Original code**: [Sven Forstmann's Fast-Quadric-Mesh-Simplification](https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification) (MIT License)
- **Papers**: [Michael Garland and Paul S. Heckbert, "Surface Simplification Using Quadric Error Metrics"](https://mgarland.org/research/quadrics.html), SIGGRAPH 1997
- **Related work**: [Hoppe, "Progressive Meshes"](http://www.microsoft.com/en-us/research/publication/progressive-meshes/), SIGGRAPH 1996

---

## Usage in LuxCore

The Simplify shape is used in LuxCore scenes via the `simplify` shape type:

```properties
# Basic usage
scene.shapes.simplified.type = simplify
scene.shapes.simplified.source = original_mesh
scene.shapes.simplified.target = 0.5

# With all parameters
scene.shapes.simplified.type = simplify
scene.shapes.simplified.source = original_mesh
scene.shapes.simplified.target = 0.25      # Reduce to 25% of original triangles
scene.shapes.simplified.edgescreensize = 0.1  # Screen-space error scaling
scene.shapes.simplified.preserveborder = true  # Preserve mesh borders
```

The algorithm is particularly useful for:
- Reducing memory usage for complex scenes
- Improving rendering performance
- Creating level-of-detail (LOD) meshes
- Preparing meshes for real-time rendering

---

*Documentation generated: 2026-09-11*
*Source: src/slg/shapes/simplify.cpp*
