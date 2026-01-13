#ifndef __INTERFACES_FRONTEND_PARSER_IPARSER_H__
#define __INTERFACES_FRONTEND_PARSER_IPARSER_H__

#include <frontend/token.h>
#include <iostream>
#include <vector>

namespace FE
{
    namespace AST
    {
        class Root;
    }

    template <typename Derived>
    class iParser
    {
      protected:
        std::istream* inStream;
        std::ostream* outStream;

      public:
        iParser(std::istream* inStream, std::ostream* outStream) : inStream(inStream), outStream(outStream) {}
        ~iParser() = default;

      public:
        void setInStream(std::istream* inStream) { this->inStream = inStream; }
        void setOutStream(std::ostream* outStream) { this->outStream = outStream; }

      public:
        std::vector<Token> parseTokens() { return static_cast<Derived*>(this)->parseTokens_impl(); }
        AST::Root*         parseAST() { return static_cast<Derived*>(this)->parseAST_impl(); }
    };
}  // namespace FE

#endif  // __INTERFACES_FRONTEND_PARSER_IPARSER_H__
