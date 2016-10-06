/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __DOCX_EXTRACTOR_H__
#define __DOCX_EXTRACTOR_H__

#include "html_extract_text.h"

namespace lily_of_the_valley
    {
    /**@brief Class to extract text from a <b>Microsoft&reg; Word (2007+)</b> stream (specifically, the <em>document.xml</em> file).
    @par Example:
    @code
        //Assuming that the contents of "document.xml" from a DOCX file is in a
        //wchar_t* buffer named "fileContents" and "fileSize" is set to the size
        //of this document.xml. Note that you should use the specified character set
        //in the XML file (usually UTF-8) when loading this file into the wchar_t buffer.
        lily_of_the_valley::docx_extract_text docxExtract;
        docxExtract(fileContents, fileSize);

        //The raw text from the file is now in a Unicode buffer.
        //This buffer can be accessed from get_filtered_text() and its length
        //from get_filtered_text_length(). Call these to copy the text into
        //a wide string.
        std::wstring fileText(docxExtract.get_filtered_text(), docxExtract.get_filtered_text_length());
    @endcode*/
    class docx_extract_text : public html_extract_text
        {
    public:
        docx_extract_text() : m_preserve_text_table_layout(false) {}
        /**Specifies how to import tables.
        @param preserve Set to true to import tables as tab-delimited cells of text.
        Set to false to simply import each cell as a separate paragraph, the tabbed structure of the rows may be lost.
        The default is to import cells as separate paragraphs.*/
        void preserve_text_table_layout(const bool preserve)
            { m_preserve_text_table_layout = preserve; }
        /**Main interface for extracting plain text from a DOCX stream.
        @param html_text The <em>document.xml</em> text to extract text from. <em>document.xml</em> should be extracted from a DOCX file (from the <em>word</em> folder).
        DOCX files are zip files, which can be opened by a library such as <em>zlib</em>.
        @param text_length The length of the <em>document.xml</em> stream.
        @returns A pointer to the parsed text, or NULL upon failure.
        Call get_filtered_text_length() to get the length of the parsed text.*/
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

            m_is_in_preformatted_text_block_stack = 1;//use "preserve spaces" logic in this XML

            //find the first < and set up where we halt our searching
            const wchar_t* start = string_util::strchr(html_text, common_lang_constants::LESS_THAN);
            const wchar_t* end = NULL;
            const wchar_t* const endSentinel = html_text+text_length;

            bool insideOfTableCell = false;
            while (start && (start < endSentinel))
                {
                const std::wstring currentTag = get_element_name(start+1);
                bool textSectionFound = false;
                //if it's a comment then look for matching comment ending sequence
                if (currentTag == L"!--")
                    {
                    end = string_util::strstr(start+1, L"-->");
                    if (!end)
                        { break; }
                    end += 3;//-->
                    }
                //if it's an instruction command then skip it
                else if (currentTag == L"w:instrText")
                    {
                    end = string_util::strstr(start+1, L"</w:instrText>");
                    if (!end)
                        { break; }
                    end += 14;
                    }
                //if it's an offset command then skip it
                else if (currentTag == L"wp:posOffset")
                    {
                    end = string_util::strstr(start+1, L"</wp:posOffset>");
                    if (!end)
                        { break; }
                    end += 15;
                    }
                else
                    {
                    //see if this should be treated as a new paragraph
                    if (currentTag == L"w:p")
                        {
                        if (!m_preserve_text_table_layout ||
                            (m_preserve_text_table_layout && !insideOfTableCell))
                            {
                            add_character(L'\n');
                            add_character(L'\n');
                            }
                        }
                    //if paragraph style indicates a list item
                    else if (currentTag == L"w:pStyle")                    
                        {
                        if (read_tag_as_string(start+1, L"w:val", 5, false) == L"ListParagraph")
                            { add_character(L'\t'); }
                        }
                    //or a tab
                    else if (currentTag == L"w:tab")                    
                        { add_character(L'\t'); }
                    //hard breaks
                    else if (currentTag == L"w:br" || currentTag == L"w:cr")                    
                        { add_character(L'\n'); }
                    //or if it's aligned center or right
                    else if (currentTag == L"w:jc")                    
                        {
                        std::wstring alignment = read_tag_as_string(start+1, L"w:val", 5, false);
                        if (alignment == L"center" ||
                            alignment == L"right" ||
                            alignment == L"both" ||
                            alignment == L"list-tab")
                            { add_character(L'\t'); }
                        }
                    //or if it's indented
                    else if (currentTag == L"w:ind")                    
                        {
                        std::wstring indentationString = read_tag_as_string(start+1, L"w:left", 6, false);
                        if (!indentationString.empty())
                            {
                            wchar_t* dummy = NULL;
                            const double alignment = std::wcstod(indentationString.c_str(), &dummy);
                            if (alignment > 0.0f)
                                { add_character(L'\t'); }
                            }
                        }
                    //tab over table cell and newline for table rows
                    else if (currentTag == L"w:tr")                    
                        {
                        add_character(L'\n');
                        add_character(L'\n');
                        }
                    else if (compare_element_case_sensitive(start+1, L"w:tc", 4, true) )                    
                        {
                        add_character(L'\t');
                        insideOfTableCell = true;
                        }
                    else if (compare_element_case_sensitive(start+1, L"/w:tc", 5) )                    
                        { insideOfTableCell = false; }
                    else if (compare_element_case_sensitive(start+1, L"w:t", 3) )  
                        { textSectionFound = true; }
                    /*find the matching >, but watch out for an errant < also in case
                    the previous < wasn't terminated properly*/
                    end = string_util::strcspn_pointer<wchar_t>(start+1, L"<>", 2);
                    if (!end)
                        { break; }
                    /*if the < tag that we started from is not terminated then feed that in as
                    text instead of treating it like a valid HTML tag.  Not common, but it happens.*/
                    else if (end[0] == common_lang_constants::LESS_THAN)
                        {
                        /*copy over the text from the unterminated < to the currently found
                        < (that we will start from in the next loop*/
                        parse_raw_text(start, end-start);
                        //set the starting point to the next < that we already found
                        start = end;
                        continue;
                        }
                    //more normal behavior, where tag is properly terminated
                    else
                        { ++end; }
                    }
                //find the next starting tag
                start = string_util::strchr(end, common_lang_constants::LESS_THAN);
                if (!start)
                    { break; }
                //copy over the text between the tags
                if (textSectionFound)
                    { parse_raw_text(end, start-end); }
                }

            return get_filtered_text();
            }
    private:
        bool m_preserve_text_table_layout;
        };
    }

/** @}*/

#endif //__DOCX_EXTRACTOR_H__
