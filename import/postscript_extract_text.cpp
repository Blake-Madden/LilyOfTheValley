#include "postscript_extract_text.h"

const wchar_t* lily_of_the_valley::postscript_extract_text::operator()(const char* ps_buffer, const size_t text_length)
    {
    clear_log();
    if (ps_buffer == NULL || ps_buffer[0] == 0 || text_length == 0)
        {
        set_filtered_text_length(0);
        return NULL;
        }
    //resize the internal buffer if new postscript stream is bigger
    if (!allocate_text_buffer(text_length))
        {
        set_filtered_text_length(0);
        return NULL;
        }

    size_t i = 0;

    const char* const endSentinel = ps_buffer+text_length;

    //see if it's a valid postscript file and whether we can support parsing it
    const char* const header = string_util::strstr(ps_buffer, "%!PS-Adobe-");
    if (!header || header > endSentinel)
        { throw postscript_header_not_found(); }
    const double version = std::strtod(header+11, NULL);
    if (version >= 3)
        { throw postscript_version_not_supported(); }

    //find the software that created this file, there may be quirks that we have to workaround
    bool createdByDVIPS = false;
    const char* const creator = string_util::strstr(ps_buffer, "%%Creator:");
    if (creator && creator < endSentinel)
        {
        const char* const endOfCreator = string_util::strstr(creator+10, "%%");
        if (endOfCreator && endOfCreator < endSentinel)
            {
            if (string_util::strnistr(creator, "dvips", (endOfCreator-creator)) ||
                string_util::strnistr(creator, "Radical Eye Software", (endOfCreator-creator)) )
                { createdByDVIPS = true; }
            }
        }

    //skip past the header block if possible
    const char* const begin = string_util::strstr(header, "%%Page:");
    if (begin)
        { i = begin-ps_buffer; }

    size_t open_paran_count = 0, close_paran_count = 0;

    bool umlautMode = false;
    bool graveMode = false;
    bool acuteMode = false;
    bool inNegativeBMode = false;
    for (/*counter already initialized*/; i < text_length; ++i)
        {
        switch (ps_buffer[i])
            {
        case '%':
            if (open_paran_count > close_paran_count)
                { add_character(ps_buffer[i]); }
            else
                {
                //skip over document definition section
                if (string_util::strncmp(ps_buffer, "%BeginDocument", 14) == 0)
                    {
                    const char* end = string_util::strstr(ps_buffer+i, "%%EndDocument");
                    if (!end)
                        {
                        //file is messed up--just return what we got
                        log_message(L"\"%%EndDocument\" element missing in Postscript file.");
                        return get_filtered_text();
                        }
                    else
                        { ps_buffer = end+13/*the length of "%%EndDocument"*/; }
                    }
                //it's a comment--move to the end of the line
                else
                    {
                    while (++i < text_length)
                        {
                        if (std::isspace(ps_buffer[i] ) )
                            { break; }
                        }
                    }
                }
            break;
        case '(':
            if (open_paran_count++ > close_paran_count)
                { add_character(ps_buffer[i]); }
            break;
        case ')':
            if (open_paran_count > ++close_paran_count)
                { add_character(ps_buffer[i]); }
            /*() are now closed, so move to the character in front
            of the next () set and see the command*/
            else
                {
                char command_char = L' ';
                bool inHyphenJoinMode = false;
                bool newLineCommandFound = false;
                inHyphenJoinMode = (ps_buffer[i-1] == '-');
                //skip over any newlines in the file between the ')' and the first command of the next text section
                while (i < (text_length-1) && isspace(static_cast<const int>(ps_buffer[i+1])))
                    { ++i; }
                long horizontalPosition = 10;
                if (ps_buffer[i+1] == '-' || isdigit(static_cast<const int>(ps_buffer[i+1])))
                    { horizontalPosition = std::strtol(ps_buffer+i+1, NULL, 10); }
                while (i < (text_length-1) && ps_buffer[i+1] != '(')
                    {
                    ++i;
                    if (ps_buffer[i] == '%')
                        {
                        if (string_util::strncmp(ps_buffer+i, "%%BeginDocument", 15) == 0)
                            {
                            const char* end = string_util::strstr(ps_buffer+i, "%%EndDocument");
                            if (!end)
                                {
                                //file is messed up--just return what we got
                                log_message(L"\"%%EndDocument\" element missing in Postscript file.");
                                return get_filtered_text();
                                }
                            else
                                {
                                i = (end + 13/*the length of "%%EndDocument"*/) - ps_buffer;
                                }
                            }
                        else if (string_util::strncmp(ps_buffer+i, "%%Page", 6) == 0)
                            { newLineCommandFound = true; }
                        }
                    else if (ps_buffer[i] == 'y' && ps_buffer[i-1] != 'F')
                        { newLineCommandFound = true; }

                    if (!std::isspace(ps_buffer[i]) )
                        { command_char = ps_buffer[i]; }
                    }

                if (newLineCommandFound)
                    { add_character(L'\n'); }
                else if ((inHyphenJoinMode ||
                        command_char == 'q' ||                        
                        command_char == 'o' ||
                        command_char == 'l' ||
                        command_char == 'm' ||
                        command_char == 'n' ||
                        command_char == 'r' ||
                        command_char == 's' ||
                        (command_char == 'b' && (horizontalPosition <= 7)) ||
                        (inNegativeBMode && command_char == 'g') ||
                        command_char == 't') &&
                        (i > 0 && ps_buffer[i-1] != 'F') )
                    {
                    //NOOP (just leave the characters together)
                    }
                else
                    { add_character(L' '); }
                inNegativeBMode = (command_char == 'b' && horizontalPosition < 0) || (inNegativeBMode && command_char == 'g');
                }
            break;
        case '\\':
                if (open_paran_count > close_paran_count)
                    {
                    ++i;
                    char* stopPoint = NULL;
                    const wchar_t octalVal = static_cast<wchar_t>(std::strtol(ps_buffer+i, &stopPoint, 8));
                    switch (ps_buffer[i])
                        {
                        case '(':
                        case ')':
                            add_character(ps_buffer[i]);
                            break;
                        case '\\':
                            if (createdByDVIPS)
                                { add_character(L'\"'); }
                            else
                                { add_character(ps_buffer[i]); }
                            break;
                        case 't':
                            add_character(L'\t');
                            break;
                        case 'n':
                            add_character(L'\n');
                            break;
                        case 'r':
                            add_character(L'\r');
                            break;
                        //escaped newline\carriage return characters should be ignored
                        case '\n':
                        case '\r':
                            break;
                        default:
                            if ((stopPoint-(ps_buffer+i)) > 1)
                                {
                                if (octalVal == 0)
                                    { add_character(L'-'); }//some sort of DVIPS quirk
                                else if (octalVal == 3)
                                    { add_character(L'*'); }
                                else if (octalVal == 11)
                                    { add_characters(L"ff", 2); }
                                else if (octalVal == 12)
                                    { add_characters(L"fi", 2); }
                                else if (octalVal == 13)
                                    { add_characters(L"fl", 2); }
                                else if (octalVal == 14)
                                    { add_characters(L"ffi", 3); }
                                else if (octalVal == 15)
                                    { add_characters(L"ffl", 3); }
                                else if (octalVal == 18)
                                    { graveMode = true; }
                                else if (octalVal == 19)
                                    { acuteMode = true; }
                                else if (octalVal == 21)
                                    { add_character(L'*'); }
                                else if (octalVal == 23)
                                    { add_character(L'v'); }
                                else if (octalVal == 24)
                                    { add_character(0x3A3); }
                                else if (octalVal == 26)
                                    { add_characters(L"nae", 3); }
                                else if (octalVal == 27)
                                    { add_characters(L"oe", 2); }
                                else if (octalVal == 28)
                                    { add_characters(L"fi", 2); }
                                else if (octalVal == 127)
                                    { umlautMode = true; }
                                else
                                    { add_character(octalVal); }
                                i += (stopPoint-(ps_buffer+i))-1;//skip to one before the end of the number because when the loop starts again it will step one more
                                }
                            else
                                {
                                add_character(ps_buffer[i]);
                                }
                            break;
                        }
                    }
                break;
        default:
            if (open_paran_count > close_paran_count)
                {
                //previous \177 flag indicates next character should be umlauted
                if (umlautMode)
                    {
                    switch (ps_buffer[i])
                        {
                    case 0x0041://A
                        add_character(0xC4);
                        break;
                    case 0x0061://a
                        add_character(0xE4);
                        break;
                    case 0x0045://E
                        add_character(0xCB);
                        break;
                    case 0x0065://e
                        add_character(0xEB);
                        break;
                    case 0x0049://I
                        add_character(0xCF);
                        break;
                    case 0x0069://i
                        add_character(0xEF);
                        break;
                    case 0x004F://O
                        add_character(0xD6);
                        break;
                    case 0x006F://o
                        add_character(0xF6);
                        break;
                    case 0x0055://U
                        add_character(0xDC);
                        break;
                    case 0x0075://u
                        add_character(0xFC);
                        break;
                    default:
                        add_character(ps_buffer[i]);
                        };
                    }
                else if (graveMode)
                    {
                    switch (ps_buffer[i])
                        {
                    case 0x0041://A
                        add_character(0xC0);
                        break;
                    case 0x0061://a
                        add_character(0xE0);
                        break;
                    case 0x0045://E
                        add_character(0xC8);
                        break;
                    case 0x0065://e
                        add_character(0xE8);
                        break;
                    case 0x0049://I
                        add_character(0xCC);
                        break;
                    case 0x0069://i
                        add_character(0xEC);
                        break;
                    case 0x004F://O
                        add_character(0xD2);
                        break;
                    case 0x006F://o
                        add_character(0xF2);
                        break;
                    case 0x0055://U
                        add_character(0xD9);
                        break;
                    case 0x0075://u
                        add_character(0xF9);
                        break;
                    default:
                        add_character(ps_buffer[i]);
                        };
                    }
                else if (acuteMode)
                    {
                    switch (ps_buffer[i])
                        {
                    case 0x0041://A
                        add_character(0xC1);
                        break;
                    case 0x0061://a
                        add_character(0xE1);
                        break;
                    case 0x0045://E
                        add_character(0xC9);
                        break;
                    case 0x0065://e
                        add_character(0xE9);
                        break;
                    case 0x0049://I
                        add_character(0xCD);
                        break;
                    case 0x0069://i
                        add_character(0xED);
                        break;
                    case 0x004F://O
                        add_character(0xD3);
                        break;
                    case 0x006F://o
                        add_character(0xF3);
                        break;
                    case 0x0055://U
                        add_character(0xDA);
                        break;
                    case 0x0075://u
                        add_character(0xFA);
                        break;
                    default:
                        add_character(ps_buffer[i]);
                        };
                    }
                else
                    { add_character(ps_buffer[i]); }
                umlautMode = false;
                graveMode = false;
                acuteMode = false;
                break;
                }
            }
        }
    trim();
    //returns the raw text buffer
    return get_filtered_text();
    }
