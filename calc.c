// Elgun Gurbanov 251ADB148
// https://github.com/equrbanofff/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT; course materials
// https://cp-algorithms.com/algebra/binary-exp.html
// https://en.cppreference.com/w/c/string/byte/isdigit.html
// https://en.cppreference.com/w/c/string/byte/strtol.html
// https://www.geeksforgeeks.org/compiler-design/recursive-descent-parser/
// https://en.cppreference.com/w/c/io/fopen.html
// https://man7.org/linux/man-pages/man2/mkdir.2.html
// https://en.cppreference.com/w/c/io/fgets.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

typedef struct {
    char *text;
    size_t pos;
    size_t base_pos;
    int error;
    size_t error_pos;
} Parser;

//skips spaces because spaces and tabs may appear between tokens
void skip_spaces(Parser *p)
{
    while (p->text[p->pos] == ' ' || p->text[p->pos] == '\t')
        p->pos++;
}

long long parse_expression(Parser *p);

//binary exponentiation makes integer power faster
//reference: https://cp-algorithms.com/algebra/binary-exp.html
long long int_power(long long base, long long exponent)
{
    long long result = 1;
    while (exponent > 0) {
        if (exponent % 2 == 1)
            result *= base;
        base *= base;
        exponent /= 2;
    }
    return result;
}

//isdigit is used to reject unary operators and invalid number starts
//reference: https://en.cppreference.com/w/c/string/byte/isdigit.html
long long parse_number(Parser *p)
{
    skip_spaces(p);

    if (!isdigit((unsigned char)p->text[p->pos])) {
        p->error = 1;
        //base_pos changes line position to full file position
        p->error_pos = p->base_pos + p->pos;
        return 0;
    }

    char *start = p->text + p->pos;
    char *end;

    //strtoll gives both the integer and where the number ends
    //reference: https://en.cppreference.com/w/c/string/byte/strtol.html
    long long value = strtoll(start, &end, 10);
    p->pos = (size_t)(end - p->text);

    return value;
}

//primary handles numbers and expressions inside parentheses
long long parse_primary(Parser *p)
{
    skip_spaces(p);
    char c = p->text[p->pos];

    if (isdigit((unsigned char)c))
        return parse_number(p);

    if (c == '(') {
        p->pos++;

        //recursive expression allows nested parentheses
        long long value = parse_expression(p);
        if (p->error)
            return 0;

        skip_spaces(p);

        //an opening parenthesis must have a closing parenthesis
        if (p->text[p->pos] != ')') {
            p->error = 1;
            p->error_pos = p->base_pos + p->pos;
            return 0;
        }

        p->pos++;
        return value;
    }

    //anything other than a number or '(' is invalid here
    p->error = 1;
    p->error_pos = p->base_pos + p->pos;
    return 0;
}

//recursive descent separates operators by precedence level
//reference: https://www.geeksforgeeks.org/compiler-design/recursive-descent-parser/
long long parse_power(Parser *p)
{
    long long left = parse_primary(p);
    if (p->error)
        return 0;

    skip_spaces(p);

    if (p->text[p->pos] == '*' && p->text[p->pos + 1] == '*') {
        p->pos += 2;
        skip_spaces(p);

        //error for negative exponent must point to exponent start
        size_t exponent_pos = p->base_pos + p->pos;

        //calling parse_power again makes ** right-associative
        long long exponent = parse_power(p);
        if (p->error)
            return 0;

        if (exponent < 0) {
            p->error = 1;
            p->error_pos = exponent_pos;
            return 0;
        }

        return int_power(left, exponent);
    }

    return left;
}

//term gives multiplication and division higher precedence than + and -
long long parse_term(Parser *p)
{
    long long value = parse_power(p);
    if (p->error)
        return 0;

    while (1) {
        skip_spaces(p);

        //do not treat the first star of ** as normal multiplication
        if (p->text[p->pos] == '*' && p->text[p->pos + 1] == '*')
            break;

        char op = p->text[p->pos];
        if (op != '*' && op != '/')
            break;

        p->pos++;
        skip_spaces(p);

        //division by zero reports the start of the right operand
        size_t right_operand_pos = p->base_pos + p->pos;
        long long right = parse_power(p);

        if (p->error)
            return 0;

        if (op == '*') {
            value *= right;
        } else {
            if (right == 0) {
                p->error = 1;
                p->error_pos = right_operand_pos;
                return 0;
            }

            //integer operands use normal c integer division
            value /= right;
        }
    }

    return value;
}

//expression is the lowest precedence level
long long parse_expression(Parser *p)
{
    long long value = parse_term(p);
    if (p->error)
        return 0;

    while (1) {
        skip_spaces(p);

        char op = p->text[p->pos];
        if (op != '+' && op != '-')
            break;

        p->pos++;

        long long right = parse_term(p);
        if (p->error)
            return 0;

        if (op == '+')
            value += right;
        else
            value -= right;
    }

    return value;
}

//a comment is valid only when # is the first non-blank character
int line_is_blank_or_comment(char *line)
{
    size_t i = 0;

    while (line[i] == ' ' || line[i] == '\t')
        i++;

    if (line[i] == '\n' || line[i] == '\0')
        return 1;

    if (line[i] == '#')
        return 1;

    return 0;
}

int main(int argc, char *argv[])
{
    //this version accepts calc -o OUTDIR input.txt
    if (argc != 4) {
        fprintf(stderr, "Usage: %s -o OUTDIR input.txt\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        return 1;
    }

    const char *output_dir = argv[2];
    const char *input_path = argv[3];

    //fopen returns null when the input file cannot be opened
    //reference: https://en.cppreference.com/w/c/io/fopen.html
    FILE *input = fopen(input_path, "r");

    if (input == NULL) {
        fprintf(stderr, "Cannot open input file: %s\n", input_path);
        return 1;
    }

    //mkdir creates the requested output directory
    //reference: https://man7.org/linux/man-pages/man2/mkdir.2.html
    mkdir(output_dir, 0777);

    char output_path[512];

    snprintf(
        output_path,
        sizeof(output_path),
        "%s/input_elgun_gurbanov_251ADB148.txt",
        output_dir
    );

    FILE *output = fopen(output_path, "w");

    if (output == NULL) {
        fprintf(stderr, "Cannot create output file\n");
        fclose(input);
        return 1;
    }

    char line[1024];

    //positions are 1-based and ignored lines still count
    size_t file_pos = 1;

    //fgets keeps the newline so exact file positions can be counted
    //reference: https://en.cppreference.com/w/c/io/fgets.html
    while (fgets(line, sizeof(line), input) != NULL) {
        if (line_is_blank_or_comment(line)) {
            file_pos += strlen(line);
            continue;
        }

        Parser parser;
        parser.text = line;
        parser.pos = 0;
        parser.base_pos = file_pos;
        parser.error = 0;
        parser.error_pos = 0;

        long long result = parse_expression(&parser);

        if (!parser.error) {
            skip_spaces(&parser);
            char c = parser.text[parser.pos];

            //anything left after a valid expression is an error
            if (c == ')') {
                parser.error = 1;
                parser.error_pos = parser.base_pos + parser.pos;
            } else if (c == '#') {
                parser.error = 1;
                parser.error_pos = parser.base_pos + parser.pos;
            } else if (c != '\0' && c != '\n') {
                parser.error = 1;
                parser.error_pos = parser.base_pos + parser.pos;
            }
        }

        if (parser.error) {
            //reopening with w removes earlier results after the first error
            fclose(output);
            output = fopen(output_path, "w");

            if (output == NULL) {
                fclose(input);
                return 1;
            }

            fprintf(output, "ERROR:%zu\n", parser.error_pos);
            fclose(output);
            fclose(input);
            return 0;
        }

        fprintf(output, "%lld\n", result);
        file_pos += strlen(line);
    }

    fclose(output);
    fclose(input);
    return 0;
}