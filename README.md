# codedoc
C++26 reflection documentation generator

## Disclaimer

Prototype. Not production ready.

## Usage

```cpp
#include "codedoc.hpp"
using codedoc::CodeDocumentation;
using codedoc::ParameterDocumentation;
using codedoc::CodeDocumentationString;

namespace mylib {

struct [[=CodeDocumentation{
    .brief       = "A two component vector.",
    .description = "Stores " + CodeDocumentationString::Code("x") + " and "
                 + CodeDocumentationString::Code("y") + ". See also "
                 + CodeDocumentationString::MakeReference<^^Mat1>() + ".",
    .tparams     = {{ {"T", "Arithmetic element type."} }}
}]] Vec1 {
    [[=CodeDocumentation{.brief = "Horizontal component."}]] float x;
    float y;                                    // undocumented members are still listed

    [[=CodeDocumentation{
        .brief   = "Scales the vector.",
        .returns = "Reference to the scaled vector.",
        .pre     = "factor is finite.",
        .post    = "The length is multiplied by abs(factor).",
    }]] Vec1& scale([[=ParameterDocumentation{.brief = "The factor to scale by."}]] float factor);
};

} // namespace mylib

int main() { codedoc::WriteDocumentation<^^mylib>("docs"); }
```

## Notes

* Designated initializers must follow declaration order of CodeDocumentation:
  brief, description, ignore, returns, pre, post, throws, notes, warning,
  since, deprecated, see, tparams.
* `.tparams` is a std::array, hence the double braces.
* Function parameters are documented by annotating the parameter itself with
  ParameterDocumentation (`.brief` / `.description`), not CodeDocumentation.
* Contract assertions (P2899 pre/post) are NOT reflectable in C++26, so
  preconditions/postconditions come from `.pre` / `.post`.
* Template parameters are not reflectable either, so they come from
  `.tparams`. For specializations the actual template arguments are listed.
* CodeDocumentationString contains std::meta::info and is therefore a
  consteval-only type: use it in annotations / consteval code only.
* Big trees can exceed the default constexpr step limit; raise it
  (e.g. -fconstexpr-steps / -fconstexpr-ops-limit).
* Passing the global namespace documents everything visible, std included.
  Document your own namespace instead.

