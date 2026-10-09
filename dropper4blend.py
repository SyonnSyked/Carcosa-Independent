bl_info = {
    "name": "Dropper",
    "description": "Saves info about objects in the scene.",
    "author": "Joseph Hocking www.newarteest.com",
    "blender": (2, 80, 0),
    "location": "File > Export > Dropper",
    "category": "Import-Export",
}

"""
install in Edit > Preferences > Add-ons
once installed: File > Export > Dropper
GPL license https://www.blender.org/support/faq/

Saves info about objects in the scene:
name, position, rotation, scale, custom properties
(ignore objects with ~[ somewhere in the name)
Select either XML or JSON for the data format.
"""


import bpy
import os
import json
import mathutils
import subprocess

# favor the accelerated C implementation, fallback to Python implementation
try:
    import xml.etree.cElementTree as et
except ImportError:
    import xml.etree.ElementTree as et
# http://eli.thegreenplace.net/2012/03/15/processing-xml-in-python-with-elementtree


def write_data(context, filepath, format):
    print("writing scene data...")

    data = build_json()

    f = open(filepath, "w", encoding="utf-8")
    f.write(data)
    f.close()

    return {"FINISHED"}


# ----- end func -----


def get_export_model_name(root):
    name = root["exportModel"]
    if (not isinstance(name, str) or not name.strip()
            or name in (".", "..") or any(c in name for c in '<>:"/\\|?*')
            or name.endswith((".", " "))):
        raise ValueError(f"{root.name}: exportModel must be a non-empty filename string")
    return name


def get_export_roots():
    roots = [obj for obj in bpy.context.scene.objects if "exportModel" in obj]
    for root in roots:
        get_export_model_name(root)
        parent = root.parent
        while parent is not None:
            if "exportModel" in parent:
                raise ValueError("Nested exportModel roots are ambiguous: " + root.name)
            parent = parent.parent
    return roots


def collect_descendant_meshes(root):
    # A mesh root can also contribute its own geometry.
    return [obj for obj in (root, *root.children_recursive) if obj.type == "MESH"]


def calculate_group_bounds(root, meshes):
    root_inverse = root.matrix_world.inverted()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    corners = []
    for mesh in meshes:
        evaluated = mesh.evaluated_get(depsgraph)
        relative = root_inverse @ evaluated.matrix_world
        corners.extend(relative @ mathutils.Vector(corner) for corner in evaluated.bound_box)
    if not corners:
        raise ValueError(f"{root.name}: exportModel has no meshes")
    low = [min(corner[i] for corner in corners) for i in range(3)]
    high = [max(corner[i] for corner in corners) for i in range(3)]
    # Match Blender's bound_box corner order and the existing JSON format.
    return [[x, y, z] for x, y, z in (
        (low[0], low[1], low[2]), (low[0], low[1], high[2]),
        (low[0], high[1], high[2]), (low[0], high[1], low[2]),
        (high[0], low[1], low[2]), (high[0], low[1], high[2]),
        (high[0], high[1], high[2]), (high[0], high[1], low[2]),
    )]


def build_json():
    dict = {}
    dict["entities"] = []

    roots = get_export_roots()
    groups = {root: collect_descendant_meshes(root) for root in roots}
    internal_meshes = {mesh for root, meshes in groups.items() for mesh in meshes if mesh != root}

    for obj in bpy.data.objects:
        if obj in internal_meshes:
            continue
        if "~[" not in obj.name or obj in groups:
            entity = {"name": get_export_model_name(obj) if obj in groups else obj.name}

            row_world = obj.matrix_world.transposed()
            converted = mathutils.Matrix.Identity(4)
            converted[0][0:3] = row_world[0][0], row_world[0][2], row_world[0][1]
            converted[1][0:3] = row_world[1][0], row_world[1][2], row_world[1][1]
            converted[2][0:3] = row_world[2][0], row_world[2][2], row_world[2][1]
            converted[3][0:3] = row_world[3][0], row_world[3][2], row_world[3][1]
            scaleZ = mathutils.Matrix.Scale(-1.0, 4, (0.0, 0.0, 1.0))
            converted = scaleZ.transposed() @ converted

            entity["matrix"] = []

            for i in converted:
                entity["matrix"].append(i[0])
                entity["matrix"].append(i[1])
                entity["matrix"].append(i[2])
                entity["matrix"].append(i[3])

            entity["is_mesh"] = False

            if obj in groups:
                entity["is_mesh"] = True
                entity["bounds"] = calculate_group_bounds(obj, groups[obj])
            elif obj.type == "MESH":
                entity["is_mesh"] = True
                entity["bounds"] = []
                bbox_corners = [mathutils.Vector(corner) for corner in obj.bound_box]
                for corner in bbox_corners:
                    # convert corners to vulkan/d3d coordinates
                    # corner[1], corner[2] = corner[2], corner[1]
                    cornerArrayForm = [
                        corner[0],
                        corner[1],
                        corner[2],
                    ]  # Makes parsing in the solution easier (maybe)
                    entity["bounds"].append(cornerArrayForm)

            # custom properties
            for k in obj.keys():
                if k not in "_RNA_UI":
                    prop = obj[k]
                    if isinstance(prop, int) or isinstance(prop, float) or isinstance(prop, str):
                        entity[k] = obj[k]
            dict["entities"].append(entity)

    model_export()

    return json.dumps(dict, indent=4, sort_keys=True)


# ----- end func -----


def build_xml():
    root = et.Element("entities")

    for obj in bpy.data.objects:
        if "~[" not in obj.name:
            e = et.Element("entity")
            et.SubElement(e, "name").text = obj.name
            et.SubElement(e, "position").attrib = {
                "x": str(obj.location.x),
                "y": str(obj.location.y),
                "z": str(obj.location.z),
            }
            et.SubElement(e, "rotation").attrib = {
                "x": str(obj.rotation_euler.x),
                "y": str(obj.rotation_euler.y),
                "z": str(obj.rotation_euler.z),
            }
            et.SubElement(e, "scale").attrib = {
                "x": str(obj.scale.x),
                "y": str(obj.scale.y),
                "z": str(obj.scale.z),
            }
            # custom properties
            for k in obj.keys():
                if k not in "_RNA_UI":
                    prop = obj[k]
                    if isinstance(prop, int) or isinstance(prop, float) or isinstance(prop, str):
                        et.SubElement(e, k).text = str(obj[k])
            root.append(e)

    indent(root)
    return et.tostring(root).decode("utf-8")


# http://effbot.org/zone/element-lib.htm#prettyprint
def indent(elem, level=0):
    i = "\n" + level * "    "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "    "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for elem in elem:
            indent(elem, level + 1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


# ----- end func -----


def export_mesh_group(export_dir, name, root, meshes):
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    # Save basis matrices as well: restoring world matrices can alter channels.
    hierarchy = [root, *root.children_recursive]
    saved_basis = [(obj, obj.matrix_basis.copy()) for obj in hierarchy]
    try:
        bpy.ops.object.select_all(action="DESELECT")
        for mesh in meshes:
            mesh.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        # Moving the whole hierarchy keeps authoring armatures aligned with meshes.
        root.matrix_world = mathutils.Matrix.Identity(4)
        bpy.context.view_layer.update()
        bpy.ops.wm.obj_export(
            filepath=os.path.join(export_dir, f"{name}.obj"),
            export_selected_objects=True,
            export_materials=True,
            export_normals=True,
            export_triangulated_mesh=True,
            apply_modifiers=True,
            forward_axis="Y",
            up_axis="Z",
        )
    finally:
        for obj, basis in saved_basis:
            obj.matrix_basis = basis
        bpy.context.view_layer.update()
        bpy.ops.object.select_all(action="DESELECT")
        for obj in selected:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = active


def model_export():
    print("----------Begin Model Export----------")
    export_dir = os.path.join(os.path.dirname(bpy.data.filepath), "Models")
    os.makedirs(export_dir, exist_ok=True)

    roots = get_export_roots()
    groups = {root: collect_descendant_meshes(root) for root in roots}
    grouped_meshes = {mesh for meshes in groups.values() for mesh in meshes}
    unique_names = set()
    for root, meshes in groups.items():
        name = get_export_model_name(root)
        if not meshes:
            raise ValueError(f"{root.name}: exportModel has no meshes")
        if name not in unique_names:
            export_mesh_group(export_dir, name, root, meshes)
            unique_names.add(name)

    for obj in bpy.context.scene.objects:
        # Non-mesh objects and internal group pieces never enter legacy export.
        if obj.type != "MESH" or obj in grouped_meshes:
            continue
        base_name, extension = os.path.splitext(obj.name)
        if base_name not in unique_names:
            export_mesh_group(export_dir, base_name, obj, [obj])
            unique_names.add(base_name)

    print("----------End Model Export----------")
    print("----------Begin Model Conversion----------")
    exe_path = os.path.join(export_dir, "Obj2Header v1.9d.exe")
    if os.path.isfile(exe_path):
        process = subprocess.Popen(
            [exe_path], cwd=export_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        output, errors = process.communicate()
    for header in unique_names:
        header_path = os.path.join(export_dir, f"{header}.h")
        if os.path.isfile(header_path):
            os.remove(header_path)
    print("----------End Model Conversion----------")


# ----- end func -----

# ----- begin gui -----

# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator


class Dropper(Operator, ExportHelper):
    """Dropper - Saves info about objects in the scene."""

    bl_idname = "dropper.scene_text"
    bl_label = "Export Scene Data"

    # ExportHelper mixin class uses this
    filename_ext = ".json"

    filter_glob: StringProperty(
        default="*.json",
        options={"HIDDEN"},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )

    # options menu next to the file selector
    data_format: EnumProperty(
        name="Data Format",
        description="Choose the data format",
        items=(
            ("OPT_JSON", "JSON", "JavaScript Object Notation"),
            ("OPT_XML", "XML", "eXtensible Markup Language"),
        ),
        default="OPT_JSON",
    )

    def execute(self, context):
        return write_data(context, self.filepath, self.data_format)


def menu_func_export(self, context):
    self.layout.operator(Dropper.bl_idname, text="Dropper")


def register():
    bpy.utils.register_class(Dropper)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(Dropper)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

    # test call
    # bpy.ops.dropper.scene_text('INVOKE_DEFAULT')
