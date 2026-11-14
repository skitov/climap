set_project("climap")
set_version("0.1.0")
set_xmakever("2.8.0")

set_languages("c++20")

add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

add_requires("doctest 2.5.3")

target("climap")
    set_kind("headeronly")
    add_headerfiles("src/**.hpp")
    add_includedirs("src", {public = true})

-- Compile-only gate so every production header is built in CI.
target("check_headers")
    set_kind("object")
    add_deps("climap")
    add_files("test/headers.cpp")

target("test_wire")
    set_kind("binary")
    add_deps("climap")
    add_files("test/wire/*.cpp")
    add_packages("doctest")
