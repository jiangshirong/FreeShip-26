# SPDX-License-Identifier: GPL-2.0-or-later
"""Blender exporter for one OBJ containing the base mesh and tagged edges."""

bl_info = {
    "name": "FreeShip Control Mesh OBJ",
    "author": "FreeShip 26",
    "version": (1, 0, 0),
    "blender": (4, 3, 0),
    "location": "File > Export",
    "category": "Import-Export",
}

import os
from pathlib import Path
import tempfile

import bpy
from bpy.props import StringProperty
from bpy_extras.io_utils import ExportHelper


def export_control_mesh(context, filepath):
    objects = sorted((obj for obj in context.selected_objects if obj.type == "MESH"),
                     key=lambda obj: obj.name)
    if not objects:
        raise ValueError("Select at least one mesh to export")
    if not any(obj.data.polygons for obj in objects):
        raise ValueError("The selected meshes do not contain faces")

    path = Path(filepath)
    units = context.scene.unit_settings
    metres = units.scale_length if units.system != "NONE" else 1.0
    offset = 1
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, mode="w", encoding="utf-8",
                                         newline="\n", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write("# FreeShip26 control mesh (Blender XYZ, metres)\n# FreeShip26 creases 1\n")
            for obj in objects:
                if obj.mode == "EDIT":
                    obj.update_from_editmode()
                mesh = obj.data
                if not mesh.polygons:
                    continue
                transform = obj.matrix_world
                reversed_winding = transform.to_3x3().determinant() < 0
                name = " ".join(obj.name.replace("#", "_").split())
                stream.write(f"o {name}\n")
                for vertex in mesh.vertices:
                    position = (transform @ vertex.co) * metres
                    stream.write("v " + " ".join(f"{value:.9g}" for value in position) + "\n")
                for face in mesh.polygons:
                    indices = list(face.vertices)
                    if reversed_winding:
                        indices.reverse()
                    stream.write("f " + " ".join(str(offset + vertex) for vertex in indices) + "\n")

                sharp = mesh.attributes.get("sharp_edge")
                crease = mesh.attributes.get("crease_edge")
                adjacent = {loop.edge_index for loop in mesh.loops}
                for edge in mesh.edges:
                    i = edge.index
                    if i in adjacent and ((sharp and sharp.data[i].value)
                                          or (crease and crease.data[i].value >= 1.0)):
                        a, b = edge.vertices
                        stream.write(f"# FreeShip26 crease {a + offset} {b + offset}\n")
                offset += len(mesh.vertices)
        os.replace(temporary, path)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


class FreeShipControlMeshExport(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.freeship_control_obj"
    bl_label = "Export FreeShip Control Mesh"
    filename_ext = ".obj"
    filter_glob: StringProperty(default="*.obj", options={"HIDDEN"})

    def execute(self, context):
        try:
            export_control_mesh(context, self.filepath)
        except (OSError, ValueError) as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        return {"FINISHED"}


def menu_entry(self, _context):
    self.layout.operator(FreeShipControlMeshExport.bl_idname, text="FreeShip Control Mesh (.obj)")


def register():
    bpy.utils.register_class(FreeShipControlMeshExport)
    bpy.types.TOPBAR_MT_file_export.append(menu_entry)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_entry)
    bpy.utils.unregister_class(FreeShipControlMeshExport)


if __name__ == "__main__":
    register()
