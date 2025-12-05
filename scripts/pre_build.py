# Porkchop pre-build script
# Ensures model files exist and generates version info

Import("env")
import os
import json
from datetime import datetime

def pre_build_callback(source, target, env):
    """Generate build info header"""
    build_info = {
        "build_time": datetime.now().isoformat(),
        "version": env.GetProjectOption("custom_version", "0.1.0")
    }
    
    info_path = os.path.join(env.get("PROJECT_SRC_DIR"), "build_info.h")
    with open(info_path, "w") as f:
        f.write("// Auto-generated build info\n")
        f.write("#pragma once\n")
        f.write(f'#define BUILD_TIME "{build_info["build_time"]}"\n')
        f.write(f'#define BUILD_VERSION "{build_info["version"]}"\n')

env.AddPreAction("buildprog", pre_build_callback)
