# -*- coding: utf-8 -*-
###########################################################################
#   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 #
#                                                                         #
#   This file is part of LuxRays.                                         #
#                                                                         #
#   LuxRays is free software; you can redistribute it and/or modify       #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 3 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   LuxRays is distributed in the hope that it will be useful,            #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program.  If not, see <http://www.gnu.org/licenses/>. #
#                                                                         #
#   LuxRays website: http://www.luxrender.net                             #
###########################################################################
#
# SmallLuxGPU v1.5 render engine Blender 2.5 plug-in
# v0.54
# Source: http://www.luxrender.net/forum/viewforum.php?f=34

import bpy
import mathutils
import os

def slg_properties():
  # Add SmallLuxGPU properties
  # (should be: bpy.types.RenderSettings; doesn't work yet)
  StringProperty = bpy.types.Scene.StringProperty
  FloatProperty = bpy.types.Scene.FloatProperty
  IntProperty = bpy.types.Scene.IntProperty
  BoolProperty = bpy.types.Scene.BoolProperty
  EnumProperty = bpy.types.Scene.EnumProperty
  FloatVectorProperty = bpy.types.Scene.FloatVectorProperty

  StringProperty(attr="slg_path", name="SmallLuxGPU Path",
      description="Full path to SmallLuxGPU's executable",
      default="", maxlen=1024, subtype="FILE_PATH")

  StringProperty(attr="slg_scene_path", name="Export scene path",
      description="Full path to directory where the exported scene is created",
      default="", maxlen=1024, subtype="FILE_PATH")
  
  StringProperty(attr="slg_scenename", name="Scene Name",
      description="Name of SmallLuxGPU scene to create",
      default="testscene", maxlen=1024)
  
  BoolProperty(attr="slg_export", name="PLY",
      description="Export PLY (mesh data) files (uncheck only if scene has already been exported)",
      default=True)
          
  BoolProperty(attr="slg_vuvs", name="UVs",
      description="Export optional vertex uv information (if available and texture is assigned)",
      default=True)
          
  BoolProperty(attr="slg_vcolors", name="VCs",
      description="Export optional vertex color information (only if present)",
      default=False)
          
  BoolProperty(attr="slg_vnormals", name="VNs",
      description="Export optional vertex normal information",
      default=False)

  BoolProperty(attr="slg_infinitelightbf", name="InfiniteLight BF",
      description="Enable brute force rendering for InifinteLight light source",
      default=False)

  BoolProperty(attr="slg_cameramotionblur", name="Camera Motion Blur",
      description="Enable camera motion blur",
      default=False)

  BoolProperty(attr="slg_low_latency", name="Low Latency",
      description="In low latency mode render is more interactive, otherwise render is faster",
      default=True)
          
  IntProperty(attr="slg_refreshrate", name="Screen Refresh Interval",
      description="How often, in milliseconds, the screen refreshes",
      default=250, min=1, soft_min=1)
  
  IntProperty(attr="slg_native_threads", name="Native Threads",
      description="Number of native CPU threads",
      default=2, min=0, max=1024, soft_min=0, soft_max=1024)
  
  IntProperty(attr="slg_devices_threads", name="OpenCL Threads",
      description="Number of OpenCL devices threads",
      default=2, min=0, max=1024, soft_min=0, soft_max=1024)
  
  EnumProperty(attr="slg_rendering_type", name="Rendering Type",
      description="Select the desired rendering type",
      items=(("0", "Path", "Path tracing"),
             ("1", "Direct", "Direct lighting only")),
      default="0")

  EnumProperty(attr="slg_accelerator_type", name="Accelerator Type",
      description="Select the desired ray tracing accelerator type",
      items=(("-1", "Default", "Default"),
             ("0", "BVH", "Bounding Volume Hierarchy"),
             ("1", "QBVH", "Quad-Bounding Volume Hierarchy"),
             ("2", "QBVH (image storage disabled)", "Quad-Bounding Volume Hierarchy with disabled image storage")),
      default="-1")

  EnumProperty(attr="slg_film_filter_type", name="Film Filter Type",
      description="Select the desired film filter type",
      items=(("0", "None", "No filter"),
             ("1", "Gaussian", "Gaussian filter")),
      default="1")

  EnumProperty(attr="slg_film_tonemap_type", name="Film Tonemap Type",
      description="Select the desired film tonemap type",
      items=(("0", "Linear", "Linear tonemapping"),
             ("1", "Reinhard02", "Reinhard '02 tonemapping")),
      default="0")

  FloatProperty(attr="slg_film_gamma", name="Gamma",
      description="Gamma correction on screen and for saving no HDR file format",
      default=2.2, min=0, max=10, soft_min=0, soft_max=10, precision=3)

  EnumProperty(attr="slg_lightstrategy", name="Light Strategy",
      description="Select the desired light strategy",
      items=(("0", "ONE_UNIFORM", "ONE_UNIFORM"),
             ("1", "ALL_UNIFORM", "ALL_UNIFORM")),
      default="0")

  EnumProperty(attr="slg_sampleperpixel", name="Sample per pixel",
      description="Select the desired number of samples per pixel for each pass",
      items=(("1", "1x1", "1x1"),
             ("2", "2x2", "2x2"),
             ("3", "3x3", "3x3"),
             ("4", "4x4", "4x4"),
             ("5", "5x5", "5x5"),
             ("6", "6x6", "6x6"),
             ("7", "7x7", "7x7"),
             ("8", "8x8", "8x8"),
             ("9", "9x9", "9x9")),
      default="4")

  EnumProperty(attr="slg_imageformat", name="Image File Format",
      description="Image file save format, saved with scene files (also Blender intermediary format)", 
      items=(("png", "PNG", "PNG"),
             ("exr", "OpenEXR", "OpenEXR"),
             ("jpg", "JPG", "JPG")),
      default="png")

  IntProperty(attr="slg_tracedepth", name="Max Path Trace Depth",
      description="Maximum path tracing depth",
      default=3, min=1, max=1024, soft_min=1, soft_max=1024)
  
  IntProperty(attr="slg_shadowrays", name="Shadow Rays",
      description="Shadow rays",
      default=1, min=1, max=1024, soft_min=1, soft_max=1024)

  EnumProperty(attr="slg_rrstrategy", name="Russian Roulette Strategy",
      description="Select the desired russian roulette strategy",
      items=(("0", "Probability", "Probability"),
             ("1", "Importance", "Importance")),
      default="1")

  IntProperty(attr="slg_rrdepth", name="Russian Roulette Depth",
      description="Russian roulette depth",
      default=5, min=1, max=1024, soft_min=1, soft_max=1024)

  FloatProperty(attr="slg_rrprob", name="Russian Roulette Probability",
      description="Russian roulette probability",
      default=0.75, min=0, max=1, soft_min=0, soft_max=1, precision=3)

  FloatProperty(attr="slg_rrcap", name="Russian Roulette Importance Cap",
      description="Russian roulette importance cap",
      default=0.25, min=0.01, max=0.99, soft_min=0.1, soft_max=0.9, precision=3)

  # Participating Media properties
  BoolProperty(attr="slg_enablepartmedia", name="Participating Media",
      description="Use single scattering participating media",
      default=False)
  FloatProperty(attr="slg_partmedia_stepsize", name="Step Size",
      description="Set ray marching step size",
      default=2.0, min=0.01, max=100, soft_min=0.1, soft_max=10, precision=2)
  FloatProperty(attr="slg_partmedia_rrprob", name="RR prob.",
      description="Russian Roulette probability",
      default=0.33, min=0.01, max=1.0, soft_min=0.1, soft_max=0.9, precision=2)
  FloatVectorProperty(attr="slg_partmedia_emission", name="Emission",
      description="Media emission",
      default=(0.0, 0.0, 0.0), min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, subtype="COLOR")
  FloatVectorProperty(attr="slg_partmedia_scattering", name="Scattering",
      description="Media scattering",
      default=(0.0, 0.0, 0.0), min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, subtype="COLOR")
  FloatVectorProperty(attr="slg_partmedia_bbox", name="Bounding Box",
      description="Media bounding box",
      default=(-10.0, -10.0, -10.0, 10.0, 10.0, 10.0), subtype="NONE", size=6)

  BoolProperty(attr="slg_enablebatchmode", name="Batch Mode",
      description="Render in background (required for animations)",
      default=False)
  
  IntProperty(attr="slg_batchmodetime", name="Batch mode max run time",
      description="Max number of seconds to run in batch mode; 0 = ignore",
      default=120, min=0, soft_min=0)
  
  IntProperty(attr="slg_batchmodespp", name="Batch mode max samples per pixel",
      description="Max number of samples per pixels in batch mode; 0 = ignore",
      default=128, min=0, soft_min=0)

  IntProperty(attr="slg_batchmode_periodicsave", name="Batch mode periodic image save",
      description="Interval in second between image save in batch mode; 0 = ignore",
      default=0, min=0, soft_min=0)
  
  BoolProperty(attr="slg_waitrender", name="Wait for SLG",
      description="Wait for render to finish; load image into render results (required for animations)",
      default=False)
  
  BoolProperty(attr="slg_opencl_cpu", name="CPU",
      description="Use OpenCL CPU devices if available",
      default=False)
  
  BoolProperty(attr="slg_opencl_gpu", name="GPU",
      description="Use OpenCL GPU devices if available",
      default=True)
  
  IntProperty(attr="slg_gpu_workgroup_size", name="GPU Workgroup Size",
      description="Use a value of 0 to use the default value for your GPU",
      default=64, min=0, max=4096, soft_min=0, soft_max=4096)
  
  IntProperty(attr="slg_platform", name="OpenCL platform",
      description="OpenCL Platform to use; if you have multiple OpenCL ICDs installed",
      default=0, min=0, max=256, soft_min=0, soft_max=256)
  
  StringProperty(attr="slg_devices", name="OpenCL devices to use",
      description="blank = default (bitwise on/off value for each device, see SLG docs)",
      default="", maxlen=64)
  
  # Add SLG Camera Lens Radius
  bpy.types.Camera.FloatProperty(attr="slg_lensradius", name="SLG DOF Lens Radius", 
      description="SmallLuxGPU camera lens radius for depth of field",
      default=0.015, min=0, max=10, soft_min=0, soft_max=10, precision=3)

  # Add Material PLY export override
  bpy.types.Material.BoolProperty(attr="slg_forceply", name="SLG Force PLY Export", 
      description="SmallLuxGPU - Force export of PLY (mesh data) related to this material",
      default=False)

  # Use some of the existing panels
  import properties_render
  properties_render.RENDER_PT_render.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_render.RENDER_PT_layers.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_render.RENDER_PT_dimensions.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_render.RENDER_PT_output.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_render.RENDER_PT_post_processing.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  del properties_render
  
  import properties_material
  properties_material.MATERIAL_PT_context_material.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_material.MATERIAL_PT_diffuse.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_material.MATERIAL_PT_shading.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_material.MATERIAL_PT_transp.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  properties_material.MATERIAL_PT_mirror.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  del properties_material
  
  import properties_texture
  for member in dir(properties_texture):
    subclass = getattr(properties_texture, member)
    try:
      subclass.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
    except:
      pass
  del properties_texture
  
  import properties_data_camera
  for member in dir(properties_data_camera):
    subclass = getattr(properties_data_camera, member)
    try:
      subclass.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
    except:
      pass
  del properties_data_camera
  
  import properties_world
  properties_world.WORLD_PT_environment_lighting.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  del properties_world

  import properties_data_lamp
  properties_data_lamp.DATA_PT_sunsky.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  del properties_data_lamp

def slg_lensradius(self, context):
  if context.scene.render.engine == 'SMALLLUXGPU_RENDER':  
    self.layout.split().column().prop(context.camera, "slg_lensradius", text="SLG Lens Radius")

def slg_forceply(self, context):
  if context.scene.render.engine == 'SMALLLUXGPU_RENDER':  
    self.layout.split().column().prop(context.material, "slg_forceply")

class RenderButtonsPanel(bpy.types.Panel):
  bl_space_type = 'PROPERTIES'
  bl_region_type = 'WINDOW'
  bl_context = "render"
  
  def poll(self, context):
    return context.scene.render.engine in self.COMPAT_ENGINES

class RENDER_PT_slrender_options(RenderButtonsPanel):
  bl_label = "SmallLuxGPU Options"
  COMPAT_ENGINES = {'SMALLLUXGPU_RENDER'}

  def draw(self, context):
    layout = self.layout
    scene = context.scene 

    split = layout.split()
    col = split.column()
    col.label(text="Full path to SmallLuxGPU's executable:")
    col.prop(scene, "slg_path", text="")
    col.label(text="Full path where the scene is exported:")
    col.prop(scene, "slg_scene_path", text="")
    col.prop(scene, "slg_scenename", text="Scene Name")
    split = layout.split(percentage=0.25)
    col = split.column()
    col.prop(scene, "slg_export")
    col = split.column()
    col.active = scene.slg_export    
    col.prop(scene, "slg_vuvs")
    col = split.column()
    col.active = scene.slg_export    
    col.prop(scene, "slg_vcolors")
    col = split.column()
    col.active = scene.slg_export
    col.prop(scene, "slg_vnormals")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_infinitelightbf")
    col = split.column()
    col.prop(scene, "slg_cameramotionblur")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_rendering_type")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_accelerator_type")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_film_filter_type")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_film_tonemap_type")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_imageformat")
    col = split.column()
    col.prop(scene, "slg_film_gamma")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_lightstrategy")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_tracedepth", text="Depth")
    col = split.column()
    col.prop(scene, "slg_shadowrays", text="Shadow")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_sampleperpixel")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_rrstrategy")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_rrdepth", text="RR Depth")
    col = split.column()
    if scene.slg_rrstrategy == "0":
        col.prop(scene, "slg_rrprob", text="RR Prob")
    else:
        col.prop(scene, "slg_rrcap", text="RR Cap")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_enablepartmedia")
    if scene.slg_enablepartmedia:
        split = layout.split()
        col = split.column()
        col.prop(scene, "slg_partmedia_stepsize", text="Step Size")
        col = split.column()
        col.prop(scene, "slg_partmedia_rrprob", text="RR Prob.")
        split = layout.split()
        col = split.column(0)
        col.prop(scene, "slg_partmedia_emission", text="Emission")
        col = split.column(1)
        col.prop(scene, "slg_partmedia_scattering", text="Scattering")
        split = layout.split()
        col = split.column()
        col.prop(scene, "slg_partmedia_bbox")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_low_latency")
    col = split.column()
    col.prop(scene, "slg_refreshrate", text="Refresh")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_enablebatchmode")
    col = split.column()
    col.prop(scene, "slg_waitrender")
    if scene.slg_enablebatchmode:
      split = layout.split()
      col = split.column()
      col.prop(scene, "slg_batchmodetime", text="Seconds")
      col = split.column()
      col.prop(scene, "slg_batchmodespp", text="Samples")
      split = layout.split()
      col = split.column()
      col.prop(scene, "slg_batchmode_periodicsave", text="Periodic save interval")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_native_threads", text="Native Threads")
    col = split.column()
    col.prop(scene, "slg_devices_threads", text="OpenCL Threads")
    split = layout.split(percentage=0.33)
    col = split.column()
    col.label(text="OpenCL devs:")
    col = split.column()
    col.prop(scene, "slg_opencl_cpu")
    col = split.column()
    col.prop(scene, "slg_opencl_gpu")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_gpu_workgroup_size", text="GPU workgroup size")
    split = layout.split()
    col = split.column()
    col.prop(scene, "slg_platform", text="Platform")
    col = split.column()
    col.prop(scene, "slg_devices", text='Devs')

class SmallLuxGPURender(bpy.types.RenderEngine):
  bl_idname = 'SMALLLUXGPU_RENDER'
  bl_label = "SmallLuxGPU"
  
  def _slgexport(self, scene, uv_flag, vc_flag, vn_flag, export, basepath, basename, suns, envmap):
    # Depends on Blender version used
    try:
        from Mathutils import Vector
    except ImportError:
        from mathutils import Vector
    from itertools import zip_longest

    ff = lambda f:format(f,'.6f').rstrip('0')

    print('SLGBP ===> Begin export')
    # Consider all materials
    mats = [m.name for m in bpy.data.materials]
    nomat = len(mats)
    mats.append('no_material_assigned')
    curmat = nomat
    mtex = [any(ts for ts in m.texture_slots if ts and (ts.map_colordiff or ts.map_normal) and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filename')) for m in bpy.data.materials]
    mtex.append(False) # nomat
    verts = [[] for i in range(len(mats))]
    vert_vcs = [[] for i in range(len(mats))]
    mvc = [False]*len(mats)
    vert_uvs = [[] for i in range(len(mats))]
    faces = [[] for i in range(len(mats))]
    sharedverts = {}
    vertnum = [0]*len(mats)
    color = [0,0,0]
    uvco = [0,0,0]
    addv = False
    # Get materials with force ply flag
    mfp = [m.name for m in bpy.data.materials if m.slg_forceply]
    # Force an update to object matrices when rendering animations
    scene.set_frame(scene.frame_current)
    sdir = '{}/{}'.format(basepath,basename)
    if export or mfp:
      # Delete existing ply files
      if export and os.path.exists(sdir):
        any(os.remove('{}/{}'.format(sdir,file)) for file in os.listdir(sdir) if file.endswith('.ply'))
      objs = [o for o in scene.objects if any(m for m in mfp if m in o.material_slots)] if not export and mfp else scene.objects
      for obj in objs:
        if not obj.restrict_render and obj.type in ['MESH', 'SURFACE', 'META', 'TEXT', 'CURVE'] and scene.layers[next((i for i in range(len(obj.layers)) if obj.layers[i]))]:
          print('SLGBP ===> Object: {}'.format(obj.name))
          # Create render mesh
          try:
            print("SLGBP    Create render mesh: {}".format(obj.name))
            mesh = obj.create_mesh(scene, True, 'RENDER')
          except:
            pass
          else:
            print("SLGBP    Xform render mesh: {}".format(obj.name))
            mesh.transform(obj.matrix)
            # Make copy of verts for fast direct index access (mesh.verts was very slow)
            if vn_flag:
              v = [tuple(vert.co)+tuple(vert.normal) for vert in mesh.verts]
            else:
              v = [tuple(vert.co) for vert in mesh.verts]
            vcd = []
            if vc_flag and mesh.active_vertex_color:
              vcd = mesh.active_vertex_color.data
            uvd = []
            if uv_flag and mesh.active_uv_texture:
              uvd = mesh.active_uv_texture.data
            # Correlate obj mat slots with global mats
            objmats = [mats.index(m.material.name) if m.material else nomat for m in obj.material_slots]
            for face, vc, uv in zip_longest(mesh.faces,vcd,uvd):
              curmat = objmats[face.material_index] if objmats else nomat 
              # Get vertex colors, if avail
              if vc:
                colors = vc.color1, vc.color2, vc.color3, vc.color4
                mvc[curmat] = True
              #Get uvs if there is an image texture attached
              if mtex[curmat] and uv:
                uvcos = uv.uv1, uv.uv2, uv.uv3, uv.uv4
              if not face.smooth:
                faces[curmat].append((vertnum[curmat], vertnum[curmat]+1, vertnum[curmat]+2))
                if len(face.verts) == 4:
                  faces[curmat].append((vertnum[curmat], vertnum[curmat]+2, vertnum[curmat]+3))
              for j, vert in enumerate(face.verts):
                if vc_flag:
                  if vc:
                    color[0] = int(255.0*colors[j][0])
                    color[1] = int(255.0*colors[j][1])
                    color[2] = int(255.0*colors[j][2])
                  else:
                    color[0] = color[1] = color[2] = 255
                if uv_flag and mtex[curmat]:
                  if uv:
                    uvco[0] = uvcos[j][0]
                    uvco[1] = 1.0 - uvcos[j][1]
                  else:
                    uvco[0] = uvco[1] = 0
                if face.smooth:
                  if (curmat,vert) in sharedverts:
                    addv = False
                  else:
                    sharedverts[curmat,vert]=vertnum[curmat]
                    addv = True
                else:
                  addv = True
                if addv:
                  verts[curmat].append(v[vert])
                  if uv_flag and mtex[curmat]:
                    vert_uvs[curmat].append(tuple(uvco))
                  if vc_flag:
                    vert_vcs[curmat].append(tuple(color))
                  vertnum[curmat] += 1
              if face.smooth:
                faces[curmat].append((sharedverts[curmat,face.verts[0]], sharedverts[curmat,face.verts[1]], sharedverts[curmat,face.verts[2]]))
                if len(face.verts) == 4:
                  faces[curmat].append((sharedverts[curmat,face.verts[0]], sharedverts[curmat,face.verts[2]], sharedverts[curmat,face.verts[3]]))
          sharedverts = {}
          # Delete working mesh
          if not mesh.users:
            print("SLGBP    delete render mesh: {}".format(obj.name))
            bpy.data.meshes.remove(mesh)

    # Get camera and lookat point
    cam = scene.camera   
    trackto = next((constraint for constraint in cam.constraints if constraint.name == 'TrackTo'), None)
    target = trackto.target.location if trackto else cam.matrix * Vector([0, 0, -10])

    print("SLGBP ===> Create scene files")
    # Check/create scene directory to hold scene files
    if not os.path.exists(sdir):
      os.mkdir(sdir)

    # Create SLG scene file
    fscn = open('{}/{}.scn'.format(sdir,basename),'w')
    fscn.write('scene.camera.lookat = {} {} {} {} {} {}\n'.format(ff(cam.location.x),ff(cam.location.y),ff(cam.location.z),ff(target[0]),ff(target[1]),ff(target[2])))
    camup = cam.matrix.rotation_part() * Vector((0,1,0))
    fscn.write('scene.camera.up = {} {} {}\n'.format(ff(camup.x),ff(camup.y),ff(camup.z)))
    fscn.write('scene.camera.fieldofview = {:g}\n'.format(cam.data.angle*180.0/3.1415926536))
    if scene.slg_cameramotionblur:
        fscn.write('scene.camera.motionblur.enable = 1\n')
        scene.set_frame(scene.frame_current - 1)
        tracktoBlur = next((constraint for constraint in scene.camera.constraints if constraint.name == 'TrackTo'), None)
        targetBlur = trackto.target.location if tracktoBlur else scene.camera.matrix * Vector([0, 0, -10])
        camupBlur = cam.matrix.rotation_part() * Vector((0,1,0))
        fscn.write('scene.camera.motionblur.lookat = {} {} {} {} {} {}\n'.format(ff(scene.camera.location.x),ff(scene.camera.location.y),ff(scene.camera.location.z),ff(targetBlur[0]),ff(targetBlur[1]),ff(targetBlur[2])))
        fscn.write('scene.camera.motionblur.up = {} {} {}\n'.format(ff(camupBlur.x),ff(camupBlur.y),ff(camupBlur.z)))
        scene.set_frame(scene.frame_current + 1)

    # DOF    
    fdist = (cam.location-cam.data.dof_object.location).magnitude if cam.data.dof_object else cam.data.dof_distance
    if fdist:
      fscn.write('scene.camera.focaldistance = {}\n'.format(ff(fdist)))
      fscn.write('scene.camera.lensradius = {}\n'.format(ff(cam.data.slg_lensradius)))

    # Infinite light, if present
    if scene.world:
      ilts = next((ts for ts in scene.world.texture_slots if ts and hasattr(ts.texture,'image')), None)
      if ilts:
        fscn.write('scene.infinitelight.file = {}'.format(bpy.utils.expandpath(ilts.texture.image.filepath).replace('\\','/')))
        portal = next((m.name for m in bpy.data.materials if m.shadeless),None)
        if portal:
          fscn.write('|{}/{}/{}.ply'.format(basepath,basename,portal.replace('.','_')))
        fscn.write('\n')
        wle = scene.world.lighting.environment_energy if scene.world.lighting.use_environment_lighting else 1.0
        fscn.write('scene.infinitelight.gain = {} {} {}\n'.format(ff(ilts.texture.factor_red*wle),ff(ilts.texture.factor_green*wle),ff(ilts.texture.factor_blue*wle)))
        fscn.write('scene.infinitelight.shift = {} {}\n'.format(ff(ilts.offset.x),ff(ilts.offset.y)))
        if scene.slg_infinitelightbf:
          fscn.write('scene.infinitelight.usebruteforce = 1\n')

    # Sun lamp
    if suns:
      # We only support one visible sun lamp
      sun = suns[0].data.sky
      matrix = suns[0].matrix
      # If envmap is also defined, only sun component is exported
      invmatrix = mathutils.Matrix(matrix).invert()
      # invmatrix[0][2], invmatrix[1][2], invmatrix[2][2]))
      if not envmap and sun.use_atmosphere:
        fscn.write('scene.skylight.dir = {} {} {}\n'.format(ff(invmatrix[0][2]),ff(invmatrix[1][2]),ff(invmatrix[2][2])))
        fscn.write('scene.skylight.turbidity = {}\n'.format(ff(sun.atmosphere_turbidity)))
        fscn.write('scene.skylight.gain = {} {} {}\n'.format(ff(sun.sun_intensity),ff(sun.sun_intensity),ff(sun.sun_intensity)))
      if sun.use_sky:
        fscn.write('scene.sunlight.dir = {} {} {}\n'.format(ff(invmatrix[0][2]),ff(invmatrix[1][2]),ff(invmatrix[2][2])))
        fscn.write('scene.sunlight.turbidity = {}\n'.format(ff(sun.atmosphere_turbidity)))
        fscn.write('scene.sunlight.gain = {} {} {}\n'.format(ff(sun.sun_brightness),ff(sun.sun_brightness),ff(sun.sun_brightness)))
    
    # Participating Media properties
    if scene.slg_enablepartmedia:
      fscn.write('scene.partecipatingmedia.singlescatering.enable = 1\n')
      fscn.write('scene.partecipatingmedia.singlescatering.stepsize = {}\n'.format(scene.slg_partmedia_stepsize))
      fscn.write('scene.partecipatingmedia.singlescatering.rrprob = {}\n'.format(scene.slg_partmedia_rrprob))
      fscn.write('scene.partecipatingmedia.singlescatering.emission = {} {} {}\n'.format(scene.slg_partmedia_emission[0], scene.slg_partmedia_emission[1], scene.slg_partmedia_emission[2]))
      fscn.write('scene.partecipatingmedia.singlescatering.scattering = {} {} {}\n'.format(scene.slg_partmedia_scattering[0], scene.slg_partmedia_scattering[1], scene.slg_partmedia_scattering[2]))

    # Process each material
    for i, mat in enumerate(mats):
      portal = False
      mat = mat.replace('.','_')
      if verts[i] or (not export and os.path.exists('{}/{}.ply'.format(sdir,mat))):
        print("SLGBP    material: {}".format(mat))
        # Create scn material
        if i == nomat:
          fscn.write('scene.materials.matte.{} = 0.75 0.75 0.75\n'.format(mat))
        else:
          m = bpy.data.materials[i]
          if m.shadeless:
            portal = True
          elif m.emit:
            fscn.write('scene.materials.light.{} = {} {} {}\n'.format(mat,ff(m.emit*m.diffuse_color[0]),ff(m.emit*m.diffuse_color[1]),ff(m.emit*m.diffuse_color[2])))
          elif m.transparency and m.alpha < 1:
            if m.raytrace_transparency.ior == 1.0:
              fscn.write('scene.materials.archglass.{} = {} {} {} {} {} {} {:b} {:b}\n'.format(mat,ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),
                ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                ff((1.0-m.alpha)*m.diffuse_color[0]),ff((1.0-m.alpha)*m.diffuse_color[1]),ff((1.0-m.alpha)*m.diffuse_color[2]),
                m.raytrace_mirror.depth>0,m.raytrace_transparency.depth>0))
            else:
              fscn.write('scene.materials.glass.{} = {} {} {} {} {} {} 1.0 {} {:b} {:b}\n'.format(mat,ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),
                ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                ff((1.0-m.alpha)*m.diffuse_color[0]),ff((1.0-m.alpha)*m.diffuse_color[1]),ff((1.0-m.alpha)*m.diffuse_color[2]),
                ff(m.raytrace_transparency.ior),m.raytrace_mirror.depth>0,m.raytrace_transparency.depth>0))
          else:
            if not m.raytrace_mirror.enabled or not m.raytrace_mirror.reflect_factor:
              fscn.write('scene.materials.matte.{} = {} {} {}\n'.format(mat,ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2])))
            else:
              refltype = 'metal' if m.raytrace_mirror.gloss_factor < 1 else 'mirror'
              gloss = ff(pow(10000.0,m.raytrace_mirror.gloss_factor)) if m.raytrace_mirror.gloss_factor < 1 else ''
              if m.raytrace_mirror.reflect_factor == 1:
                fscn.write('scene.materials.{}.{} = {} {} {} {} {:b}\n'.format(refltype,mat,ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),
                    ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),gloss,m.raytrace_mirror.depth>0))
              else:
                fscn.write('scene.materials.matte{}.{} = {} {} {} {} {} {} {} {:b}\n'.format(refltype,mat,ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]),
                    ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                    gloss,m.raytrace_mirror.depth>0))
        
        if not portal:   
          fscn.write('scene.objects.{}.{} = {}/{}/{}.ply\n'.format(mat,mat,basepath,basename,mat))
          if uv_flag and mtex[i]:
            texmap = next((ts for ts in m.texture_slots if ts and ts.map_colordiff and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filename')), None)
            if texmap:
              fscn.write('scene.objects.{}.{}.texmap = {}\n'.format(mat,mat,bpy.utils.expandpath(texmap.texture.image.filepath).replace('\\','/')))
            texbump = next((ts for ts in m.texture_slots if ts and ts.map_normal and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filename')), None)
            if texbump:
              if texbump.texture.normal_map:
                fscn.write('scene.objects.{}.{}.normalmap = {}\n'.format(mat,mat,bpy.utils.expandpath(texbump.texture.image.filepath).replace('\\','/')))
              else:
                fscn.write('scene.objects.{}.{}.bumpmap = {}\n'.format(mat,mat,bpy.utils.expandpath(texbump.texture.image.filepath).replace('\\','/')))
                fscn.write('scene.objects.{}.{}.bumpmap.scale = {}\n'.format(mat,mat,texbump.normal_factor))
        if export or mats[i] in mfp:
          # Write out PLY
          fply = open('{}/{}.ply'.format(sdir,mat), 'wb')
          fply.write(b'ply\n')
          fply.write(b'format ascii 1.0\n')
          fply.write(str.encode('comment Created by SmallLuxGPU exporter for Blender 2.5, source file: {}\n'.format((bpy.data.filepath.split('/')[-1].split('\\')[-1]))))
          fply.write(str.encode('element vertex {}\n'.format(vertnum[i])))
          fply.write(b'property float x\n')
          fply.write(b'property float y\n')
          fply.write(b'property float z\n')
          if vn_flag:
            fply.write(b'property float nx\n')
            fply.write(b'property float ny\n')
            fply.write(b'property float nz\n')
          if uv_flag and mtex[i]:
            fply.write(b'property float s\n')
            fply.write(b'property float t\n')
          if vc_flag and mvc[i]:
            fply.write(b'property uchar red\n')
            fply.write(b'property uchar green\n')
            fply.write(b'property uchar blue\n')
          fply.write(str.encode('element face {}\n'.format(len(faces[i]))))
          fply.write(b'property list uchar uint vertex_indices\n')
          fply.write(b'end_header\n')
          # Write out vertices
          for j, v in enumerate(verts[i]):
            if vn_flag:
              fply.write(str.encode('{:.6f} {:.6f} {:.6f} {:.6g} {:.6g} {:.6g}'.format(*v)))
            else:
              fply.write(str.encode('{:.6f} {:.6f} {:.6f}'.format(*v)))
            if uv_flag and mtex[i]:
              fply.write(str.encode(' {:.6g} {:.6g}'.format(*vert_uvs[i][j])))
            if vc_flag and mvc[i]:
              fply.write(str.encode(' {} {} {}'.format(*vert_vcs[i][j])))
            fply.write(b'\n')
          # Write out faces
          for f in faces[i]:
            fply.write(str.encode('3 {} {} {}\n'.format(*f)))
          fply.close()
    fscn.close()

  def render(self, scene):
    import subprocess 
    import time

    def error(error):
      self.update_stats("-=|ERROR|", error)
      # Hold the render results window while we display the error message
      while True:
        if self.test_break():
          raise Exception("SmallLuxGPU plug-in: "+error)
        time.sleep(0.1)
      
    print('SLGBP ===> plug-in starting...')
    self.update_stats("", "SmallLuxGPU: Exporting scene, please wait...")

    # Make sure we have lights in our scene
    suns = [f for f in scene.objects if f.type=="LAMP" and f.data and f.data.type=="SUN" and scene.layers[next((i for i in range(len(f.layers)) if f.layers[i]))] and (f.data.sky.use_sky or f.data.sky.use_atmosphere)]
    envmap = (scene.world and any(ts and hasattr(ts.texture,'image') for ts in scene.world.texture_slots))
    emitters = any(m.users and m.emit for m in bpy.data.materials)
    
    if not suns and not emitters and not envmap:
      error("No sun-lamp, emitters or world image texture in scene!  SLG requires at least one light source!")

    # Get resolution
    r = scene.render
    x = int(r.resolution_x*r.resolution_percentage*0.01)
    y = int(r.resolution_y*r.resolution_percentage*0.01)

    basepath = scene.slg_scene_path

    # Path where SmallLuxGPU is located
    if os.path.isdir(scene.slg_path):
      exepath = basepath+'/slg'
    elif os.path.isfile(scene.slg_path):
      exepath = scene.slg_path 
    else:
      error("Full path to SmallLuxGPU executable required")
    if not os.path.exists(exepath):
      error("Cannot find SLG executable at specified path")

    if not os.path.exists(basepath):
      error("Cannot find scenes directory") 

    # Get scene name
    basename = scene.slg_scenename
    if os.path.isfile(scene.slg_scenename):
      basename = os.path.basename(scene.slg_scenename)
    basename = basename.split('/')[-1].split('\\')[-1]
    if not basename:
      error("Invalid scene filename") 
    basename = os.path.splitext(basename)[0]
    
    self._slgexport(scene, scene.slg_vuvs, scene.slg_vcolors, scene.slg_vnormals, scene.slg_export, basepath, basename, suns, envmap)

    fcfg = open('{}/{}/render.cfg'.format(basepath,basename), 'w')
    fcfg.write('image.width = {}\n'.format(x))
    fcfg.write('image.height = {}\n'.format(y))
    fcfg.write('image.filename = {}/{}/{}.{}\n'.format(basepath,basename,basename,scene.slg_imageformat))
    fcfg.write('batch.halttime = {}\n'.format(scene.slg_enablebatchmode*scene.slg_batchmodetime))
    fcfg.write('batch.haltspp = {}\n'.format(scene.slg_enablebatchmode*scene.slg_batchmodespp))
    fcfg.write('batch.periodicsave = {}\n'.format(scene.slg_enablebatchmode*scene.slg_batchmode_periodicsave))
    fcfg.write('scene.file = {}/{}/{}.scn\n'.format(basepath,basename,basename))
    fcfg.write('scene.epsilon = {:g}\n'.format(scene.unit_settings.scale_length*0.0001))
    fcfg.write('opencl.latency.mode = {:b}\n'.format(scene.slg_low_latency))
    fcfg.write('opencl.nativethread.count = {}\n'.format(scene.slg_native_threads))
    fcfg.write('opencl.cpu.use = {:b}\n'.format(scene.slg_opencl_cpu))
    fcfg.write('opencl.gpu.use = {:b}\n'.format(scene.slg_opencl_gpu))
    fcfg.write('opencl.platform.index = {}\n'.format(scene.slg_platform))
    if scene.slg_devices.strip():
      fcfg.write('opencl.devices.select = {}\n'.format(scene.slg_devices)) 
    fcfg.write('opencl.renderthread.count = {}\n'.format(scene.slg_devices_threads))
    fcfg.write('opencl.gpu.workgroup.size = {}\n'.format(scene.slg_gpu_workgroup_size))
    fcfg.write('film.gamma = {:g}\n'.format(scene.slg_film_gamma))
    fcfg.write('film.filter.type = {}\n'.format(scene.slg_film_filter_type))
    fcfg.write('film.tonemap.type = {}\n'.format(scene.slg_film_tonemap_type))
    fcfg.write('screen.refresh.interval = {}\n'.format(scene.slg_refreshrate))
    fcfg.write('path.onlysamplespecular = {}\n'.format(scene.slg_rendering_type))
    fcfg.write('path.maxdepth = {}\n'.format(scene.slg_tracedepth))
    fcfg.write('path.russianroulette.depth = {}\n'.format(scene.slg_rrdepth))
    if scene.slg_rrstrategy == "0":
        fcfg.write('path.russianroulette.strategy = 0\n')
        fcfg.write('path.russianroulette.prob = {}\n'.format(scene.slg_rrprob))
    else:
        fcfg.write('path.russianroulette.strategy = 1\n')
        fcfg.write('path.russianroulette.cap = {}\n'.format(scene.slg_rrcap))
    fcfg.write('path.lightstrategy = {}\n'.format(scene.slg_lightstrategy))
    fcfg.write('path.shadowrays = {}\n'.format(scene.slg_shadowrays))
    fcfg.write('sampler.spp = {}\n'.format(scene.slg_sampleperpixel))
    fcfg.write('accelerator.type = {}\n'.format(scene.slg_accelerator_type))
    fcfg.close()

    print('SLGBP ===> launch SLG: {} {}/{}/render.cfg'.format(exepath,basepath,basename))
    slgproc = subprocess.Popen([exepath,'{}/{}/render.cfg'.format(basepath,basename)], cwd=basepath, shell=False)
    
    if scene.slg_waitrender:
      # Wait for SLG , convert and load image
      if scene.slg_enablebatchmode:
        self.update_stats("", "SmallLuxGPU: Batch Rendering frame# {} (see console for progress), please wait...".format(scene.frame_current))
      else:
        self.update_stats("", "SmallLuxGPU: Waiting... (in SLG window: press 'p' to save image, 'Esc' to exit)")
      # Hold the render results window hostage until SLG returns...
      while True:
        if slgproc.poll() != None:
          result = self.begin_result(0, 0, x, y)
          try:
            print('SLGBP ===> load image from file: {}/{}/{}.{}'.format(basepath,basename,basename,scene.slg_imageformat))
            result.layers[0].load_from_file('{}/{}/{}.{}'.format(basepath,basename,basename,scene.slg_imageformat))
          except:
            pass
          self.end_result(result)
          break
        if self.test_break():
          try:
            slgproc.terminate()
          except:
            pass
          break
        time.sleep(0.1)
        
def register():
  slg_properties()
  bpy.types.register(RENDER_PT_slrender_options)
  bpy.types.register(SmallLuxGPURender)
  bpy.types.DATA_PT_camera.append(slg_lensradius)
  bpy.types.MATERIAL_PT_diffuse.append(slg_forceply)    
    
def unregister():
  bpy.types.unregister(RENDER_PT_slrender_options)
  bpy.types.unregister(SmallLuxGPURender)
  bpy.types.DATA_PT_camera.remove(slg_lensradius)
  bpy.types.MATERIAL_PT_diffuse.remove(slg_forceply)    

if __name__ == "__main__":
  register()