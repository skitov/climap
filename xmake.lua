set_project("climap")
set_version("0.1.0")
set_xmakever("2.8.0")

set_languages("c++20")

add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

add_requires("doctest")

target("climap")
    set_kind("headeronly")
    add_headerfiles("src/**.hpp")
    add_includedirs("src", {public = true})

target("test_parser")
    set_kind("binary")
    add_deps("climap")
    add_files("test/parser/*.cpp")
    add_packages("doctest")
