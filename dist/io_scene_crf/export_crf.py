# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import os
import time
import struct
import bpy
import mathutils
import bpy_extras.io_utils

from .crf_objects import CRF_vertex


def _write(context, filepath,
              EXPORT_USE_VERBOSE,
              EXPORT_TRI,  # ok
              EXPORT_EDGES,
              EXPORT_NORMALS,  # not yet
              EXPORT_UV,  # ok
              EXPORT_MTL,
              EXPORT_APPLY_MODIFIERS,  # ok
              EXPORT_BLEN_OBS,
              EXPORT_GROUP_BY_OB,
              EXPORT_GROUP_BY_MAT,
              EXPORT_KEEP_VERT_ORDER,
              EXPORT_POLYGROUPS,
              EXPORT_CURVE_AS_NURBS,
              EXPORT_SEL_ONLY,  # ok
              EXPORT_ANIMATION,
              EXPORT_GLOBAL_MATRIX,
              EXPORT_PATH_MODE,
              ):  # Not used

    verbose = EXPORT_USE_VERBOSE
    base_name, ext = os.path.splitext(filepath)
    context_name = [base_name, '', '', ext]  # Base name, scene name, frame number, extension
    file = open(filepath, "wb")

    scene = context.scene
    orig_frame = scene.frame_current


    print('\nexporting crf %r' % filepath)
    time1 = time.time()
    num_meshes = len(bpy.context.selected_objects)
    if num_meshes < 1:
        raise Exception("Must select at least one object to export CRF")
    print("Number of meshes", num_meshes)
    ob_primary = bpy.context.selected_objects[0]
    print(ob_primary)

    # Exit edit mode before exporting, so current object states are exported properly.
    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')
        

    matrix_world = ob_primary.matrix_basis # world matrix so we can transform from local to global coordinates

    # write header
    file.write(b"fknc")
    file.write(struct.pack("<I", 1))
    file.write(struct.pack("<II", *(0xFFFF, 0xFFFF))) #these values are set after mesh data is written out
    file.write(struct.pack("<IHH", *(2, 6, 0xFFFF)))# object type 2, magick 6, magick 0xFFFF
    file.write(struct.pack("<I", num_meshes))    #number of meshes in file, for now just one
    LoX = ob_primary.bound_box[0][0]    #TODO, put bbox into a function
    LoY = ob_primary.bound_box[0][1]
    LoZ = ob_primary.bound_box[0][2]
    HiX = ob_primary.bound_box[6][0]
    HiY = ob_primary.bound_box[6][1]
    HiZ = ob_primary.bound_box[6][2]   
    print("Bounding box (%f, %f, %f) (%f, %f, %f)" % (LoX, LoY, LoZ, HiX, HiY, HiZ))
    file.write(struct.pack("<ffffff", *(LoX, LoY, LoZ, HiX, HiY, HiZ))) # bounding box
    # end of header

    # start mesh export loop
    model_number = 0
    for ob in bpy.context.selected_objects:
        LoX = ob.bound_box[0][0]
        LoY = ob.bound_box[0][1]
        LoZ = ob.bound_box[0][2]
        HiX = ob.bound_box[6][0]
        HiY = ob.bound_box[6][1]
        HiZ = ob.bound_box[6][2]
        
        # mesh header
        mesh = ob.data
        number_of_verteces = len(mesh.vertices)
        number_of_faces = len(mesh.tessfaces)
        file.write(struct.pack("<II", *(number_of_verteces, number_of_faces))) # number for vertices and faces
        print("Model: %i, vertices: %i, faces: %i" % (model_number, len(mesh.vertices), len(mesh.tessfaces)))
        model_number = model_number + 1
        # face/vertex index list
        #TODO, the first face always has the first two vertices switched. Don't know if this will affect
        # anything. Need to verify that this does not cause a problem.
        for face in mesh.tessfaces:
            verts_in_face = face.vertices[:]
            if verbose:
                print("face index %s, verts %s" % (face.index, verts_in_face))
            file.write(struct.pack("<HHH", *verts_in_face))

            
        # start token?
        print("Writing verts at", hex(file.tell()))
        file.write(struct.pack("<Qx", 0x0000200c01802102))
        # end mesh header

        # make sure to create uv texture layers before vertex color layers, otherwise uv layer will overwrite a vertex color layer
        if len(mesh.uv_textures) == 2:
            uv_tex0 = mesh.uv_textures[0]
            uv_tex1 = mesh.uv_textures[1]
        elif len(mesh.uv_textures) == 1:
            uv_tex0 = mesh.uv_textures[0]
            uv_tex1 = mesh.uv_textures.new()
            uv_tex1.name = "UV_Secondary"
        else:
            uv_tex0 = mesh.uv_textures.new()
            uv_tex0.name = "UV_Main"
            uv_tex1 = mesh.uv_textures.new()
            uv_tex1.name = "UV_Secondary"

        mesh.update(calc_tessface=True)
        uv_tex0 = mesh.tessface_uv_textures[0]
        uv_tex1 = mesh.tessface_uv_textures[1]
        
        # write out verteces, normals, ks, and UVs
        if "vertex_specular_colors" in mesh.tessface_vertex_colors:
            vtex_specular_colors = mesh.tessface_vertex_colors["vertex_specular_colors"]
        else:
            vtex_specular_colors_m = mesh.vertex_colors.new()
            vtex_specular_colors_m.name = "vertex_specular_colors"
            mesh.update(calc_tessface=True)
            vtex_specular_colors = mesh.tessface_vertex_colors["vertex_specular_colors"]

        if "vertex_specular_alpha" in mesh.tessface_vertex_colors:
            vtex_specular_alpha = mesh.tessface_vertex_colors["vertex_specular_alpha"]
        else:
            vtex_specular_alpha_m = mesh.vertex_colors.new()
            vtex_specular_alpha_m.name = "vertex_specular_alpha"
            mesh.update(calc_tessface=True)
            vtex_specular_alpha = mesh.tessface_vertex_colors["vertex_specular_alpha"]

        if "vertex_blendweight_xyz" in mesh.tessface_vertex_colors:
            vtex_blendweights_xyz = mesh.tessface_vertex_colors["vertex_blendweight_xyz"]
        else:
            vtex_blendweights_xyz_m = mesh.vertex_colors.new()
            vtex_blendweights_xyz_m.name = "vertex_blendweight_xyz"
            mesh.update(calc_tessface=True)
            vtex_blendweights_xyz = mesh.tessface_vertex_colors["vertex_blendweight_xyz"]

        if "vertex_blendweight_w" in mesh.tessface_vertex_colors:
            vtex_blendweights_w = mesh.tessface_vertex_colors["vertex_blendweight_w"]
        else:
            vtex_blendweights_w_m = mesh.vertex_colors.new()
            vtex_blendweights_w_m.name = "vertex_blendweight_w"
            mesh.update(calc_tessface=True)
            vtex_blendweights_w = mesh.tessface_vertex_colors["vertex_blendweight_w"]


        vert_dict = {} # will store CRF_vertex objects
        for face in mesh.tessfaces:
            verts_in_face = face.vertices[:]
            if not verts_in_face[0] in vert_dict:
                vert = CRF_vertex()
                vert.index = verts_in_face[0]
                # get vertex coords and make sure to translate from local to global
                vert.x_blend, vert.y_blend, vert.z_blend = matrix_world * mesh.vertices[verts_in_face[0]].co.xyz 
                vert.normal_x_blend, vert.normal_y_blend, vert.normal_z_blend = mesh.vertices[verts_in_face[0]].normal
                vert.normal_w_blend = 1.0
                vert.specular_blue_blend = vtex_specular_colors.data[face.index].color1[2] 
                vert.specular_green_blend = vtex_specular_colors.data[face.index].color1[1] 
                vert.specular_red_blend = vtex_specular_colors.data[face.index].color1[0] 
                vert.specular_alpha_blend = vtex_specular_alpha.data[face.index].color1[0] # only use the first color for alpha       
                vert.u0_blend = uv_tex0.data[face.index].uv1[0] 
                vert.v0_blend = uv_tex0.data[face.index].uv1[1]
                vert.u1_blend = uv_tex1.data[face.index].uv1[0] 
                vert.v1_blend = uv_tex1.data[face.index].uv1[1]
                vert.blendweights1_x_blend = vtex_blendweights_xyz.data[face.index].color1[0]
                vert.blendweights1_y_blend = vtex_blendweights_xyz.data[face.index].color1[1]
                vert.blendweights1_z_blend = vtex_blendweights_xyz.data[face.index].color1[2]
                vert.blendweights1_w_blend = vtex_blendweights_w.data[face.index].color1[0] # only use the first color for w       
                vert.blend2raw()
                vert_dict[verts_in_face[0]] = vert # put object in dictionary
                if verbose:
                    print(vert)

            if not verts_in_face[1] in vert_dict:
                vert = CRF_vertex()
                vert.index = verts_in_face[1]
                # get vertex coords and make sure to translate from local to global
                vert.x_blend, vert.y_blend, vert.z_blend = matrix_world * mesh.vertices[verts_in_face[1]].co.xyz
                vert.normal_x_blend, vert.normal_y_blend, vert.normal_z_blend = mesh.vertices[verts_in_face[1]].normal
                vert.normal_w_blend = 1.0
                vert.specular_blue_blend = vtex_specular_colors.data[face.index].color2[2] 
                vert.specular_green_blend = vtex_specular_colors.data[face.index].color2[1] 
                vert.specular_red_blend = vtex_specular_colors.data[face.index].color2[0] 
                vert.specular_alpha_blend = vtex_specular_alpha.data[face.index].color1[0] # only use the first color for alpha              
                vert.u0_blend = uv_tex0.data[face.index].uv2[0] 
                vert.v0_blend = uv_tex0.data[face.index].uv2[1]
                vert.u1_blend = uv_tex1.data[face.index].uv2[0] 
                vert.v1_blend = uv_tex1.data[face.index].uv2[1]
                vert.blendweights1_x_blend = vtex_blendweights_xyz.data[face.index].color1[0]
                vert.blendweights1_y_blend = vtex_blendweights_xyz.data[face.index].color1[1]
                vert.blendweights1_z_blend = vtex_blendweights_xyz.data[face.index].color1[2]
                vert.blendweights1_w_blend = vtex_blendweights_w.data[face.index].color1[0] # only use the first color for w
                vert.blend2raw()
                vert_dict[verts_in_face[1]] = vert # put object in dictionary
                if verbose:
                    print(vert)     

            if not verts_in_face[2] in vert_dict:
                vert = CRF_vertex()
                vert.index = verts_in_face[2]
                # get vertex coords and make sure to translate from local to global
                vert.x_blend, vert.y_blend, vert.z_blend = matrix_world * mesh.vertices[verts_in_face[2]].co.xyz
                vert.normal_x_blend, vert.normal_y_blend, vert.normal_z_blend = mesh.vertices[verts_in_face[2]].normal     
                vert.normal_w_blend = 1.0
                vert.specular_blue_blend = vtex_specular_colors.data[face.index].color3[2] 
                vert.specular_green_blend = vtex_specular_colors.data[face.index].color3[1] 
                vert.specular_red_blend = vtex_specular_colors.data[face.index].color3[0] 
                vert.specular_alpha_blend = vtex_specular_alpha.data[face.index].color1[0] # only use the first color for alpha              
                vert.u0_blend = uv_tex0.data[face.index].uv3[0] 
                vert.v0_blend = uv_tex0.data[face.index].uv3[1]
                vert.u1_blend = uv_tex1.data[face.index].uv3[0] 
                vert.v1_blend = uv_tex1.data[face.index].uv3[1]
                vert.blendweights1_x_blend = vtex_blendweights_xyz.data[face.index].color1[0]
                vert.blendweights1_y_blend = vtex_blendweights_xyz.data[face.index].color1[1]
                vert.blendweights1_z_blend = vtex_blendweights_xyz.data[face.index].color1[2]
                vert.blendweights1_w_blend = vtex_blendweights_w.data[face.index].color1[0] # only use the first color for w
                vert.blend2raw()
                vert_dict[verts_in_face[2]] = vert # put object in dictionary
                if verbose:
                    print(vert)    

        # write out vertices
        for key, vertex in vert_dict.items():
            if verbose:
                print(vertex)
            file.write(vertex.convert2bin())

        # write separator 0x000000080008000000
        file.write(struct.pack("<II", 0x00080000, 0x00000008))
        # write out second dummy vertex stream
        for i in range(0, number_of_verteces):
            file.write(struct.pack("<ff", 0, 0))
        # write mesh bounding box 
        file.write(struct.pack("<ffffff", *(LoX, LoY, LoZ, HiX, HiY, HiZ))) # bounding box
        # end mesh export loop

        diffuse_texture_file = None
        normals_texture_file = None
        specular_texture_file = None
        
        # get textures
        print(mesh.materials[0].texture_slots[0])
        print(mesh.materials[0].texture_slots[1])        
        if mesh.materials[0].texture_slots[0] == None and mesh.materials[0].texture_slots[1] == None:
               raise Exception("Missing a diffuse or normal texture")
        else:
            diffuse_texture_file = mesh.materials[0].texture_slots[0].texture.image.name
            normals_texture_file = mesh.materials[0].texture_slots[1].texture.image.name
            
        if mesh.materials[0].texture_slots[2] == None:
            print("Using a constant specular value")
        else:
            specular_texture_file = mesh.materials[0].texture_slots[2].texture.image.name


        # strip extension from filenames
        diffuse_texture_file = os.path.splitext(diffuse_texture_file)[0]
        normals_texture_file = os.path.splitext(normals_texture_file)[0]
        if specular_texture_file != None:
            specular_texture_file = os.path.splitext(specular_texture_file)[0]

        # get diffuse and specular material color
        diffuse_material_color = mesh.materials[0].diffuse_color
        specular_material_color = mesh.materials[0].specular_color
        print("Textures:", diffuse_texture_file, normals_texture_file, specular_texture_file)

        # write out textures and materials
        #TODO turn this into a state machine
        file.write(b"nm")
        file.write(struct.pack("<II", *(1, 4))) 
        file.write(b"sffd") #diffuse
        file.write(struct.pack("<I%is" % len(diffuse_texture_file), len(diffuse_texture_file), diffuse_texture_file.encode()))
        file.write(struct.pack("<I", 0))
        file.write(b"smrn") #normals           
        file.write(struct.pack("<I%ss" % len(normals_texture_file), len(normals_texture_file), normals_texture_file.encode()))
        file.write(struct.pack("<I", 0))
        file.write(b"1tsc") #const1
        file.write(struct.pack("<II", 0,0))
        
        if specular_texture_file != None:
            file.write(b"lcps") #specular
            file.write(struct.pack("<I%is" % len(specular_texture_file), len(specular_texture_file), specular_texture_file.encode()))
            file.write(struct.pack("<II", 0,2))
            file.write(b"lcps") #specular constant
            file.write(struct.pack("<fff", *specular_material_color))
            file.write(b"1tsc") #const1
            file.write(struct.pack("<II", 0,0))
            file.write(struct.pack("<I", 0))
            file.write(struct.pack("<I", 1))
            file.write(b"1tsc") #const1
            file.write(struct.pack("<II", 0,0))
        else:
            file.write(b"lcps") #specular
            file.write(struct.pack("<II", 0,0))
            file.write(struct.pack("<I", 2))
            file.write(b"lcps") #specular
            file.write(struct.pack("<fff", *specular_material_color))
            file.write(b"1tsc") #const1
            file.write(struct.pack("<II", 0,0))
            file.write(struct.pack("<II", 0,1))
            file.write(b"1tsc") #const1        
            file.write(struct.pack("<II", 0,2))
            file.write(struct.pack("16x"))  
        # end of materials
    # end of all meshes
    
    # trailer 1
    #TODO this info should be represented by a data structure
    trailer_1 = file.tell() 
    meshfile_size = trailer_1 - 0x14 # calculate size of meshfile 
    file.write(struct.pack("<IIIIIIII", *(0, 0, 0, 0, 0xFFFFFFFF, 1, 1, 0)))
    file.write(struct.pack("IIII", *(0x1b4f7cc7, 1, 0x14, meshfile_size)))
    file.write(struct.pack("IIII", *(0,0,0, 0)))
    trailer_1_end = file.tell()

    # put trailer1 and trailer2 offsets into the file header
    file.seek(0x08)
    file.write(struct.pack("<I", trailer_1)) # trailer1 file offset
    file.write(struct.pack("<I", trailer_1_end)) # trailer2 file offset

    # trailer 2
    file.seek(trailer_1_end)
    file.write(struct.pack("<III", *(0,0,9)))
    file.write(b"root node")
    file.write(struct.pack("<II", *(1, 8)))
    file.write(b"meshfile")
    file.write(struct.pack("<I", 0))
    

    file.close()
    print("CRF Export time: %.2f" % (time.time() - time1))
    # Restore old active scene.
#   orig_scene.makeCurrent()
#   Window.WaitCursor(0)


'''
Currently the exporter lacks these features:
* multiple scene export (only active scene is written)
* particles
'''


def save(operator, context, filepath="",
         use_verbose=False,
         use_triangles=False,
         use_edges=True,
         use_normals=False,
         use_uvs=True,
         use_materials=True,
         use_apply_modifiers=True,
         use_blen_objects=True,
         group_by_object=False,
         group_by_material=False,
         keep_vertex_order=False,
         use_vertex_groups=False,
         use_nurbs=True,
         use_selection=True,
         use_animation=False,
         global_matrix=None,
         path_mode='AUTO'
         ):

    _write(context, filepath,
           EXPORT_USE_VERBOSE=use_verbose,
           EXPORT_TRI=use_triangles,
           EXPORT_EDGES=use_edges,
           EXPORT_NORMALS=use_normals,
           EXPORT_UV=use_uvs,
           EXPORT_MTL=use_materials,
           EXPORT_APPLY_MODIFIERS=use_apply_modifiers,
           EXPORT_BLEN_OBS=use_blen_objects,
           EXPORT_GROUP_BY_OB=group_by_object,
           EXPORT_GROUP_BY_MAT=group_by_material,
           EXPORT_KEEP_VERT_ORDER=keep_vertex_order,
           EXPORT_POLYGROUPS=use_vertex_groups,
           EXPORT_CURVE_AS_NURBS=use_nurbs,
           EXPORT_SEL_ONLY=use_selection,
           EXPORT_ANIMATION=use_animation,
           EXPORT_GLOBAL_MATRIX=global_matrix,
           EXPORT_PATH_MODE=path_mode,
           )

    return {'FINISHED'}
