
-- defines the msbuild includes used to bind in ISPC support
externalrule "ispc"
    location "../ispc-msbuild/"
    display "Intel SPMD Programm Compiler"
    fileextension ".ispc"

    propertydefinition {
      name = "GenerateDebugInformation",
      kind = "boolean"
    }
    propertydefinition {
      name = "WarningLevel",
      kind = "integer"
    }
    propertydefinition {
      name = "Architecture",
      kind = "string"
    }
    propertydefinition {
      name = "OS",
      kind = "string"
    }
    propertydefinition {
      name = "CPU",
      kind = "string"
    }
    propertydefinition {
      name = "TargetISA",
      kind = "string"
    }
    propertydefinition {
      name = "Opt",
      kind = "string"
    }
    propertydefinition {
      name = "MathLibrary",
      kind = "string"
    }



-- mark the active component to include and use the ISPC msbuild machinery
function useISPC()
    rules { "ispc" }
end
