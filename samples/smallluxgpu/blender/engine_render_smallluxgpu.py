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
# SmallLuxGPU v1.4 render engine Blender 2.5 plug-in
# v0.43dev
# Source: http://www.luxrender.net/forum/viewforum.php?f=34

import bpy
import os

def slg_properties():
  # Add SmallLuxGPU properties
  StringProperty = bpy.types.Scene.StringProperty
  FloatProperty = bpy.types.Scene.FloatProperty
  IntProperty = bpy.types.Scene.IntProperty
  BoolProperty = bpy.types.Scene.BoolProperty
  EnumProperty = bpy.types.Scene.EnumProperty 
  
  StringProperty( attr="slg_path", name="SmallLuxGPU Path",
      description="Full path to SmallLuxGPU's executable",
      default="", maxlen=1024, subtype="FILE_PATH")
  
  StringProperty(attr="slg_scenename", name="Scene Name",
      description="Name of SmallLuxGPU scene to create",
      default="testscene", maxlen=1024)
  
  BoolProperty(attr="slg_export", name="PLY",
      description="Export ply files (uncheck only if scene has already been exported)",
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
  
  EnumProperty(attr="slg_film_type", name="Film Type",
      description="Select the desired film type",
      items=(("0", "Standard", "Standard Film version"),
             ("1", "Blurred preview", "Film with blurred preview"),
             ("2", "Gaussian filter", "Film with Gaussian filter"),
             ("3", "Gaussian filter with fast preview", "Film with Gaussian filter with fast preview")),
      default="3")
  
  EnumProperty(attr="slg_lightstrategy", name="Light Strategy",
      description="Select the desired light strategy",
      items=(("0", "ONE_UNIFORM", "ONE_UNIFORM"),
             ("1", "ALL_UNIFORM", "ALL_UNIFORM")),
      default="0")
  
  EnumProperty(attr="slg_imageformat", name="Image File Format",
      description="Image file save format, saved with scene files (or Blender intermediary format)", 
      items=(("png", "PNG", "PNG"),
             ("exr", "OpenEXR", "OpenEXR")),
      default="png")
  
  IntProperty(attr="slg_tracedepth", name="Max Path Trace Depth",
      description="Maximum path tracing depth",
      default=3, min=1, max=1024, soft_min=1, soft_max=1024)
  
  IntProperty(attr="slg_shadowrays", name="Shadow Rays",
      description="Shadow rays",
      default=1, min=1, max=1024, soft_min=1, soft_max=1024)
  
  IntProperty(attr="slg_rrdepth", name="Russian Roulette Depth",
      description="Russian roulette depth",
      default=5, min=1, max=1024, soft_min=1, soft_max=1024)
  
  FloatProperty(attr="slg_rrprob", name="Russian Roulette Probability",
      description="Russian roulette probability",
      default=0.75, min=0, max=1, soft_min=0, soft_max=1, precision=3)
  
  BoolProperty(attr="slg_enablebatchmode", name="Batch Mode",
      description="Render in background (required for animations)",
      default=False)
  
  IntProperty(attr="slg_batchmodetime", name="Batch mode max run time",
      description="Max number of seconds to run in batch mode; 0 = ignore",
      default=120, min=1, soft_min=1)
  
  IntProperty(attr="slg_batchmodespp", name="Batch mode max samples per pixel",
      description="Max number of samples per pixels in batch mode; 0 = ignore",
      default=128, min=1, soft_min=1)
  
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
  bpy.types.Camera.FloatProperty(attr="slg_lensradius",name="SLG DOF Lens Radius", 
      description="SmallLuxGPU camera lens radius for depth of field",
      default=0.015, min=0, max=10, soft_min=0, soft_max=10, precision=3)

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
  
  import properties_world
  properties_world.WORLD_PT_environment_lighting.COMPAT_ENGINES.add('SMALLLUXGPU_RENDER')
  del properties_world
  
def slg_lensradius(self, context):
  if context.scene.render.engine == 'SMALLLUXGPU_RENDER':  
    self.layout.split().column().prop(context.camera, "slg_lensradius", text="SLG Lens Radius")

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
    col.prop(scene, "slg_scenename", text="Scene Name:")
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
    col.prop(scene, "slg_film_type")
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
    col.prop(scene, "slg_rrdepth", text="RR Depth")
    col = split.column()
    col.prop(scene, "slg_rrprob", text="RR Prob")
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
    col.prop(scene, "slg_imageformat")
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
  
  def _slgexport(self, scene, uv_flag, vc_flag, vn_flag, export, basepath, basename):
    from Mathutils import Vector
    from itertools import zip_longest

    ff = lambda f:format(f,'.6f').rstrip('0')

    print('SLGBP ===> Begin export')
    # Consider all materials
    mats = [m.name for m in bpy.data.materials]
    nomat = len(mats)
    mats.append('no_material_assigned')
    curmat = nomat
    mtex = [any(ts for ts in m.texture_slots if ts and ts.map_colordiff and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filename')) for m in bpy.data.materials]
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
    # Force an update to object matrices when rendering animations
    scene.set_frame(scene.current_frame)
    sdir = '{}/scenes/{}'.format(basepath,basename)
    if export and os.path.exists(sdir):
      # Delete existing ply files
      [os.remove('{}/{}'.format(sdir,file)) for file in os.listdir(sdir) if file.endswith('.ply')]
      for obj in scene.objects:
        if not obj.restrict_render and obj.type in ['MESH', 'SURFACE', 'META', 'TEXT'] and scene.visible_layers[next((i for i in range(len(obj.layers)) if obj.layers[i]))]:
          print('SLGBP ===> Object: {}'.format(obj.name))
          # Create render mesh
          try:
            print("SLGBP    Create render mesh: {}".format(obj.name))
            mesh = obj.create_mesh(True, 'RENDER')
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
            objmats = [mats.index(m.material.name) if m.material else 0 for m in obj.material_slots]
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
    target = trackto.target.location if trackto else cam.matrix * Vector(0,0,-10)

    print("SLGBP ===> Create scene files")
    # Check/create scene directory to hold scene files
    if not os.path.exists(sdir):
      os.mkdir(sdir)

    # Create SLG scene file
    fscn = open('{}/{}.scn'.format(sdir,basename),'w')
    fscn.write('scene.camera.lookat = {} {} {} {} {} {}\n'.format(ff(cam.location.x),ff(cam.location.y),ff(cam.location.z),ff(target[0]),ff(target[1]),ff(target[2])))

    # DOF    
    fdist = (cam.location-cam.data.dof_object.location).magnitude if cam.data.dof_object else cam.data.dof_distance
    if fdist:
      fscn.write('scene.camera.focaldistance = {}\n'.format(ff(fdist)))
      fscn.write('scene.camera.lensradius = {}\n'.format(ff(cam.data.slg_lensradius)))
    
    # Infinite light, if present
    iltex = next((ts.texture for ts in scene.world.texture_slots if ts and hasattr(ts.texture,'image')), None)
    if iltex:
      fscn.write('scene.infinitelight.file = {}\n'.format(bpy.utils.expandpath(iltex.image.filename).replace('\\','/')))
      wle = scene.world.lighting.environment_energy if scene.world.lighting.use_environment_lighting else 1.0
      fscn.write('scene.infinitelight.gain = {} {} {}\n'.format(ff(iltex.factor_red*wle),ff(iltex.factor_green*wle),ff(iltex.factor_blue*wle)))
  
    # Process each material
    for i, mat in enumerate(mats):
      mat = mat.replace('.','_')
      if verts[i] or (not export and os.path.exists('{}/{}.ply'.format(sdir,mat))):
        print("SLGBP    material: {}".format(mat))
        # Create scn material
        if i == nomat:
          fscn.write('scene.materials.matte.{} = 0.75 0.75 0.75\n'.format(mat))
        else:
          m = bpy.data.materials[i]
          if m.emit:
            fscn.write('scene.materials.light.{} = {} {} {}\n'.format(mat,ff(m.emit*m.diffuse_color[0]),ff(m.emit*m.diffuse_color[1]),ff(m.emit*m.diffuse_color[2])))
          elif m.transparency and m.alpha < 1:
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
          
        fscn.write('scene.objects.{}.{} = scenes/{}/{}.ply'.format(mat,mat,basename,mat))
        if uv_flag and mtex[i]:
          texfname = next((ts.texture.image.filename for ts in m.texture_slots if ts and ts.map_colordiff and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filename')), None)
          if texfname:
            fscn.write('|{}'.format(bpy.utils.expandpath(texfname).replace('\\','/')))
        fscn.write('\n')
        if export:
          # Write out PLY
          fply = open('{}/{}.ply'.format(sdir,mat), 'wb')
          fply.write(b'ply\n')
          fply.write(b'format ascii 1.0\n')
          fply.write(str.encode('comment Created by SmallLuxGPU exporter for Blender 2.5, source file: {}\n'.format((bpy.data.filename.split('/')[-1].split('\\')[-1]))))
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
    if not any(m.users and m.emit for m in bpy.data.materials) and not any(ts and hasattr(ts.texture,'image') for ts in scene.world.texture_slots):
      error("No emitters or world image texture in scene!  SLG requires at least one light source!")

    # Get resolution
    r = scene.render
    x = int(r.resolution_x*r.resolution_percentage*0.01)
    y = int(r.resolution_y*r.resolution_percentage*0.01)

    # Path where SmallLuxGPU is located
    if os.path.isdir(scene.slg_path):
      basepath = scene.slg_path
      if scene.slg_path.endswith('/') or scene.slg_path.endswith('\\'):  
        basepath = os.path.dirname(scene.slg_path)
      exepath = basepath+'/slg'
    elif os.path.isfile(scene.slg_path):
      basepath = os.path.dirname(scene.slg_path)
      exepath = scene.slg_path 
    else:
      error("Full path to SmallLuxGPU executable required")
    if not os.path.exists(exepath):
      error("Cannot find SLG executable at specified path") 
    if not os.path.exists(basepath+'/scenes'):
      error("Cannot find scenes directory below specified SLG path") 

    # Get scene name
    basename = scene.slg_scenename
    if os.path.isfile(scene.slg_scenename):
      basename = os.path.basename(scene.slg_scenename)
    basename = basename.split('/')[-1].split('\\')[-1]
    if not basename:
      error("Invalid scene filename") 
    basename = os.path.splitext(basename)[0]
    
    self._slgexport(scene, scene.slg_vuvs, scene.slg_vcolors, scene.slg_vnormals, scene.slg_export, basepath, basename)

    fcfg = open('{}/scenes/{}/render.cfg'.format(basepath,basename), 'w')
    fcfg.write('image.width = {}\n'.format(x))
    fcfg.write('image.height = {}\n'.format(y))
    fcfg.write('image.filename = scenes/{}/{}.{}\n'.format(basename,basename,scene.slg_imageformat))
    fcfg.write('batch.halttime = {}\n'.format(scene.slg_enablebatchmode*scene.slg_batchmodetime))
    fcfg.write('batch.haltspp = {}\n'.format(scene.slg_enablebatchmode*scene.slg_batchmodespp))      
    fcfg.write('scene.file = scenes/{}/{}.scn\n'.format(basename,basename))
    fcfg.write('scene.fieldofview = {:g}\n'.format(scene.camera.data.angle*180.0/3.1415926536))
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
    fcfg.write('screen.refresh.interval = {}\n'.format(scene.slg_refreshrate))
    fcfg.write('screen.type = {}\n'.format(scene.slg_film_type))
    fcfg.write('path.maxdepth = {}\n'.format(scene.slg_tracedepth))
    fcfg.write('path.russianroulette.depth = {}\n'.format(scene.slg_rrdepth))
    fcfg.write('path.russianroulette.prob = {}\n'.format(scene.slg_rrprob))
    fcfg.write('path.lightstrategy = {}\n'.format(scene.slg_lightstrategy))
    fcfg.write('path.shadowrays = {}\n'.format(scene.slg_shadowrays))
    fcfg.close()

    print('SLGBP ===> launch SLG: "{}" scenes/{}/render.cfg'.format(exepath,basename))
    slgproc = subprocess.Popen([exepath,'scenes/{}/render.cfg'.format(basename)], cwd=basepath, shell=False)
    
    if scene.slg_waitrender:
      # Wait for SLG , convert and load image
      if scene.slg_enablebatchmode:
        self.update_stats("", "SmallLuxGPU: Batch Rendering (see console for progress), please wait...")
      else:
        self.update_stats("", "SmallLuxGPU: Waiting... (in SLG window: press 'p' to save image, 'Esc' to exit)")
      # Hold the render results window hostage until SLG returns...
      while True:
        if slgproc.poll() != None:
          result = self.begin_result(0, 0, x, y)
          try:
            print('SLGBP ===> load image from file: {}/scenes/{}/{}.{}'.format(basepath,basename,basename,scene.slg_imageformat))
            result.layers[0].load_from_file('{}/scenes/{}/{}.{}'.format(basepath,basename,basename,scene.slg_imageformat))
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
    
def unregister():
  bpy.types.unregister(RENDER_PT_slrender_options)
  bpy.types.unregister(SmallLuxGPURender)
  bpy.types.DATA_PT_camera.remove(slg_lensradius)    

if __name__ == "__main__":
  register()
