#include "codedoc.hpp"

using codedoc::CodeDocumentation;
using codedoc::CodeDocumentationString;
using codedoc::ParameterDocumentation;

namespace comprehensive_test
{

// 1. Nested namespace & linkable ancestor testing
namespace nested
{
    struct[[= CodeDocumentation {.brief = "A struct inside a nested namespace."}]] NestedStruct
    {};
}

// 2. Union support
union[[= CodeDocumentation {.brief = "A union type test."}]] TestUnion
{
    int   int_val;
    float float_val;
};

// 3. Enumeration with underlying type and documented enumerators
enum class[[= CodeDocumentation {.brief = "An enumerated type with underlying type."}]] Color : std::uint8_t
{
    Red   = 1,
    Green = 2,
    Blue  = 3
};

// 4. Type alias test
using FloatArray = float[3];

// 5. Templates (Class templates, function templates, variable templates, alias templates)
template <typename T, std::size_t Size>
struct[[= CodeDocumentation {
  .brief   = "A class template with multiple template parameters.",
  .tparams = {{{"T", "Element type."}, {"Size", "Array dimension size."}}}
}]] Buffer
{
    T data[Size];
};

template <typename T>
[[= CodeDocumentation {.brief = "A function template.", .returns = "The doubled value."}]] T double_value(
  [[= ParameterDocumentation {.brief = "Input value to double."}]] T val
)
{
    return val * 2;
}

template <typename T>
constexpr T gravity_v = T(9.81);

template <typename T>
using Matrix = T* [4];

using DoubleMatrix = Matrix<double>;

struct[[= CodeDocumentation {.brief = "Never defined type."}]] IncompleteType;

// 6. Comprehensive class testing all Doxygen fields, access specifiers, and method qualifiers
struct[[= CodeDocumentation {
  .brief       = "Comprehensive feature test class.",
  .description = "Tests all documentation fields like "
                 + CodeDocumentationString::Code("pre")
                 + ", "
                 + CodeDocumentationString::Code("post")
                 + ", "
                 + CodeDocumentationString::Bold("warning")
                 + ", and references like "
                 + CodeDocumentationString::MakeReference<^^Buffer>()
                 + ".",
  .returns     = "Various return values.",
  .pre         = "Precondition: object must be valid.",
  .post        = "Postcondition: state remains consistent.",
  .throws      = "Throws " + CodeDocumentationString::Code("std::invalid_argument") + " on failure.",
  .notes       = "Note: handle with care.",
  .warning     = "Warning: experimental API.",
  .since       = "v2.0",
  .deprecated  = "Deprecated in favor of new API.",
  .see         = CodeDocumentationString::MakeReference<^^Color>(),
  .tparams     = {}
}]] AdvancedClass
{
  private:
    [[= CodeDocumentation {.brief = "Private data member."}]] int m_private_data {};

  protected:
    [[= CodeDocumentation {.brief = "Protected mutable cache member."}]] mutable int m_cache {};

  public:
    [[= CodeDocumentation {.brief = "Static data member."}]] static int      s_instance_count;
    [[= CodeDocumentation {.brief = "Bit-field data member."}]] unsigned int m_flag : 1;

    // Constructors and Destructor
    [[= CodeDocumentation {.brief = "Default constructor."}]] AdvancedClass() = default;

    [[= CodeDocumentation {
      .brief = "Explicit converting constructor."
    }]] explicit AdvancedClass([[= ParameterDocumentation {.brief = "Initial data value."}]] int data) noexcept
            : m_private_data(data)
    {}

    [[= CodeDocumentation {.brief = "Deleted constructor."}]] AdvancedClass(double)   = delete;

    [[= CodeDocumentation {.brief = "Virtual destructor."}]] virtual ~AdvancedClass() = default;

    // Complex method qualifiers: const, volatile, ref-qualified, noexcept
    [[= CodeDocumentation {
      .brief = "Method with extensive cv and reference qualifiers.", .returns = "Current private data value."
    }]] int get_data() const volatile& noexcept
    {
        return m_private_data;
    }

    // Rvalue reference-qualified method
    void                                                                             consume() && {}

    // Virtual and pure virtual methods
    [[= CodeDocumentation {.brief = "Pure virtual interface method."}]] virtual void process() = 0;
};

// 7. Derived class testing override and final specifiers
struct[[= CodeDocumentation {.brief = "Derived test class."}]] DerivedClass : public AdvancedClass
{
    void process() override final
    {
        // Implementation
    }
};

// 8. Ignored entity test (.ignore = true)
struct[[= CodeDocumentation {.brief = "Should be ignored in output.", .ignore = true}]] IgnoredStruct
{
    int hidden_field;
};

// 9. Overloaded functions
float         overloaded(float val1, float val2);
double        overloaded(double, double);

constexpr int default_argument {42};

[[= CodeDocumentation {
  .brief = "overloaded function with default argument.",
  .see   = "Float version: "
           + CodeDocumentationString::MakeReference<
             std::meta::reflect_function(static_cast<float (&)(float, float)>(overloaded))>()
           + CodeDocumentationString::LineBreak()
           + "Double version: "
           + CodeDocumentationString::MakeReference<
             std::meta::reflect_function(static_cast<double (&)(double, double)>(overloaded))>()
}]] int
  overloaded(
    [[= ParameterDocumentation {
      .brief       = "This function argument has a description",
      .description = "This is a description for the function argument."
    }]] int val1,
    [[= ParameterDocumentation {
      .brief = "Has a default argument of "
               + CodeDocumentationString::MakeReference<^^default_argument>()
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
