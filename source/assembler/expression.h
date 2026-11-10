#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace c8 {

/// What a part of an expression is
enum class PartKind : uint8_t {
    number,  ///< A number literal: 2A, 0x2A, #42, #-1, %0101, or %..##. Its digits can still be wrong.
    name,    ///< A label or a constant. A reserved word (a register, an instruction) is an error later.
    op,      ///< An operator: + - * / % & | ^ ~ << >>
    open,    ///< (
    close,   ///< )
    invalid, ///< A character that cannot be in an expression
};

/// One part of an expression
struct ExpressionPart {
    PartKind         kind   = PartKind::invalid;
    std::string_view text;       ///< The part as written. It points into the operand.
    uint32_t         column = 0; ///< The zero-based column of the first code point
    uint32_t         length = 0; ///< The length in code points
};

/// An error in an expression, at the columns of its cause
struct ExpressionError {
    std::string message;
    uint32_t    column = 0;
    uint32_t    length = 0;
};


/// One step of the evaluation of an expression
struct ExpressionStep {
    enum class Kind : uint8_t {
        number, ///< Push the value
        name,   ///< Push the value of the name
        unary,  ///< Replace the top value
        binary, ///< Replace the two top values
    };

    Kind        kind   = Kind::number;
    char        op     = 0;  ///< + - * / % & | ^ ~, '<' for <<, '>' for >>
    int64_t     value  = 0;  ///< The value of a number
    std::string name;        ///< The name as written
    uint32_t    column = 0;  ///< The columns of the number, the name, or the operator
    uint32_t    length = 0;
};

/// A parsed expression. The steps are in postfix order.
struct Expression {
    std::vector<ExpressionStep> steps;
};


/// The value of a name, or a message that tells why the name has none
using NameLookup = std::function<std::expected<int64_t, std::string>(std::string_view name)>;


/**
 * @brief Split an operand into the parts of an expression
 *
 * @details Where a value can start, '%' starts a binary number and '#' starts
 *          a decimal number. After a value, '%' is the remainder operator.
 *          A word of letters, digits, '_', and '.' is a number if it starts
 *          with a digit or holds only hex digits. Otherwise it is a name.
 *          Spaces between the parts are allowed. They can occur only inside
 *          parentheses, because a space ends an operand.
 *
 * @param[in] text    The operand, for example "(x + 1)"
 * @param[in] column  The column of the first character of the operand
 */
[[nodiscard]]
auto split_expression(std::string_view text, uint32_t column = 0) -> std::vector<ExpressionPart>;

/**
 * @brief Parse an operand as an expression
 *
 * @details The operators have the precedence of C, from high to low:
 *          unary `- + ~`, then `* / %`, `+ -`, `<< >>`, `&`, `^`, and `|`.
 *          Parentheses group the parts. A name of a register, an instruction,
 *          a directive, or `long` is an error.
 *
 *          A unary '-' just before a number without a prefix is an error,
 *          because "-10" would be -0x10. Write #-10 or -0x10.
 *
 * @param[in] text    The operand
 * @param[in] column  The column of the first character of the operand
 */
[[nodiscard]]
auto parse_expression(std::string_view text, uint32_t column = 0) -> std::expected<Expression, ExpressionError>;

/**
 * @brief Calculate the value of an expression
 *
 * @details Division and the remainder round toward zero, as in C. `>>` keeps
 *          the sign. A value larger than 2^48 is an error, so the calculation
 *          never overflows.
 *
 * @param[in] expression  A parsed expression
 * @param[in] lookup      Gives the value of each name
 */
[[nodiscard]]
auto evaluate(const Expression& expression, const NameLookup& lookup) -> std::expected<int64_t, ExpressionError>;

} // namespace c8
