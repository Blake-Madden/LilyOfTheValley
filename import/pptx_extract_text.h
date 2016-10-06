/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __PPTX_TEXT_EXTRACT_H__
#define __PPTX_TEXT_EXTRACT_H__

#include "html_extract_text.h"

namespace lily_of_the_valley
    {
    /**@brief Class to extract text from a <b>Microsoft&reg; PowerPoint (2007+)</b> slide.
    @par Example:
    @code
        //Assuming that the contents of "slide[PAGENUMBER].xml" from a PPTX
        //file is in a wchar_t* buffer named "fileContents" and
        //"fileSize" is set to the size of this xml file.
        //Note that you should use the specified character set
        //in the XML file (usually UTF-8) when loading this file into the wchar_t buffer.
        lily_of_the_valley::pptx_extract_text pptxExtract;
        pptxExtract(fileContents, fileSize);

        //The raw text from the file is now in a Unicode buffer.
        //This buffer can be accessed from get_filtered_text() and its length
        //from get_filtered_text_length(). Call these to copy the text into
        //a wide string.
        std::wstring fileText(pptxExtract.get_filtered_text(), pptxExtract.get_filtered_text_length());
    @endcode*/
    class pptx_extract_text : public html_extract_text
        {
    public:
        /**Main interface for extracting plain text from a PowerPoint (2007+) slide.
        @param html_text The slide text to parse. Pass in the text from a <em>slide[<b>PAGENUMBER</b>].xml</em> file from a PPTX file. PPTX files are zip files, which can be opened by a library such as <em>zlib</em>
        @param text_length The length of the text.
        @returns The parsed text from the slide.*/
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

            //find the first paragraph and set up where we halt our searching
            const wchar_t* const endSentinel = html_text+text_length;
            const wchar_t* start = find_element(html_text, endSentinel, L"a:p", 3, false);
            const wchar_t* paragraphEnd = NULL;
            const wchar_t* rowEnd = NULL;
            const wchar_t* paragraphProperties = NULL;
            const wchar_t* paragraphPropertiesEnd = NULL;
            const wchar_t* textEnd = NULL;
            const wchar_t* nextBreak = NULL;
            bool isBulletedParagraph = true;
            bool isBulletedPreviousParagraph = true;

            while (start && (start < endSentinel))
                {
                isBulletedPreviousParagraph = isBulletedParagraph;
                isBulletedParagraph = true;
                paragraphEnd = find_closing_element(start, endSentinel, L"a:p", 3);
                if (!paragraphEnd)
                    { break; }
                paragraphProperties = find_element(start, paragraphEnd, L"a:pPr", 5, true);
                if (paragraphProperties)
                    {
                    paragraphPropertiesEnd = find_closing_element(paragraphProperties, paragraphEnd, L"a:pPr", 5); 
                    if (paragraphPropertiesEnd)
                        {
                        //see if the paragraphs in here are bullet points or real lines of text.
                        const wchar_t* bulletNoneTag = find_element(paragraphProperties, paragraphPropertiesEnd, L"a:buNone", 8, true);
                        if (bulletNoneTag)
                            { isBulletedParagraph = false; }
                        }
                    //if the paragraph is indented, then put a tab in front of it.
                    const std::wstring levelDepth = read_tag_as_string(paragraphProperties, L"lvl", 3, false);
                    if (!levelDepth.empty())
                        {
                        wchar_t* dummy = NULL;
                        const double levelDepthValue = std::wcstod(levelDepth.c_str(), &dummy);
                        if (levelDepthValue >= 1)
                            { add_character(L'\t'); }
                        }
                    }
                //if last paragraph as not a bullet point, but this one is then add an extra newline between them to differeniate them
                if (isBulletedParagraph && !isBulletedPreviousParagraph)
                    { add_character(L'\n'); }
                for (;;)
                    {
                    //go to the next row
                    nextBreak = find_element(start, paragraphEnd, L"a:br", 4, true);
                    start = find_element(start, paragraphEnd, L"a:r", 3, false);
                    if (!start || start > endSentinel)
                        {
                        //if no more runs in this paragraph, just see if there are any trailing breaks
                        if (nextBreak)
                            { add_character(L'\n'); }
                        break;
                        }
                    rowEnd = find_closing_element(++start, paragraphEnd, L"a:r", 3);
                    if (!rowEnd || rowEnd > endSentinel)
                        { break; }
                    //see if there is a break before this row. If so, then add some newlines to the output first.
                    if (nextBreak && nextBreak < start)
                        { add_character(L'\n'); }
                    //read the text section inside of it. If no valid text section, then
                    //just add a space (which an empty run implies) and skip to the next run.
                    start = find_element(start, rowEnd, L"a:t", 3, false);
                    if (!start || start > endSentinel)
                        {
                        if (get_filtered_text_length() > 0 &&
                            !std::iswspace(get_filtered_text()[get_filtered_text_length()-1]))
                            { add_character(common_lang_constants::SPACE); }
                        start = rowEnd;
                        continue;
                        }
                    start = string_util::strchr(start, common_lang_constants::GREATER_THAN);
                    if (!start || start > endSentinel)
                        {
                        start = rowEnd;
                        continue;
                        }
                    textEnd = find_closing_element(++start, rowEnd, L"a:t", 3);
                    if (!textEnd || textEnd > endSentinel)
                        {
                        start = rowEnd;
                        continue;
                        }
                    parse_raw_text(start, textEnd-start);
                    }
                //force bullet points to have two lines between them to show they are independent of each other
                if (isBulletedParagraph)
                    {
                    add_character(L'\n');
                    add_character(L'\n');
                    }
                //otherwise, lines might actually be paragraphs split to fit inside of a box
                else
                    { add_character(L'\n'); }
                //go to the next paragraph
                start = find_element(paragraphEnd, endSentinel, L"a:p", 3, false);
                }

            return get_filtered_text();
            }
        };
    }

/** @}*/

#endif //__PPTX_TEXT_EXTRACT_H__
