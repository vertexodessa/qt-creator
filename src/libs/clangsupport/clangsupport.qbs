import qbs 1.0

QtcLibrary {
    name: "ClangSupport"
    targetName: "Clangbackendipc"

    Depends { name: "Qt.network" }
    Depends { name: "Utils" }

    cpp.defines: base.concat("CLANGSUPPORT_LIBRARY")
    cpp.includePaths: base.concat(".")

    Group {
        files: [
            "*.h",
            "*.cpp"
        ]
    }

    Export {
        Depends { name: "Utils" }
        Depends { name: "Qt.network" }
        cpp.includePaths: [
            "."
        ]
    }
}
