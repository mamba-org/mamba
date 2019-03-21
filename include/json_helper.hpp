#include "thirdparty/simdjson/simdjson.h"

#include <ostream>

void compute_dump(ParsedJson::iterator& pjh, std::ostream& os)
{
    if (pjh.is_object())
    {
        os << "{";
        if (pjh.down())
        {
            pjh.print(os); // must be a string
            os << ":";
            pjh.next();
            compute_dump(pjh, os); // let us recurse
            while (pjh.next())
            {
                os << ",";
                pjh.print(os);
                os << ":";
                pjh.next();
                compute_dump(pjh, os); // let us recurse
            }
            pjh.up();
        }
        os << "}";
    }
    else if (pjh.is_array())
    {
        os << "[";
        if (pjh.down())
        {
            compute_dump(pjh, os); // let us recurse
            while (pjh.next())
            {
                os << ",";
                compute_dump(pjh, os); // let us recurse
            }
            pjh.up();
        }
        os << "]";
    }
    else
    {
        pjh.print(os); // just print the lone value
    }
}