/*=============================================================================
===============================================================================
                         GENERAL CODE STYLE GUIDELINES

- Use 4 spaces for indentation, no tabs.
- Use snake_case for namespaces, classes, and methods.
- Use camelCase for variables, prefix with 'm' for members.
- Always ensure variables have an initial value.
- No inline variable declarations (int x, y, z;)
- Use ALL_CAPS for constants, enumeration values, and defines.
- Use braces for all control statements, even if they are one line.
- Maximum line length is 120 characters, recommend ~100 characters.
- Code should be self-documenting and not rely on comments to understand.
  > Naming should be descriptive, but not too long. Avoid abbreviations not widely understood.
  > Using single letter variables for math equations is only acceptable if the equation is
    provided and easy to follow. Complex math should still use descriptive variable names.
- Avoid consistant nesting of code, adopt the 'early out' pattern unless there is a good reason
  not to.
- Comments explain why something is the way it is and/or provide additional context.
- Arguments should be on the same line as the function name, unless it exceeds the maximum line
  length. If so, move all arguments to new lines with two spaces of indentation, and keep the
  closing parenthesis on a new line, followed by the opening brace on the same line.
- Use line spacing appropriately to keep code readable.
  > If you translate your code to 'English', consider a blank line separation when a sentance
    would end. Eg:

    [English]
    We need to get check if the player has reached the goal on [2,4]. To do that, we need to
    get the row and column values. We get that with some math from the cell index and the grid
    width.

    [Code]
    // Variable initilization/declaration
    const int gridLocationIndex = gameBoard.get_grid_location_index();
    const int gridWidth = gameBoard.get_grid_width();

    // Extract the X and Y location
    const int gridRow = gridLocationIndex / gridWidth;
    const int gridColumn = gridLocationIndex % gridWidth;

    // Use the values
    if (gridRow == 2 && gridColumn == 4) {
        std::cout << "Well done! You reached the goal!";
        return true;
    }

    // Fail state
    return false;



===============================================================================
===============================================================================
                                   COMMENTS

All comments should be in English and remain clear and concise. Avoid using comments
to explain the code as it should be self-explanatory. Instead, comments should add context,
explain why it is done a certain way, or to help break down complex logic. All comments
should use proper grammar, spelling, and punctuation.


// Simple comment

// Simple multi-lined commment that isn't too long, but is still a bit longer
// than a single line comment. Recommend ~3 lines maximum before using a multi-
// lined comment block.

/*
 * Use multi-line comments for longer explanations, or to comment out code.
 *\/


-------------------------------------------------------------------------------

Inline comments should be used sparingly, watch the character length for the line!
Use the next tab stop between the code and the comment. If there is one space to the next
tab stop, use the next tab stop. Align inline comments when used across multiple lines of
code.


some_code_here();   // Inline comment at the end of a line of code.

[other code...]

some_other_code_here();     // Inline comment at the end of a line of code.
some_more_code_here();      // Another aligned with above comment.

code();                     // Another aligned with above comment, even after an empty line.


-------------------------------------------------------------------------------

Heading comments are used to break up code into clear sections, usually also in combination
with a #region/#endregion block.

/*===========================================================================*\/
/*===========================================================================*\/
#region                           MATHAMATICS
[code goes here...]
/*===========================================================================*\/
/*===========================================================================*\/
#endregion // MATHAMATICS

- Or, for smaller separation:

/*---------------------------------------------------------------------------*\/
// Fast-Fourier Transform
[code goes here...]


===============================================================================
===============================================================================
                               ADDITIONAL NOTES

If you need to move even one argument to a new line, do the same for all of them. Indent
each argument by two spaces instead of four, keep the closing parenthesis on a new line,
followed by the opening brace on the same line.

// OK - fits the 120 character limit (though could be considered long as it is above 100)
const int short_function(int argument_one, int argument_two = 20, int argument_three = 40, int argument_four = 60) {
    [code goes here...]
}

// BAD - exceeds line length limit
const int long_function_name_with_lots_of_arguments(int argument_one, int argument_two = 20, int argument_three = 40, int argument_four = 60) {
    [code goes here...]
}
// BAD - fits on one line, but only one argument was shifted to a new line
const int long_function_name_with_lots_of_arguments(int argument_one, int argument_two = 20, int argument_three = 40,
  int argument_four = 60
) {
    [code goes here...]
}
// BAD - fits on one line, but separate arguments on multiple lines
const int long_function_name_with_lots_of_arguments(
  int argument_one, int argument_two = 20,
  int argument_three = 40, int argument_four = 60
) {
    [code goes here...]
}
// OK - All arguments shifted, much less confusing
const int long_function_name_with_lots_of_arguments(
  int argument_one,
  int argument_two = 20,
  int argument_three = 40,
  int argument_four = 60
) {
    [code goes here...]
}

-------------------------------------------------------------------------------

Same for deeply nested code:
                                                    some_really_deeply_nested_code_with_arguments(
                                                      argument_one_on_a_new_line_with_TWO_spaces_of_indentation,
                                                      argument_two_on_a_new_line_with_TWO_spaces_of_indentation,
                                                      argument_three_on_a_new_line_with_TWO_spaces_of_indentation
                                                    ) {
                                                        // Code nested this deep should really, really be avoided...
                                                    }

-------------------------------------------------------------------------------



*/


/*===========================================================================*/
/*===========================================================================*/
#region                           .h EXAMPLE

#pragma once

// Imports at the top, starting with your own classes, then system/framework classes.
#import "my_class_b.h"
#import <stdio.h>

// Forward declarations of classes, structs, enums, etc. that are used in this file.
class my_class_c;

// Namespaces use snake_case.
namespace my_unified_namespace_name {

    // Classes use snake_case.
    class my_class_a : public my_class_b {
        // Always start with an access modifier for classes, even if it's private.
        private:

            // Variables before methods, always initilize where possible.
            // Variables use camelCase, prefix with 'm' for members.
            // Names should be descriptive, but not too long. Avoid abbreviations.
            int mIntVariable = 0;

            // Constants use ALL_CAPS.
            // Names should be descriptive, but not too long. Avoid abbreviations.
            const int A_CONSTANT_INT = 42;

            // Pointer (*) and reference (&) symbols should be next to the variable
            // name.
            my_class_c *mClassCInstance = nullptr;
            
        // When changing between 'variables', 'constructors/destructors', and
        // 'methods' section, always start with an access modifier, even if it
        // is the same as the previous section.
        public:
            // Constructors and destructors are immediately after the variables, and
            // before any methods. These should be defined in the .cpp file, even if
            // they are empty.
            my_class_a();
            ~my_class_a();

        public:
            // Methods use snake_case.
            // Public methods should be documented with a <summary> block.

            /// <summary>
            /// A public method defined in the .cpp file.
            /// </summary>
            void my_public_method_defined_in_cpp();

            /// <summary>
            /// A public method defined in the .hpp file.
            /// </summary>
            void my_public_method_defined_in_hpp() const {
                // Prefer early returns to reduce nesting and improve readability.
                // Single line statements should also be braced and on a new line for
                // readability.
                if (!some_method_inherited_from_class_b()) {
                    return;
                }
                my_private_method_defined_in_cpp();
            }

        private:
            // Private methods do not require a <summary> block, but recommended if
            // the method is complex or important.
            void my_private_method_defined_in_cpp() const;
    };
}

/*===========================================================================*/
/*===========================================================================*/
#endregion // .h EXAMPLE

/*===========================================================================*/
/*===========================================================================*/
#region                        .cpp EXAMPLE

#include "my_class_a.h"
#include "my_class_c.h"

// Use namespaces like this within the .cpp of the class, unless there is a good
// reason not to (such as name collisions).
using namespace my_unified_namespace_name;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

// Implement the constructor and destructor first.
// Leave braces on the same line when empty.

my_class_a::my_class_a() :  // Add colon on primary line, then...
    my_class_b() {}         // continue on the next line. Add comma and repeat if required.

my_class_a::~my_class_a() {
    // Pretend code is here to clean up class...
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void my_class_a::my_public_method_defined_in_cpp() {
    // Pretend code is here to do something...
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void my_class_a::my_private_method_defined_in_cpp() const {
    // Pretend code is here to do something...
}

/*===========================================================================*/
/*===========================================================================*/
#endregion // .cpp EXAMPLE


/*===========================================================================*/
/*===========================================================================*/
//                             OTHER EXAMPLES

// Use structs for data structures. They can have simple methods, but should not have 
// complex methods or inheritance - use classes for that! Access specifier isn't needed for
// inital public:, unless you also use the private: specifier
struct my_struct {
    int variable_a = 0;
    float variable_b = 0.0f;
    std::string variable_c = "Hello World";
}

// Enums should be capitalized as they are constants and must use 'enum class'. It is
// strongly recommended to use a specific underlying type (like short, int, etc.), but not
// required.
enum class my_enum : short {
    VALUE_A,
    VALUE_B = 10,
    VALUE_C
}

// Enums used as flags don't require 'enum class', however it can cause code pollution so
// do so at your discretion. They should always start at bit 1 and can be compounded using
// the OR operator.
enum my_enum_as_flags : unsigned int {
    FLAG_FOR_X = 1 << 0,
    FLAG_FOR_Y = 1 << 1,
    FLAG_FOR_Z = 1 << 2,
    FLAG_FOR_XYZ = FLAG_FOR_X | FLAG_FOR_Y | FLAG_FOR_Z
}

// A type states its own copy and move behaviour with '= delete' and '= default'; there is no
// base class for it, so a reader sees in the class what it allows. A type that owns something
// and may change hands, but must never be duplicated, deletes its copy operations and defaults
// its moves. The moves must be written out: deleting the copies alone removes them too. They
// are 'noexcept' (docs/CONVENTIONS.md section 3): a move that cannot throw lets std::vector keep
// its strong guarantee when it grows, and for a type that can also be copied it is what makes
// the vector move instead of copy. These lines sit with the constructors and stay in the header.
class my_non_copyable_but_movable_class {
    private:
        // Defaulted moves are only correct when every member moves correctly by itself, as a
        // standard container or a std::unique_ptr does. A type holding a raw handle writes its
        // two move operations by hand in the .cpp instead.
        std::vector<float> mSamples;

    public:
        my_non_copyable_but_movable_class();
        ~my_non_copyable_but_movable_class();

        my_non_copyable_but_movable_class(const my_non_copyable_but_movable_class &) = delete;
        my_non_copyable_but_movable_class &operator=(const my_non_copyable_but_movable_class &) = delete;

        my_non_copyable_but_movable_class(my_non_copyable_but_movable_class &&) noexcept = default;
        my_non_copyable_but_movable_class &operator=(my_non_copyable_but_movable_class &&) noexcept = default;
};

// A type whose address is handed out (other objects keep a pointer or reference to it) writes
// the same four lines with '= delete' on the two move operations as well.