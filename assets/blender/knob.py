# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Stephane P. Pericat
"""WILDBAND P0 Blender -> JUCE spike: renders one knob as a vertical filmstrip (assets/knob/knob.png).

    /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/knob.py
    ffmpeg -y -i build/knob/frame_%03d.png -filter_complex "tile=1x64" assets/knob/knob.png

64 square frames, top = fully anticlockwise (-135 deg), bottom = fully clockwise (+135 deg), transparent background.
JUCE picks the frame from the slider position (Look::drawRotarySlider in src/Main.cpp); JUCE, not the image, owns
hit-testing, dragging, MIDI and resizing. A placeholder object, not the product's visual design.
"""
import math
import os
import bpy

FRAMES, SIZE = 64, 192
OUT = os.path.join(os.getcwd(), "build", "knob")
ORANGE = (0.913, 0.342, 0.004, 1.0)      # #F59E0B, linear

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 64
scene.cycles.use_denoising = False
scene.render.resolution_x = scene.render.resolution_y = SIZE
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.world = bpy.data.worlds.new("world")
scene.world.use_nodes = True
scene.world.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.05, 0.05, 1.0)

def material(name, colour, metallic, roughness, emission=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = colour
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission > 0.0:
        bsdf.inputs["Emission Color"].default_value = colour
        bsdf.inputs["Emission Strength"].default_value = emission
    return m

def add(obj, mat, bevel=0.0):
    obj.data.materials.append(mat)
    if bevel > 0.0:
        mod = obj.modifiers.new("bevel", "BEVEL")
        mod.width, mod.segments = bevel, 4
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj

# a skirt, a body and a cap, with an orange pointer bar inset in the cap; the pointer points "up" (+Y) at rest
bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=1.0, depth=0.18, location=(0, 0, 0.09))
skirt = add(bpy.context.object, material("skirt", (0.02, 0.02, 0.022, 1), 0.9, 0.45), 0.03)
bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=0.8, depth=0.6, location=(0, 0, 0.45))
body = add(bpy.context.object, material("body", (0.035, 0.035, 0.04, 1), 0.85, 0.32), 0.06)
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0.47, 0.76))
pointer = bpy.context.object
pointer.scale = (0.09, 0.56, 0.03)
add(pointer, material("pointer", ORANGE, 0.0, 0.4, 1.2))
knob = bpy.data.objects.new("knob", None)
scene.collection.objects.link(knob)
for part in (skirt, body, pointer):
    part.parent = knob

bpy.ops.object.camera_add(location=(0, 0, 6), rotation=(0, 0, 0))
camera = bpy.context.object
camera.data.type = "ORTHO"
camera.data.ortho_scale = 2.3
scene.camera = camera
for location, energy, size in (((-3.0, 3.0, 5.0), 900.0, 4.0), ((4.0, -2.0, 3.0), 250.0, 6.0)):   # key from top-left, soft fill
    bpy.ops.object.light_add(type="AREA", location=location)
    light = bpy.context.object
    light.data.energy, light.data.size = energy, size
    direction = light.location.copy()
    light.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()

os.makedirs(OUT, exist_ok=True)
for frame in range(FRAMES):
    angle = math.radians(-135.0 + 270.0 * frame / (FRAMES - 1))
    knob.rotation_euler = (0.0, 0.0, -angle)          # clockwise on screen as the value rises; the light stays put
    scene.render.filepath = os.path.join(OUT, f"frame_{frame:03d}.png")
    bpy.ops.render.render(write_still=True)
