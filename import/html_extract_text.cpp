#include "html_extract_text.h"

namespace lily_of_the_valley
    {
    std::wstring html_extract_text::read_tag_as_string(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags,
            const bool allowSpacesInValue /*= false*/)
        {
        if (!text || !tag || tagSize == 0)
            { return L""; }
        assert((std::wcslen(tag) == tagSize) && "Invalid length passed to read_tag_as_string().");
        std::pair<const wchar_t*, size_t> rt = read_tag(text, tag, tagSize, allowQuotedTags, allowSpacesInValue);
        if (rt.first == NULL)
            { return L""; }
        else
            { return std::wstring(rt.first,rt.second); }
        }

    const std::pair<const wchar_t*,std::wstring> html_extract_text::find_bookmark(const wchar_t* sectionStart,
                                            const wchar_t* sectionEnd)
        {
        const wchar_t* nextAnchor = find_element(sectionStart, sectionEnd, L"a", 1);
        if (nextAnchor)
            {
            std::pair<const wchar_t*,size_t> bk = read_tag(nextAnchor, L"name", 4, false);
            if (bk.first)
                {
                std::wstring bookMark(bk.first, bk.second);
                //chop if the leading '#' from the bookmark name
                if (bookMark[0] == L'#')
                    { bookMark = bookMark.substr(1); }
                return std::pair<const wchar_t*,std::wstring>(nextAnchor, bookMark);
                }
            //if this anchor doesn't have a bookmark, then recursively look for the next candidate
            return find_bookmark(nextAnchor+1, sectionEnd);
            }
        return std::pair<const wchar_t*,std::wstring>(NULL, L"");
        }

    const wchar_t* html_extract_text::strchr_not_quoted(const wchar_t* string, const wchar_t ch)
        {
        if (!string)
            { return NULL; }
        bool is_inside_of_quotes = false;
        bool is_inside_of_single_quotes = false;
        while (string)
            {
            if (string[0] == 0)
                { return NULL; }
            else if (string[0] == 0x22)//double quote
                {
                is_inside_of_quotes = !is_inside_of_quotes;
                //whether this double quote ends a quote pair or starts a new one, turn this flag
                //off. This means that a double quote can close a single quote.
                is_inside_of_single_quotes = false;
                }
            //if a single quote already started a quote pair (and this is closing it) or
            //we are not inside of a double quote then count single quotes
            else if ((!is_inside_of_quotes || is_inside_of_single_quotes) && string[0] == 0x27)//single quote
                {
                is_inside_of_quotes = !is_inside_of_quotes;
                is_inside_of_single_quotes = true;
                }
            if (!is_inside_of_quotes && string[0] == ch)
                { return string; }
            ++string;
            }
        return NULL;
        }

    void html_extract_text::parse_raw_text(const wchar_t* text, size_t textSize)
        {
        size_t currentStartPosition = 0;
        if (textSize > 0)
            {
            while (textSize > 0)
                {
                size_t index = 0;
                //if preformatted then just look for ampersands
                if (m_is_in_preformatted_text_block_stack > 0)
                    { index = string_util::strncspn<wchar_t>(text+currentStartPosition, textSize, L"&", 1); }
                //otherwise, eat up crlfs and replace with spaces
                else
                    { index = string_util::strncspn<wchar_t>(text+currentStartPosition, textSize, L"\r\n&", 3); }
                index += currentStartPosition;//move the index to the position in the full valid string
                if (index < textSize)
                    {
                    if (text[index] == common_lang_constants::AMPERSAND)
                        {
                        const wchar_t* semicolon = string_util::strcspn_pointer<wchar_t>(text+index+1, L";< \t\n\r", 6);
                        /*this should not happen in valid HTML, but in case there is an
                        orphan & then skip it and look for the next item.*/
                        if (semicolon == NULL ||
                            (semicolon > text+textSize))
                            {
                            //copy over the proceeding text (up to the ampersand)
                            if (index > 0)
                                {
                                add_characters(text, index);
                                add_character(common_lang_constants::AMPERSAND);
                                }
                            //update indices into the raw HTML text
                            if ((index+1) > textSize)
                                { textSize = 0; }
                            else
                                { textSize -= index+1; }
                            text += index+1;
                            currentStartPosition = 0;
                            continue;
                            }
                        else
                            {
                            //copy over the proceeding text
                            if (index > 0)
                                { add_characters(text, index); }
                            //in case this is an unencoded ampersand then treat it as such
                            if (std::iswspace(text[index+1]))
                                {
                                add_character(common_lang_constants::AMPERSAND);
                                add_character(common_lang_constants::SPACE);
                                }
                            //convert an encoded number to character
                            else if (text[index+1] == common_lang_constants::POUND)
                                {
                                const wchar_t value = is_either(text[index+2], common_lang_constants::LOWER_X, common_lang_constants::UPPER_X) ?
                                    //if it is hex encoded
                                    static_cast<wchar_t>(string_util::axtoi(text+index+3, (textSize-(index+3))))/*skip "&#x"*/ :
                                    //else it is a plain numeric value
                                    static_cast<wchar_t>(string_util::atoi(text+index+2));
                                if (value != 173)//soft hyphens should just be stripped out
                                    {
                                    //ligatures
                                    if (is_within<wchar_t>(value, 0xFB00, 0xFB06))
                                        {
                                        switch (value)
                                            {
                                        case 0xFB00:
                                            add_characters(L"ff",2);
                                            break;
                                        case 0xFB01:
                                            add_characters(L"fi",2);
                                            break;
                                        case 0xFB02:
                                            add_characters(L"fl",2);
                                            break;
                                        case 0xFB03:
                                            add_characters(L"ffi",3);
                                            break;
                                        case 0xFB04:
                                            add_characters(L"ffl",3);
                                            break;
                                        case 0xFB05:
                                            add_characters(L"ft",2);
                                            break;
                                        case 0xFB06:
                                            add_characters(L"st",2);
                                            break;
                                            };
                                        }
                                    else if (value != 0)
                                        { add_character(value); }
                                    //in case conversion failed to come up with a number (incorrect encoding in the HTML maybe)
                                    else
                                        {
                                        log_message(L"Invalid numeric HTML entity: "+std::wstring(text+index, (semicolon+1)-(text+index)));
                                        add_characters(text+index, (semicolon+1)-(text+index));
                                        }
                                    }
                                }
                            //look up named entities, such as "amp" or "nbsp"
                            else
                                {
                                const wchar_t value = HTML_TABLE_LOOKUP.find(text+index+1, semicolon-(text+index+1));
                                if (value != 173)//soft hyphens should just be stripped out
                                    {
                                    //Missing semicolon and not a valid entity?  Must be an unencoded ampersand with a letter right next to it, so just copy that over
                                    if (value == common_lang_constants::QUESTION_MARK && semicolon[0] != common_lang_constants::SEMICOLON)
                                        {
                                        log_message(L"Unencoded ampersand or unknown HTML entity: "+std::wstring(text+index, semicolon-(text+index)));
                                        add_characters(text+index, (semicolon-(text+index)+1) );
                                        }
                                    else
                                        {
                                        //Check for something like "&amp;le;", which should really be "&le;".
                                        //Workaround around it and log a warning.
                                        bool leadingAmpersandEncodedCorrectly = true;
                                        if (semicolon[0] == common_lang_constants::SEMICOLON &&
                                            value == common_lang_constants::AMPERSAND)
                                            {
                                            const wchar_t* nextTerminator = semicolon+1;
                                            while (!iswspace(*nextTerminator) && *nextTerminator != common_lang_constants::SEMICOLON &&
                                                    nextTerminator < (text+textSize))
                                                { ++nextTerminator; }
                                            if (nextTerminator < (text+textSize) && *nextTerminator == common_lang_constants::SEMICOLON)
                                                {
                                                const wchar_t badlyEncodedEntity = HTML_TABLE_LOOKUP.find(semicolon+1, nextTerminator-(semicolon+1));
                                                if (badlyEncodedEntity != common_lang_constants::QUESTION_MARK)
                                                    {
                                                    log_message(L"Ampersand incorrectly encoded in HTML entity: "+std::wstring(text+index, (nextTerminator-(text+index))+1));
                                                    leadingAmpersandEncodedCorrectly = false;
                                                    semicolon = nextTerminator;
                                                    add_character(badlyEncodedEntity);
                                                    }
                                                }
                                            }
                                        //appears to be a correctly-formed entity
                                        if (leadingAmpersandEncodedCorrectly)
                                            {
                                            add_character(value);
                                            if (value == common_lang_constants::QUESTION_MARK)
                                                { log_message(L"Unknown HTML entity: "+std::wstring(text+index, semicolon-(text+index))); }
                                            //Entity not correctly terminated by a semicolon. Here we will copy over the converted entity and trailing character (a space or newline).
                                            if (semicolon[0] != common_lang_constants::SEMICOLON)
                                                {
                                                log_message(L"Missing semicolon on HTML entity: "+std::wstring(text+index, semicolon-(text+index)));
                                                add_character(semicolon[0]);
                                                }
                                            }
                                        }
                                    }
                                }
                            //update indices into the raw HTML text
                            if (static_cast<size_t>((semicolon+1)-(text)) > textSize)
                                { textSize = 0; }
                            else
                                { textSize -= (semicolon+1)-(text); }
                            text = semicolon+1;
                            currentStartPosition = 0;
                            }
                        }
                    else
                        {
                        //copy over the proceeding text
                        add_characters(text, index);
                        add_character(common_lang_constants::SPACE);
                        //update indices into the raw HTML text
                        if ((index+1) > textSize)
                            { textSize = 0; }
                        else
                            { textSize -= index+1; }
                        text += index+1;
                        currentStartPosition = 0;
                        }
                    }
                //didn't find anything else, so stop scanning this section of text
                else
                    { break; }
                }

            if (textSize > 0)
                { add_characters(text, textSize); }
            }
        }

    std::wstring html_extract_text::convert_symbol_font_section(const std::wstring& symbolFontText)
        {
        std::wstring convertedText;
        for (size_t i = 0; i < symbolFontText.length(); ++i)
            { convertedText += SYMBOL_FONT_TABLE.find(symbolFontText[i]); }
        return convertedText;
        }

    std::string html_extract_text::parse_charset(const char* pageContent, const size_t length)
        {
        std::string charset;
        if (pageContent == NULL || pageContent[0] == 0)
            { return charset; }

        const char* const end = pageContent+length;
        const char* start = string_util::strnistr(pageContent, "<meta", (end-pageContent));
        //No Meta section?
        if (!start)
            {
            //See if this XML and parse it that way. Otherwise, there is no charset.
            if (std::strncmp(pageContent, "<?xml", 5) == 0)
                {
                const char* encoding = std::strstr(pageContent, "encoding=\"");
                if (encoding)
                    {
                    encoding += 10;
                    const char* encodingEnd = std::strchr(encoding, '\"');
                    if (encodingEnd)
                        { charset = std::string(encoding, (encodingEnd-encoding)); }
                    }
                }
            return charset;
            }
        while (start && start < end)
            {
            const char* nextAngleSymbol = string_util::strnchr(start, '>', (end-start));
            const char* contentType = string_util::strnistr(start, "content-type", (end-start));
            if (!contentType || !nextAngleSymbol)
                { return charset; }
            const char* contentStart = string_util::strnistr(start, " content=", (end-start));
            if (!contentStart || !nextAngleSymbol)
                { return charset; }
            //if the content-type and content= are inside of this meta tag then it's legit, so move to it and stop looking
            if (contentType < nextAngleSymbol &&
                contentStart < nextAngleSymbol)
                {
                start = contentStart;
                break;
                }
            //otherwise, skip to the next meta tag
            start = string_util::strnistr(nextAngleSymbol, "<meta", (end-nextAngleSymbol));
            if (!start)
                { return charset; }
            }

        start += 9;
        if (start[0] == '\"' || start[0] == '\'')
            { ++start; }
        const char* nextAngle = string_util::strnchr(start, '>', (end-start));
        const char* nextClosedAngle = string_util::strnistr(start, "/>", (end-start));
        //no close angle?  This HMTL is messed up, so just return the default charset
        if (!nextAngle && !nextClosedAngle)
            { return charset; }
        //see which closing angle is the closest one
        if (nextAngle && nextClosedAngle)
            { nextAngle = std::min(nextAngle,nextClosedAngle); }
        else if (!nextAngle && nextClosedAngle)
            { nextAngle = nextClosedAngle; }

        //find and parse the content type
        bool charsetFound = false;
        const char* const contentSection = start;
        start = string_util::strnistr(contentSection, "charset=", (end-contentSection));
        if (start && start < nextAngle)
            {
            start += 8;
            charsetFound = true;
            }
        else
            {
            start = string_util::strnchr<char>(contentSection, common_lang_constants::SEMICOLON, (end-contentSection));
            if (start && start < nextAngle)
                {
                ++start;
                charsetFound = true;
                }
            }
        if (charsetFound)
            {
            //chop off any quotes and trailing whitespace
            while (start && start < nextAngle)
                {
                if (start[0] == common_lang_constants::SPACE || start[0] == '\'')
                    { ++start; }
                else
                    { break; }
                }
            const char* charsetEnd = start;
            while (charsetEnd && charsetEnd < nextAngle)
                {
                if (charsetEnd[0] != common_lang_constants::SPACE && charsetEnd[0] != '\'' && charsetEnd[0] != '\"' && charsetEnd[0] != '/' && charsetEnd[0] != '>')
                    { ++charsetEnd; }
                else
                    { break; }
                }
            charset.assign(start, charsetEnd-start);
            return charset;
            }
        else
            { return charset; }
        }

    const wchar_t* html_extract_text::stristr_not_quoted(
            const wchar_t* string, const size_t stringSize,
            const wchar_t* strSearch, const size_t strSearchSize)
        {
        if (!string || !strSearch || stringSize == 0 || strSearchSize == 0)
            { return NULL; }

        bool is_inside_of_quotes = false;
        bool is_inside_of_single_quotes = false;
        const wchar_t* const endSentinel = string+stringSize;
        while (string && (string+strSearchSize <= endSentinel))
            {
            //compare the characters one at a time
            size_t i = 0;
            for (i = 0; i < strSearchSize; ++i)
                {
                if (string[i] == 0)
                    { return NULL; }
                else if (string[i] == 0x22)//double quote
                    {
                    is_inside_of_quotes = !is_inside_of_quotes;
                    //whether this double quote ends a quote pair or starts a new one, turn this flag
                    //off. This means that a double quote can close a single quote.
                    is_inside_of_single_quotes = false;
                    }
                //if a single quote already started a quote pair (and this is closing it) or
                //we are not inside of a double quote then count single quotes
                else if ((!is_inside_of_quotes || is_inside_of_single_quotes) && string[0] == 0x27)//single quote
                    {
                    is_inside_of_quotes = !is_inside_of_quotes;
                    is_inside_of_single_quotes = true;
                    }
                if (string_util::tolower(strSearch[i]) != string_util::tolower(string[i]) )
                    { break; }
                }
            //if the substring loop completed then the substring was found.            
            if (i == strSearchSize && string+strSearchSize <= endSentinel)
                {
                //make sure we aren't inside of quotes--if so, we need to skip it.
                if (!is_inside_of_quotes)
                    { return string; }
                else
                    { string += strSearchSize; }
                }
            else
                { string += i+1; }
            }
        return NULL;
        }

    std::pair<const wchar_t*, size_t> html_extract_text::read_tag(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags,
            const bool allowSpacesInValue /*= false*/)
        {
        if (!text || !tag || tagSize == 0)
            { return std::pair<const wchar_t*, size_t>(NULL,0); }
        assert((std::wcslen(tag) == tagSize) && "Invalid length passed to read_tag().");
        const wchar_t* foundTag = find_tag(text, tag, tagSize, allowQuotedTags);
        const wchar_t* elementEnd = find_close_tag(text);
        if (foundTag && elementEnd &&
            foundTag < elementEnd)
            {
            foundTag += tagSize;
            const size_t startIndex = string_util::find_first_not_of(foundTag, (elementEnd-foundTag), L" =\"':", 5);
            if (startIndex == static_cast<size_t>(elementEnd-foundTag))
                { return std::pair<const wchar_t*, size_t>(NULL,0); }
            foundTag += startIndex;
            const wchar_t* end = 
                (allowQuotedTags && allowSpacesInValue) ?
                    string_util::strcspn_pointer(foundTag, L"\"'>;", 4) :
                allowQuotedTags ?
                    string_util::strcspn_pointer(foundTag, L" \"'>;", 5) :
                allowSpacesInValue ?
                    string_util::strcspn_pointer(foundTag, L"\"'>", 3) :
                //not allowing spaces and the tag is not inside of quotes (like a style section)
                    string_util::strcspn_pointer(foundTag, L" \"'>", 4);
            if (end && (end <= elementEnd))
                {
                //If at the end of the element, trim off any trailing spaces or a terminating '/'.
                //Note that we don't search for '/' above because it can be inside of a valid tag value
                //(such as a file path).
                if (*end == L'>')
                    {
                    while (end-1 > foundTag)
                        {
                        if (is_either<wchar_t>(*(end-1), L'/', L' '))
                            { --end; }
                        else
                            { break; }
                        }
                    }
                return std::pair<const wchar_t*, size_t>(foundTag, (end-foundTag));
                }
            else
                { return std::pair<const wchar_t*, size_t>(NULL,0); }
            }
        else
            { return std::pair<const wchar_t*, size_t>(NULL,0); }
        }

    const wchar_t* html_extract_text::find_tag(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags)
        {
        if (!text || !tag || tagSize == 0)
            { return NULL; }
        const wchar_t* foundTag = text;
        const wchar_t* const elementEnd = find_close_tag(text);
        if (!elementEnd)
            { return NULL; }
        while (foundTag)
            {
            foundTag = allowQuotedTags ?
                string_util::strnistr<wchar_t>(foundTag, tag, (elementEnd-foundTag))
                : stristr_not_quoted(foundTag, (elementEnd-foundTag), tag, tagSize);
            if (!foundTag || (foundTag > elementEnd))
                { return NULL; }
            if (foundTag == text)
                { return foundTag; }
            else if (allowQuotedTags && is_either<wchar_t>(foundTag[-1], L'\'', L'\"'))
                { return foundTag; }
            //this tag should not be count if it is really just part of a bigger tag (e.g., "color" will
            //not count if what we are really on is "bgcolor")
            else if (std::iswspace(foundTag[-1]) || (foundTag[-1] == common_lang_constants::SEMICOLON))
                { return foundTag; }
            foundTag += tagSize;
            }
        return NULL;
        }

    const wchar_t* html_extract_text::operator()(const wchar_t* html_text,
                                                 const size_t text_length,
                                                 const bool include_outer_text /*= true*/,
                                                 const bool preserve_spaces /*= false*/)
        {
        static const std::wstring HTML_STYLE_END(L"</style>");
        static const std::wstring HTML_SCRIPT(L"script");
        static const std::wstring HTML_SCRIPT_END(L"</script>");
        static const std::wstring HTML_NOSCRIPT_END(L"</noscript>");
        static const std::wstring HTML_TITLE_END(L"</title>");
        static const std::wstring HTML_COMMENT_END(L"-->");
        //reset any state variables
        clear_log();
        m_is_in_preformatted_text_block_stack = preserve_spaces ? 1 : 0;

        //verify the inputs
        if (html_text == NULL || html_text[0] == 0 || text_length == 0)
            {
            set_filtered_text_length(0);
            return NULL;
            }
        NON_UNIT_TEST_ASSERT(text_length <= string_util::strlen(html_text) );

        if (!allocate_text_buffer(text_length))
            {
            set_filtered_text_length(0);
            return NULL;
            }

        //find the first <. If not found then just parse this as encoded HTML text
        const wchar_t* start = string_util::strchr(html_text, common_lang_constants::LESS_THAN);
        if (!start)
            {
            if (include_outer_text)
                { parse_raw_text(html_text, text_length); }
            }
        //if there is text outside of the starting < section then just decode it
        else if (start > html_text && include_outer_text)
            {
            parse_raw_text(html_text, std::min<size_t>((start-html_text), text_length));
            }
        const wchar_t* end = NULL;

        const wchar_t* const endSentinel = html_text+text_length;
        while (start && (start < endSentinel))
            {
            const size_t remainingTextLength = (endSentinel-start);
            const std::wstring currentElement = get_element_name(start+1, false);
            bool isSymbolFontSection = false;
            //if it's a comment then look for matching comment ending sequence
            if (remainingTextLength >= 4 && start[0] == common_lang_constants::LESS_THAN &&
                start[1] == L'!' && start[2] == L'-' && start[3] == L'-')
                {
                end = string_util::strstr(start, HTML_COMMENT_END.c_str());
                if (!end)
                    { break; }
                end += HTML_COMMENT_END.length();
                }
            //if it's a script (e.g., javascript) then skip it
            else if ((currentElement.length() == 6 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_S,common_lang_constants::UPPER_S) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_C,common_lang_constants::UPPER_C) &&
                is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_R,common_lang_constants::UPPER_R) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I) &&
                is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_P,common_lang_constants::UPPER_P) && is_either<wchar_t>(currentElement[5],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T)))
                {
                end = string_util::stristr<wchar_t>(start, HTML_SCRIPT_END.c_str());
                if (!end)
                    { break; }
                end += HTML_SCRIPT_END.length();
                }
            //if it's a noscript (i.e., alternative text for when scripting is not available) then skip it
            else if ((currentElement.length() == 8 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_N,common_lang_constants::UPPER_N) &&
                is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_O,common_lang_constants::UPPER_O) && is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_S,common_lang_constants::UPPER_S) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_C,common_lang_constants::UPPER_C) &&
                is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_R,common_lang_constants::UPPER_R) && is_either<wchar_t>(currentElement[5],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I) &&
                is_either<wchar_t>(currentElement[6],common_lang_constants::LOWER_P,common_lang_constants::UPPER_P) && is_either<wchar_t>(currentElement[7],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T)))
                {
                end = string_util::stristr<wchar_t>(start, HTML_NOSCRIPT_END.c_str());
                if (!end)
                    { break; }
                end += HTML_NOSCRIPT_END.length();
                }
            //if it's style command section then skip it
            else if ((currentElement.length() == 5 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_S,common_lang_constants::UPPER_S) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) &&
                is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_Y,common_lang_constants::UPPER_Y) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) &&
                is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E)))
                {
                end = string_util::stristr<wchar_t>(start, HTML_STYLE_END.c_str());
                if (!end)
                    { break; }
                end += HTML_STYLE_END.length();
                }
            //if it's a title then look for matching title ending sequence
            else if ((currentElement.length() == 5 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I) &&
                is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) &&
                is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E)))
                {
                end = string_util::stristr<wchar_t>(start, HTML_TITLE_END.c_str());
                if (!end)
                    { break; }
                end += HTML_TITLE_END.length();
                }
            //stray < (i.e., < wasn't encoded) should be treated as such, instead of a tag
            else if ((remainingTextLength >= 2 && start[0] == common_lang_constants::LESS_THAN && std::iswspace(start[1])) ||
                (remainingTextLength >= 7 && start[0] == common_lang_constants::LESS_THAN &&
                start[1] == common_lang_constants::AMPERSAND && is_either<wchar_t>(start[2], common_lang_constants::LOWER_N, common_lang_constants::UPPER_N) &&
                is_either<wchar_t>(start[3], common_lang_constants::LOWER_B, common_lang_constants::UPPER_B) &&
                is_either<wchar_t>(start[4], common_lang_constants::LOWER_S, common_lang_constants::UPPER_S) &&
                is_either<wchar_t>(start[5], common_lang_constants::LOWER_P, common_lang_constants::UPPER_P) &&
                start[6] == common_lang_constants::COLON))
                {
                end = string_util::strchr(start+1, common_lang_constants::LESS_THAN);
                if (!end)
                    {
                    parse_raw_text(start, endSentinel-start);
                    break;
                    }
                /*copy over the text from the unterminated < to the currently found
                < (that we will start from in the next loop*/
                parse_raw_text(start, end-start);
                //set the starting point to the next < that we already found
                start = end;
                continue;
                }
            //read in CDATA date blocks as they appear (no HTML conversation happens here)
            else if (currentElement.length() >= 8 && start[1] == L'!' &&
                start[2] == L'[' && is_either<wchar_t>(start[3], common_lang_constants::LOWER_C, common_lang_constants::UPPER_C) &&
                is_either<wchar_t>(start[4], common_lang_constants::LOWER_D, common_lang_constants::UPPER_D) &&
                is_either<wchar_t>(start[5], common_lang_constants::LOWER_A, common_lang_constants::UPPER_A) &&
                is_either<wchar_t>(start[6], common_lang_constants::LOWER_T, common_lang_constants::UPPER_T) &&
                is_either<wchar_t>(start[7], common_lang_constants::LOWER_A, common_lang_constants::UPPER_A) &&
                start[8] == L'[')
                {
                start += 9;
                end = string_util::strstr(start, L"]]>");
                if (!end || end > endSentinel)
                    {
                    ++m_is_in_preformatted_text_block_stack;//preserve newline formatting
                    parse_raw_text(start, endSentinel-start);
                    --m_is_in_preformatted_text_block_stack;
                    break;
                    }
                add_characters(start, end-start);
                start = end;
                end += 2;
                continue;
                }
            else
                {
                //Symbol font section (we will need to do some special formatting later). First, special logic for "font" element...
                if ((currentElement.length() == 4 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_F,common_lang_constants::UPPER_F) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_O,common_lang_constants::UPPER_O) && 
                    is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_N,common_lang_constants::UPPER_N) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T)))
                    {
                    if (string_util::strnicmp(read_tag(start+1, L"face", 4, false, true).first, L"Symbol", 6) == 0 ||
                        string_util::strnicmp(read_tag(start+1, L"font-family", 11, true, true).first, L"Symbol", 6) == 0)
                        { isSymbolFontSection = true; }
                    }
                //...then any other element
                else
                    {
                    if (string_util::strnicmp(read_tag(start+1, L"font-family", 11, true, true).first, L"Symbol", 6) == 0)
                        { isSymbolFontSection = true; }
                    }
                //See if this is a preformatted section, where CRLFs should be preserved
                if ((currentElement.length() == 3 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_P,common_lang_constants::UPPER_P) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_R,common_lang_constants::UPPER_R) && is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E)))
                    { ++m_is_in_preformatted_text_block_stack; }
                //see if this should be treated as a new paragraph because it is a break, paragraph, list item, or table row
                else if ((currentElement.length() == 1 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_P,common_lang_constants::UPPER_P)) ||
                    (currentElement.length() == 5 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_A,common_lang_constants::UPPER_A) && is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_B,common_lang_constants::UPPER_B) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E)) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_R,common_lang_constants::UPPER_R)) ||
                    (currentElement.length() == 3 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I) && is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_V,common_lang_constants::UPPER_V)) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_O,common_lang_constants::UPPER_O) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L)) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_U,common_lang_constants::UPPER_U) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L)) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && currentElement[1] == common_lang_constants::NUMBER_1) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && currentElement[1] == common_lang_constants::NUMBER_2) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && currentElement[1] == common_lang_constants::NUMBER_3) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && currentElement[1] == common_lang_constants::NUMBER_4) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && currentElement[1] == common_lang_constants::NUMBER_5) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && currentElement[1] == common_lang_constants::NUMBER_6) ||
                    (currentElement.length() == 6 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_S,common_lang_constants::UPPER_S) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E) && is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E) && is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_C,common_lang_constants::UPPER_C) && is_either<wchar_t>(currentElement[5],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T)) ||
                    (currentElement.length() == 6 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_O,common_lang_constants::UPPER_O) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_P,common_lang_constants::UPPER_P) && is_either<wchar_t>(currentElement[2],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(currentElement[3],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I) && is_either<wchar_t>(currentElement[4],common_lang_constants::LOWER_O,common_lang_constants::UPPER_O) && is_either<wchar_t>(currentElement[5],common_lang_constants::LOWER_N,common_lang_constants::UPPER_N)) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T)) ||
                    (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_R,common_lang_constants::UPPER_R)) )                    
                    {
                    add_character(L'\n');
                    add_character(L'\n');
                    }
                else if (compare_element(start+1, L"br", 2, true) )
                    { add_character(L'\n'); }
                //or end of a section that is like a paragraph
                else if (remainingTextLength >= 3 && start[0] == common_lang_constants::LESS_THAN && start[1] == common_lang_constants::FORWARD_SLASH)
                    {
                    if ((remainingTextLength >= 8 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(start[3],common_lang_constants::LOWER_A,common_lang_constants::UPPER_A) && is_either<wchar_t>(start[4],common_lang_constants::LOWER_B,common_lang_constants::UPPER_B) && is_either<wchar_t>(start[5],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && is_either<wchar_t>(start[6],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E) && start[7] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 4 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_P,common_lang_constants::UPPER_P) && start[3] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && start[3] == common_lang_constants::NUMBER_1 && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && start[3] == common_lang_constants::NUMBER_2 && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && start[3] == common_lang_constants::NUMBER_3 && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && start[3] == common_lang_constants::NUMBER_4 && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && start[3] == common_lang_constants::NUMBER_5 && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_H,common_lang_constants::UPPER_H) && start[3] == common_lang_constants::NUMBER_6 && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 6 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D) && is_either<wchar_t>(start[3],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I) && is_either<wchar_t>(start[4],common_lang_constants::LOWER_V,common_lang_constants::UPPER_V) && start[5] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D) && is_either<wchar_t>(start[3],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 9 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_S,common_lang_constants::UPPER_S) && is_either<wchar_t>(start[3],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E) && is_either<wchar_t>(start[4],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && is_either<wchar_t>(start[5],common_lang_constants::LOWER_E,common_lang_constants::UPPER_E) && is_either<wchar_t>(start[6],common_lang_constants::LOWER_C,common_lang_constants::UPPER_C) && is_either<wchar_t>(start[7],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && start[8] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_O,common_lang_constants::UPPER_O) && is_either<wchar_t>(start[3],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && start[4] == common_lang_constants::GREATER_THAN) ||
                        (remainingTextLength >= 5 && is_either<wchar_t>(start[2],common_lang_constants::LOWER_U,common_lang_constants::UPPER_U) && is_either<wchar_t>(start[3],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && start[4] == common_lang_constants::GREATER_THAN))                    
                        {
                        add_character(L'\n');
                        add_character(L'\n');
                        }
                    }
                else if ((currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_L,common_lang_constants::UPPER_L) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_I,common_lang_constants::UPPER_I)) ||
                        (currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D)) )
                    {
                    add_character(L'\n');
                    add_character(L'\t');
                    }
                //or tab over table cell
                else if ((currentElement.length() == 2 && is_either<wchar_t>(currentElement[0],common_lang_constants::LOWER_T,common_lang_constants::UPPER_T) && is_either<wchar_t>(currentElement[1],common_lang_constants::LOWER_D,common_lang_constants::UPPER_D)) )                    
                    { add_character(L'\t'); }
                end = find_close_tag(start+1, true);
                if (!end)
                    {
                    //no close tag? read to the next open tag then and read this section in below
                    if ((end = string_util::strchr(start+1, common_lang_constants::LESS_THAN)) == NULL)
                        { break; }
                    }
                /*if the < tag that we started from is not terminated then feed that in as
                text instead of treating it like a valid HTML tag.  Not common, but it happens.*/
                if (end[0] == common_lang_constants::LESS_THAN)
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
            if (!start || start >= endSentinel)
                { break; }
            const size_t previousLength = get_filtered_text_length();
            //copy over the text between the tags
            parse_raw_text(end, start-end);
            /*Old HTML used to use "Symbol" font to show various math/Greek symbols (instead of proper entities).
            So if the current block of text is using the font "Symbol", then we will convert
            it to the expected symbol.*/
            if (isSymbolFontSection)
                {
                const std::wstring copiedOverText = convert_symbol_font_section(std::wstring(get_filtered_text()+previousLength, (get_filtered_text_length()-previousLength)) );
                set_filtered_text_length(previousLength);
                add_characters(copiedOverText.c_str(), copiedOverText.length());
                if (copiedOverText.length())
                    { log_message(L"Symbol font used for the following: \""+copiedOverText+L"\""); }
                }
            //after parsing this section, see if this is the end of a preformatted area
            if (string_util::strnicmp<wchar_t>(start, L"</pre>", 6) == 0)                    
                {
                if (m_is_in_preformatted_text_block_stack > 0)
                    { --m_is_in_preformatted_text_block_stack; }
                }
            }

        //get any text lingering after the last >
        if (end && end < endSentinel && include_outer_text)
            {
            parse_raw_text(end, endSentinel-end);
            }

        return get_filtered_text();
        }
    bool html_extract_text::compare_element(const wchar_t* text, const wchar_t* element,
                                            const size_t element_size,
                                            const bool accept_self_terminating_elements /*= false*/)
        {
        if (!text || !element || element_size == 0)
            { return false; }
        assert((std::wcslen(element) == element_size) && "Invalid length passed to compare_element().");
        //first see if the element matches the text (e.g., "br" or "br/" [if accepting self terminating element])
        if (string_util::strnicmp(text, element, element_size) == 0)
            {
            /*now we need to make sure that there isn't more to the element in the text.
            In other words, verify that it is either terminated by a '>' or proceeded with
            attributes; otherwise, the element in the text is not the same as the element
            that we are comparing against.*/
            text += element_size;
            //if element is missing '>' and has nothing after it then it's invalid.
            if (*text == 0)
                { return false; }
            //if immediately closed then it's valid.
            else if (*text == common_lang_constants::GREATER_THAN)
                { return true; }
            //if we are allowing terminated elements ("/>") then it's a match if we
            //are on a '/'. Otherwise, if not a space after it then fail. If it is a space,
            //then we need to scan beyond that to make sure it isn't self terminated after the space.
            else if (accept_self_terminating_elements)
                {
                return (*text == common_lang_constants::FORWARD_SLASH ||
                        std::iswspace(*text));
                }
            //if we aren't allowing "/>" and we are on a space, then just make sure
            //it isn't self terminated.
            else if (std::iswspace(*text))
                {
                const wchar_t* closeTag = find_close_tag(text);
                if (!closeTag)
                    { return false; }
                --closeTag;
                while (closeTag > text &&
                    std::iswspace(*closeTag))
                    { --closeTag; }
                return (*closeTag != common_lang_constants::FORWARD_SLASH);
                }
            else
                { return false; }
            }
        else
            { return false; }
        }
    bool html_extract_text::compare_element_case_sensitive(const wchar_t* text, const wchar_t* element,
                                                           const size_t element_size,
                                                           const bool accept_self_terminating_elements /*= false*/)
        {
        if (!text || !element || element_size == 0)
            { return false; }
        assert((std::wcslen(element) == element_size) && "Invalid length passed to compare_element().");
        //first see if the element matches the text (e.g., "br" or "br/" [if accepting self terminating element])
        if (string_util::strncmp(text, element, element_size) == 0)
            {
            /*now we need to make sure that there isn't more to the element in the text.
            In other words, verify that it is either terminated by a '>' or proceeded with
            attributes; otherwise, the element in the text is not the same as the element
            that we are comparing against.*/
            text += element_size;
            //if element is missing '>' and has nothing after it then it's invalid.
            if (*text == 0)
                { return false; }
            //if immediately closed then it's valid.
            else if (*text == common_lang_constants::GREATER_THAN)
                { return true; }
            //if we are allowing terminated elements ("/>") then it's a match if we
            //are on a '/'. Otherwise, if not a space after it then fail. If it is a space,
            //then we need to scan beyond that to make sure it isn't self terminated after the space.
            else if (accept_self_terminating_elements)
                {
                return (*text == common_lang_constants::FORWARD_SLASH ||
                        std::iswspace(*text));
                }
            //if we aren't allowing "/>" and we are on a space, then just make sure
            //it isn't self terminated.
            else if (std::iswspace(*text))
                {
                const wchar_t* closeTag = find_close_tag(text);
                if (!closeTag)
                    { return false; }
                --closeTag;
                while (closeTag > text &&
                    std::iswspace(*closeTag))
                    { --closeTag; }
                return (*closeTag != common_lang_constants::FORWARD_SLASH);
                }
            else
                { return false; }
            }
        else
            { return false; }
        }
    std::wstring html_extract_text::get_element_name(const wchar_t* text,
                                                 const bool accept_self_terminating_elements /*= true*/)
        {
        if (text == NULL)
            { return std::wstring(L""); }
        const wchar_t* start = text;
        for (;;)
            {
            if (text[0] == 0 ||
                std::iswspace(text[0]) ||
                text[0] == common_lang_constants::GREATER_THAN)
                { break; }
            else if (accept_self_terminating_elements &&
                text[0] == common_lang_constants::FORWARD_SLASH &&
                text[1] == common_lang_constants::GREATER_THAN)
                { break; }
            ++text;
            }
        return std::wstring(start, text-start);
        }
    const wchar_t* html_extract_text::find_close_tag(const wchar_t* text, const bool fail_on_overlapping_open_symbol /*= false*/)
        {
        if (text == NULL)
            { return NULL; }
        //if we are at the beginning of an open statement, skip the opening < so that we can correctly
        //look for the next opening <
        else if (text[0] == common_lang_constants::LESS_THAN)
            { ++text; }
        return string_util::find_matching_close_tag(text, common_lang_constants::LESS_THAN, common_lang_constants::GREATER_THAN, fail_on_overlapping_open_symbol);
        }
    const wchar_t* html_extract_text::find_element(const wchar_t* sectionStart,
                                           const wchar_t* sectionEnd,
                                           const wchar_t* elementTag,
                                           const size_t elementTagLength,
                                           const bool accept_self_terminating_elements /*= true*/)
        {
        if (sectionStart == NULL || sectionEnd == NULL || elementTag == NULL || elementTagLength == 0)
            { return NULL; }
        assert((std::wcslen(elementTag) == elementTagLength) && "Invalid length passed to find_element().");
        while (sectionStart && sectionStart+elementTagLength < sectionEnd)
            {
            sectionStart = string_util::strchr(sectionStart, common_lang_constants::LESS_THAN);
            if (sectionStart == NULL || sectionStart+elementTagLength > sectionEnd)
                { return NULL; }
            else if (compare_element(sectionStart+1, elementTag, elementTagLength, accept_self_terminating_elements))
                { return sectionStart; }
            else
                { sectionStart += 1/*skip the '<' and search for the next one*/; }
            }
        return NULL;
        }
    const wchar_t* html_extract_text::find_closing_element(const wchar_t* sectionStart,
                                           const wchar_t* sectionEnd,
                                           const wchar_t* elementTag,
                                           const size_t elementTagLength)
        {
        if (sectionStart == NULL || sectionEnd == NULL || elementTag == NULL || elementTagLength == 0)
            { return NULL; }
        assert((std::wcslen(elementTag) == elementTagLength) && "Invalid length passed to find_closing_element().");
        const wchar_t* start = string_util::strchr(sectionStart, common_lang_constants::LESS_THAN);
        if (start == NULL || start+elementTagLength > sectionEnd)
            { return NULL; }
        ++start;//skip '<'
        //if we are on an opening element by the same name, then skip it so that we won't
        //count it again in the stack logic below
        if (compare_element(start, elementTag, elementTagLength, true))
            { sectionStart = start+elementTagLength; }
        //else if we are on the closing element already then just return that.
        else if (start[0] == '/' && compare_element(start+1, elementTag, elementTagLength, true))
            { return --start; }

        //Do a search for the matching close tag. That means
        //that it will skip any inner elements that are the same element and go
        //go the correct closing one.
        long stackSize = 1;

        start = string_util::strchr(sectionStart, common_lang_constants::LESS_THAN);
        while (start && start+elementTagLength+1 < sectionEnd)
            {
            //if a closing element if found, then decrease the stack
            if (start[1] == L'/' && compare_element(start+2, elementTag, elementTagLength, true))
                { --stackSize; }
            //if a new opening element by the same name, then add that to the stack so that its
            //respective closing element will be skipped.
            else if (compare_element(start+1, elementTag, elementTagLength, true))
                { ++stackSize; }
            if (stackSize == 0)
                { return start; }
            start = string_util::strchr(start+1, common_lang_constants::LESS_THAN);
            }
        return NULL;
        }

    const html_utilities::symbol_font_table html_extract_text::SYMBOL_FONT_TABLE;
    const html_utilities::html_entity_table html_extract_text::HTML_TABLE_LOOKUP;
    }

namespace html_utilities
    {
    const wchar_t* html_strip_hyperlinks::operator()(const wchar_t* html_text,
                                  const size_t text_length)
        {
        if (html_text == NULL || html_text[0] == 0 || text_length == 0)
            { return NULL; }
        NON_UNIT_TEST_ASSERT(text_length <= string_util::strlen(html_text) );

        if (!allocate_text_buffer(text_length))
            { return NULL; }

        const wchar_t* const endSentinel = html_text+text_length;
        const wchar_t* currentPos = html_text, *lastEnd = html_text;
        while (currentPos && (currentPos < endSentinel))
            {
            currentPos = lily_of_the_valley::html_extract_text::find_element(currentPos, endSentinel, L"a", 1, true);
            //no more anchors, so just copy over the rest of the text and quit.
            if (!currentPos || currentPos >= endSentinel)
                {
                add_characters(lastEnd, endSentinel-lastEnd);
                break;
                }
            //if this is actually a bookmark, then we need to start over (looking for the next <a>).
            if (lily_of_the_valley::html_extract_text::find_tag(currentPos, L"name", 4, false))
                {
                currentPos += 2;
                continue;
                }
            //next <a> found, so copy over all of the text before it, then move over to the end of this element.
            add_characters(lastEnd, currentPos-lastEnd);
            currentPos = lily_of_the_valley::html_extract_text::find_close_tag(currentPos);
            if (!currentPos || currentPos >= endSentinel)
                { break; }
            lastEnd = currentPos+1;
            //now, find the matching </a> and copy over the text between that and the previous <a>.
            //Note that nested <a> would be very incorrect HTML, so we will safely assume that there aren't any.
            currentPos = lily_of_the_valley::html_extract_text::find_closing_element(currentPos, endSentinel, L"a", 1);
            if (!currentPos || currentPos >= endSentinel)
                { break; }
            add_characters(lastEnd, currentPos-lastEnd);
            //finally, find the close of this </a>, move to that, and start over again looking for the next <a>
            currentPos = lily_of_the_valley::html_extract_text::find_close_tag(currentPos);
            if (!currentPos || currentPos >= endSentinel)
                { break; }
            lastEnd = currentPos+1;
            }
        return get_filtered_text();
        }

    symbol_font_table::symbol_font_table()
        {
        //Greek alphabet
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_A, 913) );//uppercase Alpha
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_B, 914) );//uppercase Beta
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_G, 915) );//uppercase Gamma
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_D, 916) );//uppercase Delta
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_E, 917) );//uppercase Epsilon
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_Z, 918) );//uppercase Zeta
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_H, 919) );//uppercase Eta
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_Q, 920) );//uppercase Theta
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_I, 921) );//uppercase Iota
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_K, 922) );//uppercase Kappa
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_L, 923) );//uppercase Lambda
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_M, 924) );//uppercase Mu
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_N, 925) );//uppercase Nu
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_X, 926) );//uppercase Xi
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_O, 927) );//uppercase Omicron
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_P, 928) );//uppercase Pi
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_R, 929) );//uppercase Rho
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_S, 931) );//uppercase Sigma
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_T, 932) );//uppercase Tau
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_U, 933) );//uppercase Upsilon
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_F, 934) );//uppercase Phi
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_C, 935) );//uppercase Chi
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_Y, 936) );//uppercase Psi
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_W, 937) );//uppercase Omega
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_A, 945) );//lowercase alpha
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_B, 946) );//lowercase beta
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_G, 947) );//lowercase gamma
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_D, 948) );//lowercase delta
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_E, 949) );//lowercase epsilon
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_Z, 950) );//lowercase zeta
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_H, 951) );//lowercase eta
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_Q, 952) );//lowercase theta
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_I, 953) );//lowercase iota
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_K, 954) );//lowercase kappa
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_L, 955) );//lowercase lambda
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_M, 956) );//lowercase mu
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_N, 957) );//lowercase nu
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_X, 958) );//lowercase xi
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_O, 959) );//lowercase omicron
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_P, 960) );//lowercase pi
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_R, 961) );//lowercase rho
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_V, 962) );//uppercase sigmaf
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_S, 963) );//lowercase sigma
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_T, 964) );//lowercase tau
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_U, 965) );//lowercase upsilon
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_F, 966) );//lowercase phi
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_C, 967) );//lowercase chi
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_Y, 968) );//lowercase psi
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_W, 969) );//lowercase omega
        m_symbol_table.insert(std::make_pair(common_lang_constants::UPPER_J, 977) );//uppercase thetasym
        m_symbol_table.insert(std::make_pair(161, 978) );//lowercase Upsilon1
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_J, 981) );//lowercase phi1
        m_symbol_table.insert(std::make_pair(common_lang_constants::LOWER_V, 982) );//lowercase omega1
        //arrows
        m_symbol_table.insert(std::make_pair(171, 8596) );
        m_symbol_table.insert(std::make_pair(172, 8592) );
        m_symbol_table.insert(std::make_pair(173, 8593) );
        m_symbol_table.insert(std::make_pair(174, 8594) );
        m_symbol_table.insert(std::make_pair(175, 8595) );
        m_symbol_table.insert(std::make_pair(191, 8629) );
        m_symbol_table.insert(std::make_pair(219, 8660) );
        m_symbol_table.insert(std::make_pair(220, 8656) );
        m_symbol_table.insert(std::make_pair(221, 8657) );
        m_symbol_table.insert(std::make_pair(222, 8658) );
        m_symbol_table.insert(std::make_pair(223, 8659) );
        //math
        m_symbol_table.insert(std::make_pair(34, 8704) );
        m_symbol_table.insert(std::make_pair(36, 8707) );
        m_symbol_table.insert(std::make_pair(39, 8717) );
        m_symbol_table.insert(std::make_pair(42, 8727) );
        m_symbol_table.insert(std::make_pair(45, 8722) );
        m_symbol_table.insert(std::make_pair(64, 8773) );
        m_symbol_table.insert(std::make_pair(92, 8756) );
        m_symbol_table.insert(std::make_pair(94, 8869) );
        m_symbol_table.insert(std::make_pair(126, 8764) );
        m_symbol_table.insert(std::make_pair(163, 8804) );
        m_symbol_table.insert(std::make_pair(165, 8734) );
        m_symbol_table.insert(std::make_pair(179, 8805) );
        m_symbol_table.insert(std::make_pair(181, 8733) );
        m_symbol_table.insert(std::make_pair(182, 8706) );
        m_symbol_table.insert(std::make_pair(183, 8729) );
        m_symbol_table.insert(std::make_pair(185, 8800) );
        m_symbol_table.insert(std::make_pair(186, 8801) );
        m_symbol_table.insert(std::make_pair(187, 8776) );
        m_symbol_table.insert(std::make_pair(196, 8855) );
        m_symbol_table.insert(std::make_pair(197, 8853) );
        m_symbol_table.insert(std::make_pair(198, 8709) );
        m_symbol_table.insert(std::make_pair(199, 8745) );
        m_symbol_table.insert(std::make_pair(200, 8746) );
        m_symbol_table.insert(std::make_pair(201, 8835) );
        m_symbol_table.insert(std::make_pair(202, 8839) );
        m_symbol_table.insert(std::make_pair(203, 8836) );
        m_symbol_table.insert(std::make_pair(204, 8834) );
        m_symbol_table.insert(std::make_pair(205, 8838) );
        m_symbol_table.insert(std::make_pair(206, 8712) );
        m_symbol_table.insert(std::make_pair(207, 8713) );
        m_symbol_table.insert(std::make_pair(208, 8736) );
        m_symbol_table.insert(std::make_pair(209, 8711) );
        m_symbol_table.insert(std::make_pair(213, 8719) );
        m_symbol_table.insert(std::make_pair(214, 8730) );
        m_symbol_table.insert(std::make_pair(215, 8901) );
        m_symbol_table.insert(std::make_pair(217, 8743) );
        m_symbol_table.insert(std::make_pair(218, 8744) );
        m_symbol_table.insert(std::make_pair(229, 8721) );
        m_symbol_table.insert(std::make_pair(242, 8747) );
        m_symbol_table.insert(std::make_pair(224, 9674) );
        m_symbol_table.insert(std::make_pair(189, 9168) );
        m_symbol_table.insert(std::make_pair(190, 9135) );
        m_symbol_table.insert(std::make_pair(225, 9001) );
        m_symbol_table.insert(std::make_pair(230, 9115) );
        m_symbol_table.insert(std::make_pair(231, 9116) );
        m_symbol_table.insert(std::make_pair(232, 9117) );
        m_symbol_table.insert(std::make_pair(233, 9121) );
        m_symbol_table.insert(std::make_pair(234, 9122) );
        m_symbol_table.insert(std::make_pair(235, 9123) );
        m_symbol_table.insert(std::make_pair(236, 9127) );
        m_symbol_table.insert(std::make_pair(237, 9128) );
        m_symbol_table.insert(std::make_pair(238, 9129) );
        m_symbol_table.insert(std::make_pair(239, 9130) );
        m_symbol_table.insert(std::make_pair(241, 9002) );
        m_symbol_table.insert(std::make_pair(243, 8992) );
        m_symbol_table.insert(std::make_pair(244, 9134) );
        m_symbol_table.insert(std::make_pair(245, 8993) );
        m_symbol_table.insert(std::make_pair(246, 9118) );
        m_symbol_table.insert(std::make_pair(247, 9119) );
        m_symbol_table.insert(std::make_pair(248, 9120) );
        m_symbol_table.insert(std::make_pair(249, 9124) );
        m_symbol_table.insert(std::make_pair(250, 9125) );
        m_symbol_table.insert(std::make_pair(251, 9126) );
        m_symbol_table.insert(std::make_pair(252, 9131) );
        m_symbol_table.insert(std::make_pair(253, 9132) );
        m_symbol_table.insert(std::make_pair(254, 9133) );
        m_symbol_table.insert(std::make_pair(180, 215) );
        m_symbol_table.insert(std::make_pair(184, 247) );
        m_symbol_table.insert(std::make_pair(216, 172) );
        }
    wchar_t symbol_font_table::find(const wchar_t letter) const
        {
        std::map<wchar_t, wchar_t>::const_iterator pos = m_symbol_table.find(letter);
        if (pos == m_symbol_table.end() )
            { return letter; }
        return pos->second;
        }

    html_entity_table::html_entity_table()
        {
        m_table.insert(std::make_pair(std::wstring(L"apos"), common_lang_constants::APOSTROPHE) );//not standard, but common
        m_table.insert(std::make_pair(std::wstring(L"gt"), common_lang_constants::GREATER_THAN) );
        m_table.insert(std::make_pair(std::wstring(L"lt"), common_lang_constants::LESS_THAN) );
        m_table.insert(std::make_pair(std::wstring(L"amp"), common_lang_constants::AMPERSAND) );
        m_table.insert(std::make_pair(std::wstring(L"quot"), common_lang_constants::DOUBLE_QUOTE) );
        m_table.insert(std::make_pair(std::wstring(L"nbsp"), common_lang_constants::SPACE) );
        m_table.insert(std::make_pair(std::wstring(L"iexcl"), 161) );
        m_table.insert(std::make_pair(std::wstring(L"cent"), 162) );
        m_table.insert(std::make_pair(std::wstring(L"pound"), 163) );
        m_table.insert(std::make_pair(std::wstring(L"curren"), 164) );
        m_table.insert(std::make_pair(std::wstring(L"yen"), 165) );
        m_table.insert(std::make_pair(std::wstring(L"brvbar"), 166) );
        m_table.insert(std::make_pair(std::wstring(L"sect"), 167) );
        m_table.insert(std::make_pair(std::wstring(L"uml"), 168) );
        m_table.insert(std::make_pair(std::wstring(L"copy"), 169) );
        m_table.insert(std::make_pair(std::wstring(L"ordf"), 170) );
        m_table.insert(std::make_pair(std::wstring(L"laquo"), 171) );
        m_table.insert(std::make_pair(std::wstring(L"not"), 172) );
        m_table.insert(std::make_pair(std::wstring(L"shy"), 173) );
        m_table.insert(std::make_pair(std::wstring(L"reg"), 174) );
        m_table.insert(std::make_pair(std::wstring(L"macr"), 175) );
        m_table.insert(std::make_pair(std::wstring(L"deg"), 176) );
        m_table.insert(std::make_pair(std::wstring(L"plusmn"), 177) );
        m_table.insert(std::make_pair(std::wstring(L"sup2"), 178) );
        m_table.insert(std::make_pair(std::wstring(L"sup3"), 179) );
        m_table.insert(std::make_pair(std::wstring(L"acute"), 180) );
        m_table.insert(std::make_pair(std::wstring(L"micro"), 181) );
        m_table.insert(std::make_pair(std::wstring(L"para"), 182) );
        m_table.insert(std::make_pair(std::wstring(L"middot"), 183) );
        m_table.insert(std::make_pair(std::wstring(L"mcedilicro"), 184) );
        m_table.insert(std::make_pair(std::wstring(L"sup1"), 185) );
        m_table.insert(std::make_pair(std::wstring(L"ordm"), 186) );
        m_table.insert(std::make_pair(std::wstring(L"raquo"), 187) );
        m_table.insert(std::make_pair(std::wstring(L"frac14"), 188) );
        m_table.insert(std::make_pair(std::wstring(L"texfrac12t"), 189) );
        m_table.insert(std::make_pair(std::wstring(L"frac34"), 190) );
        m_table.insert(std::make_pair(std::wstring(L"iquest"), 191) );
        m_table.insert(std::make_pair(std::wstring(L"Agrave"), 192) );
        m_table.insert(std::make_pair(std::wstring(L"Aacute"), 193) );
        m_table.insert(std::make_pair(std::wstring(L"Acirc"), 194) );
        m_table.insert(std::make_pair(std::wstring(L"Atilde"), 195) );
        m_table.insert(std::make_pair(std::wstring(L"Auml"), 196) );
        m_table.insert(std::make_pair(std::wstring(L"Aring"), 197) );
        m_table.insert(std::make_pair(std::wstring(L"AElig"), 198) );
        m_table.insert(std::make_pair(std::wstring(L"Ccedil"), 199) );
        m_table.insert(std::make_pair(std::wstring(L"Egrave"), 200) );
        m_table.insert(std::make_pair(std::wstring(L"Eacute"), 201) );
        m_table.insert(std::make_pair(std::wstring(L"Ecirc"), 202) );
        m_table.insert(std::make_pair(std::wstring(L"Euml"), 203) );
        m_table.insert(std::make_pair(std::wstring(L"Igrave"), 204) );
        m_table.insert(std::make_pair(std::wstring(L"Iacute"), 205) );
        m_table.insert(std::make_pair(std::wstring(L"Icirc"), 206) );
        m_table.insert(std::make_pair(std::wstring(L"Iuml"), 207) );
        m_table.insert(std::make_pair(std::wstring(L"ETH"), 208) );
        m_table.insert(std::make_pair(std::wstring(L"Ntilde"), 209) );
        m_table.insert(std::make_pair(std::wstring(L"Ograve"), 210) );
        m_table.insert(std::make_pair(std::wstring(L"Oacute"), 211) );
        m_table.insert(std::make_pair(std::wstring(L"Ocirc"), 212) );
        m_table.insert(std::make_pair(std::wstring(L"Otilde"), 213) );
        m_table.insert(std::make_pair(std::wstring(L"Ouml"), 214) );
        m_table.insert(std::make_pair(std::wstring(L"Oslash"), 216) );
        m_table.insert(std::make_pair(std::wstring(L"times"), 215) );
        m_table.insert(std::make_pair(std::wstring(L"Ugrave"), 217) );
        m_table.insert(std::make_pair(std::wstring(L"Uacute"), 218) );
        m_table.insert(std::make_pair(std::wstring(L"Ucirc"), 219) );
        m_table.insert(std::make_pair(std::wstring(L"Uuml"), 220) );
        m_table.insert(std::make_pair(std::wstring(L"Yacute"), 221) );
        m_table.insert(std::make_pair(std::wstring(L"THORN"), 222) );
        m_table.insert(std::make_pair(std::wstring(L"szlig"), 223) );
        m_table.insert(std::make_pair(std::wstring(L"agrave"), 224) );
        m_table.insert(std::make_pair(std::wstring(L"aacute"), 225) );
        m_table.insert(std::make_pair(std::wstring(L"acirc"), 226) );
        m_table.insert(std::make_pair(std::wstring(L"atilde"), 227) );
        m_table.insert(std::make_pair(std::wstring(L"auml"), 228) );
        m_table.insert(std::make_pair(std::wstring(L"aring"), 229) );
        m_table.insert(std::make_pair(std::wstring(L"aelig"), 230) );
        m_table.insert(std::make_pair(std::wstring(L"ccedil"), 231) );
        m_table.insert(std::make_pair(std::wstring(L"egrave"), 232) );
        m_table.insert(std::make_pair(std::wstring(L"eacute"), 233) );
        m_table.insert(std::make_pair(std::wstring(L"ecirc"), 234) );
        m_table.insert(std::make_pair(std::wstring(L"euml"), 235) );
        m_table.insert(std::make_pair(std::wstring(L"igrave"), 236) );
        m_table.insert(std::make_pair(std::wstring(L"iacute"), 237) );
        m_table.insert(std::make_pair(std::wstring(L"icirc"), 238) );
        m_table.insert(std::make_pair(std::wstring(L"iuml"), 239) );
        m_table.insert(std::make_pair(std::wstring(L"eth"), 240) );
        m_table.insert(std::make_pair(std::wstring(L"ntilde"), 241) );
        m_table.insert(std::make_pair(std::wstring(L"ograve"), 242) );
        m_table.insert(std::make_pair(std::wstring(L"oacute"), 243) );
        m_table.insert(std::make_pair(std::wstring(L"ocirc"), 244) );
        m_table.insert(std::make_pair(std::wstring(L"otilde"), 245) );
        m_table.insert(std::make_pair(std::wstring(L"ouml"), 246) );
        m_table.insert(std::make_pair(std::wstring(L"divide"), 247) );
        m_table.insert(std::make_pair(std::wstring(L"oslash"), 248) );
        m_table.insert(std::make_pair(std::wstring(L"ugrave"), 249) );
        m_table.insert(std::make_pair(std::wstring(L"uacute"), 250) );
        m_table.insert(std::make_pair(std::wstring(L"ucirc"), 251) );
        m_table.insert(std::make_pair(std::wstring(L"uuml"), 252) );
        m_table.insert(std::make_pair(std::wstring(L"yacute"), 253) );
        m_table.insert(std::make_pair(std::wstring(L"thorn"), 254) );
        m_table.insert(std::make_pair(std::wstring(L"yuml"), 255) );
        m_table.insert(std::make_pair(std::wstring(L"fnof"), 402) );
        m_table.insert(std::make_pair(std::wstring(L"Alpha"), 913) );
        m_table.insert(std::make_pair(std::wstring(L"Beta"), 914) );
        m_table.insert(std::make_pair(std::wstring(L"Gamma"), 915) );
        m_table.insert(std::make_pair(std::wstring(L"Delta"), 916) );
        m_table.insert(std::make_pair(std::wstring(L"Epsilon"), 917) );
        m_table.insert(std::make_pair(std::wstring(L"Zeta"), 918) );
        m_table.insert(std::make_pair(std::wstring(L"Eta"), 919) );
        m_table.insert(std::make_pair(std::wstring(L"Theta"), 920) );
        m_table.insert(std::make_pair(std::wstring(L"Iota"), 921) );
        m_table.insert(std::make_pair(std::wstring(L"Kappa"), 922) );
        m_table.insert(std::make_pair(std::wstring(L"Lambda"), 923) );
        m_table.insert(std::make_pair(std::wstring(L"Mu"), 924) );
        m_table.insert(std::make_pair(std::wstring(L"Nu"), 925) );
        m_table.insert(std::make_pair(std::wstring(L"Xi"), 926) );
        m_table.insert(std::make_pair(std::wstring(L"Omicron"), 927) );
        m_table.insert(std::make_pair(std::wstring(L"Pi"), 928) );
        m_table.insert(std::make_pair(std::wstring(L"Rho"), 929) );
        m_table.insert(std::make_pair(std::wstring(L"Sigma"), 931) );
        m_table.insert(std::make_pair(std::wstring(L"Tau"), 932) );
        m_table.insert(std::make_pair(std::wstring(L"Upsilon"), 933) );
        m_table.insert(std::make_pair(std::wstring(L"Phi"), 934) );
        m_table.insert(std::make_pair(std::wstring(L"Chi"), 935) );
        m_table.insert(std::make_pair(std::wstring(L"Psi"), 936) );
        m_table.insert(std::make_pair(std::wstring(L"Omega"), 937) );
        m_table.insert(std::make_pair(std::wstring(L"alpha"), 945) );
        m_table.insert(std::make_pair(std::wstring(L"beta"), 946) );
        m_table.insert(std::make_pair(std::wstring(L"gamma"), 947) );
        m_table.insert(std::make_pair(std::wstring(L"delta"), 948) );
        m_table.insert(std::make_pair(std::wstring(L"epsilon"), 949) );
        m_table.insert(std::make_pair(std::wstring(L"zeta"), 950) );
        m_table.insert(std::make_pair(std::wstring(L"eta"), 951) );
        m_table.insert(std::make_pair(std::wstring(L"theta"), 952) );
        m_table.insert(std::make_pair(std::wstring(L"iota"), 953) );
        m_table.insert(std::make_pair(std::wstring(L"kappa"), 954) );
        m_table.insert(std::make_pair(std::wstring(L"lambda"), 955) );
        m_table.insert(std::make_pair(std::wstring(L"mu"), 956) );
        m_table.insert(std::make_pair(std::wstring(L"nu"), 957) );
        m_table.insert(std::make_pair(std::wstring(L"xi"), 958) );
        m_table.insert(std::make_pair(std::wstring(L"omicron"), 959) );
        m_table.insert(std::make_pair(std::wstring(L"pi"), 960) );
        m_table.insert(std::make_pair(std::wstring(L"rho"), 961) );
        m_table.insert(std::make_pair(std::wstring(L"sigmaf"), 962) );
        m_table.insert(std::make_pair(std::wstring(L"sigma"), 963) );
        m_table.insert(std::make_pair(std::wstring(L"tau"), 964) );
        m_table.insert(std::make_pair(std::wstring(L"upsilon"), 965) );
        m_table.insert(std::make_pair(std::wstring(L"phi"), 966) );
        m_table.insert(std::make_pair(std::wstring(L"chi"), 967) );
        m_table.insert(std::make_pair(std::wstring(L"psi"), 968) );
        m_table.insert(std::make_pair(std::wstring(L"omega"), 969) );
        m_table.insert(std::make_pair(std::wstring(L"thetasym"), 977) );
        m_table.insert(std::make_pair(std::wstring(L"upsih"), 978) );
        m_table.insert(std::make_pair(std::wstring(L"piv"), 982) );
        m_table.insert(std::make_pair(std::wstring(L"bull"), 8226) );
        m_table.insert(std::make_pair(std::wstring(L"hellip"), 8230) );
        m_table.insert(std::make_pair(std::wstring(L"prime"), 8242) );
        m_table.insert(std::make_pair(std::wstring(L"Prime"), 8243) );
        m_table.insert(std::make_pair(std::wstring(L"oline"), 8254) );
        m_table.insert(std::make_pair(std::wstring(L"frasl"), 8260) );
        m_table.insert(std::make_pair(std::wstring(L"weierp"), 8472) );
        m_table.insert(std::make_pair(std::wstring(L"image"), 8465) );
        m_table.insert(std::make_pair(std::wstring(L"real"), 8476) );
        m_table.insert(std::make_pair(std::wstring(L"trade"), 8482) );
        m_table.insert(std::make_pair(std::wstring(L"alefsym"), 8501) );
        m_table.insert(std::make_pair(std::wstring(L"larr"), 8592) );
        m_table.insert(std::make_pair(std::wstring(L"uarr"), 8593) );
        m_table.insert(std::make_pair(std::wstring(L"rarr"), 8594) );
        m_table.insert(std::make_pair(std::wstring(L"darr"), 8595) );
        m_table.insert(std::make_pair(std::wstring(L"harr"), 8596) );
        m_table.insert(std::make_pair(std::wstring(L"crarr"), 8629) );
        m_table.insert(std::make_pair(std::wstring(L"lArr"), 8656) );
        m_table.insert(std::make_pair(std::wstring(L"uArr"), 8657) );
        m_table.insert(std::make_pair(std::wstring(L"rArr"), 8658) );
        m_table.insert(std::make_pair(std::wstring(L"dArr"), 8659) );
        m_table.insert(std::make_pair(std::wstring(L"hArr"), 8660) );
        m_table.insert(std::make_pair(std::wstring(L"forall"), 8704) );
        m_table.insert(std::make_pair(std::wstring(L"part"), 8706) );
        m_table.insert(std::make_pair(std::wstring(L"exist"), 8707) );
        m_table.insert(std::make_pair(std::wstring(L"empty"), 8709) );
        m_table.insert(std::make_pair(std::wstring(L"nabla"), 8711) );
        m_table.insert(std::make_pair(std::wstring(L"isin"), 8712) );
        m_table.insert(std::make_pair(std::wstring(L"notin"), 8713) );
        m_table.insert(std::make_pair(std::wstring(L"ni"), 8715) );
        m_table.insert(std::make_pair(std::wstring(L"prod"), 8719) );
        m_table.insert(std::make_pair(std::wstring(L"sum"), 8721) );
        m_table.insert(std::make_pair(std::wstring(L"minus"), 8722) );
        m_table.insert(std::make_pair(std::wstring(L"lowast"), 8727) );
        m_table.insert(std::make_pair(std::wstring(L"radic"), 8730) );
        m_table.insert(std::make_pair(std::wstring(L"prop"), 8733) );
        m_table.insert(std::make_pair(std::wstring(L"infin"), 8734) );
        m_table.insert(std::make_pair(std::wstring(L"ang"), 8736) );
        m_table.insert(std::make_pair(std::wstring(L"and"), 8743) );
        m_table.insert(std::make_pair(std::wstring(L"or"), 8744) );
        m_table.insert(std::make_pair(std::wstring(L"cap"), 8745) );
        m_table.insert(std::make_pair(std::wstring(L"cup"), 8746) );
        m_table.insert(std::make_pair(std::wstring(L"int"), 8747) );
        m_table.insert(std::make_pair(std::wstring(L"there4"), 8756) );
        m_table.insert(std::make_pair(std::wstring(L"sim"), 8764) );
        m_table.insert(std::make_pair(std::wstring(L"cong"), 8773) );
        m_table.insert(std::make_pair(std::wstring(L"asymp"), 8776) );
        m_table.insert(std::make_pair(std::wstring(L"ne"), 8800) );
        m_table.insert(std::make_pair(std::wstring(L"equiv"), 8801) );
        m_table.insert(std::make_pair(std::wstring(L"le"), 8804) );
        m_table.insert(std::make_pair(std::wstring(L"ge"), 8805) );
        m_table.insert(std::make_pair(std::wstring(L"sub"), 8834) );
        m_table.insert(std::make_pair(std::wstring(L"sup"), 8835) );
        m_table.insert(std::make_pair(std::wstring(L"nsub"), 8836) );
        m_table.insert(std::make_pair(std::wstring(L"sube"), 8838) );
        m_table.insert(std::make_pair(std::wstring(L"supe"), 8839) );
        m_table.insert(std::make_pair(std::wstring(L"oplus"), 8853) );
        m_table.insert(std::make_pair(std::wstring(L"otimes"), 8855) );
        m_table.insert(std::make_pair(std::wstring(L"perp"), 8869) );
        m_table.insert(std::make_pair(std::wstring(L"sdot"), 8901) );
        m_table.insert(std::make_pair(std::wstring(L"lceil"), 8968) );
        m_table.insert(std::make_pair(std::wstring(L"rceil"), 8969) );
        m_table.insert(std::make_pair(std::wstring(L"lfloor"), 8970) );
        m_table.insert(std::make_pair(std::wstring(L"rfloor"), 8971) );
        m_table.insert(std::make_pair(std::wstring(L"lang"), 9001) );
        m_table.insert(std::make_pair(std::wstring(L"rang"), 9002) );
        m_table.insert(std::make_pair(std::wstring(L"loz"), 9674) );
        m_table.insert(std::make_pair(std::wstring(L"spades"), 9824) );
        m_table.insert(std::make_pair(std::wstring(L"clubs"), 9827) );
        m_table.insert(std::make_pair(std::wstring(L"hearts"), 9829) );
        m_table.insert(std::make_pair(std::wstring(L"diams"), 9830) );
        m_table.insert(std::make_pair(std::wstring(L"OElig"), 338) );
        m_table.insert(std::make_pair(std::wstring(L"oelig"), 339) );
        m_table.insert(std::make_pair(std::wstring(L"Scaron"), 352) );
        m_table.insert(std::make_pair(std::wstring(L"scaron"), 353) );
        m_table.insert(std::make_pair(std::wstring(L"Yuml"), 376) );
        m_table.insert(std::make_pair(std::wstring(L"circ"), 710) );
        m_table.insert(std::make_pair(std::wstring(L"tilde"), 732) );
        m_table.insert(std::make_pair(std::wstring(L"ensp"), 8194) );
        m_table.insert(std::make_pair(std::wstring(L"emsp"), 8195) );
        m_table.insert(std::make_pair(std::wstring(L"thinsp"), 8201) );
        m_table.insert(std::make_pair(std::wstring(L"zwnj"), 8204) );
        m_table.insert(std::make_pair(std::wstring(L"zwj"), 8205) );
        m_table.insert(std::make_pair(std::wstring(L"lrm"), 8206) );
        m_table.insert(std::make_pair(std::wstring(L"rlm"), 8207) );
        m_table.insert(std::make_pair(std::wstring(L"ndash"), 8211) );
        m_table.insert(std::make_pair(std::wstring(L"mdash"), 8212) );
        m_table.insert(std::make_pair(std::wstring(L"lsquo"), 8216) );
        m_table.insert(std::make_pair(std::wstring(L"rsquo"), 8217) );
        m_table.insert(std::make_pair(std::wstring(L"sbquo"), 8218) );
        m_table.insert(std::make_pair(std::wstring(L"ldquo"), 8220) );
        m_table.insert(std::make_pair(std::wstring(L"rdquo"), 8221) );
        m_table.insert(std::make_pair(std::wstring(L"bdquo"), 8222) );
        m_table.insert(std::make_pair(std::wstring(L"dagger"), 8224) );
        m_table.insert(std::make_pair(std::wstring(L"Dagger"), 8225) );
        m_table.insert(std::make_pair(std::wstring(L"permil"), 8240) );
        m_table.insert(std::make_pair(std::wstring(L"lsaquo"), 8249) );
        m_table.insert(std::make_pair(std::wstring(L"rsaquo"), 8250) );
        m_table.insert(std::make_pair(std::wstring(L"euro"), 8364) );
        }
    wchar_t html_entity_table::find(const wchar_t* html_entity, const size_t length) const
        {
        std::wstring cmpKey(html_entity, length);
        std::map<std::wstring, wchar_t>::const_iterator pos =
            m_table.find(cmpKey);
        //if not found case sensitively...
        if (pos == m_table.end() )
            {
            //...do a case insensitive search.
            std::transform(cmpKey.begin(), cmpKey.end(), cmpKey.begin(), string_util::tolower_western);
            pos = m_table.find(cmpKey);
            //if the character can't be converted, then return a question mark
            if (pos == m_table.end() )
                { return common_lang_constants::QUESTION_MARK; }
            }
        return pos->second;
        }

    const wchar_t* javascript_hyperlink_parse::operator()()
        {
        //if the end is NULL (should not happen) or if the current position is NULL or at the terminator then we are done
        if (!m_js_text_end || !m_js_text_start || m_js_text_start[0] == 0)
            { return NULL; }

        //jump over the previous link (and its trailing quote)
        m_js_text_start += (get_current_hyperlink_length() > 0) ? get_current_hyperlink_length()+1 : 0;
        if (m_js_text_start >= m_js_text_end)
            { return NULL; }

        for (;;)
            {
            m_js_text_start = string_util::strchr(m_js_text_start, common_lang_constants::DOUBLE_QUOTE);
            if (m_js_text_start && m_js_text_start < m_js_text_end)
                {
                const wchar_t* endQuote = string_util::strchr(++m_js_text_start, common_lang_constants::DOUBLE_QUOTE);
                if (endQuote && endQuote < m_js_text_end)
                    {
                    m_current_hyperlink_length = (endQuote-m_js_text_start);
                    //see if the current link has a 3 or 4 character file extension on it--if not, this is not a link
                    if (m_current_hyperlink_length < 6  ||
                        (m_js_text_start[m_current_hyperlink_length-4] != common_lang_constants::PERIOD &&
                        m_js_text_start[m_current_hyperlink_length-5] != common_lang_constants::PERIOD))
                        {
                        m_current_hyperlink_length = 0;
                        m_js_text_start = ++endQuote;
                        continue;
                        }
                    //make sure this value is a possible link
                    size_t i = 0;
                    for (i = 0; i < m_current_hyperlink_length; ++i)
                        {
                        if (is_unsafe_uri_char(m_js_text_start[i]))
                            { break; }
                        }
                    //if we didn't make through each character then there must be a bad char in this string, so move on
                    if (i < m_current_hyperlink_length)
                        {
                        m_current_hyperlink_length = 0;
                        m_js_text_start = ++endQuote;
                        continue;
                        }
                    return m_js_text_start;
                    }
                else
                    { return NULL; }
                }
            else
                { return NULL; }
            }
        }

    const wchar_t* html_image_parse::operator()()
        {
        static const std::wstring HTML_IMAGE(L"img");
        //reset
        m_current_hyperlink_length = 0;

        if (!m_html_text || m_html_text[0] == 0)
            { return NULL; }

        while (m_html_text)
            {
            m_html_text = lily_of_the_valley::html_extract_text::find_element(m_html_text, m_html_text_end, HTML_IMAGE.c_str(), HTML_IMAGE.length());
            if (m_html_text)
                {
                std::pair<const wchar_t*,size_t> imageSrc = lily_of_the_valley::html_extract_text::read_tag(m_html_text, L"src", 3, false, true);
                if (imageSrc.first)
                    {
                    m_html_text = imageSrc.first;
                    m_current_hyperlink_length = imageSrc.second;
                    return m_html_text;
                    }
                //no src in this anchor, so go to next one
                else
                    {
                    m_html_text += HTML_IMAGE.length()+1;
                    continue;
                    }
                }
            else
                { return m_html_text = NULL; }
            }
        return m_html_text = NULL;
        }

    html_hyperlink_parse::html_hyperlink_parse(const wchar_t* html_text, const size_t length) :
                m_html_text(html_text), m_html_text_end(html_text+length), m_current_hyperlink_length(0),
                m_base(NULL), m_base_length(0), m_include_image_links(true), m_current_link_is_image(false),
                m_current_link_is_javascript(false), m_inside_of_script_section(false)
        {
        //see if there is a base url that should be used as an alternative that the client should use instead
        const wchar_t* headStart = string_util::stristr<wchar_t>(m_html_text, L"<head");
        if (!headStart)
            { return; }
        const wchar_t* base = string_util::stristr<wchar_t>(headStart, L"<base");
        if (!base)
            { return; }
        base = string_util::stristr<wchar_t>(base, L"href=");
        if (!base)
            { return; }
        const wchar_t firstLinkChar = base[5];
        base += 6;
        //eat any whitespace after href=
        for (;;)
            {
            if (!std::iswspace(base[0]) || base[0] == 0)
                { break; }
            ++base;
            }
        if (base[0] == 0)
            { return; }
        //look for actual link
        const wchar_t* endQuote = NULL;
        if (firstLinkChar == common_lang_constants::DOUBLE_QUOTE ||
            firstLinkChar == common_lang_constants::APOSTROPHE)
            { endQuote = string_util::strcspn_pointer<wchar_t>(base, L"\"\'", 2); }
        //if hackish author forgot to quote the link then look for matching space
        else
            {
            --base;
            endQuote = string_util::strcspn_pointer<wchar_t>(base, L" \r\n\t>", 5);
            }

        //if src is malformed then go to next one
        if (!endQuote)
            { return; }
        m_base = base;
        m_base_length = (endQuote-base);
        }

    const wchar_t* html_hyperlink_parse::operator()()
        {
        static const std::wstring HTML_META(L"meta");
        static const std::wstring HTML_IFRAME(L"iframe");
        static const std::wstring HTML_FRAME(L"frame");
        static const std::wstring HTML_SCRIPT(L"script");
        static const std::wstring HTML_SCRIPT_END(L"</script>");
        static const std::wstring HTML_IMAGE(L"img");
        //if we are in an embedded script block, then continue parsing the links out of that instead of using the regular parser
        if (m_inside_of_script_section)
            {
            const wchar_t* currentLink = m_javascript_hyperlink_parse();
            if (currentLink)
                {
                m_current_link_is_image = false;
                m_current_link_is_javascript = false;
                m_current_hyperlink_length = m_javascript_hyperlink_parse.get_current_hyperlink_length();
                return currentLink;
                }
            else
                { m_inside_of_script_section = false; }
            }
        //reset everything
        m_current_hyperlink_length = 0;
        m_current_link_is_image = false;
        m_current_link_is_javascript = false;
        m_inside_of_script_section = false;

        if (!m_html_text || m_html_text[0] == 0)
            { return NULL; }

        for (;;)
            {
            m_html_text = string_util::strchr(m_html_text, common_lang_constants::LESS_THAN);
            if (m_html_text && m_html_text+1 < m_html_text_end)
                {
                //don't bother with termination element
                if (m_html_text[1] == L'/')
                    {
                    ++m_html_text;
                    continue;
                    }
                m_current_link_is_image = lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_IMAGE.c_str(), HTML_IMAGE.length(), false);
                m_inside_of_script_section = m_current_link_is_javascript = lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_SCRIPT.c_str(), HTML_SCRIPT.length(), false);
                if (m_inside_of_script_section)
                    {
                    const wchar_t* endAngle = lily_of_the_valley::html_extract_text::find_close_tag(m_html_text);
                    const wchar_t* endOfScriptSection = string_util::stristr<wchar_t>(m_html_text, HTML_SCRIPT_END.c_str());
                    if (endAngle && (endAngle < m_html_text_end) &&
                        endOfScriptSection && (endOfScriptSection < m_html_text_end))
                        { m_javascript_hyperlink_parse.set(endAngle, endOfScriptSection-endAngle); }
                    }

                //see if it is an IMG, Frame (sometimes they have a SRC to another HTML page), or JS link
                if ((m_include_image_links && m_current_link_is_image) ||
                    m_current_link_is_javascript  ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_FRAME.c_str(), HTML_FRAME.length(), false)  ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_IFRAME.c_str(), HTML_IFRAME.length(), false))
                    {
                    m_html_text += 4;
                    std::pair<const wchar_t*,size_t> imageSrc = lily_of_the_valley::html_extract_text::read_tag(m_html_text, L"src", 3, false, true);
                    if (imageSrc.first)
                        {
                        m_html_text = imageSrc.first;
                        m_current_hyperlink_length = imageSrc.second;
                        return m_html_text;
                        }
                    //if we are currently in a SCRIPT section (that didn't have a link in it), then start parsing that section instead
                    else if (m_inside_of_script_section)
                        {
                        const wchar_t* currentLink = m_javascript_hyperlink_parse();
                        if (currentLink)
                            {
                            m_current_link_is_image = false;
                            m_current_link_is_javascript = false;
                            m_current_hyperlink_length = m_javascript_hyperlink_parse.get_current_hyperlink_length();
                            return currentLink;
                            }
                        else
                            {
                            m_inside_of_script_section = false;
                            continue;
                            }
                        }
                    //no src in this anchor, so go to next one
                    else
                        { continue; }
                    }
                //...or it is an anchor link
                else if (lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, L"a", 1, false) ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, L"link", 4, false) ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, L"area", 4, false) )
                    {
                    ++m_html_text;//skip the <
                    std::pair<const wchar_t*,size_t> href = lily_of_the_valley::html_extract_text::read_tag(m_html_text, L"href", 4, false, true);
                    if (href.first)
                        {
                        m_html_text = href.first;
                        m_current_hyperlink_length = href.second;
                        return m_html_text;
                        }
                    //no href in this anchor, so go to next one
                    else
                        { continue; }
                    }
                //...or a redirect in the HTTP meta section
                else if (lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_META.c_str(), HTML_META.size(), false) )
                    {
                    m_html_text += HTML_META.size() + 1;
                    std::wstring httpEquiv = lily_of_the_valley::html_extract_text::read_tag_as_string(m_html_text,
                        L"http-equiv", 10, false);
                    if (string_util::stricmp(httpEquiv.c_str(), L"refresh") == 0)
                        {
                        const wchar_t* url = lily_of_the_valley::html_extract_text::find_tag(m_html_text, L"url=", 4, true);
                        if (url && (url < m_html_text_end))
                            {
                            m_html_text = url+4;
                            if (m_html_text[0] == 0)
                                { return NULL; }
                            //eat up any whitespace or single quotes
                            for (;;)
                                {
                                if (m_html_text[0] == 0 ||
                                    (!std::iswspace(m_html_text[0]) &&
                                    m_html_text[0] != common_lang_constants::APOSTROPHE))
                                    { break; }
                                ++m_html_text;
                                }
                            if (m_html_text[0] == 0)
                                { return NULL; }
                            const wchar_t* endOfTag = string_util::strcspn_pointer<wchar_t>(m_html_text, L"'\">", 3);
                            //if link is malformed then go to next one
                            if (endOfTag == NULL || (endOfTag > m_html_text_end))
                                { continue; }
                            m_current_hyperlink_length = endOfTag - m_html_text;
                            return m_html_text;
                            }
                        }
                    }
                else
                    {
                    ++m_html_text;
                    continue;
                    }
                }
            else
                { return NULL; }
            }
        }

    html_url_format::html_url_format(const wchar_t* root_url) : m_last_slash(std::wstring::npos),
                                                    m_query(std::wstring::npos)
        {
        if (root_url)
            {
            m_root_url = root_url;
            m_current_url = root_url;
            }
        m_last_slash = find_last_directory(m_root_url, m_query);
        parse_domain(m_root_url, m_root_full_domain, m_root_domain, m_root_subdomain);
        //parse this as the current URL too until the () function is called by the client
        parse_domain(m_current_url, m_current_full_domain, m_current_domain, m_current_subdomain);
        if (has_query())
            { m_image_name = parse_image_name_from_url(m_root_url.c_str()); }
        }

    const wchar_t* html_url_format::operator()(const wchar_t* path, size_t text_length, const bool is_image /*= false*/)
        {
        if (!path || text_length == 0)
            { return NULL; }
        //see if it's a valid URL already
        if (is_absolute_url(path) )
            {
            m_current_url.assign(path, text_length);
            }
        //first see if it is a queried link first
        else if (path[0] == common_lang_constants::QUESTION_MARK && (m_query != std::wstring::npos))
            {
            m_current_url.assign(m_root_url, 0, m_query);
            m_current_url.append(path, text_length);
            }
        //or a link meant for the root of the full domain
        else if (path[0] == common_lang_constants::FORWARD_SLASH)
            {
            m_current_url.assign(m_root_full_domain);
            if (m_current_url.length() > 1 && m_current_url[m_current_url.length()-1] != common_lang_constants::FORWARD_SLASH)
                { m_current_url += (common_lang_constants::FORWARD_SLASH); }
            m_current_url.append(path+1, text_length-1);
            }
        //or if "./" is in front then strip it because it is redundant
        else if (string_util::strnicmp<wchar_t>(path, L"./", 2) == 0)
            {
            m_current_url.assign(m_root_url, 0, m_last_slash+1);
            m_current_url.append(path+2, text_length-2);
            }
        //or a relative link that goes up a few folders
        else if (string_util::strncmp(path, L"../", 3) == 0)
            {
            size_t folderLevelsToGoUp = 1;
            const wchar_t* start = path+3;
            for (;;)
                {
                if (string_util::strncmp(start, L"../", 3) == 0)
                    { start = path + (3*(++folderLevelsToGoUp)); }
                else
                    { break; }
                }
            size_t lastSlash = m_last_slash-1;
            while (folderLevelsToGoUp-- > 0)
                {
                size_t currentLastSlash = m_root_url.rfind(common_lang_constants::FORWARD_SLASH, lastSlash-1);
                /*just in case there aren't enough directories in the root url then
                just piece together what you can later.*/
                if (currentLastSlash == std::wstring::npos)
                    { break; }
                lastSlash = currentLastSlash;
                }
            /*make sure we didn't go all the way back to the protocol (e.g., "http://").
            If so, the move back up a folder. This can happen if there is an incorrect "../" in front
            of this link.*/
            if ((lastSlash < m_root_url.length()-2 && lastSlash > 0) &&
                is_either<wchar_t>(common_lang_constants::FORWARD_SLASH, m_root_url[lastSlash-1], m_root_url[lastSlash+1]))
                { lastSlash = m_root_url.find(common_lang_constants::FORWARD_SLASH, lastSlash+2); }
            if (lastSlash == std::wstring::npos)
                {m_current_url = m_root_url; }
            else
                { m_current_url.assign(m_root_url, 0, lastSlash+1); }
            m_current_url.append(start, text_length-(start-path));
            }
        //...or just a regular link
        else
            {
            m_current_url.assign(m_root_url, 0, m_last_slash+1);
            m_current_url.append(path, text_length);
            }

        //if this is a bookmark then chop it off
        size_t bookmark = m_current_url.rfind(common_lang_constants::POUND);
        if (bookmark != std::wstring::npos)
            { m_current_url.erase(bookmark); }

        /*sometimes with PHP the IMG SRC is just the folder path and you
        need to append the "image" value from the page's url*/
        if (is_image && m_current_url.length() > 1 && m_current_url[m_current_url.length()-1] == common_lang_constants::FORWARD_SLASH)
            { m_current_url += m_image_name; }

        //now get the domain information about this URL
        parse_domain(m_current_url, m_current_full_domain, m_current_domain, m_current_subdomain);

        //encode any spaces in the URL
        string_util::replace_all(m_current_url, L" ", L"%20");
        return m_current_url.c_str();
        }

    std::wstring html_url_format::get_directory_path()
        {
        size_t domainDirectoryPath = 0;
        if (string_util::strnicmp<wchar_t>(m_current_url.c_str(), L"http://", 7) == 0)
            { domainDirectoryPath = 7; }
        else if (string_util::strnicmp<wchar_t>(m_current_url.c_str(), L"ftp://", 6) == 0)
            { domainDirectoryPath = 6; }
        else
            { domainDirectoryPath = 0; }

        size_t query;
        size_t last_slash = find_last_directory(m_current_url, query);

        return m_current_url.substr(domainDirectoryPath, (last_slash-domainDirectoryPath));
        }

    std::wstring html_url_format::parse_image_name_from_url(const wchar_t* url)
        {
        static const std::wstring PHP_IMAGE(L"image=");
        std::wstring image_name;
        if (url == NULL || url[0] == 0)
            { return image_name; }
        const wchar_t* start = string_util::strchr(url, common_lang_constants::QUESTION_MARK);
        if (start == NULL)
            { return image_name; }
        start = string_util::stristr(start, PHP_IMAGE.c_str());
        if (start == NULL)
            { return image_name; }
        start += PHP_IMAGE.length();
        //see if it is the in front of another command
        const wchar_t* end = string_util::strchr(start, common_lang_constants::AMPERSAND);
        if (end == NULL)
            { image_name.assign(start); }
        else
            { image_name.assign(start, end-start); }
        return image_name; 
        }

    std::wstring html_url_format::parse_top_level_domain_from_url(const wchar_t* url)
        {
        static const std::wstring WWW(L"www.");
        std::wstring tld;
        if (url == NULL || url[0] == 0)
            { return tld; }
        //move to after the "www." or (if not there) the start of the url
        const wchar_t* start = string_util::stristr(url, WWW.c_str());
        if (start)
            { start += WWW.length(); }
        else
            { start = url; }
        start = string_util::strchr(start, common_lang_constants::PERIOD);
        if (start == NULL || start[1] == 0)
            { return tld; }
        const wchar_t* end = string_util::strcspn_pointer(++start, L"/?", 2);
        if (end == NULL)
            { tld.assign(start); }
        else
            { tld.assign(start, end-start); }
        return tld;
        }

    bool html_url_format::is_url_top_level_domain(const wchar_t* url)
        {
        if (url == NULL || url[0] == 0)
            { return false; }
        //move to after the protocol's "//" or (if not there) the start of the url
        const wchar_t* start = string_util::stristr(url, L"//");
        if (start)
            { start += 2; }
        else
            { start = url; }
        //if no more slashes in the URL or if that last slash is the last character
        //then this must be just a domain
        start = string_util::strchr(start, common_lang_constants::FORWARD_SLASH);
        if (start == NULL || start[1] == 0)
            { return true; }
        else
            { return false; }
        }

    size_t html_url_format::find_last_directory(std::wstring& url, size_t& query_postion)
        {
        //if this is a queried page then see where the command is at
        query_postion = url.rfind(common_lang_constants::QUESTION_MARK);
        size_t last_slash = url.rfind(common_lang_constants::FORWARD_SLASH);

        //if this page is queried then backtrack to the '/' right before the query
        if ((query_postion != std::wstring::npos) && (last_slash != std::wstring::npos) &&
            (query_postion > 0) && (last_slash > 0) &&
            (last_slash > query_postion))
            { last_slash = url.rfind(common_lang_constants::FORWARD_SLASH, query_postion-1); }
        //see if the slash is just the one after "http:/" or if there no none at all
        if ((last_slash != std::wstring::npos &&
            (last_slash > 0) &&
            (url[last_slash-1] == common_lang_constants::FORWARD_SLASH)) ||
            last_slash == std::wstring::npos)
            {
            //e.g., http://www.website.com/
            url += common_lang_constants::FORWARD_SLASH;
            last_slash = url.length()-1;
            }
        return last_slash;
        }

    void html_url_format::parse_domain(const std::wstring& url, std::wstring& full_domain,
                            std::wstring& domain, std::wstring& subdomain)
        {
        full_domain.clear();
        domain.clear();
        size_t startIndex = 0;
        if (string_util::strnicmp<wchar_t>(url.c_str(), L"http://", 7) == 0)
            { startIndex = 7; }
        else if (string_util::strnicmp<wchar_t>(url.c_str(), L"https://", 8) == 0)
            { startIndex = 8; }
        else if (string_util::strnicmp<wchar_t>(url.c_str(), L"ftp://", 6) == 0)
            { startIndex = 6; }
        else
            { startIndex = 0; }
        size_t lastSlash = url.find(common_lang_constants::FORWARD_SLASH, startIndex);
        if (lastSlash == std::wstring::npos)
            { full_domain = url; }
        else
            { full_domain = url.substr(0, lastSlash); }

        //http://www.sales.mycompany.com: go to dot in front of ".com"
        size_t dot = full_domain.rfind(common_lang_constants::PERIOD, lastSlash);
        if (dot == std::wstring::npos || dot == 0)
            { return; }
        //now go back one more dot to go after the "www." prefix or subdomain
        dot = full_domain.rfind(common_lang_constants::PERIOD, --dot);
        //skip the dot
        if (dot != std::wstring::npos)
            { ++dot; }
        //if there is no "www." prefix then go the end of the protocol
        else
            { dot = startIndex; }
        subdomain = domain = full_domain.substr(dot, lastSlash-dot);
        //now the subdomain.  If no subdomain, then this will be the same as the domain.
        if (dot != startIndex && dot > 2)
            {
            dot = full_domain.rfind(common_lang_constants::PERIOD, dot-2);
            if (dot != std::wstring::npos)
                {
                ++dot;
                subdomain = full_domain.substr(dot, lastSlash-dot);
                }
            }
        }
    }
