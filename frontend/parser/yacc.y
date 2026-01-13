%skeleton "lalr1.cc"
%require "3.2"


%define api.namespace { FE }
%define api.parser.class { YaccParser }
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%defines

%code requires
{
    #include <memory>
    #include <vector>
    #include <string>
    #include <sstream>
    #include <frontend/ast/ast_defs.h>
    #include <frontend/ast/ast.h>
    #include <frontend/ast/stmt.h>
    #include <frontend/ast/expr.h>
    #include <frontend/ast/decl.h>
    #include <frontend/symbol/symbol_entry.h>

    using std::vector;
    using std::string;

    namespace FE
    {
        class Parser;
        class Scanner;
    }
}

%code top
{
    #include <iostream>

    #include <frontend/parser/parser.h>
    #include <frontend/parser/location.hh>
    #include <frontend/parser/scanner.h>
    #include <frontend/parser/yacc.h>
    #include <frontend/ast/ast.h>
    #include <frontend/ast/expr.h>
    #include <frontend/ast/stmt.h>
    #include <frontend/ast/decl.h>
    #include <frontend/symbol/symbol_table.h>

    using namespace FE;
    using namespace FE::AST;

    static YaccParser::symbol_type yylex(Scanner& scanner, Parser &parser)
    {
        (void)parser;
        return scanner.nextToken(); 
    }

}

%lex-param { FE::Scanner& scanner }
%lex-param { FE::Parser& parser }
%parse-param { FE::Scanner& scanner }
%parse-param { FE::Parser& parser }

%locations

%define parse.error verbose
%define api.token.prefix {TOKEN_}

// 从这开始定义你需要用到的 token
// 对于一些需要 "值" 的 token，可以在前面加上 <type> 来指定值的类型
// 例如，%token <int> INT_CONST 定义了一个名为 INT_CONST 的 token
%token <int> INT_CONST
// int类型的整数常量
%token <long long> LL_CONST
// long long类型的整数常量
%token <float> FLOAT_CONST
// float类型的浮点数常量
%token <std::string> STR_CONST ERR_TOKEN SLASH_COMMENT //L_MULTI_COMMENT R_MULTI_COMMENT
// 字符串常量 错误token ------------------注意：单行注释// 多行注释开始符/* 多行注释结束符*/ 不需要定义%token
%token <std::string> IDENT 
// 标识符
%token PLUS MINUS STAR SLASH ASSIGN PERCENT
// + - * / = %
%token LT GT LE GE EQ NEQ AND OR NOT
// < > <= >= == != && || !
%token INT FLOAT VOID CHAR 
// int float void char 
%token LONG_LONG // 这个需要保留吗？
// long long
%token IF ELSE FOR WHILE CONTINUE BREAK SWITCH CASE GOTO DO RETURN CONST
// if else for while continue break switch case goto do return const
%token SEMICOLON COMMA LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE
// ; , ( ) [ ] { }
%token END
// end


%nterm <FE::AST::Operator> UNARY_OP
%nterm <FE::AST::Type*> TYPE
%nterm <FE::AST::InitDecl*> INITIALIZER
%nterm <std::vector<FE::AST::InitDecl*>*> INITIALIZER_LIST
%nterm <FE::AST::VarDeclarator*> VAR_DECLARATOR
%nterm <std::vector<FE::AST::VarDeclarator*>*> VAR_DECLARATOR_LIST
%nterm <FE::AST::VarDeclaration*> VAR_DECLARATION
%nterm <FE::AST::ParamDeclarator*> PARAM_DECLARATOR
%nterm <std::vector<FE::AST::ParamDeclarator*>*> PARAM_DECLARATOR_LIST

%nterm <FE::AST::ExprNode*> LITERAL_EXPR
%nterm <FE::AST::ExprNode*> BASIC_EXPR
%nterm <FE::AST::ExprNode*> FUNC_CALL_EXPR
%nterm <FE::AST::ExprNode*> UNARY_EXPR
%nterm <FE::AST::ExprNode*> MULDIV_EXPR
%nterm <FE::AST::ExprNode*> ADDSUB_EXPR
%nterm <FE::AST::ExprNode*> RELATIONAL_EXPR
%nterm <FE::AST::ExprNode*> EQUALITY_EXPR
%nterm <FE::AST::ExprNode*> LOGICAL_AND_EXPR
%nterm <FE::AST::ExprNode*> LOGICAL_OR_EXPR
%nterm <FE::AST::ExprNode*> ASSIGN_EXPR
%nterm <FE::AST::ExprNode*> NOCOMMA_EXPR
%nterm <FE::AST::ExprNode*> EXPR
%nterm <std::vector<FE::AST::ExprNode*>*> EXPR_LIST

%nterm <FE::AST::ExprNode*> ARRAY_DIMENSION_EXPR
%nterm <std::vector<FE::AST::ExprNode*>*> ARRAY_DIMENSION_EXPR_LIST
%nterm <FE::AST::ExprNode*> LEFT_VAL_EXPR

%nterm <FE::AST::StmtNode*> EXPR_STMT
%nterm <FE::AST::StmtNode*> VAR_DECL_STMT
%nterm <FE::AST::StmtNode*> BLOCK_STMT
%nterm <FE::AST::StmtNode*> FUNC_DECL_STMT
%nterm <FE::AST::StmtNode*> RETURN_STMT
%nterm <FE::AST::StmtNode*> WHILE_STMT
%nterm <FE::AST::StmtNode*> IF_STMT
%nterm <FE::AST::StmtNode*> BREAK_STMT
%nterm <FE::AST::StmtNode*> CONTINUE_STMT
%nterm <FE::AST::StmtNode*> FOR_STMT
%nterm <FE::AST::StmtNode*> FUNC_BODY
%nterm <FE::AST::StmtNode*> STMT

%nterm <std::vector<FE::AST::StmtNode*>*> STMT_LIST
%nterm <FE::AST::Root*> PROGRAM

%start PROGRAM

//THEN和ELSE用于处理if和else的移进-规约冲突
%precedence THEN
%precedence ELSE
// token 定义结束

%%

/*
语法分析：补全TODO(Lab2)处的文法规则及处理函数。
如果你不打算实现float、array这些进阶要求，可将对应部分删去。
*/

//语法树匹配从这里开始
// 起始符号
PROGRAM: 
    STMT_LIST {
        $$ = new Root($1);
        parser.ast = $$;
    }
    | PROGRAM END { // 当读到END时，表示输入结束
        YYACCEPT;
    }
    ;

// 语句列表
STMT_LIST:
    STMT { // 若只匹配到一个语句，则创建一个新的语句列表
        $$ = new std::vector<StmtNode*>();
        if ($1) $$->push_back($1);
    }
    | STMT_LIST STMT { // 若已有语句列表，则将新语句加入列表
        $$ = $1;
        if ($2) $$->push_back($2);
    }
    ;


// 各种语句类型的入口
STMT:
    EXPR_STMT { // 表达式语句
        $$ = $1;
    }
    | VAR_DECL_STMT { // 变量声明语句
        $$ = $1;
    }
    | FUNC_DECL_STMT { // 函数声明语句
        $$ = $1;
    }
    | FOR_STMT { // for语句（SysY语言支持for语句吗？）
        $$ = $1;
    }
    | IF_STMT { // if语句
        $$ = $1;
    }
    | CONTINUE_STMT { // continue语句
        $$ = $1;
    }
    | SEMICOLON { // 分号（作为空语句处理）
        $$ = nullptr;
    }
    | SLASH_COMMENT { // 单行注释（作为空语句处理）
        $$ = nullptr;
    }
    //TODO(Lab2)：考虑更多语句类型
    // 2313546
    | BREAK_STMT { // break语句
        $$ = $1;
    }
    | RETURN_STMT { // return语句
        $$ = $1;
    }
    | WHILE_STMT { // while语句
        $$ = $1;
    }
    | BLOCK_STMT { // 块语句
        $$ = $1;
    }
    ;

// 匹配continue
CONTINUE_STMT:
    CONTINUE SEMICOLON {
        $$ = new ContinueStmt(@1.begin.line, @1.begin.column);
    }
    ;

// 表达式语句
EXPR_STMT:
    EXPR SEMICOLON {
        $$ = new ExprStmt($1, @1.begin.line, @1.begin.column);
    }
    ;

// 描述一个“变量声明结构”的语法规则（短语级别）
VAR_DECLARATION:
    TYPE VAR_DECLARATOR_LIST {
        $$ = new VarDeclaration($1, $2, false, @1.begin.line, @1.begin.column);
    }
    | CONST TYPE VAR_DECLARATOR_LIST {
        $$ = new VarDeclaration($2, $3, true, @1.begin.line, @1.begin.column);
    }
    ;

// 变量声明语句（语句级别）
VAR_DECL_STMT:
    /* TODO(Lab2): Implement variable declaration statement rule */
    // 2313546
    VAR_DECLARATION SEMICOLON {
        $$ = new VarDeclStmt($1, @1.begin.line, @1.begin.column);
    }
    ;

// 函数体 { ... }
FUNC_BODY:
    LBRACE RBRACE {
        $$ = nullptr;
    }
    | LBRACE STMT_LIST RBRACE {
        if (!$2 || $2->empty())
        {
            $$ = nullptr;
            delete $2;
        }
        else if ($2->size() == 1)
        {
            $$ = (*$2)[0];
            delete $2;
        }
        else $$ = new BlockStmt($2, @1.begin.line, @1.begin.column);
    }
    ;

// 函数声明语句
FUNC_DECL_STMT:
    TYPE IDENT LPAREN PARAM_DECLARATOR_LIST RPAREN FUNC_BODY {
        Entry* entry = Entry::getEntry($2);
        $$ = new FuncDeclStmt($1, entry, $4, $6, @1.begin.line, @1.begin.column);
    }
    ;

// 两种形式的for语句（第一种形式含有变量声明，第二种形式不含变量声明）
FOR_STMT:
    FOR LPAREN VAR_DECLARATION SEMICOLON EXPR SEMICOLON EXPR RPAREN STMT {
        VarDeclStmt* initStmt = new VarDeclStmt($3, @3.begin.line, @3.begin.column);
        $$ = new ForStmt(initStmt, $5, $7, $9, @1.begin.line, @1.begin.column);
    }
    | FOR LPAREN EXPR SEMICOLON EXPR SEMICOLON EXPR RPAREN STMT {
        StmtNode* initStmt = new ExprStmt($3, $3->line_num, $3->col_num);
        $$ = new ForStmt(initStmt, $5, $7, $9, @1.begin.line, @1.begin.column);
    }
    ;

// 两种形式的if语句（含else和不含else）
IF_STMT:
    /* TODO(Lab2): Implement if statement rule */
    // 2313546
    IF LPAREN EXPR RPAREN STMT %prec THEN { // 注意定义优先级
        $$ = new IfStmt($3, $5, nullptr, @1.begin.line, @1.begin.column);
    }
    | IF LPAREN EXPR RPAREN STMT ELSE STMT {
        $$ = new IfStmt($3, $5, $7, @1.begin.line, @1.begin.column);
    }
    ;

//TODO(Lab2)：按照你补充的语句类型，实现这些语句的处理
//2313247
//BREAK语句
BREAK_STMT:
    BREAK SEMICOLON {
        $$ = new BreakStmt(@1.begin.line, @1.begin.column);
    }
    ;
//两种return语句：有返回值和无返回值
RETURN_STMT:
    RETURN SEMICOLON {  // 无返回值：return;
        $$ = new ReturnStmt(nullptr, @1.begin.line, @1.begin.column);
    }
    | RETURN EXPR SEMICOLON {  // 有返回值：return expression;
        $$ = new ReturnStmt($2, @1.begin.line, @1.begin.column);
    }
    ;

//while语句，包含条件和循环体两个子节点
WHILE_STMT:
    WHILE LPAREN EXPR RPAREN STMT {
        $$ = new WhileStmt($3, $5, @1.begin.line, @1.begin.column);
    }
    ;

//块语句，处理代码块
BLOCK_STMT:
    LBRACE STMT_LIST RBRACE {
        $$ = new BlockStmt($2, @1.begin.line, @1.begin.column);
    }
    | LBRACE RBRACE { // 空语句块
        $$ = new BlockStmt(new std::vector<StmtNode*>(), @1.begin.line, @1.begin.column);
    }
    ;

// 定义函数形参（第一种形式：基本类型+标识符，第二种形式：基本类型+标识符+数组维度）
PARAM_DECLARATOR:
    TYPE IDENT {
        Entry* entry = Entry::getEntry($2);
        $$ = new ParamDeclarator($1, entry, nullptr, @1.begin.line, @1.begin.column);
    }
    | TYPE IDENT LBRACKET RBRACKET {
        std::vector<ExprNode*>* dim = new std::vector<ExprNode*>();
        dim->emplace_back(new LiteralExpr(-1, @3.begin.line, @3.begin.column));
        Entry* entry = Entry::getEntry($2);
        $$ = new ParamDeclarator($1, entry, dim, @1.begin.line, @1.begin.column);
    }
    //TODO(Lab2)：考虑函数形参更多情况
    // 2313546
    | TYPE IDENT LBRACKET RBRACKET ARRAY_DIMENSION_EXPR_LIST {
        std::vector<ExprNode*>* dim = $5;
        dim->insert(dim->begin(), new LiteralExpr(-1, @3.begin.line, @3.begin.column));
        Entry* entry = Entry::getEntry($2);
        $$ = new ParamDeclarator($1, entry, dim, @1.begin.line, @1.begin.column);
    }
    | TYPE IDENT ARRAY_DIMENSION_EXPR_LIST {
        Entry* entry = Entry::getEntry($2);
        $$ = new ParamDeclarator($1, entry, $3, @1.begin.line, @1.begin.column);
    }
    ;

// 函数参数列表（可能为空）
PARAM_DECLARATOR_LIST:
    /* empty */ {
        $$ = new std::vector<ParamDeclarator*>();
    }
    //TODO(Lab2)：考虑函数形参列表的构成情况
    // 2313546
    | PARAM_DECLARATOR { // 形式1：单个参数
        $$ = new std::vector<ParamDeclarator*>();
        $$->push_back($1);
    }
    | PARAM_DECLARATOR_LIST COMMA PARAM_DECLARATOR { // 形式2：多个参数
        $$ = $1;
        $$->push_back($3);
    }
    ;

// 变量声明
VAR_DECLARATOR:
    //TODO(Lab2)：完成变量声明符的处理
    // 2313546
    // 1.普通变量，如 a
    IDENT {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, nullptr, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, nullptr, @1.begin.line, @1.begin.column);
    }

    // 2.普通变量 + 初始化，如 a = 10
    | IDENT ASSIGN INITIALIZER {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, nullptr, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, $3, @1.begin.line, @1.begin.column);
    }

    // 3.普通数组，如 a[10][20]
    | IDENT ARRAY_DIMENSION_EXPR_LIST {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, $2, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, nullptr, @1.begin.line, @1.begin.column);
    }

    // 4.普通数组 + 初始化，如 a[10][20] = { ... }
    | IDENT ARRAY_DIMENSION_EXPR_LIST ASSIGN INITIALIZER {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, $2, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, $4, @1.begin.line, @1.begin.column);
    }

    // 5.不定长数组：IDENT [] [维度] [维度]...，如 a[]
    | IDENT LBRACKET RBRACKET {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, nullptr, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, nullptr, @1.begin.line, @1.begin.column);
    }

    // 6.不定长数组 + 维度 + 初始化，如 a[] [10] [20]
    | IDENT LBRACKET RBRACKET ARRAY_DIMENSION_EXPR_LIST {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, $4, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, nullptr, @1.begin.line, @1.begin.column);
    }

    // 7.不定长数组 + 维度 + 初始化，如 a[] [10] [20] = { ... }
    | IDENT LBRACKET RBRACKET ARRAY_DIMENSION_EXPR_LIST ASSIGN INITIALIZER {
        Entry* entry = Entry::getEntry($1);
        ExprNode* lval = new LeftValExpr(entry, $4, @1.begin.line, @1.begin.column);
        $$ = new VarDeclarator(lval, $6, @1.begin.line, @1.begin.column);
    }
    ;

// 变量声明列表
VAR_DECLARATOR_LIST:
    VAR_DECLARATOR {
        $$ = new std::vector<VarDeclarator*>();
        $$->push_back($1);
    }
    | VAR_DECLARATOR_LIST COMMA VAR_DECLARATOR {
        $$ = $1;
        $$->push_back($3);
    }
    ;

// 定义变量初始化的表达式（单个表达式，或者是用大括号括起来的初始化列表）
INITIALIZER:
    /* TODO(Lab2): Implement variable initializer rule */
    // 2313546
    NOCOMMA_EXPR {
        $$ = new Initializer($1, @1.begin.line, @1.begin.column);
    }
    | LBRACE INITIALIZER_LIST RBRACE {
        $$ = new InitializerList($2, @1.begin.line, @1.begin.column);
    }
    /* 允许尾随逗号的初始化列表，例如 {1,2,} */
    | LBRACE INITIALIZER_LIST COMMA RBRACE {
        $$ = new InitializerList($2, @1.begin.line, @1.begin.column);
    }
    | LBRACE RBRACE {
        // 空初始化列表 {}
        $$ = new InitializerList(new std::vector<InitDecl*>(), @1.begin.line, @1.begin.column);
    }
    ;

// 变量初始化列表
INITIALIZER_LIST:
    INITIALIZER {
        $$ = new std::vector<InitDecl*>();
        $$->push_back($1);
    }
    | INITIALIZER_LIST COMMA INITIALIZER {
        $$ = $1;
        $$->push_back($3);
    }
    ;

// 赋值表达式
ASSIGN_EXPR:
    // TODO(Lab2): 完成赋值表达式的处理
    // 2313546
    LEFT_VAL_EXPR ASSIGN NOCOMMA_EXPR { // 左值表达式=右值表达式
        $$ = new BinaryExpr(Operator::ASSIGN, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;

// 表达式列表
EXPR_LIST:
    NOCOMMA_EXPR {
        $$ = new std::vector<ExprNode*>();
        $$->push_back($1);
    }
    | EXPR_LIST COMMA NOCOMMA_EXPR {
        $$ = $1;
        $$->push_back($3);
    }
    ;

// 表达式
EXPR:
    NOCOMMA_EXPR {
        $$ = $1;
    }
    | EXPR COMMA NOCOMMA_EXPR {
        if ($1->isCommaExpr()) {
            CommaExpr* ce = static_cast<CommaExpr*>($1);
            ce->exprs->push_back($3);
            $$ = ce;
        } else {
            auto vec = new std::vector<ExprNode*>();
            vec->push_back($1);
            vec->push_back($3);
            $$ = new CommaExpr(vec, $1->line_num, $1->col_num);
        }
    }
    ;

// 非逗号表达式
NOCOMMA_EXPR:
    LOGICAL_OR_EXPR {
        $$ = $1;
    }
    | ASSIGN_EXPR {
        $$ = $1;
    }
    ;

//逻辑或表达式
LOGICAL_OR_EXPR:
    /* TODO(Lab2): Implement logical OR expression rule */
    // 2313546
    LOGICAL_AND_EXPR {
        $$ = $1;
    }
    | LOGICAL_OR_EXPR OR LOGICAL_AND_EXPR {
    $$ = new BinaryExpr(Operator::OR, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;

// 逻辑与表达式
LOGICAL_AND_EXPR:
    /* TODO(Lab2): Implement logical AND expression rule */
    // 2313546
    EQUALITY_EXPR {
        $$ = $1;
    }
    | LOGICAL_AND_EXPR AND EQUALITY_EXPR {
    $$ = new BinaryExpr(Operator::AND, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;

// 逻辑相等表达式
EQUALITY_EXPR:
    /* TODO(Lab2): Implement equality expression rule */
    // 2313546
    RELATIONAL_EXPR {
        $$ = $1;
    }
    | EQUALITY_EXPR EQ RELATIONAL_EXPR {
    $$ = new BinaryExpr(Operator::EQ, $1, $3, @2.begin.line, @2.begin.column);
    }
    | EQUALITY_EXPR NEQ RELATIONAL_EXPR {
    $$ = new BinaryExpr(Operator::NEQ, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;

RELATIONAL_EXPR:
    /* TODO(Lab2): Implement relational expression rule */
    //2313247
    //基础情况：关系表达式至少包含一个加减表达式（没有比较操作）
    ADDSUB_EXPR{
        $$=$1;
    }
    // 递归/组合情况：一个关系表达式，后接关系运算符和另一个加减表达式
    | RELATIONAL_EXPR LT ADDSUB_EXPR {
        // < (小于)
        $$ = new BinaryExpr(Operator::LT, $1, $3, @2.begin.line, @2.begin.column);
    }
    | RELATIONAL_EXPR GT ADDSUB_EXPR {
        // > (大于)
        $$ = new BinaryExpr(Operator::GT, $1, $3, @2.begin.line, @2.begin.column);
    }
    | RELATIONAL_EXPR LE ADDSUB_EXPR {
        // <= (小于等于)
        $$ = new BinaryExpr(Operator::LE, $1, $3, @2.begin.line, @2.begin.column);
    }
    | RELATIONAL_EXPR GE ADDSUB_EXPR {
        // >= (大于等于)
        $$ = new BinaryExpr(Operator::GE, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;



//加减表达式，处理加法和减法，优先级低于关系表达式，高于乘除表达式
ADDSUB_EXPR:
    /* TODO(Lab2): Implement addition and subtraction expression rule */
    //2313247
    MULDIV_EXPR{
        $$=$1;
    }
    | ADDSUB_EXPR PLUS MULDIV_EXPR{
        $$=new BinaryExpr(Operator::ADD, $1, $3, @2.begin.line, @2.begin.column);
    }
    | ADDSUB_EXPR MINUS MULDIV_EXPR{
        $$=new BinaryExpr(Operator::SUB, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;

//处理乘法和除法运算，优先级高于加减表达式
MULDIV_EXPR:
    /* TODO(Lab2): Implement multiplication and division expression rule */
    //2313247
    UNARY_EXPR{
        $$=$1;
    }
    | MULDIV_EXPR STAR UNARY_EXPR {
        $$ = new BinaryExpr(Operator::MUL, $1, $3, @2.begin.line, @2.begin.column);
    }
    | MULDIV_EXPR SLASH UNARY_EXPR {
        $$ = new BinaryExpr(Operator::DIV, $1, $3, @2.begin.line, @2.begin.column);
    }
    | MULDIV_EXPR PERCENT UNARY_EXPR {
        $$ = new BinaryExpr(Operator::MOD, $1, $3, @2.begin.line, @2.begin.column);
    }
    ;
//一元表达式，处理一元表达式
UNARY_EXPR:
    BASIC_EXPR {
        $$ = $1;
    }
    | UNARY_OP UNARY_EXPR {
        $$ = new UnaryExpr($1, $2, $2->line_num, $2->col_num);
    }
    ;
//处理最基本的表达式单元，是语法树的叶子节点
BASIC_EXPR:
    LITERAL_EXPR {
        $$ = $1;
    }
    | LEFT_VAL_EXPR {
        $$ = $1;
    }
    | LPAREN EXPR RPAREN {
        $$ = $2;
    }
    | FUNC_CALL_EXPR {
        $$ = $1;
    }
    ;

FUNC_CALL_EXPR:
    IDENT LPAREN RPAREN {//无参数函数调用
        std::string funcName = $1;
        if (funcName != "starttime" && funcName != "stoptime")
        {
            //普通函数调用
            Entry* entry = Entry::getEntry(funcName);
            $$ = new CallExpr(entry, nullptr, @1.begin.line, @1.begin.column);
        }
        else
        {   
            //特殊系统函数处理 
            funcName = "_sysy_" + funcName;
            std::vector<ExprNode*>* args = new std::vector<ExprNode*>();
            //自动添加行号作为参数
            args->emplace_back(new LiteralExpr(static_cast<int>(@1.begin.line), @1.begin.line, @1.begin.column));
            $$ = new CallExpr(Entry::getEntry(funcName), args, @1.begin.line, @1.begin.column);
        }
    }
    | IDENT LPAREN EXPR_LIST RPAREN {//有参数函数调用
        Entry* entry = Entry::getEntry($1);
        $$ = new CallExpr(entry, $3, @1.begin.line, @1.begin.column);
    }
    ;

//数组维度表达式
ARRAY_DIMENSION_EXPR:
    LBRACKET NOCOMMA_EXPR RBRACKET {
        $$ = $2;
    }
    ;

//数组维度表达式列表
//使用递归定义
ARRAY_DIMENSION_EXPR_LIST:
    /* TODO(Lab2): Implement variable dimension rule */
    //2313247
    ARRAY_DIMENSION_EXPR {
        // 创建表达式列表
        auto* list = new std::vector<ExprNode*>();
        list->push_back($1);
        $$ = list;
    }
    | ARRAY_DIMENSION_EXPR_LIST ARRAY_DIMENSION_EXPR {
        $1->push_back($2);
        $$ = $1;
    }
    ;

//左值表达式：处理可以出现在赋值左边的表达式
LEFT_VAL_EXPR:
    IDENT {
        Entry* entry = Entry::getEntry($1);
        $$ = new LeftValExpr(entry, nullptr, @1.begin.line, @1.begin.column);
    }
    | IDENT ARRAY_DIMENSION_EXPR_LIST {
        Entry* entry = Entry::getEntry($1);
        $$ = new LeftValExpr(entry, $2, @1.begin.line, @1.begin.column);
    }
    ;

//字面值表达式
LITERAL_EXPR:
    INT_CONST {//整型常量
        $$ = new LiteralExpr($1, @1.begin.line, @1.begin.column);
    }
    //TODO(Lab2): 处理更多字面量
    //2313247
    //LONG LONG类型的整型常量
    | LL_CONST {
        // $1 的类型为 long long
        $$ = new LiteralExpr($1, @1.begin.line, @1.begin.column);
    }
    | FLOAT_CONST {
        // 浮点数常量 (例如: 3.14)
        $$ = new LiteralExpr($1, @1.begin.line, @1.begin.column);
    }
    ;

//处理变量和函数的类型声明
TYPE:
    // TODO(Lab2): 完成类型的处理
    //2313247
    // 基本整型
    INT {
        $$ = intType; 
    }
    // 基本浮点型
    | FLOAT {
        $$ = floatType;
    }
    // 无类型（通常用于函数返回值）
    | VOID {
        $$ = voidType;
    }
    // 扩展整型
    | LONG_LONG {
        $$ = llType;
    }
    ;


//一元运算符
UNARY_OP:
    // TODO(Lab2): 完成一元运算符的处理
    //2313247
    PLUS { $$ = Operator::ADD; }    // +
    | MINUS { $$ = Operator::SUB; }  // -
    | NOT { $$ = Operator::NOT; }       // !
    ;

%%

void FE::YaccParser::error(const FE::location& location, const std::string& message)
{
    std::cerr << "msg: " << message << ", error happened at: " << location << std::endl;
}
