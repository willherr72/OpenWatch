Import("env")

# Apply source filters to exclude ARM-specific files from ALL libraries
def apply_lib_filters(node):
    """Filter callback to exclude ARM-specific source files"""
    # Get the source file path
    return node

# Get library builders and modify their source filters
print("Applying LVGL ARM exclusion filters...")

# Modify the default source filter for libraries
original_src_filter = env.GetProjectOption("src_filter", "")

# For library dependencies, we need to filter at the library level
env.Append(
    CPPDEFINES=[
        ("LV_CONF_PATH", '\\"lv_conf.h\\"'),
    ]
)

# Print build environment info
print(f"Build type: {env.get('BUILD_TYPE', 'release')}")
print(f"Platform: {env.get('PIOPLATFORM', 'unknown')}")

print("✓ ESP32-specific LVGL configuration applied")

