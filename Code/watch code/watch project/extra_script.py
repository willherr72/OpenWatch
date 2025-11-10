Import("env")
import os

def patch_busio_for_esp32(*args, **kwargs):
    """Patch Adafruit BusIO to add BitOrder typedef for ESP32 compatibility"""
    
    # Find the BusIO library directory
    libdeps_dir = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))
    busio_dir = os.path.join(libdeps_dir, "Adafruit BusIO")
    
    if not os.path.exists(busio_dir):
        print("⚠ Adafruit BusIO not found yet (will be downloaded)")
        return
    
    # Patch the SPIDevice header
    spi_device_h = os.path.join(busio_dir, "Adafruit_SPIDevice.h")
    
    if not os.path.exists(spi_device_h):
        print(f"⚠ Could not find {spi_device_h}")
        return
    
    # Read the file
    with open(spi_device_h, 'r') as f:
        content = f.read()
    
    # Check if already patched
    if "ESP32 BitOrder compatibility" in content:
        print("✓ Adafruit BusIO already patched")
        return
    
    # Add BitOrder typedef after the first #include
    patch = """
// ESP32 BitOrder compatibility patch
#ifdef ESP32
#ifndef BitOrder
typedef enum {
    LSBFIRST = 0,
    MSBFIRST = 1
} BitOrder;
#endif
#endif

"""
    
    # Find the first #include and insert patch after it
    import_pos = content.find("#include")
    if import_pos != -1:
        # Find the end of that line
        newline_pos = content.find("\n", import_pos)
        if newline_pos != -1:
            # Insert patch
            content = content[:newline_pos+1] + patch + content[newline_pos+1:]
            
            # Write back
            with open(spi_device_h, 'w') as f:
                f.write(content)
            
            print("✓ Patched Adafruit BusIO for ESP32 BitOrder compatibility")
    else:
        print("⚠ Could not patch Adafruit BusIO (no #include found)")

# Run patch before compilation
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", patch_busio_for_esp32)

print("✓ ESP32 build script loaded")

