/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __ODF_TEXT_EXTRACT_H__
#define __ODF_TEXT_EXTRACT_H__

#include "html_extract_text.h"

namespace lily_of_the_valley
    {
    /**@brief Class to extract text from a <b>Open Document Text</b> stream (specifically, the <em>content.xml</em> file).
    @par Example:
    @code
        //Assuming that the contents of "content.xml" from an ODT file is in a
        //wchar_t* buffer named "fileContents" and "fileSize" is set to the size
        //of this content.xml. Note that you should use the specified character set
        //in the XML file (usually UTF-8) when loading this file into the wchar_t buffer.
        lily_of_the_valley::odt_extract_text odtExtract;
        odtExtract(fileContents, fileSize);

        //The raw text from the file is now in a Unicode buffer.
        //This buffer can be accessed from get_filtered_text() and its length
        //from get_filtered_text_length(). Call these to copy the text into
        //a wide string.
        std::wstring fileText(odtExtract.get_filtered_text(), odtExtract.get_filtered_text_length());
    @endcode*/
    class odt_extract_text : public html_extract_text
        {
    public:
        odt_extract_text() : m_preserve_text_table_layout(false) {}
        /**Specifies how to import tables.
        @param preserve Set to true to not import text cells as separate paragraphs, but instead as cells of text with tabs
        between them. Set to false to simply import each cell as a separate paragraph, the tabbed structure of the rows will be lost.*/
        void preserve_text_table_layout(const bool preserve)
            { m_preserve_text_table_layout = preserve; }
        /**Main interface for extracting plain text from a content.xml buffer.
        @param html_text The <em>content.xml</em> text to extract text from. <em>content.xml</em> is extracted from an ODT file. ODT files are zip files, which can be opened by a library such as <em>zlib</em>
        @param text_length The length of the <em>content.xml</em> stream.
        @returns The plain text from the ODT stream.*/
        const wchar_t* operator()(const wchar_t* html_text,
                                  const size_t text_length)
            {
            static const std::wstring OFFICE_ANNOTATION(L"office:annotation");
            static const std::wstring OFFICE_ANNOTATION_END(L"</office:annotation>");
            //text section tags
            const std::wstring TEXT_P(L"text:p");
            const std::wstring TEXT_P_END(L"</text:p>");
            const std::wstring TEXT_H(L"text:h");
            const std::wstring TEXT_H_END(L"</text:h>");
            //tables
            const std::wstring TABLE_ROW(L"table:table-row");
            //paragraph info
            const std::wstring TEXT_STYLE_NAME(L"text:style-name");

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

            read_paragraph_styles(html_text, endSentinel);

            int textSectionDepth = 0;
            bool insideOfListItemOrTableCell = false;
            while (start && (start < endSentinel))
                {
                bool textSectionFound = true;
                //if it's a comment then look for matching comment ending sequence
                if ((endSentinel-start) >= 4 && start[0] == common_lang_constants::LESS_THAN &&
                    start[1] == L'!' && start[2] == L'-' && start[3] == L'-')
                    {
                    end = string_util::strstr(start, L"-->");
                    if (!end)
                        { break; }
                    end += 3;//-->
                    }
                //if it's an annotation (e.g., a note) then skip it
                else if (compare_element_case_sensitive(start+1, OFFICE_ANNOTATION.c_str(), OFFICE_ANNOTATION.length() ) )
                    {
                    end = string_util::strstr(start, OFFICE_ANNOTATION_END.c_str());
                    if (!end)
                        { break; }
                    end += OFFICE_ANNOTATION_END.length();
                    }
                else
                    {
                    //see if this should be treated as a new paragraph
                    if (compare_element_case_sensitive(start+1, TEXT_P.c_str(), TEXT_P.length(), true) ||
                        compare_element_case_sensitive(start+1, TEXT_H.c_str(), TEXT_H.length(), true))                    
                        {
                        if (!m_preserve_text_table_layout ||
                            (m_preserve_text_table_layout && !insideOfListItemOrTableCell))
                            {
                            //read the style to see if this paragraph is indented
                            std::wstring styleName = read_tag_as_string(start+1, TEXT_STYLE_NAME.c_str(), TEXT_STYLE_NAME.length(), false);
                            //if this paragraph's style is indented then include a tab in front of it
                            if (std::find(m_indented_paragraph_styles.begin(), m_indented_paragraph_styles.end(), styleName) != m_indented_paragraph_styles.end())
                                {
                                add_character(L'\n');
                                add_character(L'\n');
                                add_character(L'\t');
                                }
                            else
                                {
                                add_character(L'\n');
                                add_character(L'\n');
                                }
                            }
                        ++textSectionDepth;
                        }
                    else if (compare_element_case_sensitive(start+1, L"text:span", 9, true))
                        { ++textSectionDepth; }
                    //or end of a section
                    else if (string_util::strncmp(start, TEXT_P_END.c_str(), TEXT_P_END.length() ) == 0 ||
                        string_util::strncmp(start, TEXT_H_END.c_str(), TEXT_H_END.length() ) == 0 ||
                        string_util::strncmp(start, L"</text:span>", 12) == 0)                    
                        { --textSectionDepth; }
                    //beginning of a list item
                    else if (compare_element_case_sensitive(start+1, L"text:list-item", 14) )
                        {
                        add_character(L'\n');
                        add_character(L'\t');
                        insideOfListItemOrTableCell = true;
                        }
                    //end of a list item
                    else if (compare_element_case_sensitive(start+1, L"/text:list-item", 15) )                    
                        { insideOfListItemOrTableCell = false; }
                    //tab over table cell and newline for table rows
                    else if (compare_element_case_sensitive(start+1, TABLE_ROW.c_str(), TABLE_ROW.length() ) )                    
                        {
                        add_character(L'\n');
                        add_character(L'\n');
                        }
                    //tab over for a cell
                    else if (compare_element_case_sensitive(start+1, L"table:table-cell", 16) )                    
                        {
                        add_character(L'\t');
                        insideOfListItemOrTableCell = true;
                        }
                    else if (compare_element_case_sensitive(start+1, L"/table:table-cell", 17) )                    
                        { insideOfListItemOrTableCell = false; }
                    //or a tab
                    else if (compare_element_case_sensitive(start+1, L"text:tab", 8, true) )                    
                        { add_character(L'\t'); }
                    //hard breaks
                    else if (compare_element_case_sensitive(start+1, L"text:line-break", 15, true) )                    
                        { add_character(L'\n'); }
                    else
                        { textSectionFound = (textSectionDepth > 0) ? true : false; }
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
        ///Reads in all of the paragraph styles, looking for any styles that involve text alignment.
        void read_paragraph_styles(const wchar_t* text, const wchar_t* textEnd)
            {
            //items for reading in general style information
            const std::wstring STYLE_STYLE_END(L"</style:style>");
            const std::wstring STYLE_NAME(L"style:name");
            const std::wstring ALIGNMENT(L"fo:text-align");
            const std::wstring MARGIN_ALIGNMENT(L"fo:margin-left");

            const wchar_t* const officeStyleStart = find_element(text, textEnd, L"office:automatic-styles", 23);
            if (!officeStyleStart)
                { return; }
            const wchar_t* const officeStyleEnd = find_closing_element(officeStyleStart, textEnd, L"office:automatic-styles", 23);
            if (officeStyleEnd)
                {
                //go through all of the styles in the office styles section
                const wchar_t* currentStyleStart = find_element(officeStyleStart, textEnd, L"style:style", 11);
                while (currentStyleStart)
                    {
                    const wchar_t* currentStyleEnd = find_closing_element(currentStyleStart, textEnd, L"style:style", 11);
                    if (currentStyleStart && currentStyleEnd && (currentStyleStart < currentStyleEnd))
                        {
                        //read in the name of the current style
                        const std::wstring styleName = read_tag_as_string(currentStyleStart,
                            STYLE_NAME.c_str(), STYLE_NAME.length(), false, true);
                        if (styleName.empty())
                            {
                            currentStyleStart = currentStyleEnd + STYLE_STYLE_END.length();
                            continue;
                            }
                        currentStyleStart = find_element(currentStyleStart, currentStyleEnd, L"style:paragraph-properties", 26);
                        if (!currentStyleStart)
                            { break; }
                        else if (currentStyleStart > currentStyleEnd)
                            {
                            currentStyleStart = currentStyleEnd + STYLE_STYLE_END.length();
                            continue;
                            }
                        //read in the paragraph alignment and if it's indented then add it to our collection of indented styles
                        const std::wstring alignment = read_tag_as_string(currentStyleStart,
                            ALIGNMENT.c_str(), ALIGNMENT.length(), false, true);
                        if (alignment == L"center" || alignment == L"end")
                            {m_indented_paragraph_styles.push_back(styleName); }
                        else
                            {
                            const std::wstring marginAlignment = read_tag_as_string(currentStyleStart,
                                MARGIN_ALIGNMENT.c_str(), MARGIN_ALIGNMENT.length(), false, true);
                            if (!marginAlignment.empty())
                                {
                                wchar_t* dummy = NULL;
                                const double alignmentValue = std::wcstod(marginAlignment.c_str(), &dummy);
                                if (alignmentValue > 0.0f)
                                    { m_indented_paragraph_styles.push_back(styleName); }
                                }
                            }
                        }
                    else
                        { break; }
                    currentStyleStart = currentStyleEnd + STYLE_STYLE_END.length();
                    }
                }
            }
        std::vector<std::wstring> m_indented_paragraph_styles;

        bool m_preserve_text_table_layout;
        };
    }

/** @}*/

#endif //__ODF_TEXT_EXTRACT_H__
