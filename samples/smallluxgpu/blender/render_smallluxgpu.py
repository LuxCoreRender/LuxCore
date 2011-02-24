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
# SmallLuxGPU v1.7beta2 Blender 2.5 plug-in

bl_info = {
    "name": "SmallLuxGPU",
    "author": "see (SLG) AUTHORS.txt",
    "version": (0, 7, 2),
    "blender": (2, 5, 6),
    "location": "Render > Engine > SmallLuxGPU",
    "description": "SmallLuxGPU Exporter and Live! mode Plugin",
    "warning": "",
    "wiki_url": "http://www.luxrender.net/wiki/index.php?title=Blender_2.5_exporter",
    "tracker_url": "http://www.luxrender.net/forum/viewforum.php?f=34",
    "category": "Render"}

import bpy
import blf
from mathutils import Matrix, Vector
import os
from threading import Lock, Thread
from time import sleep
from telnetlib import Telnet
from struct import pack
from itertools import zip_longest
from subprocess import Popen
from math import isnan

# SLG Telnet interface
class SLGTelnet:
    def __init__(self):
        self.slgtn = None
        try:
            self.slgtn = Telnet('localhost',18081)
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
    sun = infinitelight = None
    live = liveanim = False
    LIVECFG, LIVESCN, LIVEMTL, LIVEOBJ, LIVEALL = 1, 2, 4, 8, 15
    livemat = None
    liveact = 0
    slgproc = thread = None
    lock = Lock()
    telnet = None
    abort = False

    # Initialize SLG scene for export
    @staticmethod
    def init(scene, errout):
        # Reset Abort flag
        SLGBP.abort = False
        SLGBP.warn = ''

        # SmallLuxGPU executable path
        SLGBP.slgpath = bpy.path.abspath(scene.slg.slgpath).replace('\\','/')
        if os.path.isdir(SLGBP.slgpath):
            SLGBP.slgpath = SLGBP.slgpath + '/slg'
        elif not os.path.isfile(SLGBP.slgpath):
            errout("Full path to SmallLuxGPU executable required")
            return False
        if not os.path.exists(SLGBP.slgpath):
            errout("Cannot find SLG executable at specified path")
            return False

        # Scenes files path
        SLGBP.spath = bpy.path.abspath(scene.slg.scene_path).replace('\\','/')
        if SLGBP.spath.endswith('/'):
            SLGBP.spath = os.path.dirname(SLGBP.spath)
        if not os.path.exists(SLGBP.spath):
            errout("Cannot find scenes directory")
            return False

        # Scene name
        SLGBP.sname = scene.slg.scenename
        if os.path.isfile(scene.slg.scenename):
            SLGBP.sname = os.path.basename(scene.slg.scenename)
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
        if os.path.exists(os.path.dirname(scene.render.filepath)):
            if not scene.render.filepath.split('/')[-1].split('\\')[-1]:
                SLGBP.image_filename = (scene.render.filepath + '{:05d}')
            else:
                i = scene.render.filepath.count('#')
                if i > 0:
                    SLGBP.image_filename = scene.render.filepath.replace('#'*i, '{:0'+str(i)+'d}')
                else:
                    SLGBP.image_filename = scene.render.filepath
            if not SLGBP.image_filename.endswith(scene.render.file_extension):
                # SLG requires extension, force one
                SLGBP.image_filename += scene.render.file_extension
        else:
            SLGBP.image_filename = '{}/{}/{}.{}'.format(SLGBP.spath,SLGBP.sname,SLGBP.sname,scene.slg.imageformat)

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
        if scene.slg.cameramotionblur:
            scene.frame_set(scene.frame_current - 1) # Blender now supports sub-frames!
            SLGBP.camdirBlur = Vector((0, 0, -10)) * scene.camera.matrix_world
            SLGBP.camlocBlur = scene.camera.matrix_world.to_translation()
            SLGBP.camupBlur = Vector((0,1,0)) * scene.camera.matrix_world.to_3x3()
            scene.frame_set(scene.frame_current + 1)

        return True

    # Get SLG .cfg properties
    @staticmethod
    def getcfg(scene):
        cfg = {}
        cfg['film.tonemap.type'] = scene.slg.film_tonemap_type
        if scene.slg.film_tonemap_type == '0':
            cfg['film.tonemap.linear.scale'] = ff(scene.slg.linear_scale)
        else:
            cfg['film.tonemap.reinhard02.burn'] = ff(scene.slg.reinhard_burn)
            cfg['film.tonemap.reinhard02.prescale'] = ff(scene.slg.reinhard_prescale)
            cfg['film.tonemap.reinhard02.postscale'] = ff(scene.slg.reinhard_postscale)
        cfg['image.filename'] = SLGBP.image_filename.format(scene.frame_current)

        # ---- End of Live properties ----
        if SLGBP.live:
            return cfg

        cfg['image.width'] = format(int(scene.render.resolution_x*scene.render.resolution_percentage*0.01))
        cfg['image.height'] = format(int(scene.render.resolution_y*scene.render.resolution_percentage*0.01))
        cfg['batch.halttime'] = format(scene.slg.enablebatchmode*scene.slg.batchmodetime)
        cfg['batch.haltspp'] = format(scene.slg.enablebatchmode*scene.slg.batchmodespp)
        cfg['batch.periodicsave'] = format(scene.slg.batchmode_periodicsave)
        cfg['scene.file'] = '{}/{}/{}.scn'.format(SLGBP.spath,SLGBP.sname,SLGBP.sname)
        cfg['scene.epsilon'] = format(0.0001/scene.unit_settings.scale_length,'g')
        cfg['opencl.latency.mode'] = format(scene.slg.low_latency, 'b')
        cfg['opencl.nativethread.count'] = format(scene.slg.native_threads)
        cfg['opencl.cpu.use'] = format(scene.slg.opencl_cpu, 'b')
        cfg['opencl.gpu.use'] = format(scene.slg.opencl_gpu, 'b')
        cfg['opencl.platform.index'] = format(scene.slg.platform)
        if scene.slg.devices.strip():
            cfg['opencl.devices.select'] = scene.slg.devices
        cfg['opencl.renderthread.count'] = format(scene.slg.devices_threads)
        cfg['opencl.gpu.workgroup.size'] = format(scene.slg.gpu_workgroup_size)
        cfg['film.gamma'] = format(scene.slg.film_gamma, 'g')
        cfg['film.filter.type'] = scene.slg.film_filter_type
        cfg['screen.refresh.interval'] = format(scene.slg.refreshrate)
        cfg['renderengine.type'] = scene.slg.rendering_type
        cfg['sppm.lookup.type'] = scene.slg.sppmlookuptype
        cfg['sppm.directlight.enable'] = format(scene.slg.sppmdirectlight, 'b')
        cfg['sppm.eyepath.maxdepth'] = format(scene.slg.sppm_eyepath_maxdepth)
        cfg['sppm.photon.alpha'] = ff(scene.slg.sppm_photon_alpha)
        cfg['sppm.photon.startradiusscale'] = ff(scene.slg.sppm_photon_radiusscale)
        cfg['sppm.photon.maxdepth'] = format(scene.slg.sppm_photon_maxdepth)
        cfg['sppm.stochastic.count'] = format(scene.slg.sppm_photon_per_pass)
        cfg['path.maxdepth'] = format(scene.slg.tracedepth)
        cfg['path.russianroulette.depth'] = format(scene.slg.rrdepth)
        cfg['path.russianroulette.strategy'] = scene.slg.rrstrategy
        if scene.slg.rrstrategy == '0':
            cfg['path.russianroulette.prob'] = ff(scene.slg.rrprob)
        else:
            cfg['path.russianroulette.cap'] = ff(scene.slg.rrcap)
        cfg['path.lightstrategy'] = scene.slg.lightstrategy
        cfg['path.shadowrays'] = format(scene.slg.shadowrays)
        cfg['path.sampler.spp'] = scene.slg.sampleperpixel
        cfg['accelerator.type'] = scene.slg.accelerator_type

        return cfg

    # Get SLG .scn scene properties
    @staticmethod
    def getscn(scene):
        scn = {}

        # Get camera and lookat direction
        cam = scene.camera
        camdir = Vector((0, 0, -1)) * cam.matrix_world

        # Camera.location not always updated, but matrix is
        camloc = cam.matrix_world.to_translation()
        scn['scene.camera.lookat'] = '{} {} {} {} {} {}'.format(ff(camloc.x),ff(camloc.y),ff(camloc.z),ff(camdir.x),ff(camdir.y),ff(camdir.z))
        camup = Vector((0,1,0)) * cam.matrix_world.to_3x3()
        scn['scene.camera.up'] = '{} {} {}'.format(ff(camup.x),ff(camup.y),ff(camup.z))

        scn['scene.camera.fieldofview'] = format(cam.data.angle*180.0/3.1415926536,'g')

        scn['scene.camera.motionblur.enable'] = format(scene.slg.cameramotionblur, 'b')
        if scene.slg.cameramotionblur:
            if SLGBP.live:
                if 'scene.camera.lookat' in SLGBP.scn:
                    scn['scene.camera.motionblur.lookat'] = SLGBP.scn['scene.camera.lookat']
                    scn['scene.camera.motionblur.up'] = SLGBP.scn['scene.camera.up']
            else:
                scn['scene.camera.motionblur.lookat'] = '{} {} {} {} {} {}'.format(ff(SLGBP.camlocBlur.x),ff(SLGBP.camlocBlur.y),ff(SLGBP.camlocBlur.z),
                        ff(SLGBP.camdirBlur[0]),ff(SLGBP.camdirBlur[1]),ff(SLGBP.camdirBlur[2]))
                scn['scene.camera.motionblur.up'] = '{} {} {}'.format(ff(SLGBP.camupBlur.x),ff(SLGBP.camupBlur.y),ff(SLGBP.camupBlur.z))

        # DOF
        fdist = (camloc-cam.data.dof_object.matrix_world.to_translation()).magnitude if cam.data.dof_object else cam.data.dof_distance
        if fdist:
            scn['scene.camera.focaldistance'] = ff(fdist)
            scn['scene.camera.lensradius'] = ff(cam.data.slg_lensradius)

        # Infinite light, if present
        if SLGBP.infinitelight:
            if not SLGBP.live:
                if hasattr(SLGBP.infinitelight.texture.image,'filepath'):
                    scn['scene.infinitelight.file'] = bpy.path.abspath(SLGBP.infinitelight.texture.image.filepath).replace('\\','/')
                    portal = [m.name for m in bpy.data.materials if m.use_shadeless]
                    if portal:
                        portalpath = '{}/{}/{}.ply'.format(SLGBP.spath,SLGBP.sname,portal[0].replace('.','_'))
                        if os.path.exists(portalpath):
                            scn['scene.infinitelight.file'] += '|'+portalpath
                        if len(portal) > 1:
                            SLGBP.warn = 'More than one portal material (Shadeless) defined!'
                            print('WARNING: ' + SLGBP.warn)
            if scene.world.light_settings.use_environment_light:
                wle = scene.world.light_settings.environment_energy
            else:
                wle = 1.0
            scn['scene.infinitelight.gain'] = '{} {} {}'.format(ff(SLGBP.infinitelight.texture.factor_red*wle),ff(SLGBP.infinitelight.texture.factor_green*wle),ff(SLGBP.infinitelight.texture.factor_blue*wle))
            scn['scene.infinitelight.shift'] = '{} {}'.format(ff(SLGBP.infinitelight.offset.x),ff(SLGBP.infinitelight.offset.y))
            if scene.slg.infinitelightbf:
                scn['scene.infinitelight.usebruteforce'] = '1'

        # Sun lamp
        if SLGBP.sun:
            # We only support one visible sun lamp
            sundir = Vector((0,0,1)) * SLGBP.sun.matrix_world.to_3x3()
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
        if scene.slg.enablepartmedia:
            scn['scene.partecipatingmedia.singlescatering.enable'] = '1'
            scn['scene.partecipatingmedia.singlescatering.bbox'] = '{} {} {} {} {} {}'.format(scene.slg.partmedia_bbox1.x, scene.slg.partmedia_bbox1.y, scene.slg.partmedia_bbox1.z, scene.slg.partmedia_bbox2.x, scene.slg.partmedia_bbox2.y, scene.slg.partmedia_bbox2.z)
            scn['scene.partecipatingmedia.singlescatering.stepsize'] = ff(scene.slg.partmedia_stepsize)
            scn['scene.partecipatingmedia.singlescatering.rrprob'] = ff(scene.slg.partmedia_rrprob)
            scn['scene.partecipatingmedia.singlescatering.emission'] = '{} {} {}'.format(scene.slg.partmedia_emission[0], scene.slg.partmedia_emission[1], scene.slg.partmedia_emission[2])
            scn['scene.partecipatingmedia.singlescatering.scattering'] = '{} {} {}'.format(scene.slg.partmedia_scattering[0], scene.slg.partmedia_scattering[1], scene.slg.partmedia_scattering[2])

        return scn

    # Get SLG .scn mat properties
    @staticmethod
    def getmatscn(scene, m):
        scn = {}
        matn = m.name.replace('.','_')
        if m.use_shadeless:
            matprop = None
        elif m.emit:
            matprop = 'scene.materials.light.{}'.format(matn)
            scn[matprop] = '{} {} {}'.format(ff(m.emit*m.diffuse_color[0]),ff(m.emit*m.diffuse_color[1]),ff(m.emit*m.diffuse_color[2]))
        elif m.use_transparency:
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
            if not m.raytrace_mirror.use or not m.raytrace_mirror.reflect_factor:
                matprop = 'scene.materials.matte.{}'.format(matn)
                scn[matprop] = '{} {} {}'.format(ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]))
            else:
                if m.raytrace_mirror.fresnel > 0:
                    matprop = 'scene.materials.alloy.{}'.format(matn)
                    scn[matprop] = '{} {} {} {} {} {} {} {} {:b}'.format(ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]),
                        ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                        ff(pow(10000.0,m.raytrace_mirror.gloss_factor)),m.raytrace_mirror.fresnel,m.raytrace_mirror.depth>0)
                else:
                    if m.raytrace_mirror.gloss_factor < 1:
                        refltype = 'metal'
                        gloss = ff(pow(10000.0,m.raytrace_mirror.gloss_factor))
                    else:
                        refltype = 'mirror'
                        gloss = ''
                    if m.raytrace_mirror.reflect_factor == 1:
                        matprop = 'scene.materials.{}.{}'.format(refltype,matn)
                        scn[matprop] = '{} {} {} {} {:b}'.format(ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),
                            ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),gloss,m.raytrace_mirror.depth>0)
                    else:
                        matprop = 'scene.materials.matte{}.{}'.format(refltype,matn)
                        scn[matprop] = '{} {} {} {} {} {} {} {:b}'.format(ff(m.diffuse_color[0]),ff(m.diffuse_color[1]),ff(m.diffuse_color[2]),
                            ff(m.raytrace_mirror.reflect_factor*m.mirror_color[0]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[1]),ff(m.raytrace_mirror.reflect_factor*m.mirror_color[2]),
                            gloss,m.raytrace_mirror.depth>0)
                        if m.diffuse_color.v+m.raytrace_mirror.reflect_factor*m.mirror_color.v > 1.0001:
                            SLGBP.warn = m.name + ': diffuse + reflected color greater than 1.0!'
                            print('WARNING: ' + SLGBP.warn)
        return matprop, scn

    # Get SLG .scn obj properties
    @staticmethod
    def getobjscn(scene):
        scn = {}
        def objscn(plyn, matn, objn, mat, tm):
            if tm:
                scn['scene.objects.{}.{}.transformation'.format(matn,objn)] = '{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}'.format(
                        ff(tm[0][0]),ff(tm[0][1]),ff(tm[0][2]),ff(tm[0][3]),ff(tm[1][0]),ff(tm[1][1]),ff(tm[1][2]),ff(tm[1][3]),
                        ff(tm[2][0]),ff(tm[2][1]),ff(tm[2][2]),ff(tm[2][3]),ff(tm[3][0]),ff(tm[3][1]),ff(tm[3][2]),ff(tm[3][3]))
            if not SLGBP.live:
                scn['scene.objects.{}.{}'.format(matn,objn)] = '{}/{}/{}.ply'.format(SLGBP.spath,SLGBP.sname,plyn)
                if scene.slg.vnormals:
                    scn['scene.objects.{}.{}.useplynormals'.format(matn,objn)] = '1'
                if scene.slg.vuvs and mat:
                    texmap = next((ts for ts in mat.texture_slots if ts and ts.use_map_color_diffuse and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filepath') and ts.use), None)
                    if texmap:
                        scn['scene.objects.{}.{}.texmap'.format(matn,objn)] = bpy.path.abspath(texmap.texture.image.filepath).replace('\\','/')
                    texbump = next((ts for ts in mat.texture_slots if ts and ts.use_map_normal and hasattr(ts.texture,'image') and hasattr(ts.texture.image,'filepath') and ts.use), None)
                    if texbump:
                        if texbump.texture.use_normal_map:
                            scn['scene.objects.{}.{}.normalmap'.format(matn,objn)] = bpy.path.abspath(texbump.texture.image.filepath).replace('\\','/')
                        else:
                            scn['scene.objects.{}.{}.bumpmap'.format(matn,objn)] = bpy.path.abspath(texbump.texture.image.filepath).replace('\\','/')
                            scn['scene.objects.{}.{}.bumpmap.scale'.format(matn,objn)] = ff(texbump.normal_factor)

        for ply in SLGBP.plys:
            plyn = ply.replace('.','_')
            slgos = SLGBP.plys[ply]
            if type(slgos) != tuple:
                # Non-instanced object meshes grouped by material
                if not SLGBP.live:
                    objscn(plyn, plyn, plyn, slgos, None)
            else:
                # Instance object
                so, dms = slgos
                for do, mat in dms:
                    matn = mat.name.replace('.','_') if mat else SLGBP.nomatn
                    if type(do) == bpy.types.ParticleSystem:
                        if do.settings.render_type == 'GROUP' and not do.settings.use_whole_group:
                            gn = next((i for i,o in enumerate(do.settings.dupli_group.objects) if o == so), 0)
                            ndgos = len(do.settings.dupli_group.objects)
                        for i,p in enumerate(do.particles):
                            if do.settings.render_type == 'GROUP' and not do.settings.use_whole_group:
                                if gn != i % ndgos:
                                    continue
                            if p.alive_state == 'ALIVE' and p.is_exist and (do.point_cache.is_baked or p.is_visible):
                                objn = do.id_data.name.replace('.','_')+'{P}'+str(i)+plyn
                                if isnan(p.rotation[0]): # Deal with Blender bug...
                                    rm = Matrix()
                                else:
                                    rm = p.rotation.to_matrix().to_4x4()
                                if do.settings.use_whole_group:
                                    tm = Matrix.Translation(p.location) * rm * so.matrix_world * Matrix.Scale(p.size,4)
                                elif do.settings.use_global_dupli:
                                    tm = Matrix.Translation(so.matrix_world.to_translation()) * Matrix.Translation(p.location) * rm * so.matrix_world.to_3x3().to_4x4() * Matrix.Scale(p.size,4)
                                else:
                                    tm = Matrix.Translation(p.location) * rm * so.matrix_world.to_3x3().to_4x4() * Matrix.Scale(p.size,4)
                                objscn(plyn, matn, objn, mat, tm)
                    else:
                        objn = do.name.replace('.','_')+plyn
                        if type(so) == bpy.types.Mesh:
                            tm = do.matrix_world
                        else:
                            if do.data == so.data:
                                # Used by both instances and dupligroups at the same time
                                tm = do.matrix_world
                            else:
                                tm = do.matrix_world * so.matrix_world
                        objscn(plyn, matn, objn, mat, tm)
        return scn

    # Export obj/mat .ply files
    @staticmethod
    def expmatply(scene):
        print('SLGBP ===> Begin export')
        # Consider all used materials
        plys = dict([(m.name, m) for m in bpy.data.materials if m.users])
        plymats = [p for p in plys]
        nomat = len(plymats)
        plymats.append(SLGBP.nomatn)
        plys[SLGBP.nomatn] = None
        curmat = nomat
        instobjs = {}
        objs = []
        rendertypes = ['MESH', 'SURFACE', 'META', 'TEXT', 'CURVE']
        inscenelayer = lambda o:scene.layers[next((i for i in range(len(o.layers)) if o.layers[i]))]
        # Get materials with force ply flag
        mfp = [m.name for m in bpy.data.materials if m.slg_forceply]

        # Add Dupli Object to ply
        def dupliobj(so, do): # source object, duplicate object
            if so in instobjs:
                if so.data.materials:
                    for i in range(len(so.data.materials)):
                        plys[(so.name+'{D}S'+str(i))][1].append((do, so.material_slots[i].material))
                else:
                    plys[(so.name+'{D}'+SLGBP.nomatn)][1].append((do, None))
            elif not so.hide_render and so.type in rendertypes:
                addo = False
                if so not in objs:
                    if scene.slg.export or mfp and any(m for m in mfp if m in so.material_slots):
                        objs.append(so)
                if inscenelayer(so):
                    addo = True
                instobjs[so] = len(plymats)
                if so.data.materials:
                    for i in range(len(so.data.materials)):
                        plyn = (so.name+'{D}S'+str(i))
                        plymats.append(plyn)
                        plys[plyn] = (so,[(do, so.material_slots[i].material)])
                        if addo:
                            plys[plyn][1].append((so, so.material_slots[i].material))
                else:
                    plyn = (so.name+'{D}'+SLGBP.nomatn)
                    plymats.append(plyn)
                    plys[plyn] = (so,[(do, None)])
                    if addo:
                        plys[plyn][1].append((so, None))

        # Objects to render
        for obj in scene.objects:
            if not obj.hide_render:
                if obj.type in rendertypes and inscenelayer(obj) and not (obj.particle_systems and not any(ps for ps in obj.particle_systems if ps.settings.use_render_emitter)):
                    # Mesh instances
                    if obj.slg_forceinst or (obj.data.users > 1 and not obj.modifiers):
                        if obj.data in instobjs:
                            if obj.data.materials:
                                for i in range(len(obj.data.materials)):
                                    plys[(obj.data.name+'S'+str(i))][1].append((obj, obj.material_slots[i].material))
                            else:
                                plys[(obj.data.name+SLGBP.nomatn)][1].append((obj, None))
                        else:
                            objs.append(obj.data)
                            instobjs[obj.data] = len(plymats)
                            if obj.data.materials:
                                for i in range(len(obj.data.materials)):
                                    plyn = (obj.data.name+'S'+str(i))
                                    plymats.append(plyn)
                                    plys[plyn] = (obj.data,[(obj, obj.material_slots[i].material)])
                            else:
                                plyn = (obj.data.name+SLGBP.nomatn)
                                plymats.append(plyn)
                                plys[plyn] = (obj.data,[(obj, None)])
                    else:
                        # Collect all non-instance meshes by material
                        if scene.slg.export or mfp and any(m for m in mfp if m in obj.material_slots):
                            objs.append(obj)
                # Dupligroups
                if obj.dupli_type == 'GROUP' and inscenelayer(obj):
                    for so in obj.dupli_group.objects:
                        dupliobj(so, obj)
                # Particle Systems
                for ps in obj.particle_systems:
                    if ps.settings.type == 'EMITTER':
                        # Dupli Object
                        if ps.settings.render_type == 'OBJECT' and ps.settings.dupli_object:
                            # Blender 2.5 Python bug!: incorrect dupli_object returned when multiple particle systems
                            dupliobj(ps.settings.dupli_object, ps)
                        # Dupli Group
                        if ps.settings.render_type == 'GROUP' and ps.settings.dupli_group:
                            for o in ps.settings.dupli_group.objects:
                                dupliobj(o, ps)

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
        if scene.slg.export or mfp:
            # Delete existing ply files
            if scene.slg.export and os.path.exists(SLGBP.sfullpath):
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
                    if type(obj) == bpy.types.Object and obj not in instobjs:
                        print("SLGBP        Xform render mesh: {}".format(obj.name))
                        mesh.transform(obj.matrix_world)
                    # Make copy of verts for fast direct index access (mesh.vertices was very slow)
                    if scene.slg.vnormals:
                        v = [vert.co[:] + vert.normal[:] for vert in mesh.vertices]
                    else:
                        v = [vert.co[:] for vert in mesh.vertices]
                    vcd = []
                    if scene.slg.vcolors and mesh.vertex_colors.active:
                        vcd = mesh.vertex_colors.active.data
                    uvd = []
                    if scene.slg.vuvs and mesh.uv_textures.active:
                        uvd = mesh.uv_textures.active.data
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
                        if not face.use_smooth or uv:
                            faces[curmat].append((vertnum[curmat], vertnum[curmat]+1, vertnum[curmat]+2))
                            if len(face.vertices) == 4:
                                faces[curmat].append((vertnum[curmat], vertnum[curmat]+2, vertnum[curmat]+3))
                        for j, vert in enumerate(face.vertices):
                            if scene.slg.vcolors:
                                if vc:
                                    color[0] = int(255.0*colors[j][0])
                                    color[1] = int(255.0*colors[j][1])
                                    color[2] = int(255.0*colors[j][2])
                                else:
                                    color[0] = color[1] = color[2] = 255
                            if scene.slg.vuvs:
                                if uv:
                                    uvco[0] = uvcos[j][0]
                                    uvco[1] = 1.0 - uvcos[j][1]
                                else:
                                    uvco[0] = uvco[1] = 0
                            if face.use_smooth and not uv:
                                if (curmat,vert) in sharedverts:
                                    addv = False
                                else:
                                    sharedverts[curmat,vert]=vertnum[curmat]
                                    addv = True
                            else:
                                addv = True
                            if addv:
                                if scene.slg.vnormals and not face.use_smooth:
                                    verts[curmat].append(v[vert][:3]+face.normal[:])
                                else:
                                    verts[curmat].append(v[vert])
                                if scene.slg.vuvs:
                                    vert_uvs[curmat].append(tuple(uvco))
                                if scene.slg.vcolors:
                                    vert_vcs[curmat].append(tuple(color))
                                vertnum[curmat] += 1
                        if face.use_smooth and not uv:
                            faces[curmat].append((sharedverts[curmat,face.vertices[0]], sharedverts[curmat,face.vertices[1]], sharedverts[curmat,face.vertices[2]]))
                            if len(face.vertices) == 4:
                                faces[curmat].append((sharedverts[curmat,face.vertices[0]], sharedverts[curmat,face.vertices[2]], sharedverts[curmat,face.vertices[3]]))
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
            if type(plys[pm]) == bpy.types.Material and plys[pm].use_shadeless:
                del plys[pm]
            # If have mesh using this plymat...
            if verts[i] or (not scene.slg.export and os.path.exists('{}/{}.ply'.format(SLGBP.sfullpath,plyn))):
                if scene.slg.export or pm in mfp:
                    # Write out PLY
                    print("SLGBP        writing PLY: {}".format(plyn))
                    fply = open('{}/{}.ply'.format(SLGBP.sfullpath,plyn), 'wb')
                    fply.write(b'ply\n')
                    #fply.write(b'format ascii 1.0\n')
                    fply.write(b'format binary_little_endian 1.0\n')
                    fply.write(str.encode('comment Created by SmallLuxGPU exporter for Blender 2.5, source file: {}\n'.format((bpy.data.filepath.split('/')[-1].split('\\')[-1]))))
                    fply.write(str.encode('element vertex {}\n'.format(vertnum[i])))
                    fply.write(b'property float x\n')
                    fply.write(b'property float y\n')
                    fply.write(b'property float z\n')
                    if scene.slg.vnormals:
                        fply.write(b'property float nx\n')
                        fply.write(b'property float ny\n')
                        fply.write(b'property float nz\n')
                    if scene.slg.vuvs:
                        fply.write(b'property float s\n')
                        fply.write(b'property float t\n')
                    if scene.slg.vcolors and mvc[i]:
                        fply.write(b'property uchar red\n')
                        fply.write(b'property uchar green\n')
                        fply.write(b'property uchar blue\n')
                    fply.write(str.encode('element face {}\n'.format(len(faces[i]))))
                    fply.write(b'property list uchar uint vertex_indices\n')
                    fply.write(b'end_header\n')
                    # Write out vertices
                    for j, v in enumerate(verts[i]):
                        if scene.slg.vnormals:
                            #fply.write(str.encode('{:.6f} {:.6f} {:.6f} {:.6g} {:.6g} {:.6g}'.format(*v)))
                            fply.write(pack('<6f', *v))
                        else:
                            #fply.write(str.encode('{:.6f} {:.6f} {:.6f}'.format(*v)))
                            fply.write(pack('<3f', *v))
                        if scene.slg.vuvs:
                            #fply.write(str.encode(' {:.6g} {:.6g}'.format(*vert_uvs[i][j])))
                            fply.write(pack('<2f', *vert_uvs[i][j]))
                        if scene.slg.vcolors and mvc[i]:
                            #fply.write(str.encode(' {} {} {}'.format(*vert_vcs[i][j])))
                            fply.write(pack('<3B', *vert_vcs[i][j]))
                        #fply.write(b'\n')
                    # Write out faces
                    for f in faces[i]:
                        #fply.write(str.encode('3 {} {} {}\n'.format(*f)))
                        fply.write(pack('<B3I', 3, *f))
                    fply.close()
            elif pm in plys:
                del plys[pm]
            if SLGBP.abort:
                return
        SLGBP.plys = plys
        SLGBP.instobjs = instobjs

    # Export SLG scene
    @staticmethod
    def export(scene):
        with SLGBP.lock:
            SLGBP.cfg = SLGBP.getcfg(scene)
            fcfg = open('{}/{}/render.cfg'.format(SLGBP.spath,SLGBP.sname), 'w')
            for k in sorted(SLGBP.cfg):
                fcfg.write(k + ' = ' + SLGBP.cfg[k] + '\n')
            fcfg.close()
            SLGBP.expmatply(scene)
            SLGBP.scn = SLGBP.getscn(scene)
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
    @staticmethod
    def runslg(scene):
        if scene.slg.enabletelnet == True:
            print('SLGBP ===> launch SLG: {} -T {}/{}/render.cfg'.format(SLGBP.slgpath,SLGBP.spath,SLGBP.sname))
            SLGBP.slgproc = Popen([SLGBP.slgpath,'-T','{}/{}/render.cfg'.format(SLGBP.spath,SLGBP.sname)], cwd=SLGBP.spath, shell=False)
        else:
            print('SLGBP ===> launch SLG: {} {}/{}/render.cfg'.format(SLGBP.slgpath,SLGBP.spath,SLGBP.sname))
            SLGBP.slgproc = Popen([SLGBP.slgpath,'{}/{}/render.cfg'.format(SLGBP.spath,SLGBP.sname)], cwd=SLGBP.spath, shell=False)

    # Export SLG scene and render it
    @staticmethod
    def exportrun(scene, anim, errout):
        sleep(0.25) # Allow time for screen to display last msg
        while True:
            if not SLGBP.init(scene, errout):
                return
            SLGBP.msg = 'SLG exporting frame: ' + str(scene.frame_current) + " (ESC to abort)"
            SLGBP.msgrefresh()
            SLGBP.export(scene)
            if not SLGBP.abort:
                SLGBP.msg = 'SLG rendering frame: ' + str(scene.frame_current) + " (ESC to abort)"
                SLGBP.msgrefresh()
                SLGBP.runslg(scene)
            if not anim:
                break
            SLGBP.slgproc.wait()
            if SLGBP.abort or scene.frame_current+scene.frame_step > scene.frame_end:
                SLGBP.msg = 'SLG animation render done.'
                SLGBP.msgrefresh()
                break
            scene.frame_set(scene.frame_current+scene.frame_step)

    # Update SLG parameters via telnet interface
    @staticmethod
    def liveconnect(scene, errout):
        SLGBP.msg = 'Connecting via telnet to SLG... (SLG Live! Mode)'
        SLGBP.msgrefresh()
        SLGBP.telnet = SLGTelnet()
        if SLGBP.telnet.connected:
            SLGBP.live = True
            SLGBP.msg = 'SLG Live! (ESC to exit)'
            SLGBP.msgrefresh()
            SLGBP.telnetecho = scene.slg.telnetecho
            if not SLGBP.telnetecho:
                SLGBP.telnet.send('echocmd.off')
        else:
            SLGBP.msg = 'Unable to connect to SLG via telnet... (SLG Live! Mode)'
            SLGBP.msgrefresh()
            errout("Can't connect.  SLG must be running with telnet interface enabled")

    @staticmethod
    def livetrigger(scene, liveact):
        SLGBP.liveact = SLGBP.liveact|liveact
        if SLGBP.thread == None or not SLGBP.thread.is_alive():
            SLGBP.thread = Thread(target=SLGBP.liveupdate,args=[scene])
            SLGBP.thread.start()

    @staticmethod
    def liveupdate(scene):
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
                if SLGBP.telnetecho != scene.slg.telnetecho:
                    SLGBP.telnetecho = scene.slg.telnetecho
                    SLGBP.telnet.send('echocmd.' + 'on' if SLGBP.telnetecho else 'off')
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
                with SLGBP.lock: 
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
            if act & SLGBP.LIVEOBJ > 0 and scene.slg.accelerator_type == '3':
                scn = SLGBP.getobjscn(scene)
                for k in scn:
                    if cmpupd(k, scn, SLGBP.scn, not wasreset, False):
                        wasreset = True
            if wasreset:
                SLGBP.telnet.send('render.start')

            sleep(0.5) # param?
            if SLGBP.liveact == 0: break

    # SLG live animation render
    @staticmethod
    def liveanimrender(scene):
        SLGBP.liveanim = True
        scene.frame_set(scene.frame_start-scene.frame_step)
        SLGBP.livetrigger(scene, SLGBP.LIVEALL)
        if scene.slg.cameramotionblur:
            # Make sure first livetrigger takes place
            sleep(0.25)
        while True:
            scene.frame_set(scene.frame_current+scene.frame_step)
            SLGBP.msg = 'SLG Live! rendering animation frame: ' + str(scene.frame_current) + " (ESC to abort)"
            SLGBP.livetrigger(scene, SLGBP.LIVEALL)
            sleep(scene.slg.batchmodetime)
            SLGBP.telnet.send('image.save')
            if not SLGBP.live or scene.frame_current+scene.frame_step > scene.frame_end: break
        SLGBP.msg = 'SLG Live! (ESC to exit)'
        SLGBP.msgrefresh()
        SLGBP.liveanim = False

def pre_draw_callback():
    # Prevent simultaneous SLGBP export and View3D update (object matrices sometimes modified during draw, e.g. dupligroups)
    SLGBP.lock.acquire()

# Output informational message to main viewport
def info_callback(context):
    blf.position(0, 75, 30, 0)
    blf.size(0, 14, 72)
    blf.draw(0, SLGBP.msg)
    if SLGBP.lock.locked():
        SLGBP.lock.release()
    if SLGBP.live:
        if SLGBP.slgproc.poll() != None:
            SLGBP.live = False
            SLGBP.abort = True
            return
        if not SLGBP.liveanim:
            # If frame is unchanged check only base SCN and OBJ
            if SLGBP.curframe == context.scene.frame_current:
                SLGBP.livetrigger(context.scene, SLGBP.LIVESCN|SLGBP.LIVEOBJ)
            # Else could be frame skip / playback where anything can change
            else:
                SLGBP.curframe = context.scene.frame_current
                SLGBP.livetrigger(context.scene, SLGBP.LIVEALL)

# SLG Render (without Blender render engine) operator
class SLGRender(bpy.types.Operator):
    bl_idname = "render.slg_render"
    bl_label = "SLG Render"
    bl_description = "SLG export and render without using Blender's render engine"

    animation = bpy.props.BoolProperty(name="Animation", description="Render Animation", default= False)

    def _error(self, msg):
        self._iserror = True
        self.report('ERROR', "SLGBP: " + msg)

    def _reset(self, context):
        context.region.callback_remove(self._pdcb)
        context.region.callback_remove(self._icb)
        context.area.tag_redraw()
        if self.properties.animation:
            SLGBP.thread = None
            if SLGBP.slgproc:
                if SLGBP.slgproc.poll() == None:
                    SLGBP.slgproc.terminate()

    def modal(self, context, event):
        if SLGBP.thread:
            if SLGBP.thread.is_alive():
                if event.type != 'ESC':
                    return {'PASS_THROUGH'}
                SLGBP.abort = True
            self._reset(context)
            if self._iserror:
                return {'CANCELLED'}
            elif SLGBP.abort:
                self.report('WARNING', "SLG export aborted.")
                return {'CANCELLED'}
            else:
                self.report('INFO', "SLG export done.")
                return {'FINISHED'}
        return {'PASS_THROUGH'}

    def invoke(self, context, event):
        if self.properties.animation and not context.scene.slg.enablebatchmode:
            self.report('ERROR', "SLGBP: Enable batch mode for animations")
            return {'CANCELLED'}
        if SLGBP.live:
            self.report('ERROR', "SLGBP: Can't export during SLG Live! mode")
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
            context.scene.frame_set(context.scene.frame_start)
        elif not SLGBP.init(context.scene, self._error):
            return {'CANCELLED'}
        self._iserror = False
        SLGBP.abort = False
        SLGBP.msg = 'SLG exporting and rendering... (ESC to abort)'
        SLGBP.msgrefresh = context.area.tag_redraw
        context.window_manager.modal_handler_add(self)
        self._icb = context.region.callback_add(info_callback, (context,), 'POST_PIXEL')
        self._pdcb = context.region.callback_add(pre_draw_callback, (), 'PRE_VIEW')
        SLGBP.msgrefresh()
        SLGBP.thread = Thread(target=SLGBP.exportrun,args=[context.scene, self.properties.animation, self._error])
        SLGBP.thread.start()
        return {'RUNNING_MODAL'}

# SLG Live operator
class SLGLive(bpy.types.Operator):
    bl_idname = "render.slg_live"
    bl_label = "SLG Live"
    bl_description = "SLG Live! mode"

    def _error(self, msg):
        self._iserror = True
        self.report('ERROR', "SLGBP: " + msg)

    def modal(self, context, event):
        if self._iserror or SLGBP.abort or event.type == 'ESC':
            SLGBP.live = False
            SLGBP.telnet.close()
            context.region.callback_remove(self._pdcb)
            context.region.callback_remove(self._icb)
            context.area.tag_redraw()
            bpy.context.user_preferences.edit.use_global_undo = self._undo
            return {'FINISHED'}
        return {'PASS_THROUGH'}

    @classmethod
    def poll(cls, context):
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
        context.window_manager.modal_handler_add(self)
        # Disable global undo as it corrupts all pointers used by SLG Live! mode
        self._undo = bpy.context.user_preferences.edit.use_global_undo
        bpy.context.user_preferences.edit.use_global_undo = False
        SLGBP.abort = False
        SLGBP.msg = 'SLG Live!'
        SLGBP.msgrefresh = context.area.tag_redraw
        SLGBP.curframe = context.scene.frame_current
        self._icb = context.region.callback_add(info_callback, (context,), 'POST_PIXEL')
        self._pdcb = context.region.callback_add(pre_draw_callback, (context,), 'PRE_VIEW')
        SLGBP.thread = Thread(target=SLGBP.liveconnect,args=[context.scene, self._error])
        SLGBP.thread.start()
        return {'RUNNING_MODAL'}

# SLG Live update all operator
class SLGLiveUpdate(bpy.types.Operator):
    bl_idname = "render.slg_liveupd"
    bl_label = "SLG Live Update All"
    bl_description = "SLG Live! mode: update all"

    @classmethod
    def poll(cls, context):
        return SLGBP.live

    # def execute(self, context):
    def invoke(self, context, event):
        SLGBP.livetrigger(context.scene, SLGBP.LIVEALL)
        return {'FINISHED'}

# SLG Live animation render operator
class SLGLiveAnim(bpy.types.Operator):
    bl_idname = "render.slg_liveanim"
    bl_label = "SLG Live Animation Render"
    bl_description = "SLG Live! mode: render animation"

    @classmethod
    def poll(cls, context):
        return SLGBP.live

    # def execute(self, context):
    def invoke(self, context, event):
        Thread(target=SLGBP.liveanimrender,args=[context.scene]).start()
        return {'FINISHED'}

class SmallLuxGPURender(bpy.types.RenderEngine):
    bl_idname = 'SLG_RENDER'
    bl_label = "SmallLuxGPU"

    def _error(self, error):
        self.update_stats("-=|ERROR|", error)
        # Hold the render results window while we display the error message
        while True:
            if self.test_break():
                break
            sleep(0.1)

    def render(self, scene):

        if SLGBP.live:
            self._error("SLGBP: Can't render during SLG Live! mode")
            return
        if SLGBP.thread:
            if SLGBP.thread.is_alive():
                self._error("SLGBP is Busy")
                return
        self.update_stats("", "SmallLuxGPU: Exporting scene, please wait...")
        if not SLGBP.init(scene, self._error):
            return
        # Force an update to object matrices when rendering animations
        scene.update()
        SLGBP.export(scene)
        SLGBP.runslg(scene)

        if scene.slg.waitrender:
            # Wait for SLG, load image
            if scene.slg.enablebatchmode:
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
                sleep(0.1)

class SLGSettings(bpy.types.PropertyGroup):
    pass

def slg_add_properties():
    # Add SmallLuxGPU properties to scene
    from bpy.props import PointerProperty, StringProperty, BoolProperty, EnumProperty, IntProperty, FloatProperty, FloatVectorProperty, CollectionProperty
    bpy.types.Scene.slg = PointerProperty(type=SLGSettings, name="SLG", description="SLG Settings")

    SLGSettings.slgpath = StringProperty(name="SmallLuxGPU Path",
        description="Full path to SmallLuxGPU's executable",
        default="", maxlen=1024, subtype="FILE_PATH")

    SLGSettings.scene_path = StringProperty(name="Export scene path",
        description="Full path to directory where the exported scene is created",
        default="", maxlen=1024, subtype="DIR_PATH")

    SLGSettings.scenename = StringProperty(name="Scene Name",
        description="Name of SmallLuxGPU scene to create",
        default="testscene", maxlen=1024)

    SLGSettings.export = BoolProperty(name="PLY",
        description="Export PLY (mesh data) files (uncheck only if scene has already been exported)",
        default=True)

    SLGSettings.vuvs = BoolProperty(name="UVs",
        description="Export optional vertex uv information (only if assigned)",
        default=True)

    SLGSettings.vcolors = BoolProperty(name="VCs",
        description="Export optional vertex color information (only if assigned)",
        default=False)

    SLGSettings.vnormals = BoolProperty(name="VNs",
        description="Export optional vertex normal information",
        default=True)

    SLGSettings.infinitelightbf = BoolProperty(name="InfiniteLight BF",
        description="Enable brute force rendering for InifinteLight light source",
        default=False)

    SLGSettings.cameramotionblur = BoolProperty(name="Camera Motion Blur",
        description="Enable camera motion blur",
        default=False)

    SLGSettings.low_latency = BoolProperty(name="Low Latency",
        description="In low latency mode render is more interactive, otherwise render is faster",
        default=True)

    SLGSettings.refreshrate = IntProperty(name="Screen Refresh Interval",
        description="How often, in milliseconds, the screen refreshes",
        default=250, min=1, soft_min=1)

    SLGSettings.native_threads = IntProperty(name="Native Threads",
        description="Number of native CPU threads",
        default=2, min=0, max=1024, soft_min=0, soft_max=1024)

    SLGSettings.devices_threads = IntProperty(name="OpenCL Threads",
        description="Number of OpenCL devices threads",
        default=2, min=0, max=1024, soft_min=0, soft_max=1024)

    SLGSettings.rendering_type = EnumProperty(name="Rendering Type",
        description="Select the desired rendering type",
        items=(("0", "Path", "Path tracing"),
               ("1", "SPPM", "Stochastic Progressive Photon Mapping"),
               ("2", "Direct", "Direct lighting only"),
               ("3", "PathGPU", "Path tracing using GPU only")),
        default="0")

    SLGSettings.sppmlookuptype = EnumProperty(name="",
        description="SPPM Lookup Type (Hybrid generally best)",
        items=(("-1", "SPPM Lookup Type", "SPPM Lookup Type"),
               ("0", "Hash Grid", "Hash Grid"),
               ("1", "KD tree", "KD tree"),
               ("2", "Hybrid Hash Grid", "Hybrid Hash Grid")),
        default="2")

    SLGSettings.sppm_eyepath_maxdepth = IntProperty(name="Eye depth",
        description="SPPM eye path max depth",
        default=16, min=1, soft_min=1)

    SLGSettings.sppm_photon_alpha = FloatProperty(name="P.Alpha",
        description="SPPM Photon Alpha: how fast the 'area of interest' around each eye hit point is reduced at each pass",
        default=0.7, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.sppm_photon_radiusscale = FloatProperty(name="P.Radius",
        description="SPPM Photon Radius: how to scale the initial 'area of interest' around each eye hit point",
        default=1.0, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.sppm_photon_per_pass = IntProperty(name="Photons",
        description="Number of (SPPM) photons to shot for each pass",
        default=2500000, min=100, soft_min=100)

    SLGSettings.sppm_photon_maxdepth = IntProperty(name="P.Depth",
        description="Max depth for (SPPM) photons",
        default=8, min=1, soft_min=1)

    SLGSettings.sppmdirectlight = BoolProperty(name="Direct Light Sampling",
        description="SPPM Direct Light Sampling",
        default=False)

    SLGSettings.accelerator_type = EnumProperty(name="Accelerator Type",
        description="Select the desired ray tracing accelerator type",
        items=(("-1", "Default", "Default"),
               ("0", "BVH", "Bounding Volume Hierarchy"),
               ("1", "QBVH", "Quad-Bounding Volume Hierarchy"),
               ("2", "QBVH (image storage disabled)", "Quad-Bounding Volume Hierarchy with disabled image storage"),
               ("3", "MQBVH (instances support)", "Multilevel Quad-Bounding Volume Hierarchy")),
        default="-1")

    SLGSettings.film_filter_type = EnumProperty(name="Film Filter Type",
        description="Select the desired film filter type",
        items=(("0", "None", "No filter"),
               ("1", "Gaussian", "Gaussian filter")),
        default="1")

    SLGSettings.film_tonemap_type = EnumProperty(
        description="Select the desired film tonemap type",
        items=(("-1", "Tonemapping", "Tonemapping"),
               ("0", "Linear tonemapping", "Linear tonemapping"),
               ("1", "Reinhard '02 tonemapping", "Reinhard '02 tonemapping")),
        default="0")

    SLGSettings.linear_scale = FloatProperty(name="scale",
        description="Linear tonemapping scale",
        default=1.0, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.reinhard_prescale = FloatProperty(name="Pre-scale",
        description="Reinhard '02 tonemapping pre-scale",
        default=1.0, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.reinhard_postscale = FloatProperty(name="Post-scale",
        description="Reinhard '02 tonemapping post-scale",
        default=1.2, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.reinhard_burn = FloatProperty(name="Burn",
        description="Reinhard '02 tonemapping burn",
        default=3.75, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.film_gamma = FloatProperty(name="Gamma",
        description="Gamma correction on screen and for saving non-HDR file format",
        default=2.2, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    SLGSettings.lightstrategy = EnumProperty(name="Light Strategy",
        description="Select the desired light strategy",
        items=(("0", "ONE_UNIFORM", "ONE_UNIFORM"),
               ("1", "ALL_UNIFORM", "ALL_UNIFORM")),
        default="0")

    SLGSettings.sampleperpixel = EnumProperty(name="Sample per pixel",
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

    SLGSettings.imageformat = EnumProperty(name="Image File Format",
        description="Image file save format, saved with scene files (also Blender intermediary format)",
        items=(("png", "PNG", "PNG"),
               ("exr", "OpenEXR", "OpenEXR"),
               ("jpg", "JPG", "JPG")), # A lot more formats supported...
        default="png")

    SLGSettings.tracedepth = IntProperty(name="Max Path Trace Depth",
        description="Maximum path tracing depth",
        default=3, min=1, max=1024, soft_min=1, soft_max=1024)

    SLGSettings.shadowrays = IntProperty(name="Shadow Rays",
        description="Shadow rays",
        default=1, min=1, max=1024, soft_min=1, soft_max=1024)

    SLGSettings.rrstrategy = EnumProperty(name="Russian Roulette Strategy",
        description="Select the desired russian roulette strategy",
        items=(("0", "Probability", "Probability"),
               ("1", "Importance", "Importance")),
        default="1")

    SLGSettings.rrdepth = IntProperty(name="Russian Roulette Depth",
        description="Russian roulette depth",
        default=5, min=1, max=1024, soft_min=1, soft_max=1024)

    SLGSettings.rrprob = FloatProperty(name="Russian Roulette Probability",
        description="Russian roulette probability",
        default=0.75, min=0, max=1, soft_min=0, soft_max=1, precision=3)

    SLGSettings.rrcap = FloatProperty(name="Russian Roulette Importance Cap",
        description="Russian roulette importance cap",
        default=0.25, min=0.01, max=0.99, soft_min=0.1, soft_max=0.9, precision=3)

    # Participating Media properties
    SLGSettings.enablepartmedia = BoolProperty(name="Participating Media",
        description="Use single scattering participating media",
        default=False)
    SLGSettings.partmedia_stepsize = FloatProperty(name="Step Size",
        description="Set ray marching step size",
        default=2.0, min=0.01, max=100, soft_min=0.1, soft_max=10, precision=2)
    SLGSettings.partmedia_rrprob = FloatProperty(name="RR prob.",
        description="Russian Roulette probability",
        default=0.33, min=0.01, max=1.0, soft_min=0.1, soft_max=0.9, precision=2)
    SLGSettings.partmedia_emission = FloatVectorProperty(name="Emission",
        description="Media emission",
        default=(0.05, 0.05, 0.05), min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, subtype="COLOR")
    SLGSettings.partmedia_scattering = FloatVectorProperty(name="Scattering",
        description="Media scattering",
        default=(0.05, 0.05, 0.05), min=0.0, max=100.0, soft_min=0.0, soft_max=100.0, subtype="COLOR")
    SLGSettings.partmedia_bbox1 = FloatVectorProperty(name="Bounding Box Min",
        description="Media bounding box min",
        default=(-10.0, -10.0, -10.0), subtype="XYZ", size=3)
    SLGSettings.partmedia_bbox2 = FloatVectorProperty(name="Bounding Box Max",
        description="Media bounding box max",
        default=(10.0, 10.0, 10.0), subtype="XYZ", size=3)

    SLGSettings.enablebatchmode = BoolProperty(name="Batch Mode",
        description="Render in background (required for animations)",
        default=False)

    SLGSettings.batchmodetime = IntProperty(name="Batch mode max run time",
        description="Max number of seconds to render; 0 = ignore",
        default=120, min=0, soft_min=0)

    SLGSettings.batchmodespp = IntProperty(name="Batch mode max samples per pixel",
        description="Max number of samples per pixels in batch mode; 0 = ignore",
        default=128, min=0, soft_min=0)

    SLGSettings.batchmode_periodicsave = IntProperty(name="Periodic image save",
        description="Save image periodically (in seconds); 0 = ignore",
        default=0, min=0, soft_min=0)

    SLGSettings.waitrender = BoolProperty(name="Wait",
        description="Wait for render to finish; load image into render results (required for animations)",
        default=False)

    SLGSettings.enabletelnet = BoolProperty(name="Telnet",
        description="Enable SLG's telnet interface to allow use of SLG Live! mode",
        default=False)

    SLGSettings.telnetecho = BoolProperty(name="Echo",
        description="Enable SLG's telnet echo of commands and informational messages (helps problem solve)",
        default=False)

    SLGSettings.opencl_cpu = BoolProperty(name="CPU",
        description="Use OpenCL CPU devices if available",
        default=False)

    SLGSettings.opencl_gpu = BoolProperty(name="GPU",
        description="Use OpenCL GPU devices if available",
        default=True)

    SLGSettings.gpu_workgroup_size = IntProperty(name="GPU Workgroup Size",
        description="Use a value of 0 to use the default value for your GPU",
        default=64, min=0, max=4096, soft_min=0, soft_max=4096)

    SLGSettings.platform = IntProperty(name="OpenCL platform",
        description="OpenCL Platform to use; if you have multiple OpenCL ICDs installed",
        default=0, min=0, max=256, soft_min=0, soft_max=256)

    SLGSettings.devices = StringProperty(name="OpenCL devices to use",
        description="blank = default (bitwise on/off value for each device, see SLG docs)",
        default="", maxlen=64)

    # Add SLG Camera Lens Radius
    bpy.types.Camera.slg_lensradius = FloatProperty(name="SLG DOF Lens Radius",
        description="SmallLuxGPU camera lens radius for depth of field",
        default=0.015, min=0, max=10, soft_min=0, soft_max=10, precision=3)

    # Add Material PLY export override
    bpy.types.Material.slg_forceply = BoolProperty(name="SLG Force PLY Export",
        description="SmallLuxGPU - Force export of PLY (mesh data) related to this material",
        default=False)

    # Add Objet Force Instance
    bpy.types.Object.slg_forceinst = BoolProperty(name="SLG Force Instance",
        description="SmallLuxGPU - Force export of instance for this object",
        default=False)

    # Use some of the existing panels
    import properties_render
    properties_render.RENDER_PT_render.COMPAT_ENGINES.add('SLG_RENDER')
    properties_render.RENDER_PT_layers.COMPAT_ENGINES.add('SLG_RENDER')
    properties_render.RENDER_PT_dimensions.COMPAT_ENGINES.add('SLG_RENDER')
    properties_render.RENDER_PT_output.COMPAT_ENGINES.add('SLG_RENDER')
    properties_render.RENDER_PT_post_processing.COMPAT_ENGINES.add('SLG_RENDER')
    del properties_render

    import properties_material
    properties_material.MATERIAL_PT_context_material.COMPAT_ENGINES.add('SLG_RENDER')
    properties_material.MATERIAL_PT_diffuse.COMPAT_ENGINES.add('SLG_RENDER')
    properties_material.MATERIAL_PT_shading.COMPAT_ENGINES.add('SLG_RENDER')
    properties_material.MATERIAL_PT_pipeline.COMPAT_ENGINES.add('SLG_RENDER')
    properties_material.MATERIAL_PT_transp.COMPAT_ENGINES.add('SLG_RENDER')
    properties_material.MATERIAL_PT_mirror.COMPAT_ENGINES.add('SLG_RENDER')
    del properties_material

    import properties_texture
    for member in dir(properties_texture):
        subclass = getattr(properties_texture, member)
        try:
            subclass.COMPAT_ENGINES.add('SLG_RENDER')
        except:
            pass
    del properties_texture

    import properties_data_camera
    for member in dir(properties_data_camera):
        subclass = getattr(properties_data_camera, member)
        try:
            subclass.COMPAT_ENGINES.add('SLG_RENDER')
        except:
            pass
    del properties_data_camera

    import properties_world
    properties_world.WORLD_PT_environment_lighting.COMPAT_ENGINES.add('SLG_RENDER')
    del properties_world

    import properties_data_lamp
    properties_data_lamp.DATA_PT_sunsky.COMPAT_ENGINES.add('SLG_RENDER')
    del properties_data_lamp

    import properties_particle
    for member in dir(properties_particle):
        subclass = getattr(properties_particle, member)
        try:
            subclass.COMPAT_ENGINES.add('SLG_RENDER')
        except:
            pass
    del properties_particle

# Add SLG Camera Lens Radius on Camera panel
def slg_lensradius(self, context):
    if context.scene.render.engine == 'SLG_RENDER':
        self.layout.split().column().prop(context.camera, "slg_lensradius", text="SLG Lens Radius")
        if SLGBP.live:
            SLGBP.livetrigger(context.scene, SLGBP.LIVESCN)

# Add Material PLY export override on Material panel
def slg_forceply(self, context):
    if context.scene.render.engine == 'SLG_RENDER':
        self.layout.split().column().prop(context.material, "slg_forceply")
        if SLGBP.live:
            SLGBP.livemat = context.material
            SLGBP.livetrigger(context.scene, SLGBP.LIVEMTL)

# Add Object Force Instance on Object panel
def slg_forceinst(self, context):
    if context.scene.render.engine == 'SLG_RENDER':
        self.layout.split().column().prop(context.object, "slg_forceinst")

def slg_livescn(self, context):
    if context.scene.render.engine == 'SLG_RENDER':
        if SLGBP.live:
            SLGBP.livetrigger(context.scene, SLGBP.LIVESCN)

# Add SLG Operators to View3D toolbar
def slg_operators(self, context):
    if context.scene.render.engine == 'SLG_RENDER':
        row = self.layout.row(align=True)
        if SLGBP.live:
            row.operator("render.slg_liveupd", text="", icon='ANIM')
            row.operator("render.slg_liveanim", text="", icon='RENDER_ANIMATION')
        else:
            row.operator("render.slg_render", text="", icon='RENDER_STILL')
            row.operator("render.slg_render", text="", icon='RENDER_ANIMATION').animation = True
        row.operator("render.slg_live", text="", icon='RENDER_RESULT')

# Add (non-render engine) Render to Menu
def slg_rendermenu(self, context):
    if context.scene.render.engine == 'SLG_RENDER':
        self.layout.separator()
        if SLGBP.live:
            self.layout.operator("render.slg_liveupd", text="SLG Live! Update All", icon='ANIM')
            self.layout.operator("render.slg_liveanim", text="SLG Live! Animation Render", icon='RENDER_ANIMATION')
        else:
            self.layout.operator("render.slg_render", text="SmallLuxGPU Export and Render Scene", icon='RENDER_STILL')
            self.layout.operator("render.slg_render", text="SmallLuxGPU Export and Render Animation", icon='RENDER_ANIMATION').animation = True

class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"

    @classmethod
    def poll(cls, context):
        return context.scene.render.engine in cls.COMPAT_ENGINES

class RENDER_PT_slg_settings(bpy.types.Panel, RenderButtonsPanel):
    bl_label = "SmallLuxGPU Settings"
    COMPAT_ENGINES = {'SLG_RENDER'}

    def draw(self, context):
        layout = self.layout
        slg = context.scene.slg

        split = layout.split()
        col = split.column()
        col.label(text="Full path to SmallLuxGPU's executable:")
        col.prop(slg, "slgpath", text="")
        col.label(text="Full path where the scene is exported:")
        col.prop(slg, "scene_path", text="")
        col.prop(slg, "scenename", text="Scene Name")
        split = layout.split(percentage=0.25)
        col = split.column()
        col.prop(slg, "export")
        col = split.column()
        col.active = slg.export
        col.prop(slg, "vuvs")
        col = split.column()
        col.active = slg.export
        col.prop(slg, "vcolors")
        col = split.column()
        col.active = slg.export
        col.prop(slg, "vnormals")
        split = layout.split()
        col = split.column()
        col.prop(slg, "infinitelightbf")
        col = split.column()
        col.prop(slg, "cameramotionblur")
        split = layout.split()
        col = split.column()
        col.prop(slg, "rendering_type")
        split = layout.split()
        col = split.column()
        col.prop(slg, "accelerator_type")
        split = layout.split()
        col = split.column()
        col.prop(slg, "film_filter_type")
        split = layout.split()
        col = split.column()
        col.prop(slg, "film_tonemap_type")
        col = split.column()
        if slg.film_tonemap_type == '0':
            col.prop(slg, "linear_scale")
        else:
            col.prop(slg, "reinhard_burn")
            split = layout.split()
            col = split.column()
            col.prop(slg, "reinhard_prescale")
            col = split.column()
            col.prop(slg, "reinhard_postscale")
        split = layout.split()
        col = split.column()
        col.prop(slg, "imageformat")
        col = split.column()
        col.prop(slg, "film_gamma")
        if slg.rendering_type == '1':
            split = layout.split()
            col = split.column()
            col.prop(slg, "sppmdirectlight")
            col = split.column()
            col.prop(slg, "sppmlookuptype")
            split = layout.split()
            col = split.column()
            col.prop(slg, "sppm_photon_alpha")
            col = split.column()
            col.prop(slg, "sppm_photon_radiusscale")
            split = layout.split()
            col = split.column()
            col.prop(slg, "sppm_eyepath_maxdepth")
            col = split.column()
            col.prop(slg, "sppm_photon_maxdepth")
            split = layout.split()
            col = split.column()
            col.prop(slg, "sppm_photon_per_pass", text="Photon count per pass")
        else:
            split = layout.split()
            col = split.column()
            col.prop(slg, "lightstrategy")
            split = layout.split()
            col = split.column()
            col.prop(slg, "tracedepth", text="Depth")
            col = split.column()
            col.prop(slg, "shadowrays", text="Shadow")
            split = layout.split()
            col = split.column()
            col.prop(slg, "sampleperpixel")
            split = layout.split()
            col = split.column()
            col.prop(slg, "rrstrategy")
            split = layout.split()
            col = split.column()
            col.prop(slg, "rrdepth", text="RR Depth")
            col = split.column()
            if slg.rrstrategy == "0":
                col.prop(slg, "rrprob", text="RR Prob")
            else:
                col.prop(slg, "rrcap", text="RR Cap")
            split = layout.split()
            col = split.column()
            col.prop(slg, "enablepartmedia")
            if slg.enablepartmedia:
                split = layout.split()
                col = split.column()
                col.prop(slg, "partmedia_stepsize", text="Step Size")
                col = split.column()
                col.prop(slg, "partmedia_rrprob", text="RR Prob.")
                split = layout.split()
                col = split.column(0)
                col.prop(slg, "partmedia_emission", text="Emission")
                col = split.column(1)
                col.prop(slg, "partmedia_scattering", text="Scattering")
                split = layout.split(percentage=0.5)
                col = split.column()
                col.prop(slg, "partmedia_bbox1")
                col = split.column()
                col.prop(slg, "partmedia_bbox2")
        split = layout.split()
        col = split.column()
        col.prop(slg, "low_latency")
        col = split.column()
        col.prop(slg, "refreshrate", text="Refresh")
        split = layout.split()
        col = split.column()
        col.prop(slg, "enablebatchmode")
        col = split.column()
        col.prop(slg, "waitrender")
        col = split.column()
        col.prop(slg, "enabletelnet")
        col = split.column()
        col.prop(slg, "telnetecho")
        if slg.enablebatchmode or SLGBP.live:
            split = layout.split()
            col = split.column()
            col.prop(slg, "batchmodetime", text="Seconds")
            col = split.column()
            col.prop(slg, "batchmodespp", text="Samples")
        split = layout.split()
        col = split.column()
        col.prop(slg, "batchmode_periodicsave", text="Periodic save interval")
        split = layout.split()
        col = split.column()
        col.prop(slg, "native_threads", text="Native Threads")
        col = split.column()
        col.prop(slg, "devices_threads", text="OpenCL Threads")
        split = layout.split(percentage=0.33)
        col = split.column()
        col.label(text="OpenCL devs:")
        col = split.column()
        col.prop(slg, "opencl_cpu")
        col = split.column()
        col.prop(slg, "opencl_gpu")
        split = layout.split()
        col = split.column()
        col.prop(slg, "gpu_workgroup_size", text="GPU workgroup size")
        split = layout.split()
        col = split.column()
        col.prop(slg, "platform", text="Platform")
        col = split.column()
        col.prop(slg, "devices", text='Devs')
        if SLGBP.live:
            SLGBP.livetrigger(context.scene, SLGBP.LIVECFG)

def register():
    bpy.utils.register_module(__name__)
    slg_add_properties()
    bpy.types.DATA_PT_camera.append(slg_lensradius)
    bpy.types.MATERIAL_PT_diffuse.append(slg_forceply)
    bpy.types.OBJECT_PT_transform.append(slg_forceinst)
    bpy.types.WORLD_PT_environment_lighting.append(slg_livescn)
    bpy.types.TEXTURE_PT_mapping.append(slg_livescn)
    bpy.types.DATA_PT_sunsky.append(slg_livescn)
    bpy.types.VIEW3D_HT_header.append(slg_operators)
    bpy.types.INFO_MT_render.append(slg_rendermenu)

def unregister():
    bpy.types.Scene.RemoveProperty("slg")
    bpy.types.DATA_PT_camera.remove(slg_lensradius)
    bpy.types.MATERIAL_PT_diffuse.remove(slg_forceply)
    bpy.types.OBJECT_PT_transform.remove(slg_forceinst)
    bpy.types.WORLD_PT_environment_lighting.remove(slg_livescn)
    bpy.types.TEXTURE_PT_mapping.remove(slg_livescn)
    bpy.types.DATA_PT_sunsky.remove(slg_livescn)
    bpy.types.VIEW3D_HT_header.remove(slg_operators)
    bpy.types.INFO_MT_render.remove(slg_rendermenu)
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
