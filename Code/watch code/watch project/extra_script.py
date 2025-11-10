Import("env", "projenv")
import os

# Function to exclude ARM-specific files from LVGL library
def exclude_lvgl_arm_files(node):
    """Filter out ARM-specific assembly and hardware-specific files"""
    node_path = node.get_path()
    
    # Exclude patterns
    exclude_patterns = [
        "helium",  # ARM Helium SIMD
        "neon",    # ARM NEON SIMD  
        "arm2d",   # ARM 2D graphics
        "dave2d",  # Renesas Dave2D
        "pxp",     # NXP PXP
        "vg_lite", # VG-Lite GPU
        "opengles",# OpenGL ES
        "sdl",     # SDL
    ]
    
    # Check if any exclude pattern is in the path
    for pattern in exclude_patterns:
        if pattern in node_path.lower():
            return None  # Exclude this file
    
    return node

# Apply filter to all library dependencies
for lib in env.GetLibBuilders():
    lib.env.AddBuildMiddleware(exclude_lvgl_arm_files, "*.[sS]")  # Assembly files
    lib.env.AddBuildMiddleware(exclude_lvgl_arm_files, "*.c")     # C files in excluded dirs
    lib.env.AddBuildMiddleware(exclude_lvgl_arm_files, "*.cpp")   # C++ files in excluded dirs

print("✓ LVGL build filter applied - excluding ARM/hardware-specific code")

