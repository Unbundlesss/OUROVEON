# cereal-optional
Optional NVP for cereal

Usage:
```
// contains base Optional NVP definition
#include <cereal_optional/optional_nvp.hpp>

// contains definitions required by JSON archives
#include <cereal_optional/optional_json.hpp>

// contains definitions required by binary archives
#include <cereal_optional/optional_binary.hpp>


struct MyAwesomeStruct {
  std::string optional;
  
  template<class A>
  void serialize(A &archive) {
    archive(CEREAL_OPTIONAL_NVP(optional)
      // or CEREAL_OPTIONAL_NVP(optional, "my default value")
      // or cereal::make_optional_nvp("foo", optional)
      // or cereal::make_optional_nvp("foo", optional, "my default value")
      );
  }
};
```

Repo contains a CMake project, you just have to link `cereal-optional` project with yours.
