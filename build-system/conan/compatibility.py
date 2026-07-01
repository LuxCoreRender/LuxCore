def compatibility(conanfile):
    result = []

    # Restore default cppstd fallback
    cppstd = conanfile.settings.get_safe("compiler.cppstd")
    if cppstd:
        for std in ["14", "17", "20", "23"]:
            if std != str(cppstd):
                result.append({
                    "settings": [("compiler.cppstd", std)]
                })

    # Windows ARM64: allow MSVC consumers to find clang-built packages
    if (str(conanfile.settings.get_safe("os")) == "Windows" and
            str(conanfile.settings.get_safe("arch")) == "armv8"):
        compiler = conanfile.settings.get_safe("compiler")
        if compiler == "msvc":
            result.append({
                "settings": [
                    ("compiler", "clang"),
                    ("compiler.version", "19"),
                    ("compiler.runtime", "dynamic"),
                    ("compiler.runtime_type", "Release"),
                    ("compiler.runtime_version", "v144"),
                    ("compiler.cppstd", "20"),
                ]
            })

    return result
