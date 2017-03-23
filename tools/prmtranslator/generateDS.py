import dsutils
import os

filepath = os.path.join(os.getenv("VRAY_PLUGIN_DESC_PATH", ""), "", "light/LightIES.json")
destpath = os.path.join(os.getenv("HOME"), "ds_tmp")
dsutils.saveJSONAsDialogScript(filepath, destpath)