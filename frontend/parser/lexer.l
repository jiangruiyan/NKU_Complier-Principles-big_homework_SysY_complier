%{
    #include <frontend/parser/yacc.h>
    #include <frontend/parser/scanner.h>
    #include <frontend/parser/location.hh>
    #include <frontend/parser/parser.h>
    #include <stdexcept>
    #include <string>
    #include <limits>
    #include <cmath>

    using namespace FE;

    static location loc;
    // location类（定义在location.hh中）
    /*
    class location {
    public:
        position begin;  // 词素开始位置
        position end;    // 词素结束位置

        void step() { begin = end; }            // 将起点移到终点
        void columns(counter_type count = 1) { end += count; }  // 列号增加
        void lines(counter_type count = 1) { end.lines(count); } // 行号增加
    };
    */
    // 成员函数：
    // step()：将起点begin移到当前的end，即“准备读取下一个token”
    // columns(n)：将当前位置（end）向右移动n列
    // lines(n)：将当前位置（end）向下移动n行，并将列号重置为1

    // YY_USER_ACTION 宏会自动插入到每个规则的代码块开头
    // 如下方的 "int"               { RETT(INT, loc) }
    // 实际上会被转换为
    // "int"               { loc.step(); loc.columns(yyleng); RETT(INT, loc) }
    // 这样就能自动更新 loc 的位置
    // yyleng 是当前匹配的字符串的长度

    // 在每次匹配到一个token时，都会自动更新loc的位置
    #define YY_USER_ACTION      \
        loc.step();             \
        loc.columns(yyleng);

    // 告诉词法分析器在文件结束时返回1，表示结束（EOF）
    #define yywrap() 1
    // 返回一个token对象
    // 对于带值和不带值的token，都调用YaccParser的静态函数make_xxx来创建token对象
    #define RETT(x, y) \
        return YaccParser::make_##x(y);

    // 定义制表符的宽度，用于列号计算
    #define TAB_WIDTH 4

    int handleTab(); // 处理制表符，返回制表符应占用的列数
    long long convertToInt(const char* str, const char end, bool& isLongLong); //把字符串形式的整数（支持十进制、八进制、十六进制）转换为long long类型；如果超出范围，标记isLongLong
    float convertToFloatDec(const char* str); // 把字符串形式的十进制浮点数转换为float类型
    float convertToFloatHex(const char* str); // 把字符串形式的十六进制浮点数转换为float类型
%}

%option c++
%option noyywrap debug
%option yyclass="Scanner"
%option prefix="Yacc_"

%x COMMENT

%%

%{
/*
在词法分析中，你需要重点考虑result的四种类型，分别为int_token, float_token, string_token, symbol_token。
之所以要定义这四种类型，是因为我们只返回这个token的类型是不够的，我们还需要知道这个token的属性。
例如我们需要知道INT_CONST具体的值是多少。
如果你需要其它 token，需要先到 yacc.y 中定义 token，然后在这里实现。

TODO(Lab1)：参考SysY2022的定义，实现更多的词法，如浮点数、十六进制/八进制数等
*/
%}

\n              {
                    loc.lines(yyleng); // 处理换行符，把列号column重置为1，把行号line增加yyleng（此时yyleng应该为1）
                    loc.step(); // 更新位置，把当前token的起点begin移动到最新的终点end
                }

[ \f\r\v]+    { /* empty */ } // 忽略\f换页符、\r回车符、\v换行符和空格
[\t]    {
    loc.columns(handleTab());
}


"if"                { RETT(IF, loc) } // 关键字要在标识符规则之前定义（重要）
"else"              { RETT(ELSE, loc) } // 这里的注释居然不能写在行首？可能是会自动匹配“//”了
"for"               { RETT(FOR, loc) }
"while"             { RETT(WHILE, loc) }
"continue"          { RETT(CONTINUE, loc) }
"break"             { RETT(BREAK, loc) }
"switch"            { RETT(SWITCH, loc) }
"case"              { RETT(CASE, loc) }
"goto"              { RETT(GOTO, loc) }
"do"                { RETT(DO, loc) }
"return"            { RETT(RETURN, loc) }
"const"             { RETT(CONST, loc) }

"int"               { RETT(INT, loc) }
"float"             { RETT(FLOAT, loc) }
"void"              { RETT(VOID, loc) }
"char"              { RETT(CHAR, loc) }


"<="                { RETT(LE, loc) } // 先写双字符运算符，再写单字符运算符，避免冲突
">="                { RETT(GE, loc) }
"=="                { RETT(EQ, loc) }
"!="                { RETT(NEQ, loc) }
"&&"                { RETT(AND, loc) }
"||"                { RETT(OR, loc) }
"!"                 { RETT(NOT, loc) }
"<"                 { RETT(LT, loc) }
">"                 { RETT(GT, loc) }
"+"                 { RETT(PLUS, loc) }
"-"                 { RETT(MINUS, loc) }
"*"                 { RETT(STAR, loc) }
"/"                 { RETT(SLASH, loc) }
"%"                 { RETT(PERCENT, loc) }
"="                 { RETT(ASSIGN, loc) }

";"                 { RETT(SEMICOLON, loc) }
","                 { RETT(COMMA, loc) }




"//"[^\n]* {
    loc.columns(yyleng);
    loc.step();
}

"/*" { BEGIN(COMMENT); }

<COMMENT>{
    "*/" {
        loc.step();      // 更新位置
        BEGIN(INITIAL);
    }

    [^*\n]+  { /* 忽略中间内容，不更新列号，因为注释内位置不重要 */ }
    "*"      { /* 忽略单个星号 */ }
    \n       { loc.lines(1); loc.step(); }  // 只更新行号一次
    <<EOF>>  {
        _parser.reportError(loc, "Unterminated comment");
        BEGIN(INITIAL);
    }
}


[A-Za-z_]+[A-Za-z0-9_]* {
    auto str = std::string(yytext);
    return YaccParser::make_IDENT(str, loc); 
} // 标识符

\"[^\"]*\"          { 
    std::string str(yytext);
    if (!str.empty()) str = str.substr(1, str.size() - 2);
    return YaccParser::make_STR_CONST(str, loc); 
} // 字符串常量


[1-9][0-9]*|0[xX][0-9a-fA-F]+|0[0-7]* { // 请注意：单独的0会在0[0-7]*中被匹配到
    try {
        bool isLL = false;
        long long result = convertToInt(yytext, '\0', isLL);
        if (isLL) return YaccParser::make_LL_CONST(result, loc); // 超出int范围，返回long long常量
        return YaccParser::make_INT_CONST(static_cast<int>(result), loc);
    } catch (const std::exception& e) {
        _parser.reportError(loc, std::string("Error parsing hexadecimal int: ") + e.what()); // hexadecimal?
        auto str = std::string(yytext);
        return YaccParser::make_ERR_TOKEN(str, loc);
    }
} // 十进制、十六进制、八进制整数常量


([0-9]+\.[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+ {
// 规则解析：
// [0-9]+\.[0-9]* 表示有整数部分 + 小数点 + 可选小数部分（如 1., 3.14）
// \.[0-9]+ 表示没有整数部分但有小数部分（如 .25）
// ([eE][+-]?[0-9]+)? 表示可选的科学计数法（如 e-3, E+10）
// [0-9]+[eE][+-]?[0-9]+ 表示纯指数形式的科学计数法（如 2e10, 5E-2）
    try {
        float result = convertToFloatDec(yytext);
        return YaccParser::make_FLOAT_CONST(result, loc);
    } catch (const std::exception& e) {
        _parser.reportError(loc, std::string("Error parsing decimal float: ") + e.what());
        auto str = std::string(yytext);
        return YaccParser::make_ERR_TOKEN(str, loc);
    }
} // 十进制浮点数常量


[0][xX]([0-9a-fA-F]+\.[0-9a-fA-F]*|\.[0-9a-fA-F]+|[0-9a-fA-F]+)[pP][+-]?[0-9]+ {
// 规则解析：
// [0-9a-fA-F]+\.[0-9a-fA-F]* 表示有整数部分 + 小数点 + 可选小数部分
// \.[0-9a-fA-F]+ 表示没有整数部分，但有小数部分
// [0-9a-fA-F]+ 表示只有整数部分
// [pP][+-]?[0-9]+ 表示必须有的二进制指数部分（有p或P），例如 p+1、p-4
    try {
        float result = convertToFloatHex(yytext);
        return YaccParser::make_FLOAT_CONST(result, loc);
    } catch (const std::exception& e) {
        _parser.reportError(loc, std::string("Error parsing hexadecimal float: ") + e.what());
        auto str = std::string(yytext);
        return YaccParser::make_ERR_TOKEN(str, loc);
    }
} // 十六进制浮点数常量

"("                 { RETT(LPAREN, loc) }
")"                 { RETT(RPAREN, loc) }
"["                 { RETT(LBRACKET, loc) }
"]"                 { RETT(RBRACKET, loc) }
"{"                 { RETT(LBRACE, loc) }
"}"                 { RETT(RBRACE, loc) }

<<EOF>>         { RETT(END, loc); }

%%

int handleTab()
{
    // 该函数用于处理制表符，根据当前位置计算制表符应占用的列数
    // 如，在TAB_WIDTH为4的情况下：
    // 对于" \t"，则会对 '\t' 返回 3
    // 对于"   \t"，则会对 '\t' 返回 1
    // 对于"    \t"，则会对 '\t' 返回 4

    int& c = loc.begin.column;
    return TAB_WIDTH - ((c - 1) % TAB_WIDTH) - 1;
}

long long convertToInt(const char* str, const char end, bool& isLongLong)
{
    // 该函数用于将字符串转换为整数，能正确处理格式无误的十进制、十六进制和八进制整数
    // 接受一个 isLongLong 引用参数，用于指示结果是否超出 int 范围

    int         base       = 10;
    long long   result     = 0;
    int         isNegative = 0;
    const char* ptr        = str;
    static int  zeroOffset = '0';
    static int  aOffset    = 'a' - 10;
    static int  AOffset    = 'A' - 10;
    int*        offset     = NULL;
    isLongLong             = false;

    if (*ptr == '-')
    {
        isNegative = 1;
        ++ptr;
    }
    else if (*ptr == '+') { ++ptr; }

    if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X'))
    {
        base = 16;
        ptr += 2;
    }
    else if (ptr[0] == '0')
    {
        base = 8;
        ptr += 1;
    }

    while (*ptr != end)
    {
        int value = 0;

        if (base == 16)
        {
            if (*ptr >= '0' && *ptr <= '9')
                offset = &zeroOffset;
            else if (*ptr >= 'a' && *ptr <= 'f')
                offset = &aOffset;
            else if (*ptr >= 'A' && *ptr <= 'F')
                offset = &AOffset;
        }
        else { offset = &zeroOffset; }

        value  = *ptr - *offset;
        result = result * base + value;
        ++ptr;
    }

    if (isNegative) result = -result;

    if (result > std::numeric_limits<int>::max() || result < std::numeric_limits<int>::min())
    {
        isLongLong = true;
        if (result > std::numeric_limits<long long>::max() || result < std::numeric_limits<long long>::min())
        {
            throw std::out_of_range(str + std::string(" overflow or underflow for long long"));
        }
    }

    return result;
}

float convertToFloatDec(const char* str)
{
    // 该函数用于将字符串转换为浮点数，能正确处理格式无误的十进制浮点数

    const char* head = str;
    if (str == NULL) { return 0.0f; }

    int sign = 1;
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+') { str++; }

    double integerPart = 0.0;
    while (isdigit(*str))
    {
        integerPart = integerPart * 10 + (*str - '0');
        str++;
    }

    double fractionPart = 0.0;
    if (*str == '.')
    {
        str++;
        double divisor = 10.0;
        while (isdigit(*str))
        {
            fractionPart += (*str - '0') / divisor;
            divisor *= 10.0;
            str++;
        }
    }

    double value = integerPart + fractionPart;

    if (*str == 'e' || *str == 'E')
    {
        str++;
        int expSign = 1;
        if (*str == '-')
        {
            expSign = -1;
            str++;
        }
        else if (*str == '+') { str++; }

        int exponent = 0;
        while (isdigit(*str))
        {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }

        value *= pow(10, expSign * exponent);
    }

    value = sign * value;

    if (value > std::numeric_limits<float>::max() || value < -std::numeric_limits<float>::max())
    {
        throw std::out_of_range(
            head + ((value > std::numeric_limits<float>::max()) ? std::string(" overflow") : std::string(" underflow")));
    }

    return static_cast<float>(value);
}

float convertToFloatHex(const char* str)
{
    // 该函数用于将字符串转换为浮点数，能正确处理格式无误的十六进制浮点数

    const char* head = str;
    if (str == NULL) { return 0.0f; }

    int sign = 1;
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+') { str++; }

    if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) { str += 2; }

    unsigned long long integerPart = 0;
    while (isxdigit(*str))
    {
        int digit = 0;
        if (isdigit(*str)) { digit = (*str - '0'); }
        else if (*str >= 'a' && *str <= 'f') { digit = (*str - 'a' + 10); }
        else if (*str >= 'A' && *str <= 'F') { digit = (*str - 'A' + 10); }
        integerPart = integerPart * 16 + digit;
        str++;
    }

    double fractionPart = 0.0;
    if (*str == '.')
    {
        str++;
        unsigned long long numerator   = 0;
        unsigned long long denominator = 1;
        while (isxdigit(*str))
        {
            int digit = 0;
            if (isdigit(*str)) { digit = (*str - '0'); }
            else if (*str >= 'a' && *str <= 'f') { digit = (*str - 'a' + 10); }
            else if (*str >= 'A' && *str <= 'F') { digit = (*str - 'A' + 10); }
            numerator = numerator * 16 + digit;
            denominator *= 16;
            str++;
        }
        fractionPart = (double)numerator / (double)denominator;
    }

    double value = (double)integerPart + fractionPart;

    if (*str == 'p' || *str == 'P')
    {
        str++;
        int expSign = 1;
        if (*str == '-')
        {
            expSign = -1;
            str++;
        }
        else if (*str == '+') { str++; }

        int exponent = 0;
        while (isdigit(*str))
        {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }

        value *= pow(2, expSign * exponent);
    }

    value = sign * value;

    if (value > std::numeric_limits<float>::max() || value < -std::numeric_limits<float>::max())
    {
        throw std::out_of_range(
            head + ((value > std::numeric_limits<float>::max()) ? std::string(" overflow") : std::string(" underflow")));
    }

    return static_cast<float>(value);
}
