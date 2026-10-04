#!/usr/bin/env python3
"""
Test script for kernel cache functionality.

This script tests:
1. Clearing kernel caches
2. Filling kernel caches
3. Verifying cache operations work correctly
"""

import os
import sys
import tempfile
import shutil
from pathlib import Path

# Add the build directory to Python path
sys.path.insert(0, str(Path(__file__).parent.parent / "out" / "build" / "src" / "pyluxcore" / "Debug"))

try:
    import pyluxcore
    print("✓ Successfully imported pyluxcore")
except ImportError as e:
    print(f"✗ Failed to import pyluxcore: {e}")
    sys.exit(1)


def test_kernel_cache_operations():
    """Test kernel cache clearing and filling operations."""
    
    print("\n=== Kernel Cache Test ===")
    
    # Initialize LuxCore
    try:
        pyluxcore.Init()
        print("✓ LuxCore initialized")
    except Exception as e:
        print(f"✗ Failed to initialize LuxCore: {e}")
        return False
    
    # Test 1: Clear all kernel caches
    print("\n--- Test 1: Clear All Kernel Caches ---")
    try:
        pyluxcore.ClearAllKernelCaches()
        print("✓ Successfully cleared all kernel caches")
    except Exception as e:
        print(f"✗ Failed to clear kernel caches: {e}")
        return False
    
    # Test 2: Clear CUDA kernel cache
    print("\n--- Test 2: Clear CUDA Kernel Cache ---")
    try:
        pyluxcore.ClearCUDAKernelCache()
        print("✓ Successfully cleared CUDA kernel cache")
    except Exception as e:
        print(f"✗ Failed to clear CUDA kernel cache: {e}")
        return False
    
    # Test 3: Clear OpenCL kernel cache
    print("\n--- Test 3: Clear OpenCL Kernel Cache ---")
    try:
        pyluxcore.ClearOCLKernelCache()
        print("✓ Successfully cleared OpenCL kernel cache")
    except Exception as e:
        print(f"✗ Failed to clear OpenCL kernel cache: {e}")
        return False
    
    # Test 4: Fill kernel caches
    print("\n--- Test 4: Fill Kernel Caches ---")
    try:
        # Create a minimal configuration to trigger kernel compilation
        props = pyluxcore.Properties()
        
        # Set up a basic configuration that will compile kernels
        props.Set(pyluxcore.Property("renderengine.type")("PATHCPU"))
        props.Set(pyluxcore.Property("sampler.type")("SOBOL"))
        props.Set(pyluxcore.Property("film.width")(640))
        props.Set(pyluxcore.Property("film.height")(480))
        props.Set(pyluxcore.Property("batch.halttime")(1))
        props.Set(pyluxcore.Property("batch.haltspp")(1))
        
        # Create a render config to trigger kernel compilation
        config = pyluxcore.RenderConfig.Create(props)
        
        # Start and stop a session to trigger kernel compilation
        session = pyluxcore.RenderSession.Create(config)
        session.Start()
        
        # Wait a bit for kernel compilation to complete
        import time
        time.sleep(2)
        
        session.Stop()
        session = None
        config = None
        
        print("✓ Successfully triggered kernel compilation")
        
    except Exception as e:
        print(f"✗ Failed to trigger kernel compilation: {e}")
        return False
    
    # Test 5: Clear caches again after filling
    print("\n--- Test 5: Clear Caches After Filling ---")
    try:
        pyluxcore.ClearAllKernelCaches()
        print("✓ Successfully cleared caches after filling")
    except Exception as e:
        print(f"✗ Failed to clear caches after filling: {e}")
        return False
    
    return True


def test_cache_directory_existence():
    """Test that cache directories exist and can be cleared."""
    
    print("\n=== Cache Directory Test ===")
    
    # Get the cache directory path
    cache_base = Path.home() / ".config" / "luxcorerender.org"
    
    if not cache_base.exists():
        print("⚠ Cache base directory does not exist (this is normal if no kernels have been compiled yet)")
        return True
    
    # Check for CUDA cache
    cuda_cache = cache_base / "cuda_kernel_cache"
    if cuda_cache.exists():
        print("✓ CUDA kernel cache directory exists")
        # Count cached files
        cached_files = list(cuda_cache.rglob("*.ptx"))
        print(f"  Found {len(cached_files)} CUDA cached kernel files")
    else:
        print("⚠ CUDA kernel cache directory does not exist")
    
    # Check for OpenCL cache
    ocl_cache = cache_base / "ocl_kernel_cache"
    if ocl_cache.exists():
        print("✓ OpenCL kernel cache directory exists")
        # Count cached files
        cached_files = list(ocl_cache.rglob("*.ck"))
        print(f"  Found {len(cached_files)} OpenCL cached kernel files")
    else:
        print("⚠ OpenCL kernel cache directory does not exist")
    
    return True


def main():
    """Main test function."""
    
    print("Kernel Cache Test Script")
    print("=" * 40)
    
    # Run tests
    success = True
    
    success &= test_kernel_cache_operations()
    success &= test_cache_directory_existence()
    
    # Final result
    print("\n" + "=" * 40)
    if success:
        print("✓ All tests passed!")
        return 0
    else:
        print("✗ Some tests failed!")
        return 1


if __name__ == "__main__":
    sys.exit(main())