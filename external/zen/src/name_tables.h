#ifndef ZEN_NAME_TABLES_H
#define ZEN_NAME_TABLES_H

#include "vm.h"
#include <cstring>

namespace zen
{
    struct OperatorName
    {
        const char *name;
        uint8_t len;
        int slot;
    };

    static const OperatorName kOperatorNames[] = {
        {"__add__", 7, VM::SLOT_ADD},
        {"__radd__", 8, VM::SLOT_RADD},
        {"__sub__", 7, VM::SLOT_SUB},
        {"__rsub__", 8, VM::SLOT_RSUB},
        {"__mul__", 7, VM::SLOT_MUL},
        {"__rmul__", 8, VM::SLOT_RMUL},
        {"__div__", 7, VM::SLOT_DIV},
        {"__rdiv__", 8, VM::SLOT_RDIV},
        {"__mod__", 7, VM::SLOT_MOD},
        {"__rmod__", 8, VM::SLOT_RMOD},
        {"__neg__", 7, VM::SLOT_NEG},
        {"__eq__", 6, VM::SLOT_EQ},
        {"__lt__", 6, VM::SLOT_LT},
        {"__le__", 6, VM::SLOT_LE},
        {"__str__", 7, VM::SLOT_STR},
    };

    static inline int operator_slot_for_name(const char *name, int len)
    {
        for (int i = 0; i < (int)(sizeof(kOperatorNames) / sizeof(kOperatorNames[0])); i++)
        {
            if (kOperatorNames[i].len == len &&
                memcmp(kOperatorNames[i].name, name, (size_t)len) == 0)
                return kOperatorNames[i].slot;
        }
        return -1;
    }
}

#endif /* ZEN_NAME_TABLES_H */
