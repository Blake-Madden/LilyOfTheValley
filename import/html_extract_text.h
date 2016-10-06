/**@addtogroup Importing
@brief Classes for importing and parsing text.
@date 2005-2016
@copyright Oleander Software, Ltd.
@author Oleander Software, Ltd.
@details This program is free software; you can redistribute it and/or modify
it under the terms of the BSD License.
* @{*/

#ifndef __HTML_EXTRACT_TEXT_H__
#define __HTML_EXTRACT_TEXT_H__

#include <map>
#include <vector>
#include <algorithm>
#include "extract_text.h"
#include "../indexing/common_lang_constants.h"

namespace html_utilities
    {
    /**@brief Class to convert a character to its symbol equivalent (for the notorious Symbol font).*/
    class symbol_font_table
        {
    public:
        symbol_font_table();
        /**Finds a letter's symbol equivalent and returns it. If there is no symbol equivalent, then the
           original letter is returned.
           @param letter The letter to find.
           @returns The symbol equivalent of the letter, or the letter if no equivalent is found. For example, 'S' will return 'Σ'.*/
        wchar_t find(const wchar_t letter) const;
    private:
        std::map<wchar_t, wchar_t> m_symbol_table;
        };

    /**@brief Class to convert an HTML entity (e.g., "&amp;") to its literal value.*/
    class html_entity_table
        {
    public:
        html_entity_table();
        /**@returns The unicode value of an entity, or '?' if not valid.
        @param html_entity The entity to look up.*/
        wchar_t operator[](const wchar_t* html_entity) const
            { return find(html_entity, std::wcslen(html_entity)); }
        /**Searches for an entity.
        @returns The unicode value of an entity, or '?' if not valid.
        @param html_entity The entity to look up.
        @param length The length of the entity string.
        @note This function first must do a case sensitive search because some HTML entities
        are case sensitive (e.g., "Dagger" and "dagger" result in different values). Of course,
        before XHTML, most HTML was very liberal with casing, so if a case sensitve search fails,
        then a case insensitive search is performed. In this case, whatever the HTML author's
        intention for something like "&SIGMA;" may be misinterpretted (should it be a lowercase or
        uppercase sigma symbol?)--the price you pay for sloppy HTML.*/
        wchar_t find(const wchar_t* html_entity, const size_t length) const;
    private:
        std::map<std::wstring, wchar_t> m_table;
        };

    /**@brief Functor that accepts a block of script text and returns the links in it, one-by-one.
    Links will be anything inside of double quotes that appear to be a path to a file or webpage.*/
    class javascript_hyperlink_parse
        {
    public:
        /**Empty Constructor*/
        javascript_hyperlink_parse() :
          m_js_text_start(NULL), m_js_text_end(NULL), m_current_hyperlink_length(0)
            {}
        /**Constructor.
           @param js_text The Javascript to analyze.
           @param length The length of js_text.*/
        javascript_hyperlink_parse(const wchar_t* js_text, const size_t length) :
          m_js_text_start(js_text), m_js_text_end(js_text+length), m_current_hyperlink_length(0)
            {}
        /**Sets the Javascript to analyze.
           @param js_text The Javascript to analyze.
           @param length The length of js_text.*/
        void set(const wchar_t* js_text, const size_t length)     
            {
            m_js_text_start = js_text;
            m_js_text_end = (js_text+length);
            m_current_hyperlink_length = 0;
            }
        /**Main function that returns the next link in the file.
        @returns A pointer to the next link, or NULL when there are no more links.*/
        const wchar_t* operator()();
        /**@returns The length of the current hyperlink.*/
        inline size_t get_current_hyperlink_length() const
            { return m_current_hyperlink_length; }
    private:
        const wchar_t* m_js_text_start;
        const wchar_t* m_js_text_end;
        size_t m_current_hyperlink_length;
        };

    /**@brief Functor that accepts a block of HTML text and returns the image links in it, one-by-one.*/
    class html_image_parse
        {
    public:
        /**Constructor.
           @param html_text The HTML text to analyze.
           @param length The length of html_text.*/
        html_image_parse(const wchar_t* html_text, const size_t length) :
                m_html_text(html_text), m_html_text_end(html_text+length), m_current_hyperlink_length(0)
            {}
        /**Main function that returns the next link in the file.
        @returns either the next link or NULL when there are no more links.*/
        const wchar_t* operator()();
        /**@returns The length of the current hyperlink.*/
        inline size_t get_current_hyperlink_length() const
            { return m_current_hyperlink_length; }
    private:
        const wchar_t* m_html_text;
        const wchar_t* m_html_text_end;
        size_t m_current_hyperlink_length;
        };

    /**@brief Functor that accepts a block of HTML text and returns the links in it, one-by-one.
    Links will include base hrefs, link hrefs, anchor hrefs, image map hrefs, image links, javascript links, and HTTP redirects.*/
    class html_hyperlink_parse
        {
    public:
        /**Constructor.
           @param html_text The HTML text to analyze.
           @param length The length of html_text.*/
        html_hyperlink_parse(const wchar_t* html_text, const size_t length);
        ///@returns The base web directory.
        const wchar_t* get_base_url() const
            { return m_base; }
        ///@returns The length of the base web directory.
        const size_t get_base_url_length () const
            { return m_base_length; }
        /**Main function that returns the next link in the file.
        @returns Either the next link or NULL when there are no more links.*/
        const wchar_t* operator()();
        /**@returns The length of the current hyperlink.*/
        inline size_t get_current_hyperlink_length() const
            { return m_current_hyperlink_length; }
        /**Specifies whether or not image links should be reviewed.
        @param include Set to false to skip image links, true to include them.*/
        inline void include_image_links(const bool include)
            { m_include_image_links = include; }
        /**@returns True if the current hyperlink is pointing to an image.*/
        inline bool is_current_link_an_image() const
            { return m_current_link_is_image; }
        /**@returns True if the current hyperlink is pointing to a javascript.*/
        inline bool is_current_link_a_javascript() const
            { return m_current_link_is_javascript; }
        /**Finds the end of an url by searching for the first illegal character.
           @param text the HTML text to analyze.
           @returns The valid end of the URL (i.e., the first illegal character).*/
        static const wchar_t* find_url_end(const wchar_t* text)
            {
            while (text && is_legal_url_character(text[0]))
                { ++text; }
            return text;
            }
        /**Indicates whether or not character is legal to be in an URL. If false, then that character
           should be hex encoded.
           @param ch The character to review.
           @returns Whether the character is illegal and needs to be encoded.*/
        static inline bool is_legal_url_character(const wchar_t ch)
            {
            return (((ch >= 0x41) && (ch <= 0x5A)) || //A-Z
                ((ch >= 0x61) && (ch <= 0x7A)) || //a-1
                ((ch >= 0x30) && (ch <= 0x39)) || //0-9
                (ch == common_lang_constants::PERIOD) ||
                (ch == 0x24) || //$
                (ch == common_lang_constants::AMPERSAND) ||
                (ch == 0x2B) || //+
                (ch == 0x2C) || //,
                (ch == 0x2F) || ///
                (ch == 0x3A) || //:
                (ch == 0x3B) || //;
                (ch == 0x3D) || //=
                (ch == 0x3F) || //?
                (ch == 0x40)//@
                );
            }
    private:
        const wchar_t* m_html_text;
        const wchar_t* m_html_text_end;
        size_t m_current_hyperlink_length;
        const wchar_t* m_base;
        size_t m_base_length;
        bool m_include_image_links;
        bool m_current_link_is_image;
        bool m_current_link_is_javascript;
        bool m_inside_of_script_section;
        javascript_hyperlink_parse m_javascript_hyperlink_parse;
        };

    /**@brief Wrapper class to generically handle hyperlink parsing for either javascript or HTML files.*/
    class hyperlink_parse
        {
    public:
        enum hyperlink_parse_method
            {
            html,
            script
            };
        /**Constructor to initialize parser.
           @param html_text The text to parse.
           @param length The length of the text being parsed.
           @param method How to parse the text. Either html or script.*/
        hyperlink_parse(const wchar_t* html_text, const size_t length, const hyperlink_parse_method method) :
            m_html_hyperlink_parse(html_text, length), m_javascript_hyperlink_parse(html_text, length), m_method(method)
            {}
        /**Main function that returns the next link in the file.
        @returns A pointer to the next link, or NULL when there are no more links.*/
        const wchar_t* operator()()
            {
            return (get_parse_method() == html) ? m_html_hyperlink_parse() : m_javascript_hyperlink_parse();
            }
        /**@returns The HTML parser.*/
        html_hyperlink_parse get_html_parser() const
            { return m_html_hyperlink_parse; }
        /**@return The Javascript parser.*/
        javascript_hyperlink_parse get_script_parser() const
            { return m_javascript_hyperlink_parse; }
        /**@return The parsing method, either html or script.*/
        hyperlink_parse_method get_parse_method() const
            { return m_method; }
        /**@return the length of the current hyperlink.*/
        inline size_t get_current_hyperlink_length() const
            {
            return (get_parse_method() == html) ?
                m_html_hyperlink_parse.get_current_hyperlink_length() : m_javascript_hyperlink_parse.get_current_hyperlink_length();
            }
    private:
        html_hyperlink_parse m_html_hyperlink_parse;
        javascript_hyperlink_parse m_javascript_hyperlink_parse;
        hyperlink_parse_method m_method;
        };

    /**@brief Class to format a given filepath into a full URL, using a base URL as the starting point.*/
    class html_url_format
        {
    public:
        /**CTOR, which accepts the base URL to format any relative links with.
        @param root_url The base directory to format the URLs to.*/
        html_url_format(const wchar_t* root_url);
        /**Main interface.
            @param path The filepath to format.
            @param text_length The length of the filepath.
            @param is_image Whether or not the filepath is to an image. This is needed when formatting a full
            URL to an image from a PHP request.
            @returns The fully-formed file path.*/
        const wchar_t* operator()(const wchar_t* path, size_t text_length, const bool is_image = false);
        /**@returns The domain of the base URL.
            For example, a base URL of "http://www.business.yahoo.com" will return "yahoo.com".*/
        std::wstring get_root_domain() const
            { return m_root_domain; }
        /**@returns The subdomain of the base URL.
            For example, a base URL of "http://www.business.yahoo.com" will return "business.yahoo.com".*/
        std::wstring get_root_subdomain() const
            { return m_root_subdomain; }
        /**@returns The full domain of the base URL (domain, subdomain, and protocol).*/
        std::wstring get_root_full_domain() const
            { return m_root_full_domain; }

        /**@returns The domain of the current URL.
            For example, a base URL of "http://www.business.yahoo.com" will return "yahoo.com".*/
        std::wstring get_domain() const
            { return m_current_domain; }
        /**@returns The subdomain of the current URL.
            For example, a base URL of "http://www.business.yahoo.com" will return "business.yahoo.com".*/
        std::wstring get_subdomain() const
            { return m_current_subdomain; }
        /**@returns The full domain of the current URL (domain, subdomain, and protocol).*/
        std::wstring get_full_domain() const
            { return m_current_full_domain; }

        /**@returns The subdomain and folder structure of the current URL.*/
        std::wstring get_directory_path();
        /**@returns Whether or not URL starts with a server protocol (e.g., "http"). If not, then it
            is a relative URL.
            @param url The url to analyze.*/
        inline static bool is_absolute_url(const wchar_t* url)
            {
            return (string_util::strnicmp<wchar_t>(url, L"http://", 7) == 0 ||
                    string_util::strnicmp<wchar_t>(url, L"https://", 8) == 0 ||
                    string_util::strnicmp<wchar_t>(url, L"www.", 4) == 0 ||
                    string_util::strnicmp<wchar_t>(url, L"ftp://", 6) == 0);
            }
        /**@returns Whether or not this URL has a PHP query command.*/
        bool has_query() const
            { return m_query != std::wstring::npos; }
        /**Looks for "image=" in the PHP command.
            Example, "www.mypage.com?image=hi.jpg&loc=location" would return "hi.jpg".
            @param url The url to analyze.
            @returns The image name.*/
        static std::wstring parse_image_name_from_url(const wchar_t* url);
        /**@returns The top-level domain (e.g., .com or .org) from an url.
            @param url The url to analyze.*/
        static std::wstring parse_top_level_domain_from_url(const wchar_t* url);
        /**@returns Whether an URL is just a domain and not a subfolder or file.
            @param url The url to analyze.*/
        static bool is_url_top_level_domain(const wchar_t* url);
    protected:
        /**@returns The position of the top level direction in an URL, as well as
        the position of the query comment in it (if there is one). Also will add a slash
        to the URL if need.
        @param[in,out] url The URL to parse.
        @param[out] query_postion The position in the URL of the query (e.g., PHP command).*/
        static size_t find_last_directory(std::wstring& url, size_t& query_postion);
        /**Parses an URL into a full domain (domain, subdomain, and protocol), a domain, and subdomain.
            @param url The URL to parse.
            @param[out] full_domain The full domain (domain, subdomain, and protocol) of the URL.
            @param[out] domain The domain of the URL.
            @param[out] subdomain The subdomain of the URL.*/
        static void parse_domain(const std::wstring& url, std::wstring& full_domain,
                            std::wstring& domain, std::wstring& subdomain);
    private:
        //info about the original starting URL
        std::wstring m_root_url;
        std::wstring m_root_full_domain;
        std::wstring m_root_domain;
        std::wstring m_root_subdomain;
        std::wstring m_image_name;
        size_t m_last_slash;
        size_t m_query;
        //the current url
        std::wstring m_current_url;
        std::wstring m_current_subdomain;
        std::wstring m_current_full_domain;
        std::wstring m_current_domain;
        };

    /**@brief Class to strip hyperlinks from an HTML stream. The rest of the HTML's format is preserved.*/
    class html_strip_hyperlinks : public lily_of_the_valley::extract_text
        {
    public:
        /**Main interface for extracting plain text from an HTML buffer.
        @param html_text The HTML text to strip.
        @param text_length The length of the HTML text.
        @returns The HTML stream with the hyperlinks removed from it.*/
        const wchar_t* operator()(const wchar_t* html_text,
                                  const size_t text_length);
        };

    inline bool is_unsafe_uri_char(const wchar_t character)
        { return (character > 127 /*extended ASCII*/ || character < 33 /*control characters and space*/); }
    }

namespace lily_of_the_valley
    {
    /**@brief Class to extract text from an <b>HTML</b> stream.
    @par Example:
    @code
    //insert your own HTML file here
    std::ifstream fs("PatientReport.htm", std::ios::in|std::ios::binary|std::ios::ate);
    if (fs.is_open())
        {
        //read an HTML file into a char* buffer first...
        size_t fileSize = fs.tellg();
        char* fileContents = new char[fileSize+1];
        std::auto_ptr<char> deleteBuffer(fileContents);
        std::memset(fileContents, 0, fileSize+1);
        fs.seekg(0, std::ios::beg);
        fs.read(fileContents, fileSize);

        //...then convert it to a Unicode buffer.
        //Note that you should use the specified character set
        //in the HTML file (usually UTF-8) when loading this file into a wchar_t buffer.
        //Here we are using the standard mbstowcs() function which assumes the current
        //system's character set; however, it is recommended to parse the character set
        //from the HTML file (with parse_charset()) and use a
        //system-dependent function (e.g., MultiByteToWideChar() on Win32)
        //to properly convert the char buffer to Unicode using that character set.
        wchar_t* convertedFileContents = new wchar_t[fileSize+1];
        std::auto_ptr<wchar_t> deleteConvertedBuffer(convertedFileContents);
        const size_t convertedFileSize = std::mbstowcs(convertedFileContents, fileContents, fileSize);

        //convert the Unicode HTML data into raw text
        lily_of_the_valley::html_extract_text htmlExtract;
        htmlExtract(convertedFileContents, convertedFileSize, true, false);
        //The raw text from the file is now in a Unicode buffer.
        //This buffer can be accessed from get_filtered_text() and its length
        //from get_filtered_text_length(). Call these to copy the text into
        //a wide string.
        std::wstring fileText(htmlExtract.get_filtered_text(), htmlExtract.get_filtered_text_length());
        }
    @endcode*/
    class html_extract_text : public extract_text
        {
    public:
        /**Main interface for extracting plain text from an HTML buffer.
        @param html_text The HTML text to strip.
        @param text_length The length of the HTML text.
        @param include_outer_text Whether text outside of the first and last <> should be included. Recommended true.
        @param preserve_spaces Whether embedded newlines should be included in the output. If false,
        then they will be replaced with spaces, which is the default for HTML renderers. Recommended false.
        @returns The plain text from the HTML stream.*/
        const wchar_t* operator()(const wchar_t* html_text,
                                  const size_t text_length,
                                  const bool include_outer_text,
                                  const bool preserve_spaces);
        /**Compares (case insensitively) raw HTML text with an element constant to see if the current element that
        we are on is the one we are looking for.
        @param text The current position in the HTML buffer that we are examining.
        @param element The element that we are comparing against the current position.
        @param element_size The length of the element that we are comparing against.
        @param accept_self_terminating_elements Whether we should match elements that close themselves
        (i.e., don't have a matching </[element]>, but rather end where it is declared). For example,
        "<br />" is a self-terminating element. You would set this to false if you only want to read
        text inbetween opening an closing tags.
        @returns True if the current position matches the element.
        @note Be sure to skip the starting '<' first.*/
        static bool compare_element(const wchar_t* text, const wchar_t* element,
                                   const size_t element_size,
                                   const bool accept_self_terminating_elements = false);
        /**Compares (case sensitively) raw HTML text with an element constant to see if the current element that
        we are on is the one we are looking for. Be sure to skip the starting '<' first.
        @param text The current position in the HTML buffer that we are examining.
        @param element The element that we are comparing against the current position.
        @param element_size The length of the element that we are comparing against.
        @param accept_self_terminating_elements Whether we should match elements that close themselves
        (i.e., don't have a matching </[element]>, but rather end where it is declared). For example,
        "<br />" is a self-terminating element. You would set this to false if you only want to read
        text inbetween opening an closing tags.
        @returns True if the current position matches the element.
        @note This function is case sensitive, so it should only be used for XML or strict HTML 4.0.*/
        static bool compare_element_case_sensitive(const wchar_t* text, const wchar_t* element,
                                   const size_t element_size,
                                   const bool accept_self_terminating_elements = false);
        /**@returns The current element that the stream is on. This assumes that you have
           already skipped the leading < symbol.
           @param text The HTML stream to analyze.
           @param accept_self_terminating_elements Whether to analyze element such as "<br />.*/
        static std::wstring get_element_name(const wchar_t* text,
                                             const bool accept_self_terminating_elements = true);
        /**@returns The matching > to a <, or NULL if not found.
           @param text The HTML stream to analyze.
           @param fail_on_overlapping_open_symbol Whether it should immediately return failure if the next
            '<' is found before a closing '>' is found.*/
        static const wchar_t* find_close_tag(const wchar_t* text, const bool fail_on_overlapping_open_symbol = false);
        /**Searches for a tag inside of an element and returns its value (or empty string if not found).
        @param text The start of the element section.
        @param tag The inner tag to search for (e.g., \"bgcolor\").
        @param tagSize The length of the tag to search for.
        @param allowQuotedTags Set this parameter to true for tags that are inside of quotes (e.g., style values).
               For example, to read \"bold\" as the value for \"font-weight\" from <span style=\"font-weight: bold;\">,
               this should be set to true.
        @param allowSpacesInValue Whether there can be a spaces in the tag's value.
               Usually you would only see that with complex strings values, such as a font name.
        @returns The pointer to the tag value and its length. Returns NULL and length of zero on failure.*/
        static std::pair<const wchar_t*, size_t> read_tag(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags,
            const bool allowSpacesInValue = false);
        /**Same as read_tag(), except it return a standard string object instead of a raw pointer.
        @param text The start of the element section.
        @param tag The inner tag to search for (e.g., \"bgcolor\").
        @param tagSize The length of the tag to search for.
        @param allowQuotedTags Set this parameter to true for tags that are inside of quotes (e.g., style values).
               For example, to read \"bold\" from <span style=\"font-weight: bold;\">, this would need
               to be true. Usually this would be false.
        @param allowSpacesInValue Whether there can be a spaces in the tag's value.
               Usually you would only see that with complex strings values, such as a font name.
        @returns The tag value as a string, or empty string on failure.*/
        static std::wstring read_tag_as_string(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags,
            const bool allowSpacesInValue = false);
        /**Searches a buffer range for an element (e.g., "<h1>").
           @returns The pointer to the next element, or NULL if not found.
           @param sectionStart The start of the HTML buffer.
           @param sectionEnd The end of the HTML buffer.
           @param elementTag The name of the element (e.g., "table") to find.
           @param elementTagLength The length of elementTag.
           @param accept_self_terminating_elements True to accept tags such as "<br />". Usually should be true,
            use false here if searching for opening and closing elements with content in them.*/
        static const wchar_t* find_element(const wchar_t* sectionStart,
                                           const wchar_t* sectionEnd,
                                           const wchar_t* elementTag,
                                           const size_t elementTagLength,
                                           const bool accept_self_terminating_elements = true);
        /**Searches a buffer range for an element's matching close (e.g., "</h1>").
           If the search starts on the starting element, then it will search for the matching close
           tag (i.e., it will skip inner elements that have the same name and go to the correct
           closing element).
           @returns The pointer to the closing element, or NULL if not found.
           @param sectionStart The start of the HTML buffer.
           @param sectionEnd The end of the buffer.
           @param elementTag The element (e.g., "h1") whose respective ending element that we are looking for.
           @param elementTagLength The length of elementTag.*/
        static const wchar_t* find_closing_element(const wchar_t* sectionStart,
                                           const wchar_t* sectionEnd,
                                           const wchar_t* elementTag,
                                           const size_t elementTagLength);
        /**Searches for an attribute inside of an element.
           @returns The position of the attribute (or NULL if not found).
           @param text The start of the element section.
           @param tag The inner tag to search for (e.g., \"bgcolor\").
           @param tagSize The length of the attribute to search for.
           @param allowQuotedTags Set this parameter to true for tags that are inside of quotes
               (e.g., style values like \"font-weight\", as in <span style=\"font-weight: bold;\">).
               To find \"font-weight\" inside of the style tag, this parameter should be true. Usually this would be false.*/
        static const wchar_t* find_tag(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags);
        /**Searches a buffer range for a bookmark (e.g., "<a name="citation" />").
           @param sectionStart The start of the HTML buffer.
           @param sectionEnd The end of the HTML buffer.
           @returns The start of the element (i.e., "<a name="citation" /> and the bookmark name (e.g., "citation").*/
        static const std::pair<const wchar_t*,std::wstring> find_bookmark(const wchar_t* sectionStart,
                                            const wchar_t* sectionEnd);
        /**Searches for a single character in a string, but making sure that
           it is not inside of a pair of double or single quotes. This is specifically tailored for
           " and ' type of quotes used for HTML attributes.
           @param string The string to search in.
           @param ch The character to search for.
           @returns The (pointer) position of where the character is, or NULL if not found.*/
        static const wchar_t* strchr_not_quoted(const wchar_t* string, const wchar_t ch);
        /**Searches for substring in string (case-insensitive), but make sure that
           what you are searching for is not in quotes. This is specifically tailored for
           " and ' type of quotes used for HTML attributes.
           @param string The string to search in.
           @param stringSize The length of string to search within.
           @param strSearch The substring to search for.
           @param strSearchSize The length of strSearch.
           @returns The (pointer) position of where the character is, or NULL if not found.*/
        static const wchar_t* stristr_not_quoted(
            const wchar_t* string, const size_t stringSize,
            const wchar_t* strSearch, const size_t strSearchSize);
        /**@returns The charset from the meta section of an HTML buffer.
           @param pageContent The meta section to analyze.
           @param length The length of pageContent.*/
        static std::string parse_charset(const char* pageContent, const size_t length);
    protected:
        static std::wstring convert_symbol_font_section(const std::wstring& symbolFontText);
        void parse_raw_text(const wchar_t* text, size_t textSize);

        size_t m_is_in_preformatted_text_block_stack;
        static const html_utilities::symbol_font_table SYMBOL_FONT_TABLE;
        static const html_utilities::html_entity_table HTML_TABLE_LOOKUP;
        };
    }

/** @}*/

#endif //__HTML_EXTRACT_TEXT_H__
