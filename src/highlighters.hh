#ifndef highlighters_hh_INCLUDED
#define highlighters_hh_INCLUDED

#include "color.hh"
#include "highlighter.hh"
#include "option.hh"

namespace Kakoune
{

void register_highlighters();

String option_to_string(BufferRange range);
BufferRange option_from_string(Meta::Type<BufferRange>, StringView str);

using LineAndSpec = std::tuple<LineCount, String>;
using LineAndSpecList = TimestampedList<LineAndSpec>;

constexpr StringView option_type_name(Meta::Type<LineAndSpecList>)
{
    return "line-specs";
}
void option_update(LineAndSpecList& opt, const Context& context);
void option_list_postprocess(Vector<LineAndSpec, MemoryDomain::Options>& opt);

using RangeAndString = std::tuple<BufferRange, String>;
using RangeAndStringList = TimestampedList<RangeAndString>;

constexpr StringView option_type_name(Meta::Type<RangeAndStringList>)
{
    return "range-specs";
}
void option_update(RangeAndStringList& opt, const Context& context);
void option_list_postprocess(Vector<RangeAndString, MemoryDomain::Options>& opt);

}

#endif // highlighters_hh_INCLUDED
