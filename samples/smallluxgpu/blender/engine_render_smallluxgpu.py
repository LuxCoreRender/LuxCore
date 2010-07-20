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
# SmallLuxGPU v1.6 Blender 2.5 plug-in
# v0.62dev
# Source: http://www.luxrender.net/forum/viewforum.php?f=34

import bpy
import blf
import mathutils
import os
import time
import threading
import telnetlib
from itertools import zip_longest
from subprocess import Popen

# SLG Telnet interface
class SLGTelnet:
    def __init__(self):
        self.slgtn = None
        try:
            self.slgtn = telnetlib.Telnet('localhost',18081)
            self.slgtn.read_until(b'SmallLuxGPU Telnet Server Interface')
            self.connected = True
        except:
            self.connected = False
    def __del__(self):
        if self.slgtn:
            self.slgtn.close()
    def send(self, cmd):
        try:
            self.slgtn.write(str.encode(cmd+'\n'))
            r = self.slgtn.expect([b'OK',b'ERROR'],2)
            if r[0] == 0:
                return True
            else:
                return False
        except:
            return False
    def close(self):
        if self.slgtn:
            self.slgtn.close()

# Formatted float filter
ff = lambda f:format(f,'.6f').rstrip('0')

# SmallLuxGPU Blender Plugin: one instance
class SLGBP:
    slgpath = spath = sname = sfullpath = image_filename = ''
    nomatn = 'nomat'
    cfg = {}
    scn = {}
    sun = None
    live = liveanim = False
    LIVECFG, LIVESCN, LIVEMTL, LIVEALL = 1, 2, 4, 7
    livemat = None
    liveact = 0
    slgproc = thread = None
    telnet = None
    abort = False

    # Initialize SLG scene for export
    @classmethod
    def init(cls, scene, errout):
        # Reset Abort flag
        SLGBP.abort = False
        SLGBP.warn = ''

        # SmallLuxGPU executable path
        if os.path.isdir(scene.slg_path):
            SLGBP.slgpath = scene.slg_path + '/slg'
        elif os.path.isfile(scene.slg_path):
            SLGBP.slgpath = scene.slg_path
        else:
            errout("Full path to SmallLuxGPU executable required")
            return False
        if not os.path.exists(SLGBP.slgpath):
            errout("Cannot find SLG executable at specified path")
            return False

        # Scenes files path
        SLGBP.spath = scene.slg_scene_path.replace('\\','/')
        if SLGBP.spath.endswith('/'):
            SLGBP.spath = os.path.dirname(SLGBP.spath)
        if not os.path.exists(SLGBP.spath):
            errout("Cannot find scenes directory")
            return False

        # Scene name
        SLGBP.sname = scene.slg_scenename
        if os.path.isfile(scene.slg_scenename):
            SLGBP.sname = os.path.basename(scene.slg_scenename)
        SLGBP.sname = SLGBP.sname.split('/')[-1].split('\\')[-1]
        if not SLGBP.sname:
            errout("Invalid scene name")
            return False
        SLGBP.sname = os.path.splitext(SLGBP.sname)[0]

        # Make sure we have at least one light in our scene
        SLGBP.sun = next((f for f in scene.objects if f.type=="LAMP" and f.data and f.data.type=="SUN" and scene.layers[next((i for i in range(len(f.layers)) if f.layers[i]))] and (f.data.sky.use_sky or f.data.sky.use_atmosphere)),None)
        if scene.world:
            SLGBP.infinitelight = next((ts for ts in scene.world.texture_slots if ts and hasattr(ts.texture,'image')), None)
        else:
            SLGBP.infinitelight = None
        emitters = any(m.users and m.emit for m in bpy.data.materials)
        if not SLGBP.sun and not SLGBP.infinitelight and not emitters:
            errout("No lights in scene!  SLG requires an emitter, world image or sun-lamp")
            return False

        # Determine output image filename (template)
        if os.path.exists(os.path.dirname(scene.render.output_path)):
            if not scene.render.output_path.split('/')[-1].split('\\')[-1]:
                SLGBP.image_filename = (scene.render.output_path + '{:05d}')
            else:
                i = scene.render.output_path.count('#')
                if i > 0:
                    SLGBP.image_filename = scene.render.output_path.replace('#'*i, '{:0'+str(i)+'d}')
                else:
                    SLGBP.image_filename = scene.render.output_path
            if not SLGBP.image_filename.endswith(scene.render.file_extension):
                # SLG requires extension, force one
                SLGBP.image_filename += scene.render.file_extension
        else:
            SLGBP.image_filename = '{}/{}/{}.{}'.format(SLGBP.spath,SLGBP.sname,SLGBP.sname,scene.slg_imageformat)

        # Check/create scene directory to hold scene files
        print("SLGBP ===> Create scene directory")
        SLGBP.sfullpath = '{}/{}'.format(SLGBP.spath,SLGBP.sname)
        if not os.path.exists(SLGBP.sfullpath):
            try:
                os.mkdir(SLGBP.sfullpath)
            except:
                errout("Cannot create SLG scenes directory")
                return False

        # Get motion blur parameters
        if scene.slg_cameramotionblur:
            scene.set_frame(scene.frame_current - 1)
            SLGBP.camdirBlur = scene.camera.matrix_world * mathutils.Vector((0, 0, -10))
            SLGBP.camlocBlur = scene.camera.matrix_world.translation_part()
            SLGBP.camupBlur = scene.camera.matrix_world.rotation_part() * mathutils.Vector((0,1,0))
            scene.set_frame(scene.frame_current + 1)

        return True

    # Get SLG .cfg properties
    @classmethod
    def getcfg(cls, scene):
        cfg = {}
        cfg['film.tonemap.type'] = scene.slg_film_tonemap_type
        if scene.slg_film_tonemap_type == '0':
            cfg['film.tonemap.linear.scale'] = ff(scene.slg_linear_scale)
        else:
            cfg['film.tonemap.reinhard02.burn'] = ff(scene.slg_reinhard_burn)
            cfg['film.tonemap.reinhard02.prescale'] = ff(scene.slg_reinhard_prescale)
            cfg['film.tonemap.reinhard02.postscale'] = ff(scene.slg_reinhard_postscale)
        cfg['image.filename'] = SLGBP.image_filename.format(scene.frame_current)

        # ---- End of Live properties ----
        if SLGBP.live:
            return cfg

        cfg['image.width'] = format(int(scene.render.resolution_x*scene.render.resolution_percentage*0.01))
        cfg['image.height'] = format(int(scene.render.resolution_y*scene.render.resolution_percentage*0.01))
        cfg['batch.halttime'] = format(scene.slg_enablebatchmode*scene.slg_batchmodetime)
        cfg['batch.haltspp'] = format(scene.slg_enablebatchmode*scene.slg_batchmodespp)
        cfg['batch.periodicsave'] = format(scene.slg_batchmode_periodicsave)
        cfg['scene.file'] = '{}/{}/{}.scn'.format(SLGBP.spath,SLGBP.sname,SLGBP.sname)
        cfg['scene.epsilon'] = format(0.0001/scene.unit_settings.scale_length,'g')
        cfg['opencl.latency.mode'] = format(scene.slg_low_latency, 'b')
        cfg['opencl.nativethread.count'] = format(scene.slg_native_threads)
        cfg['opencl.cpu.use'] = format(scene.slg_opencl_cpu, 'b')
        cfg['opencl.gpu.use'] = format(scene.slg_opencl_gpu, 'b')
        cfg['opencl.platform.index'] = format(scene.slg_platform)
        if scene.slg_devices.strip():
            cfg['opencl.devices.select'] = scene.slg_devices
        cfg['opencl.renderthread.count'] = format(scene.slg_devices_threads)
        cfg['opencl.gpu.workgroup.size'] = format(scene.slg_gpu_workgroup_size)
        cfg['film.gamma'] = format(scene.slg_film_gamma, 'g')
        cfg['film.filter.type'] = scene.slg_film_filter_type
        cfg['screen.refresh.interval'] = format(scene.slg_refreshrate)
        cfg['renderengine.type'] = scene.slg_rendering_type
        cfg['sppm.lookup.type'] = scene.slg_sppmlookuptype
        cfg['sppm.eyepath.maxdepth'] = format(scene.slg_sppm_eyepath_maxdepth)
        cfg['sppm.photon.alpha'] = ff(scene.slg_sppm_photon_alpha)
        cfg['sppm.photon.startradiusscale'] = ff(scene.slg_sppm_photon_radiusscale)
        cfg['sppm.photon.maxdepth'] = format(scene.slg_sppm_photon_maxdepth)
        cfg['sppm.stochastic.count'] = format(scene.slg_sppm_photon_per_pass)
        cfg['path.maxdepth'] = format(scene.slg_tracedepth)
        cfg['path.russianroulette.depth'] = format(scene.slg_rrdepth)
        cfg['path.russianroulette.strategy'] = scene.slg_rrstrategy
        if scene.slg_rrstrategy == '0':
            cfg['path.russianroulette.prob'] = ff(scene.slg_rrprob)
        else:
            cfg['path.russianroulette.cap'] = ff(scene.slg_rrcap)
        cfg['path.lightstrategy'] = scene.slg_lightstrategy
        cfg['path.shadowrays'] = format(scene.slg_shadowrays)
        cfg['path.sampler.spp'] = scene.slg_sampleperpixel
        cfg['accelerator.type'] = scene.slg_accelerator_type

        return cfg

    # Get SLG .scn scene properties
    @classmethod
    def getscn(cls, scene):
        scn = {}

        # Get camera and lookat direction
        cam = scene.camera
        camdir = cam.matrix_world * mathutils.Vector((0, 0, -1))

        # Camera.location not always updated, but matrix is
        camloc = cam.matrix_world.translation_part()
        scn['scene.camera.lookat'] = '{} {} {} {} {} {}'.format(ff(camloc.x),ff(camloc.y),ff(camloc.z),ff(camdir.x),ff(camdir.y),ff(camdir.z))
        camup = cam.matrix_world.rotation_part() * mathutils.Vector((0,1,0))
        scn['scene.camera.up'] = '{} {} {}'.format(ff(camup.x),ff(camup.y),ff(camup.z))

        scn['scene.camera.fieldofview'] = format(cam.data.angle*180.0/3.1415926536,'g')

        scn['scene.camera.motionblur.enable'] = format(scene.slg_cameramotionblur, 'b')
        if scene.slg_cameramotionblur:
            if SLGBP.live:
                if 'scene.camera.lookat' in SLGBP.scn:
                    scn['scene.camera.motionblur.lookat'] = SLGBP.scn['scene.camera.lookat']
                    scn['scene.camera.motionblur.up'] = SLGBP.scn['scene.camera.up']
            else:
                scn['scene.camera.motionblur.lookat'] = '{} {} {} {} {} {}'.format(ff(SLGBP.camlocBlur.x),ff(SLGBP.camlocBlur.y),ff(SLGBP.camlocBlur.z),
                        ff(SLGBP.camdirBlur[0]),ff(SLGBP.camdirBlur[1]),ff(SLGBP.camdirBlur[2]))
                scn['scene.camera.motionblur.up'] = '{} {} {}'.format(ff(SLGBP.camupBlur.x),ff(SLGBP.camupBlur.y),ff(SLGBP.camupBlur.z))

        # DOF
        fdist = (camloc-cam.data.dof_object.matrix_world.translation_part()).magnitude if cam.data.dof_object else cam.data.dof_distance
        if fdist:
            scn['scene.camera.focaldistance'] = ff(fdist)
            scn['scene.camera.lensradius'] = ff(cam.data.slg_lensradius)

        # Infinite light, if present
        if SLGBP.infinitelight:
            scn['scene.infinitelight.file'] = bpy.utils.expandpath(SLGBP.infinitelight.texture.image.filepath).replace('\\','/')
            portal = [m.name for m in bpy.data.materials if m.shadeless]
            if portal:
                scn['scene.infinitelight.file'] += '|{}/{}/{}.ply'.format(SLGBP.spath,SLGBP.sname,portal[0].replace('.','_'))
                if len(portal) > 1:
                    SLGBP.warn = 'More than one portal material (Shadeless) defined'
                    print('WARNING: ' + SLGBP.warn)
            if scene.world.lighting.use_environment_lighting:
                wle = scene.world.lighting.environment_energy
            else:
                wle = 1.0
            scn['scene.infinitelight.gain'] = '{} {} {}'.format(ff(SLGBP.infinitelight.texture.factor_red*wle),ff(SLGBP.infinitelight.texture.factor_green*wle),ff(SLGBP.infinitelight.texture.factor_blue*wle))
            scn['scene.infinitelight.shift'] = '{} {}'.format(ff(SLGBP.infinitelight.offset.x),ff(SLGBP.infinitelight.offset.y))
            if scene.slg_infinitelightbf:
                scn['scene.infinitelight.usebruteforce'] = '1'

        # Sun lamp
        if SLGBP.sun:
            # We only support one visible sun lamp
            sundir = SLGBP.sun.matrix_world.rotation_part() * mathutils.Vector((0,0,1))
            sky = SLGBP.sun.data.sky
            # If envmap is also defined, only sun component is exported
            if not SLGBP.infinitelight and sky.use_atmosphere:
                scn['scene.skylight.dir'] = '{} {} {}'.format(ff(sundir.x),ff(sundir.y),ff(sundir.z))
                scn['scene.skylight.turbidity'] = ff(sky.atmosphere_turbidity)
                scn['scene.skylight.gain'] = '{} {} {}'.format(ff(sky.sun_intensity),ff(sky.sun_intensity),ff(sky.sun_intensity))
            if sky.use_sky:
                scn['scene.sunlight.dir'] = '{} {} {}'.format(ff(sundir.x),ff(sundir.y),ff(sundir.z))
                scn['scene.sunlight.turbidity'] = ff(sky.atmosphere_turbidity)
                scn['scene.sunlight.relsize'] = ff(sky.sun_size)
                scn['scene.sunlight.gain'] = '{} {} {}'.format(ff(sky.sun_brightness),ff(sky.sun_brightness),ff(sky.sun_brightness))

        # Participating Media properties
        if scene.slg_enablepartmedia:
            scn['scene.partecipatingmedia.singlescatering.enable'] = '1'
            scn['scene.partecipatingmedia.singlescatering.bbox'] = '{} {} {} {} {} {}'.format(scene.slg_partmedia_bbox1.x, scene.slg_partmedia_bbox1.y, scene.slg_partmedia_bbox1.z, scene.slg_partmedia_bbox2.x, scene.slg_partmedia_bbox2.y, scene.slg_partmedia_bbox2.z)
            scn['scene.partecipatingmedia.singlescatering.stepsize'] = ff(scene.slg_partmedia_stepsize)
            scn['scene.partecipatingmedia.singlescatering.rrprob'] = ff(scene.slg_partmedia_rrprob)
            scn['scene.partecipatingmedia.singlescatering.emission'] = '{} {} {}'.format(scene.slg_partmedia_emission[0], scene.slg_partmedia_emission[1], scene.slg_partmedia_emission[2])
            scn['scene.partecipatingmedia.singlescatering.scattering'] = '{} {} {}'.format(scene.slg_partmedia_scattering[0], scene.slg_partmedia_scattering[1], scene.slg_partmedia_scattering[2])

        return scn

    # Get SLG .scn mat properties
    @classmethod
    def getmatscn(cls, scene, m):
        scn = {}
        matn = m.name.replace('.','_')
        if m.shadeless:
            matprop = None
        elif m.emit:
            matprop = 'scene.materials.light.{}'.format(matn)
            scn[matprop] = '{} {} {}'.format(ff(m.emit*m.diffuse_color[0]),ff(m.emit*m.diffuse_color[1]),ff(m.emit*m.diffuse_color[2]))
        elif m.transparency:
            if m.raytrace_transparency.ior == 1.0:
                matprop = 'scene.materials.archglass.{}'.format(matn)
                scn[matprop] = '{} {} {} {} {} {} {:b} {:b}'.format(ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),
                    ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                    ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]),
                    m.raytrace_mirror.depth>0,m.raytrace_transparency.depth>0)
            else:
                matprop = 'scene.materials.glass.{}'.format(matn)
                scn[matprop] = '{} {} {} {} {} {} 1.0 {} {:b} {:b}'.format(ff(m.mirror_color[0]),ff(m.mirror_color[1]),ff(m.mirror_color[2]),
                    ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]),
                    ff(m.raytrace_transparency.ior),m.raytrace_mirror.depth>0,m.raytrace_transparency.depth>0)
        else:
            if not m.raytrace_mirror.enabled or not m.raytrace_mirror.reflect_factor:
                matprop = 'scene.materials.matte.{}'.format(matn)
                scn[matprop] = '{} {} {}'.format(ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]))
            else:
                refltype = 'metal' if m.raytrace_mirror.gloss_factor < 1 else 'mirror'
                gloss = ff(pow(10000.0,m.raytrace_mirror.gloss_factor)) if m.raytrace_mirror.gloss_factor < 1 else ''
                if m.raytrace_mirror.reflect_factor == 1:
                    matprop = 'scene.materials.{}.{}'.format(refltype,matn)
                    scn[matprop] = '{} {} {} {} {:b}'.format(ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),
                        ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),gloss,m.raytrace_mirror.depth>0)
                else:
                    matprop = 'scene.materials.matte{}.{}'.format(refltype,matn)
                    scn[matprop] = '{} {} {} {} {} {} {} {:b}'.format(ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]),
                        ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                        gloss,m.raytrace_mirror.depth>0)
                    if m.diffuse_color.v+m.raytrace_mirror.reflect_factor*m.mirror_color.v > 1.0:
                        SLGBP.warn = m.name + ': diffuse + reflected color greater than 1.0!'
                        print('WARNING: ' + SLGBP.warn)
        return matprop, scn

    # Get SLG .scn obj properties
    @classmethod
    def getobjscn(cls, scene):
        scn = {}
        for ply in SLGBP.plys:
            for obj in SLGBP.plys[ply]:
                plyn = ply.replace('.','_')
                if type(obj) == tuple:
                    matn = obj[1].name.replace('.','_') if obj[1] else SLGBP.nomatn
                    name = obj[0].name.replace('.','_')+plyn
                    m = obj[1]
                    if obj[0] in SLGBP.instobjs:
                        # If used both in instances and in dupligroups at the same time
                        if obj[0].data in SLGBP.instobjs:
                            if '{D}' in plyn:
                                continue
                            else:
                                om = obj[0].matrix_world
                        else:
                            om = mathutils.Matrix()
                    else:
                        om = obj[0].matrix_world
                    scn['scene.objects.{}.{}.transformation'.format(matn,name)] = '{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}'.format(
                            ff(om[0][0]),ff(om[0][1]),ff(om[0][2]),ff(om[0][3]),ff(om[1][0]),ff(om[1][1]),ff(om[1][2]),ff(om[1][3]),
                            ff(om[2][0]),ff(om[2][1]),ff(om[2][2]),ff(om[2][3]),ff(om[3][0]),ff(om[3][1]),ff(om[3][2]),ff(om[3][3]))
                else:
                    matn = name = plyn
                    m = obj

                scn['scene.objects.{}.{}'.format(matn,name)] = '{}/{}/{}.ply'.format(SLGBP.spath,SLGBP.sname,plyn)
                if scene.slg_vnormals:
                    scn['scene.objects.{}.{}.useplynormals'.format(matn,name)] = '1'
                if scene.slg_vuvs and m:
                    texmap = next((ts for ts in m.texture_slots if ts and ts.map_colordiff and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filepath') and ts.enabled), None)
                    if texmap:
                        scn['scene.objects.{}.{}.texmap'.format(matn,name)] = bpy.utils.expandpath(texmap.texture.image.filepath).replace('\\','/')
                    texbump = next((ts for ts in m.texture_slots if ts and ts.map_normal and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filepath') and ts.enabled), None)
                    if texbump:
                        if texbump.texture.normal_map:
                            scn['scene.objects.{}.{}.normalmap'.format(matn,name)] = bpy.utils.expandpath(texbump.texture.image.filepath).replace('\\','/')
                        else:
                            scn['scene.objects.{}.{}.bumpmap'.format(matn,name)] = bpy.utils.expandpath(texbump.texture.image.filepath).replace('\\','/')
                            scn['scene.objects.{}.{}.bumpmap.scale'.format(matn,name)] = ff(texbump.normal_factor)
        return scn

    # Export obj/mat .ply files
    @classmethod
    def expmatply(cls, scene):
        print('SLGBP ===> Begin export')
        # Consider all used materials
        plys = dict([(m.name, [m]) for m in bpy.data.materials if m.users])
        plymats = [p for p in plys]
        nomat = len(plymats)
        plymats.append(SLGBP.nomatn)
        plys[SLGBP.nomatn] = [None]
        curmat = nomat
        instobjs = {}
        objs = []
        rendertypes = ['MESH', 'SURFACE', 'META', 'TEXT', 'CURVE']
        inscenelayer = lambda o:scene.layers[next((i for i in range(len(o.layers)) if o.layers[i]))]
        # Get materials with force ply flag
        mfp = [m.name for m in bpy.data.materials if m.slg_forceply]
        # Objects to render
        for obj in scene.objects:
            if not obj.restrict_render:
                if obj.type in rendertypes and inscenelayer(obj):
                    # Mesh instances
                    if obj.data.users > 1 and not obj.modifiers:
                        if obj.data in instobjs:
                            if obj.data.materials:
                                for i in range(len(obj.data.materials)):
                                    plys[(obj.data.name+'S'+str(i))].append((obj, obj.material_slots[i].material))
                            else:
                                plys[(obj.data.name+SLGBP.nomatn)].append((obj, None))
                        else:
                            objs.append(obj.data)
                            instobjs[obj.data] = len(plymats)
                            if obj.data.materials:
                                for i in range(len(obj.data.materials)):
                                    plyn = (obj.data.name+'S'+str(i))
                                    plymats.append(plyn)
                                    plys[plyn] = [(obj, obj.material_slots[i].material)]
                            else:
                                plyn = (obj.data.name+SLGBP.nomatn)
                                plymats.append(plyn)
                                plys[plyn] = [(obj, None)]
                    else:
                        # Collect all non-instance meshes by material
                        if scene.slg_export or mfp and any(m for m in mfp if m in obj.material_slots):
                            objs.append(obj)
                # Dupligroups
                if obj.dupli_type == 'GROUP' and inscenelayer(obj):
                    for o in obj.dupli_group.objects:
                        if o in instobjs:
                            if o.data.materials:
                                for i in range(len(o.data.materials)):
                                    plys[(o.name+'{D}S'+str(i))].append((obj, o.material_slots[i].material))
                            else:
                                plys[(o.name+'{D}'+SLGBP.nomatn)].append((obj, None))
                        elif not o.restrict_render and o.type in rendertypes:
                            addo = False
                            if o not in objs:
                                if scene.slg_export or mfp and any(m for m in mfp if m in o.material_slots):
                                    objs.append(o)
                            if inscenelayer(o):
                                addo = True
                            instobjs[o] = len(plymats)
                            if o.data.materials:
                                for i in range(len(o.data.materials)):
                                    plyn = (o.name+'{D}S'+str(i))
                                    plymats.append(plyn)
                                    plys[plyn] = [(obj, o.material_slots[i].material)]
                                    if addo:
                                        plys[plyn].append((o, o.material_slots[i].material))
                            else:
                                plyn = (o.name+'{D}'+SLGBP.nomatn)
                                plymats.append(plyn)
                                plys[plyn] = [(obj, None)]
                                if addo:
                                    plys[plyn].append((o, None))

        verts = [[] for i in range(len(plymats))]
        vert_vcs = [[] for i in range(len(plymats))]
        mvc = [False]*len(plymats)
        vert_uvs = [[] for i in range(len(plymats))]
        faces = [[] for i in range(len(plymats))]
        sharedverts = {}
        vertnum = [0]*len(plymats)
        color = [0,0,0]
        uvco = [0,0]
        addv = False
        # Only get mesh data if PLY export or a material has "force ply"
        if scene.slg_export or mfp:
            # Delete existing ply files
            if scene.slg_export and os.path.exists(SLGBP.sfullpath):
                any(os.remove('{}/{}'.format(SLGBP.sfullpath,file)) for file in os.listdir(SLGBP.sfullpath) if file.endswith('.ply'))
            for obj in objs:
                print('SLGBP ===> Object: {}'.format(obj.name))
                # Create render mesh
                try:
                    if type(obj) == bpy.types.Object:
                        print("SLGBP        Create render mesh: {}".format(obj.name))
                        mesh = obj.create_mesh(scene, True, 'RENDER')
                    else:
                        print("SLGBP        Using render mesh: {}".format(obj.name))
                        mesh = obj
                except:
                    pass
                else:
                    if type(obj) == bpy.types.Object:
                        print("SLGBP        Xform render mesh: {}".format(obj.name))
                        mesh.transform(obj.matrix_world)
                    # Make copy of verts for fast direct index access (mesh.verts was very slow)
                    if scene.slg_vnormals:
                        v = [tuple(vert.co) + tuple(vert.normal) for vert in mesh.verts]
                    else:
                        v = [tuple(vert.co) for vert in mesh.verts]
                    vcd = []
                    if scene.slg_vcolors and mesh.active_vertex_color:
                        vcd = mesh.active_vertex_color.data
                    uvd = []
                    if scene.slg_vuvs and mesh.active_uv_texture:
                        uvd = mesh.active_uv_texture.data
                    if obj in instobjs:
                        # Correlate obj mat slots with plymat slots
                        onomat = instobjs[obj]
                        objmats = [i for i in range(onomat, onomat+len(mesh.materials))]
                    else:
                        # Correlate obj mat slots with global mat pool
                        onomat = nomat
                        objmats = [plymats.index(ms.material.name) if ms.material else onomat for ms in obj.material_slots]
                    for face, vc, uv in zip_longest(mesh.faces,vcd,uvd):
                        curmat = objmats[face.material_index] if objmats else onomat
                        # Get vertex colors, if avail
                        if vc:
                            colors = vc.color1, vc.color2, vc.color3, vc.color4
                            mvc[curmat] = True
                        # Get uvs if there is an image texture attached
                        if uv:
                            uvcos = uv.uv1, uv.uv2, uv.uv3, uv.uv4
                        if not face.smooth or uv:
                            faces[curmat].append((vertnum[curmat], vertnum[curmat]+1, vertnum[curmat]+2))
                            if len(face.verts) == 4:
                                faces[curmat].append((vertnum[curmat], vertnum[curmat]+2, vertnum[curmat]+3))
                        for j, vert in enumerate(face.verts):
                            if scene.slg_vcolors:
                                if vc:
                                    color[0] = int(255.0*colors[j][0])
                                    color[1] = int(255.0*colors[j][1])
                                    color[2] = int(255.0*colors[j][2])
                                else:
                                    color[0] = color[1] = color[2] = 255
                            if scene.slg_vuvs:
                                if uv:
                                    uvco[0] = uvcos[j][0]
                                    uvco[1] = 1.0 - uvcos[j][1]
                                else:
                                    uvco[0] = uvco[1] = 0
                            if face.smooth and not uv:
                                if (curmat,vert) in sharedverts:
                                    addv = False
                                else:
                                    sharedverts[curmat,vert]=vertnum[curmat]
                                    addv = True
                            else:
                                addv = True
                            if addv:
                                if scene.slg_vnormals and not face.smooth:
                                    verts[curmat].append(v[vert][:3]+tuple(face.normal))
                                else:
                                    verts[curmat].append(v[vert])
                                if scene.slg_vuvs:
                                    vert_uvs[curmat].append(tuple(uvco))
                                if scene.slg_vcolors:
                                    vert_vcs[curmat].append(tuple(color))
                                vertnum[curmat] += 1
                        if face.smooth and not uv:
                            faces[curmat].append((sharedverts[curmat,face.verts[0]], sharedverts[curmat,face.verts[1]], sharedverts[curmat,face.verts[2]]))
                            if len(face.verts) == 4:
                                faces[curmat].append((sharedverts[curmat,face.verts[0]], sharedverts[curmat,face.verts[2]], sharedverts[curmat,face.verts[3]]))
                sharedverts = {}
                # Delete working mesh
                if not mesh.users:
                    print("SLGBP        delete render mesh: {}".format(obj.name))
                    bpy.data.meshes.remove(mesh)
                if SLGBP.abort:
                    return
        # Process each plymat
        for i, pm in enumerate(plymats):
            plyn = pm.replace('.','_')
            # Remove portal materials
            if type(plys[pm][0]) == bpy.types.Material and plys[pm][0].shadeless:
                del plys[pm]
            # If have mesh using this plymat...
            if verts[i] or (not scene.slg_export and os.path.exists('{}/{}.ply'.format(SLGBP.sfullpath,plyn))):
                if scene.slg_export or pm in mfp:
                    # Write out PLY
                    print("SLGBP        writing PLY: {}".format(plyn))
                    fply = open('{}/{}.ply'.format(SLGBP.sfullpath,plyn), 'wb')
                    fply.write(b'ply\n')
                    fply.write(b'format ascii 1.0\n')
                    fply.write(str.encode('comment Created by SmallLuxGPU exporter for Blender 2.5, source file: {}\n'.format((bpy.data.filepath.split('/')[-1].split('\\')[-1]))))
                    fply.write(str.encode('element vertex {}\n'.format(vertnum[i])))
                    fply.write(b'property float x\n')
                    fply.write(b'property float y\n')
                    fply.write(b'property float z\n')
                    if scene.slg_vnormals:
                        fply.write(b'property float nx\n')
                        fply.write(b'property float ny\n')
                        fply.write(b'property float nz\n')
                    if scene.slg_vuvs:
                        fply.write(b'property float s\n')
                        fply.write(b'property float t\n')
                    if scene.slg_vcolors and mvc[i]:
                        fply.write(b'property uchar red\n')
                        fply.write(b'property uchar green\n')
                        fply.write(b'property uchar blue\n')
                    fply.write(str.encode('element face {}\n'.format(len(faces[i]))))
                    fply.write(b'property list uchar uint vertex_indices\n')
                    fply.write(b'end_header\n')
                    # Write out vertices
                    for j, v in enumerate(verts[i]):
                        if scene.slg_vnormals:
                            fply.write(str.encode('{:.6f} {:.6f} {:.6f} {:.6g} {:.6g} {:.6g}'.format(*v)))
                        else:
                            fply.write(str.encode('{:.6f} {:.6f} {:.6f}'.format(*v)))
                        if scene.slg_vuvs:
                            fply.write(str.encode(' {:.6g} {:.6g}'.format(*vert_uvs[i][j])))
                        if scene.slg_vcolors and mvc[i]:
                            fply.write(str.encode(' {} {} {}'.format(*vert_vcs[i][j])))
                        fply.write(b'\n')
                    # Write out faces
                    for f in faces[i]:
                        fply.write(str.encode('3 {} {} {}\n'.format(*f)))
                    fply.close()
            else:
                del plys[pm]
            if SLGBP.abort:
                return
        SLGBP.plys = plys
        SLGBP.instobjs = instobjs

    # Export SLG scene
    @classmethod
    def export(cls, scene):
        SLGBP.cfg = SLGBP.getcfg(scene)
        fcfg = open('{}/{}/render.cfg'.format(SLGBP.spath,SLGBP.sname), 'w')
        for k in sorted(SLGBP.cfg):
            fcfg.write(k + ' = ' + SLGBP.cfg[k] + '\n')
        fcfg.close()
        SLGBP.scn = SLGBP.getscn(scene)
        SLGBP.expmatply(scene)
        SLGBP.matprops = {}
        for m in bpy.data.materials:
            if m.users:
                matprop, scn = SLGBP.getmatscn(scene, m)
                SLGBP.matprops[m.name] = matprop
                SLGBP.scn.update(scn)
        SLGBP.scn['scene.materials.matte.'+SLGBP.nomatn] = '0.75 0.75 0.75'
        SLGBP.scn.update(SLGBP.getobjscn(scene))
        fscn = open('{}/{}/{}.scn'.format(SLGBP.spath,SLGBP.sname,SLGBP.sname), 'w')
        for k in sorted(SLGBP.scn):
            fscn.write(k + ' = ' + SLGBP.scn[k] + '\n')
        fscn.close()

    # Run SLG executable with current scene
    @classmethod
    def runslg(cls, scene):
        if scene.slg_enabletelnet == True:
            print('SLGBP ===> launch SLG: {} -T {}/{}/render.cfg'.format(SLGBP.slgpath,SLGBP.spath,SLGBP.sname))
            SLGBP.slgproc = Popen([SLGBP.slgpath,'-T','{}/{}/render.cfg'.format(SLGBP.spath,SLGBP.sname)], cwd=SLGBP.spath, shell=False)
        else:
            print('SLGBP ===> launch SLG: {} {}/{}/render.cfg'.format(SLGBP.slgpath,SLGBP.spath,SLGBP.sname))
            SLGBP.slgproc = Popen([SLGBP.slgpath,'{}/{}/render.cfg'.format(SLGBP.spath,SLGBP.sname)], cwd=SLGBP.spath, shell=False)

    # Export SLG scene and render it
    @classmethod
    def exportrun(cls, scene):
        SLGBP.export(scene)
        if not SLGBP.abort:
            SLGBP.runslg(scene)

    # Update SLG parameters via telnet interface
    @classmethod
    def liveconnect(cls, scene, errout):
        SLGBP.msg = 'Connecting via telnet to SLG... (SLG Live Mode)'
        SLGBP.msgrefresh()
        SLGBP.telnet = SLGTelnet()
        if SLGBP.telnet.connected:
            SLGBP.live = True
            SLGBP.msg = 'SLG Live! (ESC to exit)'
            SLGBP.msgrefresh()
        else:
            SLGBP.msg = 'Unable to connect to SLG via telnet... (SLG Live Mode)'
            SLGBP.msgrefresh()
            errout("Can't connect.  SLG must be running with telnet interface enabled")

    @classmethod
    def livetrigger(cls, scene, liveact):
        SLGBP.liveact = SLGBP.liveact|liveact
        if SLGBP.thread == None or not SLGBP.thread.is_alive():
            SLGBP.thread = threading.Thread(target=SLGBP.liveupdate,args=[scene])
            SLGBP.thread.start()

    @classmethod
    def liveupdate(cls, scene):
        def cmpupd(prop, src, dst, reset, force):
            if not force and prop in dst and src[prop] == dst[prop]:
                return False
            if reset:
                SLGBP.telnet.send('render.stop')
                SLGBP.telnet.send('image.reset')
            SLGBP.telnet.send('set ' + prop + ' = '+ src[prop])
            dst[prop] = src[prop]
            return True
        while True:
            act = SLGBP.liveact
            SLGBP.liveact = 0
            wasreset = False
            if act & SLGBP.LIVECFG > 0:
                cfg = SLGBP.getcfg(scene)
                # Changing film.tonemap.type resets all tonemap params; must re-initialize
                forcetm = cmpupd('film.tonemap.type', cfg, SLGBP.cfg, False, False)
                del cfg['film.tonemap.type']
                for k in cfg:
                    # Tonemapping does not require a film reset
                    if k.startswith('film.tonemap'):
                        cmpupd(k, cfg, SLGBP.cfg, False, forcetm)
                    elif cmpupd(k, cfg, SLGBP.cfg, not wasreset, False):
                        wasreset = True
            if act & SLGBP.LIVESCN > 0:
                scn = SLGBP.getscn(scene)
                for k in scn:
                    if cmpupd(k, scn, SLGBP.scn, not wasreset, False):
                        wasreset = True
            if act & SLGBP.LIVEMTL > 0:
                if SLGBP.livemat:
                    matprop, scn = SLGBP.getmatscn(scene, SLGBP.livemat)
                    if matprop != SLGBP.matprops[SLGBP.livemat.name]:
                        del SLGBP.scn[SLGBP.matprops[SLGBP.livemat.name]]
                        SLGBP.matprops[SLGBP.livemat.name] = matprop
                    SLGBP.livemat = None
                else:
                    scn = {}
                    for m in bpy.data.materials:
                        if m.users:
                            matprop, matscn = SLGBP.getmatscn(scene, m)
                            scn.update(matscn)
                            if matprop != SLGBP.matprops[m.name]:
                                del SLGBP.scn[SLGBP.matprops[m.name]]
                                SLGBP.matprops[m.name] = matprop
                for k in scn:
                    if cmpupd(k, scn, SLGBP.scn, not wasreset, False):
                        wasreset = True
            if wasreset:
                SLGBP.telnet.send('render.start')

            time.sleep(0.5) # param?
            if SLGBP.liveact == 0: break

    # SLG live animation render
    @classmethod
    def liveanimrender(cls, scene):
        SLGBP.liveanim = True
        scene.set_frame(scene.frame_start-scene.frame_step)
        SLGBP.livetrigger(scene, SLGBP.LIVEALL)
        if scene.slg_cameramotionblur:
            # Make sure first livetrigger takes place
            time.sleep(0.25)
        while True:
            scene.set_frame(scene.frame_current+scene.frame_step)
            SLGBP.msg = 'SLG Live! rendering animation frame: ' + str(scene.frame_current) + " (ESC to abort)"
            SLGBP.livetrigger(scene, SLGBP.LIVEALL)
            time.sleep(scene.slg_batchmodetime)
            SLGBP.telnet.send('image.save')
            if not SLGBP.live or scene.frame_current+scene.frame_step > scene.frame_end: break
        SLGBP.msg = 'SLG Live! (ESC to exit)'
        SLGBP.msgrefresh()
        SLGBP.liveanim = False

# Output informational message to main viewport
def info_callback(context):
    blf.position(0, 75, 30, 0)
    blf.size(0, 14, 72)
    blf.draw(0, SLGBP.msg)
    if SLGBP.live:
        if SLGBP.slgproc.poll() != None:
            SLGBP.live = False
            SLGBP.abort = True
            return
        if not SLGBP.liveanim:
            # If frame is unchanged check only base SCN
            if SLGBP.curframe == context.scene.frame_current:
                SLGBP.livetrigger(context.scene, SLGBP.LIVESCN)
            # Else could be frame skip / playback where anything can change
            else:
                SLGBP.curframe = context.scene.frame_current
                SLGBP.livetrigger(context.scene, SLGBP.LIVEALL)

# SLG Render (without Blender render engine) operator
class SLGRender(bpy.types.Operator):
    bl_idname = "scene.slg_render"
    bl_label = "SLG Render"
    bl_description = "SLG export and render without using Blender's render engine"

    animation = bpy.props.BoolProperty(name="Animation", description="Render Animation", default= False)

    def _error(self, msg):
        self._iserror = True
        self.report('ERROR', "SLGBP: " + msg)

    def _reset(self, context):
        SLGBP.thread = None
        context.region.callback_remove(SLGBP.icb)
        context.area.tag_redraw()
        if self.properties.animation:
            context.scene.render.fps = self._orig_fps
            context.scene.render.fps_base = self._orig_fps_base
            if SLGBP.slgproc:
                if SLGBP.slgproc.poll() == None:
                    SLGBP.slgproc.terminate()

    def modal(self, context, event):
        if self.properties.animation:
            if event.type == 'ESC':
                SLGBP.abort = True
                bpy.ops.screen.animation_play()
                self._reset(context)
                self.report('WARNING', "SLG animation render aborted.")
                return {'CANCELLED'}
            if event.type == 'TIMER0':
                SLGBP.slgproc.wait()
                if not SLGBP.init(context.scene, self._error):
                    bpy.ops.screen.animation_play()
                    self._reset(context)
                    return {'CANCELLED'}
                SLGBP.exportrun(context.scene)
                if context.scene.frame_current == context.scene.frame_end:
                    bpy.ops.screen.animation_play()
                    SLGBP.slgproc.wait()
                    self._reset(context)
                    self.report('INFO', "SLG animation render done.")
                    return {'FINISHED'}
        return {'PASS_THROUGH'}

    def invoke(self, context, event):
        if self.properties.animation and not context.scene.slg_enablebatchmode:
            self.report('ERROR', "SLGBP: Enable batch mode for animations")
            return {'CANCELLED'}
        if SLGBP.live:
            self.report('ERROR', "SLGBP: Can't export during SLG live mode")
            return {'CANCELLED'}
        if SLGBP.thread:
            if SLGBP.thread.is_alive():
                self.report('ERROR', "SLGBP is busy")
                return {'CANCELLED'}
        if SLGBP.slgproc:
            if SLGBP.slgproc.poll() == None:
                self.report('ERROR', "SLG is already running")
                return {'CANCELLED'}
        if self.properties.animation:
            context.scene.set_frame(context.scene.frame_start)
        if not SLGBP.init(context.scene, self._error):
            return {'CANCELLED'}
        self._iserror = False
        SLGBP.abort = False
        if self.properties.animation:
            context.manager.add_modal_handler(self)
            SLGBP.msg = 'SLG exporting and rendering... (ESC to abort)'
            SLGBP.icb = context.region.callback_add(info_callback, (context,), 'POST_PIXEL')
            SLGBP.exportrun(context.scene)
            self._orig_fps = context.scene.render.fps
            self._orig_fps_base = context.scene.render.fps_base
            context.scene.render.fps = 1
            context.scene.render.fps_base = 5
            bpy.ops.screen.animation_play()
            return {'RUNNING_MODAL'}
        else:
            SLGBP.exportrun(context.scene)
            if SLGBP.warn:
                self.report('WARNING', SLGBP.warn)
            else:
                self.report('INFO', "SLG export done.")
        return {'FINISHED'}

# SLG Live operator
class SLGLive(bpy.types.Operator):
    bl_idname = "scene.slg_live"
    bl_label = "SLG Live"
    bl_description = "SLG Live mode"

    def _error(self, msg):
        self._iserror = True
        self.report('ERROR', "SLGBP: " + msg)

    def modal(self, context, event):
        if self._iserror or SLGBP.abort or event.type == 'ESC':
            SLGBP.live = False
            SLGBP.telnet.close()
            context.region.callback_remove(SLGBP.icb)
            context.area.tag_redraw()
            return {'FINISHED'}
        return {'PASS_THROUGH'}

    def poll(self, context):
        return not SLGBP.live

    #def execute(self, context):
    def invoke(self, context, event):
        if SLGBP.thread:
            if SLGBP.thread.is_alive():
                self.report('ERROR', "SLGBP is busy")
                return {'CANCELLED'}
        if SLGBP.slgproc:
            if SLGBP.slgproc.poll() != None:
                self.report('ERROR', "SLG must be running with telnet interface enabled")
                return {'CANCELLED'}
        self._iserror = False
        context.manager.add_modal_handler(self)
        SLGBP.abort = False
        SLGBP.msg = 'SLG Live!'
        SLGBP.msgrefresh = context.area.tag_redraw
        SLGBP.curframe = context.scene.frame_current
        SLGBP.icb = context.region.callback_add(info_callback, (context,), 'POST_PIXEL')
        SLGBP.thread = threading.Thread(target=SLGBP.liveconnect,args=[context.scene, self._error])
        SLGBP.thread.start()
        return {'RUNNING_MODAL'}

# SLG Live update all operator
class SLGLiveUpdate(bpy.types.Operator):
    bl_idname = "scene.slg_liveupd"
    bl_label = "SLG Live Update All"
    bl_description = "SLG Live mode: update all"

    def poll(self, context):
        return SLGBP.live

    # def execute(self, context):
    def invoke(self, context, event):
        SLGBP.livetrigger(context.scene, SLGBP.LIVEALL)
        return {'FINISHED'}

# SLG Live animation render operator
class SLGLiveAnim(bpy.types.Operator):
    bl_idname = "scene.slg_liveanim"
    bl_label = "SLG Live Animation Render"
    bl_description = "SLG Live mode: render animation"

    def poll(self, context):
        return SLGBP.live

    # def execute(self, context):
    def invoke(self, context, event):
        threading.Thread(target=SLGBP.liveanimrender,args=[context.scene]).start()
        return {'FINISHED'}

class SmallLuxGPURender(bpy.types.RenderEngine):
    bl_idname = 'SMALLLUXGPU_RENDER'
    bl_label = "SmallLuxGPU"

    def _error(self, error):
        self.update_stats("-=|ERROR|", error)
        # Hold the render results window while we display the error message
        while True:
            if self.test_break():
                break
            time.sleep(0.1)

    def render(self, scene):

        if SLGBP.live:
            self._error("SLGBP: Can't render during SLG live mode")
            return
        if SLGBP.thread:
            if SLGBP.thread.is_alive():
                self._error("SLGBP is Busy")
                return
        self.update_stats("", "SmallLuxGPU: Exporting scene, please wait...")
        if not SLGBP.init(scene, self._error):
            return
        # Force an update to object matrices when rendering animations
        scene.set_frame(scene.frame_current)
        SLGBP.export(scene)
        SLGBP.runslg(scene)

        if scene.slg_waitrender:
            # Wait for SLG, load image
            if scene.slg_enablebatchmode:
                self.update_stats("", "SmallLuxGPU: Batch Rendering frame# {} (see console for progress), please wait...".format(scene.frame_current))
            else:
                self.update_stats("", "SmallLuxGPU: Waiting... (in SLG window: press 'p' to save image, 'Esc' to exit)")
            # Hold the render results window hostage until SLG returns...
            while True:
                if SLGBP.slgproc.poll() != None:
                    result = self.begin_result(0, 0, int(SLGBP.cfg['image.width']), int(SLGBP.cfg['image.height']))
                    try:
                        print('SLGBP ===> load image from file: ' + SLGBP.image_filename.format(scene.frame_current))
                        result.layers[0].load_from_file(SLGBP.image_filename.format(scene.frame_current))
                        # Delete SLG's file (Blender can save its own)
                        os.remove(SLGBP.image_filename.format(scene.frame_current))
                    except:
                        pass
                    self.end_result(result)
                    break
                if self.test_break():
                    try:
                        SLGBP.slgproc.terminate()
                    except:
                        pass
                    break
                time.sleep(0.1)

def slg_properties():
    # Add SmallLuxGPU properties to scene
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
        description="Export optional vertex uv information (only if assigned)",
        default=True)

    BoolProperty(attr="slg_vcolors", name="VCs",
        description="Export optional vertex color information (only if assigned)",
        default=False)

    BoolProperty(attr="slg_vnormals", name="VNs",
        description="Export optional vertex normal information",
        default=True)

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
               ("2", "Direct", "Direct lighting only"),
               ("1", "SPPM", "Stochastic Progressive Photon Mapping")),
        default="0")

    EnumProperty(attr="slg_sppmlookuptype", name="",
        description="SPPM Lookup Type (Hybrid generally best)",
        items=(("-1", "SPPM Lookup Type", "SPPM Lookup Type"),
               ("0", "Hash Grid", "Hash Grid"),
               ("1", "KD tree", "KD tree"),
               ("2", "Hybrid Hash Grid", "Hybrid Hash Grid")),
        default="2")

    IntProperty(attr="slg_sppm_eyepath_maxdepth", name="Eye depth",
        description="SPPM eye path max depth",
        default=16, min=1, soft_min=1)

    FloatProperty(attr="slg_sppm_photon_alpha", name="P.Alpha",
        description="SPPM Photon Alpha: how fast the 'area of interest' around each eye hit point is reduced at each pass",
        default=0.7, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    FloatProperty(attr="slg_sppm_photon_radiusscale", name="P.Radius",
        description="SPPM Photon Radius: how to scale the initial 'area of interest' around each eye hit point",
        default=1.0, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    IntProperty(attr="slg_sppm_photon_per_pass", name="Photons",
        description="Number of (SPPM) photons to shot for each pass",
        default=2500000, min=100, soft_min=100)

    IntProperty(attr="slg_sppm_photon_maxdepth", name="P.Depth",
        description="Max depth for (SPPM) photons",
        default=8, min=1, soft_min=1)

    EnumProperty(attr="slg_accelerator_type", name="Accelerator Type",
        description="Select the desired ray tracing accelerator type",
        items=(("-1", "Default", "Default"),
               ("0", "BVH", "Bounding Volume Hierarchy"),
               ("1", "QBVH", "Quad-Bounding Volume Hierarchy"),
               ("2", "QBVH (image storage disabled)", "Quad-Bounding Volume Hierarchy with disabled image storage"),
               ("3", "MQBVH (instances support)", "Multilevel Quad-Bounding Volume Hierarchy")),
        default="-1")

    EnumProperty(attr="slg_film_filter_type", name="Film Filter Type",
        description="Select the desired film filter type",
        items=(("0", "None", "No filter"),
               ("1", "Gaussian", "Gaussian filter")),
        default="1")

    EnumProperty(attr="slg_film_tonemap_type",
        description="Select the desired film tonemap type",
        items=(("-1", "Tonemapping", "Tonemapping"),
               ("0", "Linear tonemapping", "Linear tonemapping"),
               ("1", "Reinhard '02 tonemapping", "Reinhard '02 tonemapping")),
        default="0")

    FloatProperty(attr="slg_linear_scale", name="scale",
        description="Linear tonemapping scale",
        default=1.0, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    FloatProperty(attr="slg_reinhard_prescale", name="Pre-scale",
        description="Reinhard '02 tonemapping pre-scale",
        default=1.0, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    FloatProperty(attr="slg_reinhard_postscale", name="Post-scale",
        description="Reinhard '02 tonemapping post-scale",
        default=1.2, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    FloatProperty(attr="slg_reinhard_burn", name="Burn",
        description="Reinhard '02 tonemapping burn",
        default=3.75, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    FloatProperty(attr="slg_film_gamma", name="Gamma",
        description="Gamma correction on screen and for saving non-HDR file format",
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
               ("jpg", "JPG", "JPG")), # A lot more formats supported...
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
        default=(0.05, 0.05, 0.05), min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, subtype="COLOR")
    FloatVectorProperty(attr="slg_partmedia_scattering", name="Scattering",
        description="Media scattering",
        default=(0.05, 0.05, 0.05), min=0.0, max=100.0, soft_min=0.0, soft_max=100.0, subtype="COLOR")
    FloatVectorProperty(attr="slg_partmedia_bbox1", name="Bounding Box Min",
        description="Media bounding box min",
        default=(-10.0, -10.0, -10.0), subtype="XYZ", size=3)
    FloatVectorProperty(attr="slg_partmedia_bbox2", name="Bounding Box Max",
        description="Media bounding box max",
        default=(10.0, 10.0, 10.0), subtype="XYZ", size=3)

    BoolProperty(attr="slg_enablebatchmode", name="Batch Mode",
        description="Render in background (required for animations)",
        default=False)

    IntProperty(attr="slg_batchmodetime", name="Batch mode max run time",
        description="Max number of seconds to render; 0 = ignore",
        default=120, min=0, soft_min=0)

    IntProperty(attr="slg_batchmodespp", name="Batch mode max samples per pixel",
        description="Max number of samples per pixels in batch mode; 0 = ignore",
        default=128, min=0, soft_min=0)

    IntProperty(attr="slg_batchmode_periodicsave", name="Periodic image save",
        description="Save image periodically (in seconds); 0 = ignore",
        default=0, min=0, soft_min=0)

    BoolProperty(attr="slg_waitrender", name="Wait",
        description="Wait for render to finish; load image into render results (required for animations)",
        default=False)

    BoolProperty(attr="slg_enabletelnet", name="Telnet",
        description="Enable SLG telnet interface to enable live interaction from Blender",
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

# Add SLG Camera Lens Radius on Camera panel
def slg_lensradius(self, context):
    if context.scene.render.engine == 'SMALLLUXGPU_RENDER':
        self.layout.split().column().prop(context.camera, "slg_lensradius", text="SLG Lens Radius")
        if SLGBP.live:
            SLGBP.livetrigger(context.scene, SLGBP.LIVESCN)

# Add Material PLY export override on Material panel
def slg_forceply(self, context):
    if context.scene.render.engine == 'SMALLLUXGPU_RENDER':
        self.layout.split().column().prop(context.material, "slg_forceply")
        if SLGBP.live:
            SLGBP.livemat = context.material
            SLGBP.livetrigger(context.scene, SLGBP.LIVEMTL)

def slg_livescn(self, context):
    if context.scene.render.engine == 'SMALLLUXGPU_RENDER':
        if SLGBP.live:
            SLGBP.livetrigger(context.scene, SLGBP.LIVESCN)

# Add (non-render engine) Render to View3D toolbar
def slg_rendernre(self, context):
    if context.scene.render.engine == 'SMALLLUXGPU_RENDER':
        row = self.layout.row(align=True)
        if SLGBP.live:
            row.operator("scene.slg_liveupd", text="", icon='ANIM')
            row.operator("scene.slg_liveanim", text="", icon='RENDER_ANIMATION')
        else:
            row.operator("scene.slg_render", text="", icon='RENDER_STILL')
            row.operator("scene.slg_render", text="", icon='RENDER_ANIMATION').animation = True
        row.operator("scene.slg_live", text="", icon='RENDER_RESULT')

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

        # Wrong Context
#         split = layout.split()
#         col = split.column()
#         col.operator("scene.slg_render", text="Still", icon="RENDER_STILL")
#         col = split.column()
#         col.operator("scene.slg_render", text="Anim", icon="RENDER_ANIMATION").animation = True
#         col = split.column()
#         col.operator("scene.slg_live", text="Live", icon="RENDER_RESULT")
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
        col = split.column()
        if scene.slg_film_tonemap_type == '0':
            col.prop(scene, "slg_linear_scale")
        else:
            col.prop(scene, "slg_reinhard_burn")
            split = layout.split()
            col = split.column()
            col.prop(scene, "slg_reinhard_prescale")
            col = split.column()
            col.prop(scene, "slg_reinhard_postscale")
        split = layout.split()
        col = split.column()
        col.prop(scene, "slg_imageformat")
        col = split.column()
        col.prop(scene, "slg_film_gamma")
        if scene.slg_rendering_type == '1':
            split = layout.split()
            col = split.column()
            col.prop(scene, "slg_sppmlookuptype")
            col = split.column()
            col.prop(scene, "slg_sppm_eyepath_maxdepth")
            split = layout.split()
            col = split.column()
            col.prop(scene, "slg_sppm_photon_alpha")
            col = split.column()
            col.prop(scene, "slg_sppm_photon_radiusscale")
            split = layout.split()
            col = split.column()
            col.prop(scene, "slg_sppm_photon_per_pass", text="Photon count per pass")
            col = split.column()
            col.prop(scene, "slg_sppm_photon_maxdepth")
        else:
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
                split = layout.split(percentage=0.5)
                col = split.column()
                col.prop(scene, "slg_partmedia_bbox1")
                col = split.column()
                col.prop(scene, "slg_partmedia_bbox2")
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
        col = split.column()
        col.prop(scene, "slg_enabletelnet")
        if scene.slg_enablebatchmode or SLGBP.live:
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
        if SLGBP.live:
            SLGBP.livetrigger(context.scene, SLGBP.LIVECFG)

def register():
    slg_properties()
    bpy.types.register(SLGRender)
    bpy.types.register(SLGLive)
    bpy.types.register(SLGLiveUpdate)
    bpy.types.register(SLGLiveAnim)
    bpy.types.register(RENDER_PT_slrender_options)
    bpy.types.register(SmallLuxGPURender)
    bpy.types.DATA_PT_camera.append(slg_lensradius)
    bpy.types.MATERIAL_PT_diffuse.append(slg_forceply)
    bpy.types.WORLD_PT_environment_lighting.append(slg_livescn)
    bpy.types.TEXTURE_PT_mapping.append(slg_livescn)
    bpy.types.DATA_PT_sunsky.append(slg_livescn)
    bpy.types.VIEW3D_HT_header.append(slg_rendernre)

def unregister():
    bpy.types.unregister(SLGRender)
    bpy.types.unregister(SLGLive)
    bpy.types.unregister(SLGLiveUpdate)
    bpy.types.unregister(SLGLiveAnim)
    bpy.types.unregister(RENDER_PT_slrender_options)
    bpy.types.unregister(SmallLuxGPURender)
    bpy.types.DATA_PT_camera.remove(slg_lensradius)
    bpy.types.MATERIAL_PT_diffuse.remove(slg_forceply)
    bpy.types.WORLD_PT_environment_lighting.remove(slg_livescn)
    bpy.types.TEXTURE_PT_mapping.remove(slg_livescn)
    bpy.types.DATA_PT_sunsky.remove(slg_livescn)
    bpy.types.VIEW3D_HT_header.remove(slg_rendernre)

if __name__ == "__main__":
    register()