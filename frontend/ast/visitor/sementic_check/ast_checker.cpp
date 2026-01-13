#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <debug.h>

namespace FE::AST
{
    bool ASTChecker::visit(Root& node)
    {
        // TODO(Lab3-1): 实现根节点的语义检查
        // 重置符号表，遍历所有顶层语句进行检查，确保存在main函数
        //(void)node;
        //TODO("Lab3-1: Implement Root node semantic checking");

        // 恢复初始状态
        errors.clear();
        glbSymbols.clear();

        symTable.reset(); // 重置符号表

        // funcDecls.clear(); 
        // libFuncRegister();

        auto stmts_ptr = node.getStmts();
        if (!stmts_ptr) return true;

        bool ok = true;

        // 遍历所有顶层语句
        for (auto stmt : *stmts_ptr)
        {
            if (!stmt) continue;

            // 是否是全局变量声明语句
            if (stmt->isVarDeclStmt())
            {
                auto *vdeclStmt = static_cast<VarDeclStmt*>(stmt);
                auto *vdecl = vdeclStmt->decl; // VarDeclaration*
                for (auto *decltor : *vdecl->decls)
                {
                    // decltor->lval应该是LeftValExpr类型
                    auto *lval = dynamic_cast<LeftValExpr*>(decltor->lval);
                    if (!lval || !lval->entry)
                    {
                        errors.push_back("Invalid global variable declarator at line " + std::to_string(decltor->line_num));
                        ok = false;
                        continue;
                    }
                }
            }
            else if (auto *fdeclStmt = dynamic_cast<FuncDeclStmt*>(stmt)) // 函数声明语句
            {
                auto *entry = fdeclStmt->entry;
                if (!entry)
                {
                    errors.push_back("Anonymous function declaration at line " + std::to_string(fdeclStmt->line_num));
                    ok = false;
                }
                else
                {
                    if (funcDecls.find(entry) != funcDecls.end())
                    {
                        errors.push_back("redefinition of function '" + entry->getName() +
                                        "' at line " + std::to_string(fdeclStmt->line_num));
                        ok = false;
                    }
                    else
                    {
                        funcDecls[entry] = fdeclStmt; // 记录函数声明
                    }

                    // 根据文档，在SysY语言中main函数必须：
                    // 1. 无参数；
                    // 2. 返回类型为int。
                    // 和C语言有一点区别！
                    if (entry->getName() == "main")
                    {
                        if (fdeclStmt->retType != intType)
                        {
                            errors.push_back("main function must return int at line " + std::to_string(fdeclStmt->line_num));
                            ok = false;
                        }
                        if (fdeclStmt->params && !fdeclStmt->params->empty())
                        {
                            errors.push_back("main must have no parameters at line " + std::to_string(fdeclStmt->line_num));
                            ok = false;
                        }
                    }
                }
            }

            // 继续访问顶层语句进行语义检查
            if (!apply(*this, *stmt)) ok = false;
        }

        // 检查main函数是否存在
        FE::Sym::Entry* mainEntry = FE::Sym::Entry::getEntry("main");
        if (funcDecls.find(mainEntry) == funcDecls.end())
        {
            errors.push_back("function 'int main(){}' is not defined");
            ok = false;
        }

        // exit global scope
        symTable.exitScope();

        return ok;
    }

    void ASTChecker::libFuncRegister()
    {
        // 示例实现：注册 SysY 标准库函数到 funcDecls 中
        // 这样在语义检查时可以识别并检查对库函数的调用
        // 包括：getint, getch, getarray, getfloat, getfarray,
        //      putint, putch, putarray, putfloat, putfarray,
        //      _sysy_starttime, _sysy_stoptime
        using SymEnt = FE::Sym::Entry;

        // int getint(), getch(), getarray(int a[]);
        static SymEnt* getint   = SymEnt::getEntry("getint");
        static SymEnt* getch    = SymEnt::getEntry("getch");
        static SymEnt* getarray = SymEnt::getEntry("getarray");

        // float getfloat();
        static SymEnt* getfloat = SymEnt::getEntry("getfloat");

        // int getfarray(float a[]);
        static SymEnt* getfarray = SymEnt::getEntry("getfarray");

        // void putint(int a), putch(int a), putarray(int n, int a[]);
        static SymEnt* putint   = SymEnt::getEntry("putint");
        static SymEnt* putch    = SymEnt::getEntry("putch");
        static SymEnt* putarray = SymEnt::getEntry("putarray");

        // void putfloat(float a);
        static SymEnt* putfloat = SymEnt::getEntry("putfloat");

        // void putfarray(int n, float a[]);
        static SymEnt* putfarray = SymEnt::getEntry("putfarray");

        // void starttime(), stoptime();
        static SymEnt* _sysy_starttime = SymEnt::getEntry("_sysy_starttime");
        static SymEnt* _sysy_stoptime  = SymEnt::getEntry("_sysy_stoptime");

        // int getint()
        funcDecls[getint] = new FuncDeclStmt(intType, getint, nullptr);

        // int getch()
        funcDecls[getch] = new FuncDeclStmt(intType, getch, nullptr);

        // int getarray(int a[])
        auto getarray_params = new std::vector<ParamDeclarator*>();
        auto getarray_param  = new ParamDeclarator(TypeFactory::getPtrType(intType), SymEnt::getEntry("a"));
        getarray_param->attr.val.value.type = TypeFactory::getPtrType(intType);
        getarray_params->push_back(getarray_param);
        funcDecls[getarray] = new FuncDeclStmt(intType, getarray, getarray_params);

        // float getfloat()
        funcDecls[getfloat] = new FuncDeclStmt(floatType, getfloat, nullptr);

        // int getfarray(float a[])
        auto getfarray_params = new std::vector<ParamDeclarator*>();
        auto getfarray_param  = new ParamDeclarator(TypeFactory::getPtrType(floatType), SymEnt::getEntry("a"));
        getfarray_param->attr.val.value.type = TypeFactory::getPtrType(floatType);
        getfarray_params->push_back(getfarray_param);
        funcDecls[getfarray] = new FuncDeclStmt(intType, getfarray, getfarray_params);

        // void putint(int a)
        auto putint_params                = new std::vector<ParamDeclarator*>();
        auto putint_param                 = new ParamDeclarator(intType, SymEnt::getEntry("a"));
        putint_param->attr.val.value.type = intType;
        putint_params->push_back(putint_param);
        funcDecls[putint] = new FuncDeclStmt(voidType, putint, putint_params);

        // void putch(int a)
        auto putch_params                = new std::vector<ParamDeclarator*>();
        auto putch_param                 = new ParamDeclarator(intType, SymEnt::getEntry("a"));
        putch_param->attr.val.value.type = intType;
        putch_params->push_back(putch_param);
        funcDecls[putch] = new FuncDeclStmt(voidType, putch, putch_params);

        // void putarray(int n, int a[])
        auto putarray_params                 = new std::vector<ParamDeclarator*>();
        auto putarray_param1                 = new ParamDeclarator(intType, SymEnt::getEntry("n"));
        putarray_param1->attr.val.value.type = intType;
        auto putarray_param2 = new ParamDeclarator(TypeFactory::getPtrType(intType), SymEnt::getEntry("a"));
        putarray_param2->attr.val.value.type = TypeFactory::getPtrType(intType);
        putarray_params->push_back(putarray_param1);
        putarray_params->push_back(putarray_param2);
        funcDecls[putarray] = new FuncDeclStmt(voidType, putarray, putarray_params);

        // void putfloat(float a)
        auto putfloat_params                = new std::vector<ParamDeclarator*>();
        auto putfloat_param                 = new ParamDeclarator(floatType, SymEnt::getEntry("a"));
        putfloat_param->attr.val.value.type = floatType;
        putfloat_params->push_back(putfloat_param);
        funcDecls[putfloat] = new FuncDeclStmt(voidType, putfloat, putfloat_params);

        // void putfarray(int n, float a[])
        auto putfarray_params                 = new std::vector<ParamDeclarator*>();
        auto putfarray_param1                 = new ParamDeclarator(intType, SymEnt::getEntry("n"));
        putfarray_param1->attr.val.value.type = intType;
        auto putfarray_param2 = new ParamDeclarator(TypeFactory::getPtrType(floatType), SymEnt::getEntry("a"));
        putfarray_param2->attr.val.value.type = TypeFactory::getPtrType(floatType);
        putfarray_params->push_back(putfarray_param1);
        putfarray_params->push_back(putfarray_param2);
        funcDecls[putfarray] = new FuncDeclStmt(voidType, putfarray, putfarray_params);

        // void _sysy_starttime(int lineno)
        auto starttime_params                = new std::vector<ParamDeclarator*>();
        auto starttime_param                 = new ParamDeclarator(intType, SymEnt::getEntry("lineno"));
        starttime_param->attr.val.value.type = intType;
        starttime_params->push_back(starttime_param);
        funcDecls[_sysy_starttime] = new FuncDeclStmt(voidType, _sysy_starttime, starttime_params);

        // void _sysy_stoptime(int lineno)
        auto stoptime_params                = new std::vector<ParamDeclarator*>();
        auto stoptime_param                 = new ParamDeclarator(intType, SymEnt::getEntry("lineno"));
        stoptime_param->attr.val.value.type = intType;
        stoptime_params->push_back(stoptime_param);
        funcDecls[_sysy_stoptime] = new FuncDeclStmt(voidType, _sysy_stoptime, stoptime_params);
    }
}  // namespace FE::AST
