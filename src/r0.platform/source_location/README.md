# source_location
cross-platform pre c++20 `source_location`

It uses :
```cpp
#include <source_location>
```
or
``` cpp
#include <experimental/source_location>
```
if they are available.

[LC]: https://github.com/flagarde/source_location/actions/workflows/Linux-Clang.yml
[LCB]: https://github.com/flagarde/source_location/actions/workflows/Linux-Clang.yml/badge.svg

[LG]: https://github.com/flagarde/source_location/actions/workflows/Linux-GCC.yml
[LGB]: https://github.com/flagarde/source_location/actions/workflows/Linux-GCC.yml/badge.svg

[MC]: https://github.com/flagarde/source_location/actions/workflows/MacOS-Clang.yml
[MCB]: https://github.com/flagarde/source_location/actions/workflows/MacOS-Clang.yml/badge.svg

[MG]: https://github.com/flagarde/source_location/actions/workflows/MacOS-GCC.yml
[MGB]: https://github.com/flagarde/source_location/actions/workflows/MacOS-GCC.yml/badge.svg

[MS]: https://github.com/flagarde/source_location/actions/workflows/Windows-MSYS2.yml
[MSB]: https://github.com/flagarde/source_location/actions/workflows/Windows-MSYS2.yml/badge.svg

[MM]: https://github.com/flagarde/source_location/actions/workflows/Windows-MSVC.yml
[MMB]: https://github.com/flagarde/source_location/actions/workflows/Windows-MSVC.yml/badge.svg

## Builds
|        | Linux Clang | Linux GCC | MacOS Clang | MacOS GCC | Windows M2sys | Windows MSVC |
|--------|-------------|-----------|-------------|-----------|---------------|--------------|
| Github |[![Linux Clang][LCB]][LC]|[![Linux GCC][LGB]][LG]|[![MacOS Clang][MCB]][MC]|[![MacOS GCC][MGB]][MG]|[![Windows MSYS2][MSB]][MS]|[![Windows MSVC][MMB]][MM]|

## Example

```cpp
#include <iostream>
#include <source_location/source_location.hpp>

int main()
{
   source_location loc(source_location::current());
   std::cout << loc.file_name() << std::endl;
   std::cout << loc.function_name() << std::endl;
   std::cout << loc.line() << std::endl;
}
```
## Tests
 * On `Linux` :
    * `gcc-9` : `c++98`, `c++11`, `c++17`, `c++20`
    * `gcc-10` : `c++98`, `c++11`, `c++17`, `c++20`
    * `clang-10` : `c++98`, `c++11`, `c++17`, `c++20`
    * `clang-11` : `c++98`, `c++11`, `c++17`, `c++20`
  * On `MacOS` :
    * `gcc-9` : `c++98`, `c++11`, `c++17`, `c++20`
    * `gcc-10` : `c++98`, `c++11`, `c++17`, `c++20`
    * `gcc-11` : `c++98`, `c++11`, `c++17`, `c++20`
    * `clang-12` : `c++98`, `c++11`, `c++17`, `c++20`
  * On `Windows` :
    * `MSVC 19.28.29915.0` : `c++98`, `c++11`, `c++17`, `c++20`
    * `MSYS2 GNU 10.2.0` : `c++98`, `c++11`, `c++17`, `c++20`
    * `MSYS2 Clang 12.0.0` : `c++98`, `c++11`, `c++17`, `c++20`
