#ifndef filter_hh_INCLUDED
#define filter_hh_INCLUDED

#include "function_group.hh"
#include "function_registry.hh"
#include "memoryview.hh"
#include "string.hh"
#include "utils.hh"

#include <functional>

namespace Kakoune
{

class Buffer;
struct Selection;

// A Filter is a function which is applied to a Buffer and a pending
// Modification in order to mutate the Buffer or the Modification
// prior to it's application.

using FilterFunc = std::function<void (Buffer& buffer, Selection& selection, String& content)>;
using FilterAndId = std::pair<String, FilterFunc>;
using FilterGroup = FunctionGroup<Buffer&, Selection&, String&>;

using FilterParameters = memoryview<String>;
using FilterFactory = std::function<FilterAndId (FilterParameters params)>;

struct FilterRegistry : FunctionRegistry<FilterFactory>,
                        Singleton<FilterRegistry>
{};

}

#endif // filter_hh_INCLUDED
