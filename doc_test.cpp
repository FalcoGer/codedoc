#include "codedoc.hpp"

using Doc    = codedoc::CodeDocumentation;
using DocStr = codedoc::CodeDocumentationString;
using PDoc   = codedoc::ParameterDocumentation;

namespace comprehensive_test
{

// 1. Nested namespace & linkable ancestor testing
namespace nested
{
    struct[[= Doc {.brief = "A struct inside a nested namespace."}]] NestedStruct
    {};
}

// 2. Union support
union[[= Doc {.brief = "A union type test."}]] TestUnion
{
    int   int_val;
    float float_val;
};

// 3. Enumeration with underlying type and documented enumerators
enum class[[= Doc {.brief = "An enumerated type with underlying type."}]] Color : std::uint8_t
{
    Red   = 1,
    Green = 2,
    Blue  = 3
};

// 4. Type alias test
using FloatArray = float[3];

// 5. Templates (Class templates, function templates, variable templates, alias templates)
template <typename T, std::size_t Size>
struct[[= Doc {
  .brief   = "A class template with multiple template parameters.",
  .tparams = {{{"T", "Element type."}, {"Size", "Array dimension size."}}}
}]] Buffer
{
    T data[Size];
};

template <typename T>
[[= Doc {.brief = "A function template.", .returns = "The doubled value."}]] T double_value(
  [[= PDoc {.brief = "Input value to double."}]] T val
)
{
    return val * 2;
}

template <typename T>
constexpr T gravity_v = T(9.81);

template <typename T>
using Matrix       = T* [4];

using DoubleMatrix = Matrix<double>;

struct[[= Doc {.brief = "Never defined type."}]] IncompleteType;

// 6. Comprehensive class testing all Doxygen fields, access specifiers, and method qualifiers
struct[[= Doc {
  .brief       = "Comprehensive feature test class.",
  .description = "Tests all documentation fields like "
                 + DocStr::Code("pre")
                 + ", "
                 + DocStr::Code("post")
                 + ", "
                 + DocStr::Bold("warning")
                 + ", and references like "
                 + DocStr::MakeReference<^^Buffer>()
                 + ".",
  .returns     = "Various return values.",
  .pre         = "Precondition: object must be valid.",
  .post        = "Postcondition: state remains consistent.",
  .throws      = "Throws " + DocStr::Code("std::invalid_argument") + " on failure.",
  .notes       = "Note: handle with care.",
  .warning     = "Warning: experimental API.",
  .since       = "v2.0",
  .deprecated  = "Deprecated in favor of new API.",
  .see         = DocStr::MakeReference<^^Color>(),
  .tparams     = {}
}]] AdvancedClass
{
  private:
    [[= Doc {.brief = "Private data member."}]] int m_private_data {};

  protected:
    [[= Doc {.brief = "Protected mutable cache member."}]] mutable int m_cache {};

  public:
    [[= Doc {.brief = "Static data member."}]] static int      s_instance_count;
    [[= Doc {.brief = "Bit-field data member."}]] unsigned int m_flag : 1;

    // Constructors and Destructor
    [[= Doc {.brief = "Default constructor."}]] AdvancedClass() = default;

    [[= Doc {
      .brief = "Explicit converting constructor."
    }]] explicit AdvancedClass([[= PDoc {.brief = "Initial data value."}]] int data) noexcept
            : m_private_data(data)
    {}

    [[= Doc {.brief = "Deleted constructor."}]] AdvancedClass(double)   = delete;

    [[= Doc {.brief = "Virtual destructor."}]] virtual ~AdvancedClass() = default;

    // Complex method qualifiers: const, volatile, ref-qualified, noexcept
    [[= Doc {
      .brief = "Method with extensive cv and reference qualifiers.", .returns = "Current private data value."
    }]] int get_data() const volatile& noexcept
    {
        return m_private_data;
    }

    // Rvalue reference-qualified method
    void                                                               consume() && {}

    // Virtual and pure virtual methods
    [[= Doc {.brief = "Pure virtual interface method."}]] virtual void process() = 0;
};

// 7. Derived class testing override and final specifiers
struct[[= Doc {.brief = "Derived test class."}]] DerivedClass : public AdvancedClass
{
    void process() override final
    {
        // Implementation
    }
};

// 8. Ignored entity test (.ignore = true)
struct[[= Doc {.brief = "Should be ignored in output.", .ignore = true}]] IgnoredStruct
{
    int hidden_field;
};

// 9. Overloaded functions
float         overloaded(float val1, float val2);
double        overloaded(double, double);

constexpr int default_argument {42};

[[= Doc {
  .brief = "overloaded function with default argument.",
  .see   = "Float version: "
           + DocStr::MakeReference<std::meta::reflect_function(static_cast<float (&)(float, float)>(overloaded))>()
           + DocStr::LineBreak()
           + "Double version: "
           + DocStr::MakeReference<std::meta::reflect_function(static_cast<double (&)(double, double)>(overloaded))>()
}]] int
  overloaded(
    [[= PDoc {
      .brief       = "This function argument has a description",
      .description = "This is a description for the function argument."
    }]] int val1,
    [[= PDoc {
      .brief = "Has a default argument of "
               + DocStr::MakeReference<^^default_argument>()
               + " = "
               + std::define_static_string(std::format("{}", default_argument))
    }]] int val2 = default_argument
  )
{
    return val1 + val2;
}

float overloaded(float val1, float val2)
{
    return val1 + val2;
}
double overloaded(double val1, double val2)
{
    return val1 + val2;
}

}    // namespace comprehensive_test

int main()
{
    // Generate documentation for the comprehensive test suite
    codedoc::WriteDocumentation<^^comprehensive_test>("docs");
    codedoc::WriteDocumentation<^^codedoc>("inception");
}
