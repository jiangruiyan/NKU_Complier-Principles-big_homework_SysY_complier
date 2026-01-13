#ifndef __FRONTEND_SYMBOL_SYMBOL_ENTRY_H__
#define __FRONTEND_SYMBOL_SYMBOL_ENTRY_H__

#include <string>
#include <unordered_map>

namespace FE::Sym
{
    class Entry
    {
        friend class EntryDeleter;

      private:
        static std::unordered_map<std::string, Entry*> entryMap;
        static void                                    clear();

      public:
        static Entry* getEntry(std::string name);

      private:
        Entry(std::string name = "NULL");
        ~Entry() = default;
        std::string name;

      public:
        const std::string& getName();
    };

    class EntryDeleter
    {
      private:
        EntryDeleter();
        ~EntryDeleter();

      public:
        EntryDeleter(const EntryDeleter&)                   = delete;
        EntryDeleter&        operator=(const EntryDeleter&) = delete;
        static EntryDeleter& getInstance();
    };
}  // namespace FE::Sym

#endif  // __FRONTEND_SYMBOL_SYMBOL_ENTRY_H__
