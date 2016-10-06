/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __HHC_HHK_EXTRACT_TEXT_H__
#define __HHC_HHK_EXTRACT_TEXT_H__
 
#include "html_extract_text.h"

namespace lily_of_the_valley
    {
    /**@brief Class to extract text from an <b>HHK (Microsoft&reg; HTML Workshop Index)/HHC (Microsoft&reg; HTML Workshop Table of Contents)</b> stream.*/
    class hhc_hhk_extract_text : public html_extract_text
        {
    public:
        /**Main interface for extracting plain text from a HTML Workshop index or
        table of contents buffer. This text is the labels shown in the TOC and index.
        @param html_text The HHK/HHC text to extract from.
        @param text_length The length of the text.
        @returns A pointer to the parsed text, or NULL upon failure.*/
        const wchar_t* operator()(const wchar_t* html_text,
                                  const size_t text_length)
            {
            clear_log();
            if (html_text == NULL || html_text[0] == 0 || text_length == 0)
                {
                set_filtered_text_length(0);
                return NULL;
                }
            assert(text_length <= string_util::strlen(html_text) );

            if (!allocate_text_buffer(text_length))
                {
                set_filtered_text_length(0);
                return NULL;
                }

            //find the first < and set up where we halt our searching
            const wchar_t* start = string_util::strchr(html_text, common_lang_constants::LESS_THAN);
            const wchar_t* endSentinel = html_text+text_length;

            while (start && (start < endSentinel))
                {
                const std::wstring currentTag = get_element_name(start+1);

                if (currentTag == L"param")
                    {
                    if (read_tag_as_string(start+6/*skip over "<param"*/, L"name", 4, false) == L"Name")
                        {
                        const std::wstring StrValue = read_tag_as_string(start+6/*skip over "<param"*/, L"value", 5, false, true);
                        parse_raw_text(StrValue.c_str(), StrValue.length());
                        add_character(L'\n');
                        add_character(L'\n');
                        }
                    }
                //find the next starting tag
                start = string_util::strchr(start+1, common_lang_constants::LESS_THAN);
                if (!start)
                    { break; }
                }

            return get_filtered_text();
            }
        };
    }

/** @}*/
    
#endif //__HHC_HHK_EXTRACT_TEXT_H__
