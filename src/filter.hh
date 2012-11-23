#ifndef filter_hh_INCLUDED
#define filter_hh_INCLUDED

#include <functional>

#include "string.hh"
#include "utils.hh"
#include "memoryview.hh"
#include "function_registry.hh"

namespace Kakoune
{

class Buffer;
class Selection;

// A Filter is a function which is applied to a Buffer and a pending
// Modification in order to mutate the Buffer or the Modification
// prior to it's application.

using FilterFunc = std::function<void (Buffer& buffer, Selection& selection, String& content)>;
using FilterAndId = std::pair<String, FilterFunc>;

using FilterParameters = memoryview<String>;
using FilterFactory = std::function<FilterAndId (const FilterParameters& params)>;

struct FilterRegistry : FunctionRegistry<FilterFactory>,
                        Singleton<FilterRegistry>
{};

}

#endif // filter_hh_INCLUDED
