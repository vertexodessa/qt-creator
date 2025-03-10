import qbs.FileInfo

QtcPlugin {
    name: "CppEditor"

    Depends { name: "Qt.widgets" }
    Depends { condition: project.withAutotests; name: "Qt.testlib" }

    Depends { name: "CPlusPlus" }
    Depends { name: "Utils" }

    Depends { name: "Core" }
    Depends { name: "TextEditor" }
    Depends { name: "ProjectExplorer" }

    Depends { name: "app_version_header" }

    pluginTestDepends: [
        "QmakeProjectManager",
        "QbsProjectManager",
    ]

    cpp.defines: base
    Properties {
        condition: qbs.toolchain.contains("msvc")
        cpp.defines: base.concat("_SCL_SECURE_NO_WARNINGS")
    }

    files: [
        "abstracteditorsupport.cpp",
        "abstracteditorsupport.h",
        "abstractoverviewmodel.h",
        "baseeditordocumentparser.cpp",
        "baseeditordocumentparser.h",
        "baseeditordocumentprocessor.cpp",
        "baseeditordocumentprocessor.h",
        "builtincursorinfo.cpp",
        "builtincursorinfo.h",
        "builtineditordocumentparser.cpp",
        "builtineditordocumentparser.h",
        "builtineditordocumentprocessor.cpp",
        "builtineditordocumentprocessor.h",
        "builtinindexingsupport.cpp",
        "builtinindexingsupport.h",
        "clangbasechecks.ui", // FIXME: Is this used at all?
        "clangdiagnosticconfig.cpp",
        "clangdiagnosticconfig.h",
        "clangdiagnosticconfigsmodel.cpp",
        "clangdiagnosticconfigsmodel.h",
        "clangdiagnosticconfigsselectionwidget.cpp",
        "clangdiagnosticconfigsselectionwidget.h",
        "clangdiagnosticconfigswidget.cpp",
        "clangdiagnosticconfigswidget.h",
        "clangdiagnosticconfigswidget.ui",
        "compileroptionsbuilder.cpp",
        "compileroptionsbuilder.h",
        "cppautocompleter.cpp",
        "cppautocompleter.h",
        "cppbuiltinmodelmanagersupport.cpp",
        "cppbuiltinmodelmanagersupport.h",
        "cppcanonicalsymbol.cpp",
        "cppcanonicalsymbol.h",
        "cppchecksymbols.cpp",
        "cppchecksymbols.h",
        "cppcodeformatter.cpp",
        "cppcodeformatter.h",
        "cppcodemodelinspectordialog.cpp",
        "cppcodemodelinspectordialog.h",
        "cppcodemodelinspectordialog.ui",
        "cppcodemodelinspectordumper.cpp",
        "cppcodemodelinspectordumper.h",
        "cppcodemodelsettings.cpp",
        "cppcodemodelsettings.h",
        "cppcodemodelsettingspage.cpp",
        "cppcodemodelsettingspage.h",
        "cppcodemodelsettingspage.ui",
        "cppcodestylepreferences.cpp",
        "cppcodestylepreferences.h",
        "cppcodestylepreferencesfactory.cpp",
        "cppcodestylepreferencesfactory.h",
        "cppcodestylesettings.cpp",
        "cppcodestylesettings.h",
        "cppcodestylesettingspage.cpp",
        "cppcodestylesettingspage.h",
        "cppcodestylesettingspage.ui",
        "cppcodestylesnippets.h",
        "cppcompletionassist.cpp",
        "cppcompletionassist.h",
        "cppcompletionassistprocessor.cpp",
        "cppcompletionassistprocessor.h",
        "cppcompletionassistprovider.cpp",
        "cppcompletionassistprovider.h",
        "cppcurrentdocumentfilter.cpp",
        "cppcurrentdocumentfilter.h",
        "cppcursorinfo.h",
        "cppdoxygen.cpp",
        "cppdoxygen.h",
        "cppdoxygen.kwgen",
        "cppeditorwidget.cpp",
        "cppeditorwidget.h",
        "cppeditor.qrc",
        "cppeditor_global.h",
        "cppeditorconstants.h",
        "cppeditordocument.cpp",
        "cppeditordocument.h",
        "cppeditoroutline.cpp",
        "cppeditoroutline.h",
        "cppeditorplugin.cpp",
        "cppeditorplugin.h",
        "cppelementevaluator.cpp",
        "cppelementevaluator.h",
        "cppfileiterationorder.cpp",
        "cppfileiterationorder.h",
        "cppfilesettingspage.cpp",
        "cppfilesettingspage.h",
        "cppfilesettingspage.ui",
        "cppfindreferences.cpp",
        "cppfindreferences.h",
        "cppfollowsymbolundercursor.cpp",
        "cppfollowsymbolundercursor.h",
        "cppfunctiondecldeflink.cpp",
        "cppfunctiondecldeflink.h",
        "cpphighlighter.cpp",
        "cpphighlighter.h",
        "cppincludehierarchy.cpp",
        "cppincludehierarchy.h",
        "cppincludesfilter.cpp",
        "cppincludesfilter.h",
        "cppindexingsupport.cpp",
        "cppindexingsupport.h",
        "cppinsertvirtualmethods.cpp",
        "cppinsertvirtualmethods.h",
        "cpplocalrenaming.cpp",
        "cpplocalrenaming.h",
        "cpplocalsymbols.cpp",
        "cpplocalsymbols.h",
        "cpplocatordata.cpp",
        "cpplocatordata.h",
        "cpplocatorfilter.cpp",
        "cpplocatorfilter.h",
        "cppminimizableinfobars.cpp",
        "cppminimizableinfobars.h",
        "cppmodelmanager.cpp",
        "cppmodelmanager.h",
        "cppmodelmanagersupport.cpp",
        "cppmodelmanagersupport.h",
        "cppoutline.cpp",
        "cppoutline.h",
        "cppoverviewmodel.cpp",
        "cppoverviewmodel.h",
        "cppparsecontext.cpp",
        "cppparsecontext.h",
        "cpppointerdeclarationformatter.cpp",
        "cpppointerdeclarationformatter.h",
        "cppprojectpartchooser.cpp",
        "cppprojectpartchooser.h",
        "cpppreprocessordialog.cpp",
        "cpppreprocessordialog.h",
        "cpppreprocessordialog.ui",
        "cppprojectfile.cpp",
        "cppprojectfile.h",
        "cppprojectfilecategorizer.cpp",
        "cppprojectfilecategorizer.h",
        "cppprojectinfogenerator.cpp",
        "cppprojectinfogenerator.h",
        "cppprojectupdater.cpp",
        "cppprojectupdater.h",
        "cppprojectupdaterinterface.h",
        "cppquickfix.cpp",
        "cppquickfix.h",
        "cppquickfixassistant.cpp",
        "cppquickfixassistant.h",
        "cppquickfixes.cpp",
        "cppquickfixes.h",
        "cppquickfixprojectsettings.cpp",
        "cppquickfixprojectsettings.h",
        "cppquickfixprojectsettingswidget.cpp",
        "cppquickfixprojectsettingswidget.h",
        "cppquickfixprojectsettingswidget.ui",
        "cppquickfixsettings.cpp",
        "cppquickfixsettings.h",
        "cppquickfixsettingspage.cpp",
        "cppquickfixsettingspage.h",
        "cppquickfixsettingswidget.cpp",
        "cppquickfixsettingswidget.h",
        "cppquickfixsettingswidget.ui",
        "cppqtstyleindenter.cpp",
        "cppqtstyleindenter.h",
        "cpprefactoringchanges.cpp",
        "cpprefactoringchanges.h",
        "cppselectionchanger.cpp",
        "cppselectionchanger.h",
        "cppsemanticinfo.h",
        "cppsemanticinfoupdater.cpp",
        "cppsemanticinfoupdater.h",
        "cppsourceprocessor.cpp",
        "cppsourceprocessor.h",
        "cpptoolsjsextension.cpp",
        "cpptoolsjsextension.h",
        "cpptoolsreuse.cpp",
        "cpptoolsreuse.h",
        "cpptoolssettings.cpp",
        "cpptoolssettings.h",
        "cpptypehierarchy.cpp",
        "cpptypehierarchy.h",
        "cppuseselectionsupdater.cpp",
        "cppuseselectionsupdater.h",
        "cppvirtualfunctionassistprovider.cpp",
        "cppvirtualfunctionassistprovider.h",
        "cppvirtualfunctionproposalitem.cpp",
        "cppvirtualfunctionproposalitem.h",
        "cppworkingcopy.cpp",
        "cppworkingcopy.h",
        "cursorineditor.h",
        "doxygengenerator.cpp",
        "doxygengenerator.h",
        "editordocumenthandle.cpp",
        "editordocumenthandle.h",
        "functionutils.cpp",
        "functionutils.h",
        "generatedcodemodelsupport.cpp",
        "generatedcodemodelsupport.h",
        "headerpathfilter.cpp",
        "headerpathfilter.h",
        "includeutils.cpp",
        "includeutils.h",
        "indexitem.cpp",
        "indexitem.h",
        "insertionpointlocator.cpp",
        "insertionpointlocator.h",
        "projectinfo.cpp",
        "projectinfo.h",
        "projectpart.cpp",
        "projectpart.h",
        "resourcepreviewhoverhandler.cpp",
        "resourcepreviewhoverhandler.h",
        "searchsymbols.cpp",
        "searchsymbols.h",
        "semantichighlighter.cpp",
        "semantichighlighter.h",
        "senddocumenttracker.cpp",
        "senddocumenttracker.h",
        "stringtable.cpp",
        "stringtable.h",
        "symbolfinder.cpp",
        "symbolfinder.h",
        "symbolsfindfilter.cpp",
        "symbolsfindfilter.h",
        "typehierarchybuilder.cpp",
        "typehierarchybuilder.h",
        "usages.h",
        "wrappablelineedit.cpp", // FIXME: Is this used?
        "wrappablelineedit.h",
    ]

    Group {
        name: "TestCase"
        condition: qtc.testsEnabled || project.withAutotests
        files: [
            "cpptoolstestcase.cpp",
            "cpptoolstestcase.h",
        ]
    }

    Group {
        name: "Tests"
        condition: qtc.testsEnabled
        cpp.defines: outer.concat(['SRCDIR="' + FileInfo.path(filePath) + '"'])
        files: [
            "compileroptionsbuilder_test.cpp",
            "compileroptionsbuilder_test.h",
            "cppcodegen_test.cpp",
            "cppcodegen_test.h",
            "cppcompletion_test.cpp",
            "cppcompletion_test.h",
            "cppdoxygen_test.cpp",
            "cppdoxygen_test.h",
            "cppheadersource_test.cpp",
            "cppheadersource_test.h",
            "cppincludehierarchy_test.cpp",
            "cppincludehierarchy_test.h",
            "cpplocalsymbols_test.cpp",
            "cpplocalsymbols_test.h",
            "cpplocatorfilter_test.cpp",
            "cpplocatorfilter_test.h",
            "cppmodelmanager_test.cpp",
            "cppmodelmanager_test.h",
            "cpppointerdeclarationformatter_test.cpp",
            "cpppointerdeclarationformatter_test.h",
            "cppquickfix_test.cpp",
            "cppquickfix_test.h",
            "cppsourceprocessor_test.cpp",
            "cppsourceprocessor_test.h",
            "cppsourceprocessertesthelper.cpp",
            "cppsourceprocessertesthelper.h",
            "cppuseselections_test.cpp",
            "cppuseselections_test.h",
            "fileandtokenactions_test.cpp",
            "fileandtokenactions_test.h",
            "followsymbol_switchmethoddecldef_test.cpp",
            "followsymbol_switchmethoddecldef_test.h",
            "modelmanagertesthelper.cpp",
            "modelmanagertesthelper.h",
            "projectinfo_test.cpp",
            "projectinfo_test.h",
            "symbolsearcher_test.cpp",
            "symbolsearcher_test.h",
            "typehierarchybuilder_test.cpp",
            "typehierarchybuilder_test.h",
        ]
    }

    Export {
        Depends { name: "CPlusPlus" }
        Depends { name: "Qt.concurrent" }
    }
}
