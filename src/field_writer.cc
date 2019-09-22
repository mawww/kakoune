#include "field_writer.hh"
#include "string.hh"

namespace Kakoune
{

String debug_dump(RemoteBuffer const& buffer)
{
    String result;
    char hex[25], ascii[17] = {};

    for (unsigned row = 0; row < buffer.size(); row += 16)
    {
        snprintf(hex, sizeof hex, "  %04x: ", row);
        result += hex;
        for (unsigned column = 0; column < 16; ++column)
        {
            if (row+column >= buffer.size())
            {
                result += "   ";
                ascii[column] = ' ';
            }
            else
            {
                unsigned b = static_cast<unsigned char>(buffer[row+column]);
                snprintf(hex, sizeof hex, "%02x ", b);
                result += hex;
                if (b >= 0x20 && b < 127)
                    ascii[column] = b;
                else
                    ascii[column] = '.';
            }
        }
        result += " ";
        result += ascii;
        result += "\n";
    }
    return result;
}

}
