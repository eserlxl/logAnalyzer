- [ ] Sanitize XML element names in exportAsXml and add escaping/custom-field coverage
      Mode: repair
      Surfaces: src/export/xml.cpp, tests/export/core.cpp
      Status: Rejected
      Rejection: Mis-scoped surface. src/export/xml.cpp is dead code — src/CMakeLists.txt file(GLOB_RECURSE) compiles it, but its Exporter::exportAsXml is a duplicate definition that the linker never pulls (its only external symbol is already provided by src/export/json.cpp:236, which is linked for exportAsJson). Edits to xml.cpp have zero runtime effect (verified: output still came from json.cpp's <log>/uppercase-tag version). The real exporter with the key-sanitization bug is src/export/json.cpp:236; the dead duplicate (a latent ODR violation) must be removed separately. Re-scope to src/export/json.cpp.
